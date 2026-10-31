#include <array>
#include <format>
#include <optional>
#include <string>
#include <thread>
#include <variant>
#include <windows.h>
#include <kotatsu/zest/zest.h>
#include <kotatsu/support/utf16.h>
#include <kotatsu/ipc/codec/bincode.h>
#include <kotatsu/ipc/framing.h>

#include "hook/windows/connection.h"
#include "support/messages.h"

namespace ct = catter::hook;

namespace {

/// A pipe of this process's own, named after `test`.
std::string pipe_name(std::string_view test) {
    return std::format(R"(\\.\pipe\catter-connection-tests-{}-{})", GetCurrentProcessId(), test);
}

/// The one instance of pipe `name`, as a session listens on it.
HANDLE listen(std::string_view name) {
    std::wstring wide;
    kotatsu::to_utf16(wide, name);
    return CreateNamedPipeW(wide.c_str(),
                            PIPE_ACCESS_DUPLEX,
                            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                            1,
                            4096,
                            4096,
                            0,
                            nullptr);
}

/// Reads a request `started` from `pipe` and answers it with `result`: the pid
/// it read; none when it read no such request.
std::optional<std::int64_t> answer(HANDLE pipe, bool result) {
    kotatsu::ipc::FrameParser parser;
    std::array<char, 4096> buffer;
    while(true) {
        DWORD read = 0;
        if(!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) ||
           read == 0) {
            return std::nullopt;
        }
        auto step = parser.feed(std::string_view(buffer.data(), read));
        if(!step.frame || !*step.frame) {
            continue;
        }
        kotatsu::ipc::BincodeCodec codec;
        auto incoming = codec.parse_message(std::move(**step.frame));
        auto* request = std::get_if<kotatsu::ipc::IncomingRequest>(&incoming);
        using Started = catter::ipc::Request<catter::ipc::RequestType::STARTED>;
        if(request == nullptr || request->method != Started::method) {
            return std::nullopt;
        }
        auto params = codec.deserialize_value<Started::Params>(request->params);
        auto response = codec.encode_success_response(request->id, result);
        auto frame = kotatsu::ipc::frame_header(response->size()) + *response;
        DWORD written = 0;
        WriteFile(pipe, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr);
        return params ? std::optional(params->pid) : std::nullopt;
    }
}

ZEST_SUITE(win_payload_connection) {
ZEST_CASE(request_gets_its_result) {
    const auto name = pipe_name("request");
    HANDLE server = listen(name);
    ZASSERT(server != INVALID_HANDLE_VALUE);
    std::optional<std::int64_t> asked;
    std::thread session([&] {
        ConnectNamedPipe(server, nullptr);
        asked = answer(server, true);
    });

    auto connection = ct::connect(name);
    ZASSERT(connection.has_value());
    using Started = catter::ipc::Request<catter::ipc::RequestType::STARTED>;
    auto result =
        connection->send_request<Started::Result>(Started::method, Started::Params{.pid = 7});
    session.join();
    CloseHandle(server);

    ZEXPECT(asked == 7);
    ZASSERT(result.has_value());
    ZEXPECT(*result);
};

ZEST_CASE(busy_pipe_is_waited_for) {
    const auto name = pipe_name("busy");
    std::wstring wide;
    kotatsu::to_utf16(wide, name);
    HANDLE server = listen(name);
    ZASSERT(server != INVALID_HANDLE_VALUE);
    HANDLE first = CreateFileW(wide.c_str(),
                               GENERIC_READ | GENERIC_WRITE,
                               0,
                               nullptr,
                               OPEN_EXISTING,
                               0,
                               nullptr);
    ZASSERT(first != INVALID_HANDLE_VALUE);

    bool second = false;
    std::thread waiting([&] { second = ct::connect(name).has_value(); });
    // The only instance is taken until the first client goes.
    ConnectNamedPipe(server, nullptr);
    CloseHandle(first);
    DisconnectNamedPipe(server);
    ConnectNamedPipe(server, nullptr);
    waiting.join();
    CloseHandle(server);

    ZEXPECT(second);
};

ZEST_CASE(no_session_is_no_connection) {
    ZEXPECT(!ct::connect(pipe_name("none")).has_value());
};
};  // ZEST_SUITE(win_payload_connection)

}  // namespace
