#include "js.h"
#include "libqjs/qjs.h"
#include "libutil/meta.h"
#include <quickjs.h>
#include "libconfig/js-lib.h"
#include "apitool.h"
#include <optional>

namespace catter::core::js {

namespace {
qjs::Runtime rt;
qjs::Object js_mod_obj;
}  // namespace

void init_qjs() {
    rt = qjs::Runtime::create();

    const qjs::Context& ctx = rt.context();
    auto& mod = ctx.cmodule("catter-c");
    for(auto& reg: catter::apitool::api_registers) {
        reg(mod, ctx);
    }
    // init js lib
    ctx.eval(catter::config::data::js_lib, "catter", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    ctx.eval("import * as catter from 'catter'; globalThis.__catter_mod = catter;",
             "get-mod.js",
             JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
    try {
        js_mod_obj = ctx.global_this()["__catter_mod"].to<qjs::Object>().value();
    } catch(...) {
        throw std::runtime_error("Failed to get catter module object");
    }
};

/**
 * Run a JavaScript file content in a new QuickJS runtime and context.
 *
 * @param content The JavaScript code to execute.
 * @param filename The name of the file (used for error reporting).
 * @throws qjs::Exception if there is an error during execution.
 */
qjs::Value run_js_file(std::string_view content, const std::string_view filename) {
    const qjs::Context& ctx = rt.context();
    return ctx.eval(content, filename.data(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT);
};

qjs::Object& js_mod_object() {
    return js_mod_obj;
}

}  // namespace catter::core::js
