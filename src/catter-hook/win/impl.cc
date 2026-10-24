
#include <algorithm>
#include <atomic>
#include <cassert>
#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <io.h>
#include <uv.h>
#include <cpptrace/exceptions.hpp>
#include <kota/async/async.h>
#include <kota/meta/enum.h>

#include "util/crossplat.h"
#include "util/data.h"
#include "util/encoding.h"
#include "util/exception.h"
#include "util/guard.h"
#include "util/kotatsu.h"
#include "util/log.h"
#include "win/env.h"
#include "win/inject.h"
#include "winapi/win32.h"

namespace catter::proxy::hook {

namespace {
std::wstring cmdline_of(const catter::data::command& cmd) {
    std::string full_cmd;
    for(const auto& arg: cmd.args) {
        full_cmd += win::quote_win32_arg(arg) + " ";
    }
    return util::to_wide(full_cmd);
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

/// The environment block CreateProcessW takes with CREATE_UNICODE_ENVIRONMENT.
std::vector<wchar_t> build_environment_block(const std::vector<std::string>& env) {
    std::vector<wchar_t> env_block;
    for(const auto& entry: env) {
        auto wide = util::to_wide(entry);
        env_block.insert(env_block.end(), wide.begin(), wide.end());
        env_block.push_back(L'\0');
    }

    env_block.push_back(L'\0');
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
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   std::format("Failed to create {} capture pipe", name));
    }

    if(!SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) {
        auto err = GetLastError();
        CloseHandle(read);
        CloseHandle(write);
        throw catter::system_error(
            err,
            std::system_category(),
            std::format("Failed to disable inheritance for {} capture pipe", name));
    }

    return AnonymousPipe{.read = win::Handle(read), .write = win::Handle(write)};
}

kota::pipe open_capture_pipe(win::Handle pipe, std::string_view name, kota::event_loop& loop) {
    auto fd = uv_open_osfhandle(pipe.get());
    if(fd < 0) {
        throw cpptrace::runtime_error(
            std::format("Failed to convert {} pipe handle to CRT fd", name));
    }

    auto opened = kota::pipe::open(fd, kota::pipe::options{}, loop);
    if(!opened) {
        throw cpptrace::runtime_error(
            std::format("{} pipe open failed: {}", name, opened.error().message()));
    }
    pipe.release();  // Ownership transferred to kotatsu

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
                      catter::system_error(GetLastError(), std::system_category()).what());
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

class ProcessWaiter {
public:
    ProcessWaiter(HANDLE process_handle, kota::relay relay) noexcept :
        process_handle(process_handle), relay(std::move(relay)) {}

    ProcessWaiter(const ProcessWaiter&) = delete;
    ProcessWaiter(ProcessWaiter&&) = default;
    ProcessWaiter& operator= (const ProcessWaiter&) = delete;
    ProcessWaiter& operator= (ProcessWaiter&&) = default;

    ~ProcessWaiter() {
        if(this->valid()) {
            UnregisterWaitEx(this->wait_handle, INVALID_HANDLE_VALUE);
        }
    }

    bool await_ready() noexcept {
        this->init();
        assert(this->valid() && "ProcessExitState wait handle not initialized");
        return false;
    }

    void await_suspend(std::coroutine_handle<> awaiter) noexcept {
        this->awaiter_handle = awaiter;
        return;
    }

    kota::result<kota::process::exit_status> await_resume() noexcept {
        DWORD exit_code = 0;
        if(!GetExitCodeProcess(this->process_handle, &exit_code)) {
            return kota::outcome_error(kota::error(uv_translate_sys_error(GetLastError())));
        } else {
            return kota::process::exit_status{
                .status = static_cast<int64_t>(exit_code),
                .term_signal = 0,  // Windows does not have Unix-style signals
            };
        }
    }

private:
    bool valid() const noexcept {
        return this->process_handle != INVALID_HANDLE_VALUE;
    }

    void init() noexcept {
        if(!RegisterWaitForSingleObject(
               &this->wait_handle,
               this->process_handle,
               [](void* context, BOOLEAN did_timeout) {
                   auto* self = static_cast<ProcessWaiter*>(context);
                   self->relay.send([self]() { self->awaiter_handle.resume(); });
               },
               this,
               INFINITE,
               WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE)) {
            // libuv handle this case by aborting the process, so we do the same
            std::println("Failed to call RegisterWaitForSingleObject: {}, aborting",
                         catter::system_error(GetLastError(), std::system_category()).what());
            std::abort();
        }
    }

    HANDLE process_handle;
    kota::relay relay;
    HANDLE wait_handle{};
    std::coroutine_handle<> awaiter_handle{};
};

kota::task<int64_t, kota::error> wait_for_process_exit(RunningProcess process,
                                                       kota::event_loop* loop) noexcept {
    auto wait_ret = co_await ProcessWaiter(process.process_handle(), loop->create_relay());
    if(!wait_ret) {
        co_return kota::outcome_error(wait_ret.error());
    }
    co_return wait_ret->status;
}

StartedProcess start_process(data::command cmd,
                             data::ipcid_t id,
                             const std::string& pipe_name,
                             std::string proxy_path) {
    auto env = std::move(cmd.env);
    upsert_environment_variable(env, win::ENV_VAR_IPC_ID<char>, std::to_string(id));
    upsert_environment_variable(env, win::ENV_VAR_PROXY_PATH<char>, proxy_path);
    upsert_environment_variable(env, win::ENV_VAR_IPC_PIPE<char>, pipe_name);

    auto env_block = build_environment_block(env);  // Double null termination

    auto stdout_pipe = create_capture_pipe("stdout");
    auto stderr_pipe = create_capture_pipe("stderr");

    STARTUPINFOEXW si{
        .StartupInfo =
            {
                          .cb = sizeof(STARTUPINFOEXW),
                          .dwFlags = STARTF_USESTDHANDLES,
                          .hStdInput = GetStdHandle(STD_INPUT_HANDLE),
                          .hStdOutput = stdout_pipe.write.get(),
                          .hStdError = stderr_pipe.write.get(),
                          },
    };
    PROCESS_INFORMATION pi{};

    // The command inherits its standard handles only. It would inherit every
    // inheritable handle of the proxy otherwise, catter's own pipes among
    // them, and hold the session open as long as it, or a server it started,
    // lives.
    std::vector<HANDLE> inherited;
    for(HANDLE handle:
        {si.StartupInfo.hStdInput, si.StartupInfo.hStdOutput, si.StartupInfo.hStdError}) {
        DWORD flags = 0;
        if(handle != nullptr && handle != INVALID_HANDLE_VALUE &&
           GetHandleInformation(handle, &flags) && (flags & HANDLE_FLAG_INHERIT) &&
           std::ranges::find(inherited, handle) == inherited.end()) {
            inherited.push_back(handle);
        }
    }
    SIZE_T attributes_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributes_size);
    std::vector<std::byte> attributes(attributes_size);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if(!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attributes_size)) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to initialize the process attributes");
    }
    auto attributes_guard =
        util::make_guard([&]() noexcept { DeleteProcThreadAttributeList(si.lpAttributeList); });
    if(!UpdateProcThreadAttribute(si.lpAttributeList,
                                  0,
                                  PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                  inherited.data(),
                                  inherited.size() * sizeof(HANDLE),
                                  nullptr,
                                  nullptr)) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to set the inherited handles");
    }

    auto cmdline = cmdline_of(cmd);
    auto executable = util::to_wide(cmd.executable);
    auto cwd = util::to_wide(cmd.cwd);

    LOG_INFO("| -> Catter-Proxy Final Executing command: \n    exe = {} \n    args = {}",
             cmd.executable,
             util::from_wide(cmdline));

    if(!CreateProcessW(executable.c_str(),
                       cmdline.data(),
                       nullptr,
                       nullptr,
                       TRUE,
                       CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                       env_block.data(),
                       cwd.empty() ? nullptr : cwd.c_str(),
                       &si.StartupInfo,
                       &pi)) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to create process");
    }

    RunningProcess process = {pi};

    if(!try_inject(process.process_handle(),
                   catter::util::get_catter_root_path() / win::DLL_NAME)) {
        throw cpptrace::runtime_error("Failed to inject DLL into target process");
    }

    if(ResumeThread(process.thread_handle()) == static_cast<DWORD>(-1)) {
        throw catter::system_error(GetLastError(),
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

kota::task<data::process_result>
    run(data::command cmd, data::ipcid_t id, std::string pipe_name, std::string proxy_path) {

    return capture_process_result(
        [cmd, id, pipe_name, proxy_path](kota::event_loop& loop) mutable -> catter::process_info {
            LOG_INFO("new command id is: {}", id);
            auto started = start_process(std::move(cmd), id, pipe_name, std::move(proxy_path));

            return {
                .wait_task = wait_for_process_exit(std::move(started.process), &loop),
                .stdout_pipe = open_capture_pipe(std::move(started.stdout_read), "stdout", loop),
                .stderr_pipe = open_capture_pipe(std::move(started.stderr_read), "stderr", loop),
            };
        });
};
};  // namespace catter::proxy::hook
