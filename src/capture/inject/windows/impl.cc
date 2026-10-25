
#include <atomic>
#include <cassert>
#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <uv.h>
#include <cpptrace/exceptions.hpp>
#include <kota/async/async.h>
#include <kota/meta/enum.h>

#include "capture/inject/windows/inject.h"
#include "session/data.h"
#include "support/config/hook_windows.h"
#include "support/encoding.h"
#include "support/exception.h"
#include "support/log.h"
#include "support/winapi/win32.h"

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

/// The proxy's standard handle `which`, duplicated for the command to inherit
/// as its own; null when the proxy has no valid one.
win::Handle std_handle_for_command(DWORD which) {
    HANDLE own = GetStdHandle(which);
    // INVALID_HANDLE_VALUE is also the pseudo handle of the current process,
    // which DuplicateHandle would duplicate.
    if(own == INVALID_HANDLE_VALUE) {
        return {};
    }

    // The proxy's own handle is not inherited beside the command's copy: a
    // server the command starts would keep that second copy, and with it
    // whatever reads the build's output, open for as long as it lives.
    SetHandleInformation(own, HANDLE_FLAG_INHERIT, 0);
    HANDLE duplicate = nullptr;
    if(!DuplicateHandle(GetCurrentProcess(),
                        own,
                        GetCurrentProcess(),
                        &duplicate,
                        0,
                        TRUE,
                        DUPLICATE_SAME_ACCESS)) {
        // None, or one the build passed without letting the proxy inherit it.
        return {};
    }
    return win::Handle(duplicate);
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

RunningProcess start_process(data::command cmd,
                             data::ipcid_t id,
                             const std::string& pipe_name,
                             std::string proxy_path) {
    auto env = std::move(cmd.env);
    upsert_environment_variable(env, win::ENV_VAR_IPC_ID<char>, std::to_string(id));
    upsert_environment_variable(env, win::ENV_VAR_PROXY_PATH<char>, proxy_path);
    upsert_environment_variable(env, win::ENV_VAR_IPC_PIPE<char>, pipe_name);

    auto env_block = build_environment_block(env);  // Double null termination

    auto input = std_handle_for_command(STD_INPUT_HANDLE);
    auto output = std_handle_for_command(STD_OUTPUT_HANDLE);
    auto error = std_handle_for_command(STD_ERROR_HANDLE);
    STARTUPINFOW si{
        .cb = sizeof(STARTUPINFOW),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdInput = input.get(),
        .hStdOutput = output.get(),
        .hStdError = error.get(),
    };
    PROCESS_INFORMATION pi{};

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
                       CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                       env_block.data(),
                       cwd.empty() ? nullptr : cwd.c_str(),
                       &si,
                       &pi)) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to create process");
    }

    RunningProcess process = {pi};

    inject(process.process_handle(), cmd.executable);

    if(ResumeThread(process.thread_handle()) == static_cast<DWORD>(-1)) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to resume target process");
    }

    return process;
}
}  // namespace

kota::task<data::process_result>
    run(data::command cmd, data::ipcid_t id, std::string pipe_name, std::string proxy_path) {
    LOG_INFO("new command id is: {}", id);
    auto process = start_process(std::move(cmd), id, pipe_name, std::move(proxy_path));

    auto exit = co_await ProcessWaiter(process.process_handle(),
                                       kota::event_loop::current().create_relay());
    if(!exit) {
        throw cpptrace::runtime_error(
            std::format("Process wait failed `{}`", exit.error().message()));
    }
    co_return data::process_result{.code = exit->status};
};
};  // namespace catter::proxy::hook
