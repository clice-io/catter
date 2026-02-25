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
#include <vector>

#include <eventide/async/process.h>
#include <eventide/async/stream.h>
#include <eventide/async/loop.h>

#include "ipc.h"
#include "util/data.h"
#include "util/function_ref.h"
#include "util/crossplat.h"
#include "util/eventide.h"
#include "config/ipc.h"
#include "config/catter-proxy.h"

namespace catter {

template <typename ServiceFactoryResult>
struct ServiceFactoryLike_helper : std::false_type {};

template <typename ServiceType>
    requires std::derived_from<ServiceType, ipc::Service>
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

    virtual bool start(data::ServiceMode mode) = 0;
    virtual void finish(int64_t code) = 0;

    /**
     * Run a session with the given shell and service factory.
     * @param factory The factory should be a callable that takes an ipcid_t and returns a
     * unique_ptr to a Service instance.
     */
    template <typename ServiceFactoryType>
        requires ServiceFactoryLike<ServiceFactoryType>
    void run(const std::vector<std::string>& shell, ServiceFactoryType&& factory) {
#ifndef _WIN32
        if(std::filesystem::exists(config::ipc::PIPE_NAME)) {
            std::filesystem::remove(config::ipc::PIPE_NAME);
        }
#endif
        auto acc_ret = eventide::pipe::listen(config::ipc::PIPE_NAME,
                                              eventide::pipe::options(),
                                              default_loop());

        if(!acc_ret) {
            throw std::runtime_error(
                std::format("Failed to create acceptor: {}", acc_ret.error().message()));
        }

        this->acc = std::make_unique<Acceptor>(std::move(*acc_ret));

        std::string executable;
        std::vector<std::string> args;

        using Result = ServiceFactoryResult<ServiceFactoryType>;
        if constexpr(std::convertible_to<Result, std::unique_ptr<ipc::DefaultService>>) {
            executable = (util::get_catter_root_path() / config::proxy::EXE_NAME).string();
            args = {executable, "-p", "0", "--exec", shell[0], "--"};
            append_range_to_vector(args, shell);
            if(!this->start(data::ServiceMode::DEFAULT)) {
                throw std::runtime_error("Failed to start session in DEFAULT mode");
            }
        } else {
            static_assert(false, "Unsupported service factory type");
        }

        auto acceptor = [&](data::ipcid_t id, eventide::pipe&& client) -> eventide::task<void> {
            return ipc::accept(factory(id), std::move(client));
        };

        auto loop_task = this->loop(acceptor);
        auto spawn_task = this->spawn(executable, args);

        default_loop().schedule(loop_task);
        default_loop().schedule(spawn_task);

        default_loop().run();

        loop_task.result();  // Propagate exceptions from loop task
        this->finish(spawn_task.result());
    }

private:
    eventide::task<void>
        loop(util::function_ref<eventide::task<void>(data::ipcid_t, eventide::pipe&&)> acceptor);

    eventide::task<int64_t> spawn(std::string executable, std::vector<std::string> args);

    std::unique_ptr<Acceptor> acc = nullptr;
};

}  // namespace catter
