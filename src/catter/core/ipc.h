#pragma once

#include <eventide/async/stream.h>
#include <memory>

#include "util/data.h"

namespace catter::ipc {

class Service {
public:
    Service() = default;
    Service(const Service&) = default;
    Service(Service&&) = default;
    Service& operator= (const Service&) = default;
    Service& operator= (Service&&) = default;

    virtual ~Service() = default;
};

class DefaultService : public Service {
public:
    DefaultService() = default;
    virtual ~DefaultService() override = default;

    virtual data::ipcid_t create(data::ipcid_t parent_id) = 0;
    virtual data::action make_decision(data::command cmd) = 0;
    virtual void finish(int64_t code) = 0;
    virtual void report_error(data::ipcid_t parent_id, std::string error_msg) = 0;
};

eventide::task<void> accept(std::unique_ptr<DefaultService> service, eventide::pipe client);

}  // namespace catter::ipc
