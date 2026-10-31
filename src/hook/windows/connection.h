#pragma once

#include <optional>
#include <string_view>
#include <kotatsu/ipc/blocking_client.h>
#include <kotatsu/ipc/codec/bincode.h>

namespace catter::hook {

/// A connection to the catter session, over which the hook asks what to do
/// with a program it is to start. CreateProcess waits for the answers: a
/// request blocks until its result comes.
using Connection = kotatsu::ipc::BlockingClient<kotatsu::ipc::BincodeCodec>;

/// Connects to `pipe`; none when there is no session to connect to.
std::optional<Connection> connect(std::string_view pipe);

}  // namespace catter::hook
