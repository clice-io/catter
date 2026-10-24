#include "apitool.h"

#include "js.h"
#include "util/encoding.h"

namespace catter::apitool {
using api_register = void (*)(const catter::qjs::CModule&, const catter::qjs::Context&);

std::vector<api_register>& api_registers() {
    static std::vector<api_register> registers{};
    return registers;
}
}  // namespace catter::apitool

namespace catter::capi::util {
std::filesystem::path absolute_of(std::string js_path) {
    auto js_fs_path = catter::util::from_utf8(js_path);
    if(js_fs_path.is_absolute()) {
        return js_fs_path;
    }

    return (js::get_global_runtime_config().pwd / js_fs_path).lexically_normal();
}
}  // namespace catter::capi::util
