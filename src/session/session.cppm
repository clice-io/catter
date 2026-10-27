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
    explicit Session(Handler& handler) : handler(handler) {}

    /// The id of a new command.
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

private:
    Handler& handler;

    std::exception_ptr error;

    data::ipcid_t last_id = 0;
};

/// A way to capture a build: it starts the build, and reports each command the
/// build starts to the session.
class Backend {
public:
    virtual ~Backend() = default;

    /// Its name, which the handlers are told.
    virtual std::string_view name() const noexcept = 0;

    /// Whether the commands it reports wait for the handlers' decisions and
    /// run as they decide: a backend that only observes the build hears of
    /// each command once it runs.
    virtual bool follows_decisions() const noexcept = 0;

    /// Runs `build`, its commands reported to `session`: how the build ended.
    virtual kota::task<kota::process::exit_status> run(const Build& build,
                                                       Session& session) const = 0;
};

/// Runs the session of `handler` over `backend`: the build `on_start` gives,
/// if any, its commands decided by `handler`, and then its end. How the build
/// ended; one that did not start ended with 0.
kota::task<kota::process::exit_status> run_session(Handler& handler,
                                                   const Backend& backend,
                                                   Build build);

}  // namespace catter
