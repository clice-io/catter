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

#include <eventide/common/functional.h>
#include <eventide/async/async.h>

#include "ipc.h"
#include "util/data.h"
#include "util/eventide.h"
#include "config/ipc.h"

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
    using Acceptor = eventide::acceptor<eventide::pipe>;

    struct ProcessLaunchPlan {
        std::string executable;
        std::vector<std::string> args;
    };

    template <typename ServiceFactoryType>
    struct RunPlan {
        ProcessLaunchPlan launch_plan;
        ServiceFactoryType factory;
    };

    template <typename ServiceFactoryType>
        requires ServiceFactoryLike<std::decay_t<ServiceFactoryType>>
    static auto make_run_plan(ProcessLaunchPlan launch_plan, ServiceFactoryType&& factory) {
        return RunPlan<std::decay_t<ServiceFactoryType>>{
            .launch_plan = std::move(launch_plan),
            .factory = std::forward<ServiceFactoryType>(factory),
        };
    }

    /**
     * Run a session with the given launch plan and service factory.
     * @param factory The factory should be a callable that takes an ipcid_t and returns a
     * unique_ptr to a Service instance.
     */
    template <typename ServiceFactoryType>
        requires ServiceFactoryLike<ServiceFactoryType>
    int64_t run(RunPlan<ServiceFactoryType> run_plan) {
#ifndef _WIN32
        if(std::filesystem::exists(config::ipc::pipe_name())) {
            std::filesystem::remove(config::ipc::pipe_name());
        }
#endif
        auto acc_ret = eventide::pipe::listen(config::ipc::pipe_name(),
                                              eventide::pipe::options(),
                                              default_loop());

        if(!acc_ret) {
            throw std::runtime_error(
                std::format("Failed to create acceptor: {}", acc_ret.error().message()));
        }

        this->acc = std::make_unique<Acceptor>(std::move(*acc_ret));

        auto acceptor = [&](data::ipcid_t id, eventide::pipe&& client) -> eventide::task<void> {
            return ipc::accept(run_plan.factory(id), std::move(client));
        };

        auto loop_task = this->loop(acceptor);
        auto spawn_task = this->spawn(std::move(run_plan.launch_plan.executable),
                                      std::move(run_plan.launch_plan.args));

        default_loop().schedule(loop_task);
        default_loop().schedule(spawn_task);

        default_loop().run();

        loop_task.result();  // Propagate exceptions from loop task
        return spawn_task.result();
    }

private:
    eventide::task<void> loop(
        eventide::function_ref<eventide::task<void>(data::ipcid_t, eventide::pipe&&)> acceptor);

    eventide::task<int64_t> spawn(std::string executable, std::vector<std::string> args);

    std::unique_ptr<Acceptor> acc = nullptr;
};

}  // namespace catter
