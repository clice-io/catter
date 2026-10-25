#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <kota/async/async.h>
#include <kota/ipc/codec/json.h>

#include "handler.h"
#include "protocol.h"

namespace catter::core {

/// A handler in another process, which catter speaks the protocol of
/// protocol.h to: one catter starts, over its stdin and stdout, or one that
/// listens on a TCP port. A failure of the handler (it stops, answers with an
/// error, or not in time) is an exception, which ends the session.
class RemoteHandler final : public Handler {
public:
    struct Options {
        /// How catter names the handler when it reports on it: its command,
        /// or the address it listens at.
        std::string name;

        /// What `initialize` tells the handler of the session.
        std::string session;

        std::string mode;

        /// How long catter waits for an answer, and for a handler it started
        /// to exit once the session is over; zero waits as long as it takes.
        std::chrono::milliseconds timeout;
    };

    /// Starts the handler `command` runs, outside the session. On Windows the
    /// command line goes to the program as it is; elsewhere it is split as a
    /// POSIX shell splits words, without expanding anything.
    static std::unique_ptr<RemoteHandler> spawn(std::string_view command, Options options);

    /// Connects to the handler listening at `port` of the first of `hosts`
    /// that takes the connection.
    static kota::task<std::unique_ptr<RemoteHandler>> connect(std::vector<std::string> hosts,
                                                              int port,
                                                              Options options);

    kota::task<std::optional<Build>> on_start(Build build) override;

    kota::task<Action> on_command(data::ipcid_t id,
                                  std::optional<data::ipcid_t> parent,
                                  data::command command) override;

    kota::task<> on_error(data::ipcid_t id,
                          std::optional<data::ipcid_t> parent,
                          std::string message) override;

    kota::task<> on_exit(data::ipcid_t id, data::process_result result) override;

    kota::task<> on_finish(data::process_result result) override;

    /// Ends the conversation: the handler reads the end of its input, and
    /// catter waits for one it started to exit, which it kills once the
    /// timeout has passed.
    kota::task<> close();

private:
    RemoteHandler(std::unique_ptr<kota::ipc::Transport> transport,
                  kota::process process,
                  Options options);

    /// The action on a command: the handler's, or skip from one that does
    /// not decide.
    kota::task<Action> decide(data::ipcid_t id,
                              std::optional<data::ipcid_t> parent,
                              data::command command);

    template <typename Result, typename Params>
    kota::task<Result> request(std::string_view method, std::string_view what, Params params);

    template <typename Params>
    void notify(std::string_view method, const Params& params);

    /// What catter reports of a handler whose output has ended, `before` what.
    std::string stopped_message(std::string_view before) const;

    /// Why the handler left `what` without an answer.
    std::string failure(std::string_view what, const kota::ipc::Error& error) const;

private:
    Options options;

    /// How the handler's output ended, once it has.
    std::optional<kota::ipc::ReadError> output_end;

    kota::ipc::JsonPeer peer;

    /// Set once the peer has stopped.
    kota::event stopped;

    /// The handler catter started; inert for one it connected to.
    kota::process process;

    protocol::InitializeResult subscription{};

    /// When each command was decided, which its exit is measured from.
    std::unordered_map<data::ipcid_t, std::chrono::steady_clock::time_point> decided;
};

}  // namespace catter::core
