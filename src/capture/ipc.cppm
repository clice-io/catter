module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:capture.ipc;

import :session.process;
import :session.session;

/// The requests of the IPC, as kota's peers send and serve them.
namespace kota::ipc::protocol {
using namespace catter::ipc;

template <RequestType Type>
struct RequestTraits<Request<Type>> : Request<Type> {};

}  // namespace kota::ipc::protocol

namespace catter::ipc {

/// The pipe the build reports its commands to `session` over while it runs, one
/// for each session so that sessions running at once never meet: a named pipe
/// on Windows, a socket in catter's data directory elsewhere. Each client is a
/// command, which catter-proxy reports, or on Windows the hook as it starts a
/// program; there catter makes the program the hook started run as decided,
/// and waits for its exit.
class Server {
public:
    using Start = kota::function<kota::task<kota::process::exit_status>()>;

    explicit Server(Session& session);

    Server(const Server&) = delete;
    Server& operator= (const Server&) = delete;

    std::string_view pipe() const noexcept {
        return name;
    }

    /// Runs the build with `start`, and serves the pipe while it runs: how the
    /// build ended.
    kota::task<kota::process::exit_status> serve(Start start);

#ifdef CATTER_WINDOWS
    /// Waits until the exit of each program the hook started that has ended is
    /// told. The build sees a program end before catter does: the commands it
    /// starts after, and its own end, wait for those exits to be told first.
    kota::task<> ended();
#endif

private:
    class Client;

#ifdef CATTER_WINDOWS
    /// A program the hook started whose exit is not told yet.
    struct Untold {
        const kota::process* program;

        /// Set once its exit is told: shared with those waiting for it.
        std::shared_ptr<kota::event> told;
    };
#endif

    kota::task<> accept();

    /// Answers the requests of client `id` until it goes.
    kota::task<> answer(data::ipcid_t id, kota::pipe pipe);

private:
    Session& session;

    std::string name;

    std::optional<kota::acceptor<kota::pipe>> acceptor;

#ifdef CATTER_WINDOWS
    std::list<Untold> untold;
#endif
};

}  // namespace catter::ipc
