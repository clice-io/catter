module;

#include "support/prelude.h"

#include "support/messages.h"

module catter:protocol.remote_handler;

import :protocol.protocol;
import :session.handler;

namespace catter {

/// A handler in another process, which catter speaks the protocol of
/// protocol.cppm to: one catter starts, over its stdin and stdout, or one that
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

        /// Whether the build's commands follow the handlers' decisions: a
        /// handler that decides is refused when they do not.
        bool decisions;

        /// How long catter waits for an answer, and for a handler it started
        /// to exit once the session is over; zero waits as long as it takes.
        chrono::milliseconds timeout;
    };

    /// Starts the handler `command` runs, outside the session. On Windows the
    /// command line goes to the program as it is; elsewhere it is split as a
    /// POSIX shell splits words, without expanding anything.
    static std::unique_ptr<RemoteHandler> spawn(std::string_view command, Options options);

    /// Connects to the handler listening at `address`, `host:port`, the host
    /// of an IPv6 address in brackets: on this machine, but with
    /// `allow_remote`.
    static async::task<std::unique_ptr<RemoteHandler>> connect(std::string_view address,
                                                               bool allow_remote,
                                                               Options options);

    async::task<std::optional<Build>> on_start(Build build) override;

    async::task<Action> on_command(data::ipcid_t id,
                                   std::optional<data::ipcid_t> parent,
                                   data::command command) override;

    async::task<> on_error(data::ipcid_t id,
                           std::optional<data::ipcid_t> parent,
                           std::string message) override;

    async::task<> on_exit(data::ipcid_t id, data::process_result result) override;

    async::task<> on_finish(data::process_result result) override;

    /// Ends the conversation: the handler reads the end of its input, and
    /// catter waits for one it started to exit, which it kills once the
    /// timeout has passed.
    async::task<> close();

private:
    RemoteHandler(std::unique_ptr<kotatsu::ipc::Transport> transport,
                  async::process process,
                  Options options);

    /// The action on a command: the handler's, or skip from one that does
    /// not decide; none for a command the handler's filter leaves out.
    async::task<std::optional<Action>> decide(data::ipcid_t id,
                                              std::optional<data::ipcid_t> parent,
                                              data::command command);

    template <typename Result, typename Params>
    async::task<Result> request(std::string_view method, std::string_view what, Params params);

    template <typename Params>
    void notify(std::string_view method, const Params& params);

    /// What catter reports of a handler whose conversation has ended, `before`
    /// what.
    std::string stopped_message(std::string_view before) const;

    /// Why the handler left `what` without an answer.
    std::string failure(std::string_view what, const kotatsu::ipc::Error& error) const;

private:
    Options options;

    kotatsu::ipc::JSONPeer peer;

    /// Set once the peer has stopped.
    async::event stopped;

    /// The handler catter started; inert for one it connected to.
    async::process process;

    protocol::InitializeResult subscription{};

    /// The commands the handler was told of, whose exits it is told.
    std::unordered_set<data::ipcid_t> told;
};

}  // namespace catter
