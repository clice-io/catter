module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:session.session;

import :session.handler;

namespace catter {

/// The build's commands as a capture backend reports them: each decided by
/// the handlers and told to them. The first exception a handler throws ends
/// the session: no handler is called after it, every command decided after
/// it fails without running, and `rethrow` throws it once the build has
/// ended.
class Session {
public:
    using Start = kota::function<kota::task<kota::process::exit_status>()>;
    using Accept = kota::function<kota::task<void>(data::ipcid_t, kota::pipe&&)>;

    explicit Session(Handler& handler) : handler(handler) {}

    /// The id of a new command: each client of the pipe takes one, and on
    /// Windows the build command, which catter decides itself.
    data::ipcid_t new_id() noexcept;

    /// What the handlers decide for command `id`, which `parent` started, none
    /// for the build command: it runs as it is unless they change it, and a
    /// command that is not to run keeps it as it was sent.
    kota::task<data::action> decide(data::ipcid_t id,
                                    std::optional<data::ipcid_t> parent,
                                    data::command command);

    /// Tells the handlers how command `id` ended.
    kota::task<> exited(data::ipcid_t id, data::process_result result);

    /// Tells the handlers that catter could not capture command `id`:
    /// `message` says why.
    kota::task<> uncaptured(data::ipcid_t id,
                            std::optional<data::ipcid_t> parent,
                            std::string message);

    /// Ends the session with `error`, unless it has ended already.
    void fail(std::exception_ptr error) noexcept;

    /// Throws what ended the session, if anything did.
    void rethrow() const;

    /// Runs the build with `start` and, while it runs, serves each client that
    /// connects to the pipe `pipe_name`, which the build reports its commands
    /// to, with `accept`, as a new command: how the build ended.
    kota::task<kota::process::exit_status> serve(std::string pipe_name, Start start, Accept accept);

private:
    kota::task<void> loop(Accept accept);

    kota::task<kota::process::exit_status> start(Start start);

private:
    Handler& handler;

    std::exception_ptr error;

    std::unique_ptr<kota::acceptor<kota::pipe>> acc = nullptr;

    data::ipcid_t last_id = 0;
};

}  // namespace catter
