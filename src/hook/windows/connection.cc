#include "hook/windows/connection.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <windows.h>
#include <kota/support/utf16.h>

namespace catter::hook {

std::optional<Connection> Connection::open(std::string_view pipe) {
    std::wstring name;
    kota::to_utf16(name, pipe);
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
            return Connection(handle);
        }
        // Every instance of the pipe is taken by a command being decided.
        if(GetLastError() != ERROR_PIPE_BUSY ||
           !WaitNamedPipeW(name.c_str(), NMPWAIT_WAIT_FOREVER)) {
            return std::nullopt;
        }
    }
}

Connection::Connection(Connection&& other) noexcept :
    pipe(std::exchange(other.pipe, INVALID_HANDLE_VALUE)), last_id(other.last_id),
    codec(std::move(other.codec)), parser(std::move(other.parser)),
    unread(std::move(other.unread)) {}

Connection::~Connection() {
    if(pipe != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe);
    }
}

bool Connection::send(std::string_view payload) {
    auto frame = kota::ipc::frame_header(payload.size());
    frame += payload;
    DWORD written = 0;
    return WriteFile(pipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr) &&
           written == frame.size();
}

std::optional<std::string> Connection::receive() {
    std::array<char, 4096> buffer;
    while(true) {
        auto step = parser.feed(unread);
        unread.erase(0, step.consumed);
        if(step.frame) {
            if(!*step.frame) {
                return std::nullopt;
            }
            return std::move(**step.frame);
        }
        DWORD read = 0;
        if(!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) ||
           read == 0) {
            return std::nullopt;
        }
        unread.append(buffer.data(), read);
    }
}

}  // namespace catter::hook
