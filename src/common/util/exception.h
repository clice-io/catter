#pragma once

#include <format>
#include <string>
#include <utility>
#include <cpptrace/from_current.hpp>

namespace catter::util {

template <typename... Args>
inline std::string format_exception(std::format_string<Args...> fmt, Args&&... args) {
    auto trace = cpptrace::from_current_exception();

    return std::format("{}\nStack trace:\n{}",
                       std::format(fmt, std::forward<Args>(args)...),
                       trace.empty() ? "<no stack trace available>" : trace.to_string());
}

}  // namespace catter::util
