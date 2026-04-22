#pragma once
#include <print>
#include <cpptrace/exceptions.hpp>
#include <kota/async/io/loop.h>
#include <kota/async/vocab/error.h>
#include <kota/ipc/codec/bincode.h>
#include <kota/ipc/peer.h>
#include <kota/ipc/transport.h>

#include "util/data.h"
#include "util/kotatsu.h"

namespace catter::proxy::ipc {

struct Peer {
    using RequestType = catter::ipc::RequestType;
    template <RequestType Type>
    using Request = catter::ipc::Request<Type>;

    template <typename Tag, typename Traits = typename kota::ipc::protocol::RequestTraits<Tag>>
    typename kota::task<typename Traits::Result>
        send_request(const typename Traits::Params& params) {
        if(auto ret = co_await this->peer.send_request<Tag>(params); !ret.has_value()) {
            throw cpptrace::runtime_error(
                std::format("IPC request failed: {}", ret.error().message));
        } else {
            co_return *ret;
        }
    }

    kota::task<> run() {
        return this->peer.run();
    }

    auto close() {
        return this->peer.close();
    }

    kota::task<bool> check_mode(data::ServiceMode mode) {
        co_return co_await this->send_request<Request<RequestType::CHECK_MODE>>(mode);
    }

    kota::task<data::ipcid_t> create(data::ipcid_t parent_id) {
        co_return co_await this->send_request<Request<RequestType::CREATE>>(parent_id);
    }

    kota::task<data::action> make_decision(data::command cmd) {
        co_return co_await this->send_request<Request<RequestType::MAKE_DECISION>>(cmd);
    }

    kota::task<void> finish(data::process_result result) {
        co_await this->send_request<Request<RequestType::FINISH>>(result);
    }

    kota::task<void> report_error(data::ipcid_t parent_id, std::string error_msg) noexcept {
        try {
            co_await this->send_request<Request<RequestType::REPORT_ERROR>>({parent_id, error_msg});
        } catch(...) {
            // can't do anything if reporting error failed, just swallow the error
        }
        co_return;
    }

public:
    kota::ipc::BincodePeer peer;
};

}  // namespace catter::proxy::ipc
