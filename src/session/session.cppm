module;

#include "modules/prelude.h"

module catter:session.session;

import :session.data;
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

    struct ProcessLaunchPlan {
        std::string cwd;
        std::string executable;
        /// The arguments after argv[0].
        std::vector<std::string> args;
    };

    struct RunPlan {
        ProcessLaunchPlan launch_plan;
        /// The pipe the session listens on, which the launched process reports to.
        std::string pipe_name;
        ClientAcceptor callback;
    };

    /// Create a run plan with the given launch plan and client accepted callback.
    /// @param launch_plan The plan for launching the process.
    /// @param pipe_name The pipe to listen on, which the launched process reports to.
    /// @param factory The factory for creating service instances when a client is accepted.
    /// @return A run plan containing the launch plan and client accepted callback.
    template <typename ServiceFactoryType>
        requires ServiceFactoryLike<std::decay_t<ServiceFactoryType>>
    static auto make_run_plan(ProcessLaunchPlan launch_plan,
                              std::string pipe_name,
                              ServiceFactoryType&& factory) {
        return RunPlan{
            .launch_plan = std::move(launch_plan),
            .pipe_name = std::move(pipe_name),
            .callback =
                [factory = std::forward<ServiceFactoryType>(factory)](data::ipcid_t id,
                                                                      kota::pipe&& client) {
                    return ipc::accept(factory(id), std::move(client));
                },
        };
    }

    /// Run the session with the given run plan.
    /// @param run_plan The run plan containing the launch plan and service factory.
    /// @return How the launched process ended.
    kota::task<kota::process::exit_status> run(RunPlan run_plan);

private:
    kota::task<void> loop(ClientAcceptor acceptor);

    kota::task<kota::process::exit_status> spawn(std::string executable,
                                                 std::vector<std::string> args,
                                                 std::string cwd);

    std::unique_ptr<PipeAcceptor> acc = nullptr;
};

}  // namespace catter
