#include "session/ipc.h"

#include <cassert>
#include <cstddef>
#include <format>
#include <memory>
#include <new>
#include <optional>
#include <print>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#ifdef CATTER_MAC
#include <csignal>
#endif
#include <kota/support/functional.h>
#include <kota/support/type_traits.h>
#include <kota/meta/enum.h>
#include <kota/ipc/codec/bincode.h>

#include "session/data.h"
#include "support/log.h"

namespace catter::ipc {
using namespace data;

kota::task<void> accept(std::unique_ptr<InjectService> service, kota::pipe client) {
    kota::ipc::BincodePeer peer(kota::event_loop::current(),
                                std::make_unique<kota::ipc::StreamTransport>(std::move(client)));
    using Context = kota::ipc::BincodePeer::RequestContext;

#ifdef CATTER_MAC
    // The program the proxy started, until it tells how it ended: macOS has
    // nothing to end it with its proxy.
    std::optional<int64_t> running;
    peer.on_notification(Started::method, [&](const Started& started) { running = started.pid; });
#endif

    peer.on_request(Request<RequestType::CHECK_MODE>::method,
                    [&](const Context& ctx, Request<RequestType::CHECK_MODE>::Params params)
                        -> kota::ipc::RequestResult<Request<RequestType::CHECK_MODE>> {
                        co_return params == data::ServiceMode::INJECT;
                    });

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

    peer.on_request(Request<RequestType::FINISH>::method,
                    [&](const Context& ctx, const Request<RequestType::FINISH>::Params& params)
                        -> kota::ipc::RequestResult<Request<RequestType::FINISH>> {
#ifdef CATTER_MAC
                        running.reset();
#endif
                        co_await service->finish(params);
                        co_return nullptr;
                    });

    peer.on_request(
        Request<RequestType::REPORT_ERROR>::method,
        [&](const Context& ctx, const Request<RequestType::REPORT_ERROR>::Params& params)
            -> kota::ipc::RequestResult<Request<RequestType::REPORT_ERROR>> {
            co_await service->report_error(params.parent_id, params.error_msg);
            co_return nullptr;
        });

    co_await peer.run();
    LOG_INFO("IPC peer disconnected");
#ifdef CATTER_MAC
    if(running) {
        LOG_WARN("The proxy of program {} is gone: the program is killed", *running);
        ::kill(static_cast<pid_t>(*running), SIGKILL);
    }
#endif
    co_return;
}

}  // namespace catter::ipc
