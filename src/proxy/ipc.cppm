module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:proxy.ipc;

import :capture.ipc;

namespace catter::proxy {

struct Peer {
    using RequestType = catter::ipc::RequestType;
    template <RequestType Type>
    using Request = catter::ipc::Request<Type>;

    template <typename Tag, typename Traits = typename kota::ipc::protocol::RequestTraits<Tag>>
    typename kota::task<typename Traits::Result>
        send_request(const typename Traits::Params& params) {
        if(auto ret =
               co_await this->peer.template send_request<typename Traits::Result>(Traits::method,
                                                                                  params);
           !ret.has_value()) {
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

    kota::task<Request<RequestType::MAKE_DECISION>::Result>
        make_decision(std::optional<data::ipcid_t> parent, data::command cmd) {
        co_return co_await this->send_request<Request<RequestType::MAKE_DECISION>>(
            {.parent = parent, .command = std::move(cmd)});
    }

#ifdef CATTER_MAC
    void started(int64_t pid) {
        program = pid;
        // The program runs all the same.
        using Started = Request<RequestType::STARTED>;
        (void)this->peer.send_notification(Started::method, Started::Params{pid});
    }
#endif

    kota::task<void> finish(data::process_result result) {
        co_await this->send_request<Request<RequestType::FINISH>>(result);
        co_return;
    }

    kota::task<void> report_error(std::optional<data::ipcid_t> parent,
                                  std::string message) noexcept {
        try {
            co_await this->send_request<Request<RequestType::REPORT_ERROR>>(
                {.parent = parent, .message = std::move(message)});
        } catch(...) {
            // can't do anything if reporting error failed, just swallow the error
        }
        co_return;
    }

public:
    kota::ipc::BincodePeer peer;

#ifdef CATTER_MAC
    /// The program the proxy stands in for, from its start until its exit.
    std::optional<int64_t> program;
#endif
};

}  // namespace catter::proxy
