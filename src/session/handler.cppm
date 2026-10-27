module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:session.handler;

namespace catter {

/// The build a session runs.
struct Build {
    /// The build command, its program first.
    std::vector<std::string> command;

    std::string cwd;
};

/// What a handler decides for a command.
struct Action {
    enum class Type : std::uint8_t {
        /// Run the command as it is.
        Skip,
        /// Do not run the command: it exits 0.
        Drop,
        /// Run `command` in its place.
        Modify,
    };

    Type type = Type::Skip;

    /// For `Modify`, the command that runs instead.
    data::command command;
};

/// Decides what each command of a session does, and is told how the session
/// goes. A handler is created with its own arguments, before the session
/// starts.
///
/// - `on_start` comes first. When it returns no build, none runs, nothing
///   else is called, and catter exits 0.
/// - While the build runs, each command comes to `on_command`, or to
///   `on_error` when catter could not capture it, and to `on_exit` once it has
///   run. The calls of different commands interleave: one can start while
///   another is suspended.
/// - `on_finish` comes last, with how the build ended.
///
/// A replayed build runs nothing: there, the actions do nothing.
///
/// An exception a call throws ends the session, which is how a handler aborts
/// the build: the commands that start after it fail without running, nothing
/// else is called, and once the build has ended catter prints the exception's
/// message and exits 1.
class Handler {
public:
    virtual ~Handler() = default;

    /// The build to run, changed or as it is, or none; as it is by default.
    virtual kota::task<std::optional<Build>> on_start(Build build);

    /// `parent` is the command that started this one, none for the build
    /// command.
    virtual kota::task<Action> on_command(data::ipcid_t id,
                                          std::optional<data::ipcid_t> parent,
                                          data::command command) = 0;

    /// `message` says why catter could not capture the command.
    virtual kota::task<> on_error(data::ipcid_t id,
                                  std::optional<data::ipcid_t> parent,
                                  std::string message) = 0;

    /// Nothing by default.
    virtual kota::task<> on_exit(data::ipcid_t id, data::process_result result);

    virtual kota::task<> on_finish(data::process_result result) = 0;
};

/// Handlers in a fixed order, acting as one. A command goes to each in turn,
/// as the ones before left it, until one drops it; the other events go to
/// every handler. The chain does not own its handlers.
class HandlerChain final : public Handler {
public:
    explicit HandlerChain(std::vector<Handler*> handlers);

    kota::task<std::optional<Build>> on_start(Build build) override;

    kota::task<Action> on_command(data::ipcid_t id,
                                  std::optional<data::ipcid_t> parent,
                                  data::command command) override;

    kota::task<> on_error(data::ipcid_t id,
                          std::optional<data::ipcid_t> parent,
                          std::string message) override;

    kota::task<> on_exit(data::ipcid_t id, data::process_result result) override;

    kota::task<> on_finish(data::process_result result) override;

private:
    std::vector<Handler*> handlers;
};

}  // namespace catter
