
#include <format>
#include <system_error>
#include <cassert>
#include <string>
#include <stdexcept>

#include <eventide/reflection/enum.h>

#include <windows.h>
#include <libloaderapi.h>
#include <minwindef.h>
#include <Psapi.h>

#include "util/log.h"
#include "win/win32.h"

#include "win/win32.h"

using namespace std::literals;

namespace catter::proxy::hook {
win::Handle RtlCreateUserThread(HANDLE hProcess,
                                LPTHREAD_START_ROUTINE lpBaseAddress,
                                LPVOID lpSpace) {
    // undocumented.ntinternals.com
    using NtStatus = LONG;
    using RtlCreateUserThreadType = NtStatus NTAPI(HANDLE ProcessHandle,
                                                   PSECURITY_DESCRIPTOR SecurityDescriptor,
                                                   BOOL CreateSuspended,
                                                   ULONG StackZeroBits,
                                                   PULONG StackReserved,
                                                   PULONG StackCommit,
                                                   LPTHREAD_START_ROUTINE StartAddress,
                                                   LPVOID StartParameter,
                                                   PHANDLE ThreadHandle,
                                                   LPVOID ClientID);
    HANDLE hRemoteThread = NULL;

    const auto status =
        win::get_function_from_ntdll<RtlCreateUserThreadType>("RtlCreateUserThread")(hProcess,
                                                                                     NULL,
                                                                                     0,
                                                                                     0,
                                                                                     0,
                                                                                     0,
                                                                                     lpBaseAddress,
                                                                                     lpSpace,
                                                                                     &hRemoteThread,
                                                                                     NULL);
    if(status < 0) {
        throw std::runtime_error(std::format(
            "Failed to create remote thread with RtlCreateUserThread, NTSTATUS=0x{:08X}",
            static_cast<unsigned long>(status)));
    }
    return hRemoteThread;
}

win::Handle NtCreateThreadEx(HANDLE hProcess,
                             LPTHREAD_START_ROUTINE lpBaseAddress,
                             LPVOID lpSpace) {
    // undocumented.ntinternals.com
    using NtStatus = LONG;
    using NtCreateThreadExType = NtStatus NTAPI(PHANDLE ThreadHandle,
                                                ACCESS_MASK DesiredAccess,
                                                LPVOID ObjectAttributes,
                                                HANDLE ProcessHandle,
                                                LPTHREAD_START_ROUTINE lpStartAddress,
                                                LPVOID lpParameter,
                                                BOOL CreateSuspended,
                                                DWORD dwStackSize,
                                                DWORD Unknown1,
                                                DWORD Unknown2,
                                                LPVOID Unknown3);
    HANDLE hRemoteThread = NULL;

    const auto status =
        win::get_function_from_ntdll<NtCreateThreadExType>("NtCreateThreadEx")(&hRemoteThread,
                                                                               GENERIC_ALL,
                                                                               NULL,
                                                                               hProcess,
                                                                               lpBaseAddress,
                                                                               lpSpace,
                                                                               FALSE,
                                                                               0,
                                                                               0,
                                                                               0,
                                                                               NULL);
    if(status < 0) {
        throw std::runtime_error(
            std::format("Failed to create remote thread with NtCreateThreadEx, NTSTATUS=0x{:08X}",
                        static_cast<unsigned long>(status)));
    }
    return hRemoteThread;
}

enum class InjectMethod { CreateRemoteThread, NtCreateThread, RtlCreateUserThread };

win::Handle inject(HANDLE hProcess, LPVOID lpSpace, InjectMethod method) {
    auto loadLibraryAddr = reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA);
    switch(method) {
        case InjectMethod::NtCreateThread: {
            return NtCreateThreadEx(hProcess, loadLibraryAddr, lpSpace);
        }

        case InjectMethod::RtlCreateUserThread: {
            return RtlCreateUserThread(hProcess, loadLibraryAddr, lpSpace);
        }
        default: {
            return CreateRemoteThread(hProcess, NULL, 0, loadLibraryAddr, lpSpace, 0, NULL);
        }
    }
}

bool try_inject(HANDLE hProcess, const std::filesystem::path& dll_path) {
    auto dll_path_str = dll_path.string();
    const auto dll_path_size = dll_path_str.size() + 1;
    auto Space =
        win::RemoteMemory(hProcess, NULL, dll_path_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if(WriteProcessMemory(hProcess, Space.get(), dll_path_str.c_str(), dll_path_size, NULL) == 0) {
        throw std::runtime_error("failed to write into process");
    }

    for(auto method: eventide::refl::reflection<InjectMethod>::member_values) {
        try {
            auto thread = inject(hProcess, Space.get(), method);
            if(auto error = win::wait_for_object(thread.get(), 3s);error) {
                throw std::system_error(error.value(), std::system_category(), "Failed to wait for remote thread");
            }
            DWORD remote_exit_code = 0;
            if(!GetExitCodeThread(thread.get(), &remote_exit_code)) {
                throw std::system_error(GetLastError(),
                                        std::system_category(),
                                        "Failed to get remote thread exit code");
            }
            if(remote_exit_code != 0) {
                return true;
            }
        } catch(const std::exception& e) {
            LOG_ERROR("Injection with method {} failed: {}",
                      eventide::refl::enum_name(method),
                      e.what());
        }
    }
    return false;
}
}