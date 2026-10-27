module;

#include "modules/prelude.h"

#include "support/log.h"
#include "support/messages.h"

module catter;

import :session.data;
import :session.ipc;
import :session.session;

namespace catter::ipc {
using namespace data;

/// The build command is started with parent 0: no command started it.
static std::optional<ipcid_t> parent_of(ipcid_t parent_id) {
    if(parent_id == 0) {
        return std::nullopt;
    }
    return parent_id;
}

kota::task<ipcid_t> Service::create(ipcid_t parent_id) noexcept {
    this->parent_id = parent_id;
    co_return id;
}

kota::task<action> Service::make_decision(command cmd) noexcept {
    co_return co_await session.decide(id, parent_of(parent_id), std::move(cmd));
}

kota::task<> Service::finish(process_result result) noexcept {
    co_await session.exited(id, std::move(result));
}

kota::task<> Service::report_error(ipcid_t parent_id, std::string error_msg) noexcept {
    co_await session.failed(id, parent_of(parent_id), std::move(error_msg));
}

kota::task<void> accept(std::unique_ptr<Service> service, kota::pipe client) {
    kota::ipc::BincodePeer peer(kota::event_loop::current(),
                                std::make_unique<kota::ipc::StreamTransport>(std::move(client)));
    using Context = kota::ipc::BincodePeer::RequestContext;

#ifdef CATTER_MAC
    // The program the proxy started, until it tells how it ended: macOS has
    // nothing to end it with its proxy.
    std::optional<int64_t> running;
    using Started = Request<RequestType::STARTED>;
    peer.on_notification(Started::method,
                         [&](const Started::Params& started) { running = started.pid; });
#endif

    peer.on_request(Request<RequestType::CREATE>::method,
                    [&](const Context& ctx, Request<RequestType::CREATE>::Params params)
                        -> kota::ipc::RequestResult<Request<RequestType::CREATE>> {
                        co_return co_await service->create(params);
                    });

    peer.on_request(
        Request<RequestType::MAKE_DECISION>::method,
        [&](const Context& ctx, const Request<RequestType::MAKE_DECISION>::Params& params)
            -> kota::ipc::RequestResult<Request<RequestType::MAKE_DECISION>> {
            co_return co_await service->make_decision(params);
        });

    peer.on_request(
        Request<RequestType::REPORT_ERROR>::method,
        [&](const Context& ctx, const Request<RequestType::REPORT_ERROR>::Params& params)
            -> kota::ipc::RequestResult<Request<RequestType::REPORT_ERROR>> {
            co_await service->report_error(params.parent_id, params.error_msg);
            co_return nullptr;
        });

#ifdef CATTER_WINDOWS
    peer.on_request(Request<RequestType::STARTED>::method,
                    [&](const Context& ctx, const Request<RequestType::STARTED>::Params& params)
                        -> kota::ipc::RequestResult<Request<RequestType::STARTED>> {
                        co_return co_await service->started(params.pid);
                    });
#else
    peer.on_request(Request<RequestType::FINISH>::method,
                    [&](const Context& ctx, const Request<RequestType::FINISH>::Params& params)
                        -> kota::ipc::RequestResult<Request<RequestType::FINISH>> {
#ifdef CATTER_MAC
                        running.reset();
#endif
                        co_await service->finish(params);
                        co_return nullptr;
                    });
#endif

    co_await peer.run();
    LOG_INFO("IPC peer disconnected");
#ifdef CATTER_WINDOWS
    co_await service->wait_started();
#endif
#ifdef CATTER_MAC
    if(running) {
        LOG_WARN("The proxy of program {} is gone: the program is killed", *running);
        ::kill(static_cast<pid_t>(*running), SIGKILL);
    }
#endif
    co_return;
}

}  // namespace catter::ipc
