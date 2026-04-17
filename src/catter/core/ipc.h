#pragma once
#include <memory>

#include "util/data.h"

#include <kota/async/async.h>

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

    virtual ipcid_t create(ipcid_t parent_id) = 0;
    virtual data::action make_decision(data::command cmd) = 0;
    virtual void finish(data::process_result result) = 0;
    virtual void report_error(ipcid_t parent_id, std::string error_msg) = 0;
};

kota::task<void> accept(std::unique_ptr<InjectService> service, kota::pipe client);

}  // namespace catter::ipc
