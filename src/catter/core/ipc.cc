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
#include "util/serde.h"

namespace catter::ipc {
using namespace data;

kota::task<void> accept(std::unique_ptr<InjectService> service, kota::pipe client) {
    kota::ipc::BincodePeer peer(kota::event_loop::current(),
                                std::make_unique<kota::ipc::StreamTransport>(std::move(client)));
    using Context = kota::ipc::BincodePeer::RequestContext;
    // auto service_mode = Serde<ServiceMode>::deserialize(BufferReader(*sm_packet));
    // assert(service_mode == ServiceMode::INJECT && "Unsupported service mode received");

    peer.on_request<req::Create>(
        [&](const Context& ctx,
            const req::Create::Params& params) -> kota::ipc::RequestResult<req::Create> {
            co_return service->create(params);
        });

    peer.on_request<req::MakeDecision>(
        [&](const Context& ctx, const req::MakeDecision::Params& params)
            -> kota::ipc::RequestResult<req::MakeDecision> {
            co_return service->make_decision(params);
        });

    peer.on_request<req::Finish>(
        [&](const Context& ctx,
            const req::Finish::Params& params) -> kota::ipc::RequestResult<req::Finish> {
            service->finish(params);
            co_return nullptr;
        });

    peer.on_request<req::ReportError>(
        [&](const Context& ctx,
            const req::ReportError::Params& params) -> kota::ipc::RequestResult<req::ReportError> {
            service->report_error(params.parent_id, params.error_msg);
            co_return nullptr;
        });

    co_await peer.run();
    LOG_INFO("IPC peer disconnected");
    co_return;
}

}  // namespace catter::ipc
