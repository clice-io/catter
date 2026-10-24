#pragma once

#include <string_view>
#include <kota/async/runtime/task.h>

#include "handler.h"
#include "util/data.h"

namespace catter::core {

class RuntimeDriver {
public:
    virtual ~RuntimeDriver() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual kota::task<data::process_result> execute(const Build& build,
                                                     Handler& handler) const = 0;
};

const RuntimeDriver* find_runtime_driver(std::string_view name) noexcept;
const RuntimeDriver* default_runtime_driver() noexcept;

}  // namespace catter::core
