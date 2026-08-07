#pragma once
#include <memory>
#include <kota/async/async.h>

#include "util/data.h"

namespace catter::ipc {

using ServiceMode = data::ServiceMode;
using ipcid_t = data::ipcid_t;

class ServiceBase {
public:
    ServiceBase() = default;
    ServiceBase(const ServiceBase&) = default;
    ServiceBase(ServiceBase&&) = default;
    ServiceBase& operator= (const ServiceBase&) = default;
    ServiceBase& operator= (ServiceBase&&) = default;

    virtual ~ServiceBase() = default;
};

class InjectService : public ServiceBase {
public:
    InjectService() = default;
    virtual ~InjectService() override = default;

    virtual kota::task<ipcid_t> create(ipcid_t parent_id) noexcept = 0;
    virtual kota::task<data::action> make_decision(data::command cmd) noexcept = 0;
    virtual kota::task<> finish(data::process_result result) noexcept = 0;
    virtual kota::task<> report_error(ipcid_t parent_id, std::string error_msg) noexcept = 0;
};

kota::task<void> accept(std::unique_ptr<InjectService> service, kota::pipe client);

}  // namespace catter::ipc
