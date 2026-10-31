#include "hook/windows/connection.h"

#include <optional>
#include <string>
#include <string_view>
#include <windows.h>
#include <kotatsu/support/utf16.h>

namespace catter::hook {

std::optional<Connection> connect(std::string_view pipe) {
    std::wstring name;
    kotatsu::to_utf16(name, pipe);
    while(true) {
        // Not inheritable: no program the build starts gets it.
        HANDLE handle = CreateFileW(name.c_str(),
                                    GENERIC_READ | GENERIC_WRITE,
                                    0,
                                    nullptr,
                                    OPEN_EXISTING,
                                    0,
                                    nullptr);
        if(handle != INVALID_HANDLE_VALUE) {
            return std::optional<Connection>(std::in_place, handle);
        }
        // Every instance of the pipe is taken by a command being decided.
        if(GetLastError() != ERROR_PIPE_BUSY ||
           !WaitNamedPipeW(name.c_str(), NMPWAIT_WAIT_FOREVER)) {
            return std::nullopt;
        }
    }
}

}  // namespace catter::hook
