// clang-format off
// ## Windows Specific Tests
// RUN: %if system-windows %{ %it_catter_hook --test CreateProcessA | FileCheck %s --check-prefix=CHECK-OUTPUT %}
// RUN: %if system-windows %{ %it_catter_hook --test CreateProcessW | FileCheck %s --check-prefix=CHECK-OUTPUT %}

// ## Unix Specific Tests
// RUN: %if !system-windows %{ %it_catter_hook --test execve | FileCheck %s --check-prefix=CHECK-OUTPUT %}
// RUN: %if !system-windows %{ %it_catter_hook --test execv | FileCheck %s --check-prefix=CHECK-OUTPUT %}
// RUN: %if !system-windows %{ %it_catter_hook --test execvp | FileCheck %s --check-prefix=CHECK-OUTPUT %}
// RUN: %if !system-windows %{ %it_catter_hook --test execl | FileCheck %s --check-prefix=CHECK-OUTPUT %}
// RUN: %if !system-windows %{ %it_catter_hook --test posix_spawn | FileCheck %s --check-prefix=CHECK-OUTPUT %}
// RUN: %if !system-windows %{ %it_catter_hook --test posix_spawnp | FileCheck %s --check-prefix=CHECK-OUTPUT %}

// CHECK-OUTPUT: -p 0 --exec /bin/echo -- /bin/echo Hello, World!
// clang-format on
#include <functional>
#include <iostream>
#include <ranges>
#include <string>
#include <format>
#include <print>
#include <unordered_map>
#include <vector>

#include "hook.h"
#include "util/crossplat.h"
#include "util/log.h"
#include "util/option.h"

#ifdef CATTER_WINDOWS

#include <windows.h>

namespace test {

void CreateProcessA() {
    char cmdline[] = "/bin/echo Hello, World!";

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{.cb = sizeof(STARTUPINFOA)};
    if(!CreateProcessA(nullptr, cmdline, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to create process with injected dll");
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void CreateProcessW() {
    wchar_t cmdline[] = L"/bin/echo Hello, World!";
    PROCESS_INFORMATION pi{};
    STARTUPINFOW si{.cb = sizeof(STARTUPINFOW)};
    if(!CreateProcessW(nullptr, cmdline, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to create process with injected dll");
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

std::unordered_map<std::string, std::function<void()>> funcs = {
    {"CreateProcessA", CreateProcessA},
    {"CreateProcessW", CreateProcessW},
};
}  // namespace test
#else
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
extern char** environ;

namespace test {

template <typename... Args>
std::vector<char*> argvify(Args&&... args) {
    return {const_cast<char*>(args)..., nullptr};
}

void execve() {
    char exec_name[] = "/bin/echo";
    auto argv = argvify(exec_name, "Hello, World!");
    if(::execve(exec_name, argv.data(), environ) != 0) {
        throw std::system_error(errno, std::system_category(), "execve failed");
    }
}

void execv() {
    char exec_name[] = "/bin/echo";
    if(::execv(exec_name, argvify(exec_name, "Hello, World!").data()) != 0) {
        throw std::system_error(errno, std::system_category(), "execv failed");
    }
}

void execvp() {
    char exec_name[] = "/bin/echo";
    if(::execvp(exec_name, argvify(exec_name, "Hello, World!").data()) != 0) {
        throw std::system_error(errno, std::system_category(), "execvp failed");
    }
}

void execl() {
    char exec_name[] = "/bin/echo";
    if(::execl(exec_name, exec_name, "Hello, World!", static_cast<char*>(nullptr)) != 0) {
        throw std::system_error(errno, std::system_category(), "execl failed");
    }
}

void posix_spawn() {
    char exec_name[] = "/bin/echo";
    pid_t pid = 0;
    if(::posix_spawn(&pid,
                     exec_name,
                     nullptr,
                     nullptr,
                     argvify(exec_name, "Hello, World!").data(),
                     environ) != 0) {
        throw std::system_error(errno, std::system_category(), "posix_spawn failed");
    }
    int status;
    if(::waitpid(pid, &status, 0) < 0) {
        throw std::system_error(errno, std::system_category(), "waitpid failed");
    }
}

void posix_spawnp() {
    char exec_name[] = "/bin/echo";
    pid_t pid = 0;
    if(::posix_spawnp(&pid,
                      exec_name,
                      nullptr,
                      nullptr,
                      argvify(exec_name, "Hello, World!").data(),
                      environ) != 0) {
        throw std::system_error(errno, std::system_category(), "posix_spawnp failed");
    }
    int status;
    if(::waitpid(pid, &status, 0) < 0) {
        throw std::system_error(errno, std::system_category(), "waitpid failed");
    }
}

std::unordered_map<std::string, std::function<void()>> funcs = {
    {"execve",       execve      },
    {"execv",        execv       },
    {"execvp",       execvp      },
    {"execl",        execl       },
    {"posix_spawn",  posix_spawn },
    {"posix_spawnp", posix_spawnp}
};

}  // namespace test
#endif

int main(int argc, char* argv[]) {
    catter::log::mute_logger();

    try {
        auto args = catter::util::save_argv(argc, argv);

        if(args.size() == 3 && args[1] == "--test") {
            std::string executable = catter::util::get_executable_path().string();

            catter::ipc::data::command cmd{
                .working_dir = std::filesystem::current_path().string(),
                .executable = executable,
                .args = {executable, args[2]},
                .env = catter::util::get_environment(),
            };

            return catter::proxy::hook::run(cmd, 0);
        } else if(args.size() == 2) {
            if(auto it = test::funcs.find(args[1]); it != test::funcs.end()) {
                std::invoke(it->second);
                return 0;
            } else {
                std::println("Unknown function: {}", args[1]);
                return -1;
            }
        } else {
            std::string line;
            for(auto& arg: args) {
                line.append(arg).append(" ");
            }
            line.pop_back();
            std::print("{}", line);
            return 0;
        }
    } catch(const std::exception& e) {
        std::println("Exception: {}", e.what());
        return -1;
    }
}
