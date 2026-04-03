#include "eventide/async/io/loop.h"
#include "eventide/async/runtime/sync.h"
#include "eventide/async/vocab/error.h"
#include <coroutine>
#include <eventide/async/runtime/task.h>
#include <format>
#include <string>
#include <system_error>
#include <filesystem>
#include <vector>
#include <memory>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>
#include <string.h>
#include <io.h>

#include <eventide/reflection/enum.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <uv.h>
#include <windows.h>
#include <libloaderapi.h>
#include <minwindef.h>
#include <Psapi.h>

#include "util/log.h"
#include "util/data.h"
#include "util/crossplat.h"
#include "util/eventide.h"

#include "win/env.h"
#include "win/win32.h"
#include "win/inject.h"

namespace catter::proxy::hook {

namespace {
std::string cmdline_of(const catter::data::command& cmd) noexcept {
    std::string full_cmd;
    for(const auto& arg: cmd.args) {
        full_cmd += win::quote_win32_arg(arg) + " ";
    }
    return full_cmd;
}

bool env_key_equals(std::string_view env_entry, std::string_view key) noexcept {
    const auto separator = env_entry.find('=');
    return separator == key.size() && _strnicmp(env_entry.data(), key.data(), key.size()) == 0;
}

void upsert_environment_variable(std::vector<std::string>& env,
                                 std::string_view key,
                                 std::string value) {
    std::string entry = std::format("{}={}", key, value);
    for(auto& existing: env) {
        if(env_key_equals(existing, key)) {
            existing = std::move(entry);
            return;
        }
    }

    env.push_back(std::move(entry));
}

std::vector<char> build_environment_block(std::vector<std::string> env) {
    size_t env_block_size = 1;
    for(const auto& entry: env) {
        env_block_size += entry.size() + 1;
    }

    std::vector<char> env_block;
    env_block.reserve(env_block_size);

    for(const auto& entry: env) {
        std::span<const char> span(entry.c_str(), entry.size() + 1);
        env_block.append_range(span);
    }

    env_block.push_back('\0');
    return env_block;
}

struct AnonymousPipe {
    win::Handle read{};
    win::Handle write{};
};

AnonymousPipe create_capture_pipe(std::string_view name) {
    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = TRUE,
    };

    HANDLE read = nullptr;
    HANDLE write = nullptr;
    if(!CreatePipe(&read, &write, &sa, 0)) {
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                std::format("Failed to create {} capture pipe", name));
    }

    if(!SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) {
        auto err = GetLastError();
        CloseHandle(read);
        CloseHandle(write);
        throw std::system_error(
            err,
            std::system_category(),
            std::format("Failed to disable inheritance for {} capture pipe", name));
    }

    return AnonymousPipe{.read = win::Handle(read), .write = win::Handle(write)};
}

eventide::pipe open_capture_pipe(win::Handle pipe, std::string_view name) {
    auto fd = uv_open_osfhandle(pipe.get());
    if(fd < 0) {
        throw std::runtime_error(std::format("Failed to convert {} pipe handle to CRT fd", name));
    }

    auto opened = eventide::pipe::open(fd, eventide::pipe::options{}, catter::default_loop());
    if(!opened) {
        throw std::runtime_error(
            std::format("{} pipe open failed: {}", name, opened.error().message()));
    }
    pipe.release();  // Ownership transferred to eventide

    return std::move(*opened);
}

eventide::pipe open_capture_pipe(win::Handle pipe,
                                 std::string_view name,
                                 eventide::event_loop& loop) {
    auto fd = uv_open_osfhandle(pipe.get());
    if(fd < 0) {
        throw std::runtime_error(std::format("Failed to convert {} pipe handle to CRT fd", name));
    }

    auto opened = eventide::pipe::open(fd, eventide::pipe::options{}, loop);
    if(!opened) {
        throw std::runtime_error(
            std::format("{} pipe open failed: {}", name, opened.error().message()));
    }
    pipe.release();  // Ownership transferred to eventide

    return std::move(*opened);
}

class RunningProcess {
public:
    RunningProcess() = default;

    RunningProcess(PROCESS_INFORMATION pi) : process(pi.hProcess), thread(pi.hThread) {}

    RunningProcess(const RunningProcess&) = delete;
    RunningProcess(RunningProcess&&) noexcept = default;
    RunningProcess& operator= (const RunningProcess&) = delete;
    RunningProcess& operator= (RunningProcess&&) noexcept = default;

    ~RunningProcess() {
        cleanup();
    }

    HANDLE process_handle() const noexcept {
        return process.get();
    }

    HANDLE thread_handle() const noexcept {
        return thread.get();
    }

private:
    void cleanup() noexcept {
        if(!process.valid()) {
            return;
        }

        DWORD exit_code = 0;
        if(GetExitCodeProcess(process.get(), &exit_code)) {
            if(exit_code == STILL_ACTIVE) {
                LOG_ERROR("Process is still active during cleanup, terminating it forcefully");
                TerminateProcess(process.get(), static_cast<UINT>(-1));
            }
        } else {
            LOG_ERROR("Failed to get exit code of process during cleanup: {}",
                      std::system_error(GetLastError(), std::system_category()).what());
        }
    }

    win::Handle process = INVALID_HANDLE_VALUE;
    win::Handle thread = INVALID_HANDLE_VALUE;
};

struct StartedProcess {
    RunningProcess process;
    win::Handle stdout_read{};
    win::Handle stderr_read{};
};

class ProcessExitState {
public:
    ProcessExitState(RunningProcess&& process, eventide::event_loop* loop) :
        process(std::move(process)), loop(loop), wait_handle{} {

        HANDLE wait_object;

        if(!RegisterWaitForSingleObject(
               &wait_object,
               process.process_handle(),
               [](void* context, BOOLEAN did_timeout) {
                   auto* self = static_cast<ProcessExitState*>(context);
                   self->loop->post([self]() { self->awaiter_handle.resume(); });
               },
               this,
               INFINITE,
               WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE)) {
            throw std::system_error(GetLastError(),
                                    std::system_category(),
                                    "Failed to register child process exit wait callback");
        }
        wait_handle = win::Handle(wait_object);
    }

    ProcessExitState(const ProcessExitState&) = delete;
    ProcessExitState(ProcessExitState&&) = default;
    ProcessExitState& operator= (const ProcessExitState&) = delete;
    ProcessExitState& operator= (ProcessExitState&&) = default;

    ~ProcessExitState() {
        if(wait_handle.valid()) {
            UnregisterWaitEx(wait_handle.get(), INVALID_HANDLE_VALUE);
        }
    }

    bool await_ready() noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> awaiter) noexcept {
        this->awaiter_handle = awaiter;
        return;
    }

    int64_t await_resume() {
        DWORD exit_code = 0;
        if(!GetExitCodeProcess(this->process.process_handle(), &exit_code)) {
            throw std::system_error(GetLastError(),
                                    std::system_category(),
                                    "Failed to get child process exit code");
        } else {
            return static_cast<int64_t>(exit_code);
        }
    }

private:
    RunningProcess process{};
    eventide::event_loop* loop{};
    win::Handle wait_handle{};
    std::coroutine_handle<> awaiter_handle{};
};

eventide::task<int64_t, eventide::error> wait_for_process_exit(RunningProcess&& process,
                                                               eventide::event_loop* loop) {

    return [](ProcessExitState state) -> eventide::task<int64_t, eventide::error> {
        co_return co_await state;
    }({std::move(process), loop});
}

StartedProcess start_process(data::command cmd, data::ipcid_t id, std::string proxy_path) {
    auto env = std::move(cmd.env);
    upsert_environment_variable(env, win::ENV_VAR_IPC_ID<char>, std::to_string(id));
    upsert_environment_variable(env, win::ENV_VAR_PROXY_PATH<char>, proxy_path);

    auto env_block = build_environment_block(std::move(env));  // Double null termination

    auto stdout_pipe = create_capture_pipe("stdout");
    auto stderr_pipe = create_capture_pipe("stderr");

    STARTUPINFOA si{
        .cb = sizeof(STARTUPINFOA),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdInput = GetStdHandle(STD_INPUT_HANDLE),
        .hStdOutput = stdout_pipe.write.get(),
        .hStdError = stderr_pipe.write.get(),
    };
    PROCESS_INFORMATION pi{};

    std::string cmdline = cmdline_of(cmd);

    LOG_INFO("| -> Catter-Proxy Final Executing command: \n    exe = {} \n    args = {}",
             cmd.executable,
             cmdline);

    if(!CreateProcessA(cmd.executable.c_str(),
                       cmdline.data(),
                       nullptr,
                       nullptr,
                       TRUE,
                       CREATE_SUSPENDED,
                       env_block.data(),
                       cmd.cwd.empty() ? nullptr : cmd.cwd.c_str(),
                       &si,
                       &pi)) {
        throw std::system_error(GetLastError(), std::system_category(), "Failed to create process");
    }

    RunningProcess process = {pi};

    if(!try_inject(process.process_handle(),
                   catter::util::get_catter_root_path() / win::DLL_NAME)) {
        throw std::runtime_error("Failed to inject DLL into target process");
    }

    if(ResumeThread(process.thread_handle()) == static_cast<DWORD>(-1)) {
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to resume target process");
    }

    return StartedProcess{
        .process = std::move(process),
        .stdout_read = std::move(stdout_pipe.read),
        .stderr_read = std::move(stderr_pipe.read),
    };
}
}  // namespace

data::process_result run(data::command cmd, data::ipcid_t id, std::string proxy_path) {

    return capture_process_result(
        [cmd, id, proxy_path](eventide::event_loop& loop) mutable -> catter::process_info {
            LOG_INFO("new command id is: {}", id);
            auto started = start_process(std::move(cmd), id, std::move(proxy_path));

            return {
                .wait_task = wait_for_process_exit(std::move(started.process), &loop),
                .stdout_pipe = open_capture_pipe(std::move(started.stdout_read), "stdout", loop),
                .stderr_pipe = open_capture_pipe(std::move(started.stderr_read), "stderr", loop),
            };
        });
};
};  // namespace catter::proxy::hook
