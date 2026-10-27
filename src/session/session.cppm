module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:session.session;

import :session.handler;
import :session.ipc;

namespace catter {

template <typename ServiceFactoryResult>
struct ServiceFactoryLike_helper : std::false_type {};

template <typename ServiceType>
    requires std::derived_from<ServiceType, ipc::ServiceBase>
struct ServiceFactoryLike_helper<std::unique_ptr<ServiceType>> : std::true_type {};

template <typename ServiceFactoryType>
using ServiceFactoryResult = std::invoke_result_t<ServiceFactoryType, data::ipcid_t>;

template <typename ServiceFactoryType>
concept ServiceFactoryLike =
    std::invocable<ServiceFactoryType, data::ipcid_t> &&
    ServiceFactoryLike_helper<ServiceFactoryResult<ServiceFactoryType>>::value;

class Session {
public:
    using PipeAcceptor = kota::acceptor<kota::pipe>;
    using ClientAcceptor = kota::function<kota::task<void>(data::ipcid_t, kota::pipe&&)>;
    using Start = kota::function<kota::task<kota::process::exit_status>()>;

    struct RunPlan {
        /// Starts the build and waits for its end.
        Start start;
        /// The pipe the session listens on, which the build reports its commands to.
        std::string pipe_name;
        ClientAcceptor callback;
    };

    /// Create a run plan with the given start and client accepted callback.
    /// @param start Starts the build and waits for its end.
    /// @param pipe_name The pipe to listen on, which the build reports its commands to.
    /// @param factory The factory for creating service instances when a client is accepted.
    /// @return A run plan containing the start and client accepted callback.
    template <typename ServiceFactoryType>
        requires ServiceFactoryLike<std::decay_t<ServiceFactoryType>>
    static auto make_run_plan(Start start, std::string pipe_name, ServiceFactoryType&& factory) {
        return RunPlan{
            .start = std::move(start),
            .pipe_name = std::move(pipe_name),
            .callback =
                [factory = std::forward<ServiceFactoryType>(factory)](data::ipcid_t id,
                                                                      kota::pipe&& client) {
                    return ipc::accept(factory(id), std::move(client));
                },
        };
    }

    /// Run the session with the given run plan.
    /// @param run_plan The run plan containing the start and service factory.
    /// @return How the build ended.
    kota::task<kota::process::exit_status> run(RunPlan run_plan);

    /// The id of a new command: each client that connects takes one, and on
    /// Windows the build command, which catter starts itself.
    data::ipcid_t new_id() noexcept;

private:
    kota::task<void> loop(ClientAcceptor acceptor);

    kota::task<kota::process::exit_status> start(Start start);

private:
    std::unique_ptr<PipeAcceptor> acc = nullptr;

    data::ipcid_t last_id = 0;
};

}  // namespace catter
