#pragma once

#include <string_view>
#include <kota/async/runtime/task.h>

#include "js/capi/type.h"
#include "util/data.h"

namespace catter::core {

class RuntimeDriver {
public:
    virtual ~RuntimeDriver() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual const js::CatterRuntime& runtime() const noexcept = 0;
    virtual kota::task<data::process_result> execute(const js::CatterConfig& config) const = 0;
};

const RuntimeDriver* find_runtime_driver(std::string_view name) noexcept;
const RuntimeDriver& default_runtime_driver() noexcept;

js::ProcessResult to_js_process_result(data::process_result result);

}  // namespace catter::core
