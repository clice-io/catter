module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:session.ipc;

import :session.session;

namespace catter::ipc {

/// A client of the session's pipe, which reports a command of the build to
/// the session and asks what it is to do: catter-proxy on Unix, the hook on
/// Windows. A client reports one command, `id`.
class Service {
public:
    Service(Session& session, data::ipcid_t id) noexcept : session(session), id(id) {}

    Service(const Service&) = delete;
    Service& operator= (const Service&) = delete;

    virtual ~Service() = default;

    /// `parent_id` is the command that started this one, 0 for the build
    /// command.
    kota::task<data::ipcid_t> create(data::ipcid_t parent_id) noexcept;

    virtual kota::task<data::action> make_decision(data::command cmd) noexcept;

    kota::task<> finish(data::process_result result) noexcept;

    kota::task<> report_error(data::ipcid_t parent_id, std::string error_msg) noexcept;

#ifdef CATTER_WINDOWS
    /// The hook started the command's program suspended, as process `pid`:
    /// whether it is made to run as decided, which the hook resumes then, or
    /// cannot run, which the hook ends.
    virtual kota::task<bool> started(std::int64_t pid) noexcept = 0;

    /// Waits for the program started to exit, and finishes the command as
    /// finish() does; at once when none was. The hook goes once it has
    /// started the program.
    virtual kota::task<> wait_started() noexcept = 0;
#endif

protected:
    Session& session;

    data::ipcid_t id;

    data::ipcid_t parent_id = 0;
};

kota::task<void> accept(std::unique_ptr<Service> service, kota::pipe client);

}  // namespace catter::ipc
