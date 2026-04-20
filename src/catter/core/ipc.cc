#include "ipc.h"

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
#include <kota/support/functional.h>
#include <kota/support/type_traits.h>
#include <kota/meta/enum.h>
#include <kota/ipc/codec/bincode.h>

#include "util/data.h"
#include "util/enum.h"
#include "util/log.h"

namespace catter::ipc {
using namespace data;

kota::task<void> accept(std::unique_ptr<InjectService> service, kota::pipe client) {
    kota::ipc::BincodePeer peer(kota::event_loop::current(),
                                std::make_unique<kota::ipc::StreamTransport>(std::move(client)));
    using Context = kota::ipc::BincodePeer::RequestContext;
    // auto service_mode = Serde<ServiceMode>::deserialize(BufferReader(*sm_packet));
    // assert(service_mode == ServiceMode::INJECT && "Unsupported service mode received");

    peer.on_request<Request<RequestType::CREATE>>(
        [&](const Context& ctx, const Request<RequestType::CREATE>::Params& params)
            -> kota::ipc::RequestResult<Request<RequestType::CREATE>> {
            co_return service->create(params);
        });

    peer.on_request<Request<RequestType::MAKE_DECISION>>(
        [&](const Context& ctx, const Request<RequestType::MAKE_DECISION>::Params& params)
            -> kota::ipc::RequestResult<Request<RequestType::MAKE_DECISION>> {
            co_return service->make_decision(params);
        });

    peer.on_request<Request<RequestType::FINISH>>(
        [&](const Context& ctx, const Request<RequestType::FINISH>::Params& params)
            -> kota::ipc::RequestResult<Request<RequestType::FINISH>> {
            service->finish(params);
            co_return nullptr;
        });

    peer.on_request<Request<RequestType::REPORT_ERROR>>(
        [&](const Context& ctx, const Request<RequestType::REPORT_ERROR>::Params& params)
            -> kota::ipc::RequestResult<Request<RequestType::REPORT_ERROR>> {
            service->report_error(params.parent_id, params.error_msg);
            co_return nullptr;
        });

    co_await peer.run();
    LOG_INFO("IPC peer disconnected");
    co_return;
}

}  // namespace catter::ipc
