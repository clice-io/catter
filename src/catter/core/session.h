#pragma once
#include <cassert>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <format>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <kota/support/functional.h>
#include <kota/async/async.h>

#include "ipc.h"
#include "util/data.h"

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
        std::vector<std::string> args;
    };

    struct RunPlan {
        ProcessLaunchPlan launch_plan;
        ClientAcceptor callback;
    };

    /**
     * Create a run plan with the given launch plan and client accepted callback.
     * @param launch_plan The plan for launching the process.
     * @param factory The factory for creating service instances when a client is accepted.
     * @return A run plan containing the launch plan and client accepted callback.
     */
    template <typename ServiceFactoryType>
        requires ServiceFactoryLike<std::decay_t<ServiceFactoryType>>
    static auto make_run_plan(ProcessLaunchPlan launch_plan, ServiceFactoryType&& factory) {
        return RunPlan{
            .launch_plan = std::move(launch_plan),
            .callback =
                [factory = std::forward<ServiceFactoryType>(factory)](data::ipcid_t id,
                                                                      kota::pipe&& client) {
                    return ipc::accept(factory(id), std::move(client));
                },
        };
    }

    /**
     * Run the session with the given run plan.
     * @param run_plan The run plan containing the launch plan and service factory.
     * @return The exit code of the spawned process.
     */
    int64_t run(RunPlan run_plan);

private:
    kota::task<void> loop(ClientAcceptor acceptor);

    kota::task<int64_t> spawn(std::string executable,
                              std::vector<std::string> args,
                              std::string cwd);

    std::unique_ptr<PipeAcceptor> acc = nullptr;
};

}  // namespace catter
