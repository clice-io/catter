#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <kota/ipc/codec/bincode.h>
#include <kota/ipc/framing.h>

#include "support/messages.h"

namespace catter::hook {

/// A connection to the catter session, over which the hook asks what to do
/// with a program it is to start. CreateProcess waits for the answers: a
/// request blocks until its result comes.
class Connection {
public:
    /// Connects to `pipe`; none when there is no session to connect to.
    static std::optional<Connection> open(std::string_view pipe);

    Connection(Connection&& other) noexcept;
    Connection& operator= (Connection&&) = delete;

    ~Connection();

    /// Sends request `Type` and waits for its result; none when the session
    /// fails it or goes.
    template <ipc::RequestType Type>
    std::optional<typename ipc::Request<Type>::Result>
        request(const typename ipc::Request<Type>::Params& params) {
        using Request = ipc::Request<Type>;
        last_id += 1;
        auto message = codec.encode_request(last_id, Request::method, params);
        if(!message || !send(*message)) {
            return std::nullopt;
        }
        auto payload = receive();
        if(!payload) {
            return std::nullopt;
        }
        auto incoming = codec.parse_message(std::move(*payload));
        auto* response = std::get_if<kota::ipc::IncomingResponse>(&incoming);
        if(response == nullptr) {
            return std::nullopt;
        }
        auto result = codec.deserialize_value<typename Request::Result>(response->result);
        if(!result) {
            return std::nullopt;
        }
        return std::move(*result);
    }

private:
    explicit Connection(void* pipe) noexcept : pipe(pipe) {}

    /// Writes the frame of `payload`.
    bool send(std::string_view payload);

    /// Reads the payload of the next frame.
    std::optional<std::string> receive();

private:
    void* pipe;

    std::int64_t last_id = 0;

    kota::ipc::BincodeCodec codec;

    kota::ipc::FrameParser parser;

    /// What was read past the last frame.
    std::string unread;
};

}  // namespace catter::hook
