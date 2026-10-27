module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:session.ipc;

namespace catter::ipc {

class ServiceBase {
public:
    ServiceBase() = default;
    ServiceBase(const ServiceBase&) = default;
    ServiceBase(ServiceBase&&) = default;
    ServiceBase& operator= (const ServiceBase&) = default;
    ServiceBase& operator= (ServiceBase&&) = default;

    virtual ~ServiceBase() = default;
};

class InjectService : public ServiceBase {
public:
    InjectService() = default;
    virtual ~InjectService() override = default;

    virtual kota::task<data::ipcid_t> create(data::ipcid_t parent_id) noexcept = 0;
    virtual kota::task<data::action> make_decision(data::command cmd) noexcept = 0;
    virtual kota::task<> finish(data::process_result result) noexcept = 0;
    virtual kota::task<> report_error(data::ipcid_t parent_id, std::string error_msg) noexcept = 0;

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
};

kota::task<void> accept(std::unique_ptr<InjectService> service, kota::pipe client);

}  // namespace catter::ipc
