#include "qjs.h"

namespace catter::qjs {
namespace detail {
std::string dump(JSContext* ctx) {
    JSValue exception_val = JS_GetException(ctx);

    // Get the error name
    JSValue name = JS_GetPropertyStr(ctx, exception_val, "name");
    const char* error_name = JS_ToCString(ctx, name);

    // Get the stack trace
    JSValue stack = JS_GetPropertyStr(ctx, exception_val, "stack");
    const char* stack_str = JS_ToCString(ctx, stack);

    // Get the message
    const char* msg = JS_ToCString(ctx, exception_val);
    std::string result = std::format("Error Name: {}\nMessage: {}\nStack Trace:\n{}",
                                     error_name ? error_name : "Unknown",
                                     msg ? msg : "No message",
                                     stack_str ? stack_str : "No stack trace");

    JS_FreeCString(ctx, error_name);
    JS_FreeCString(ctx, stack_str);
    JS_FreeCString(ctx, msg);
    JS_FreeValue(ctx, name);
    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exception_val);
    return result;
}
}  // namespace detail

void CModule::add_functor(std::string_view name, Functor_move&& func) const {
    static JSClassID id = 0;

    auto rt = JS_GetRuntime(this->ctx);
    if(id == 0) {
        JS_NewClassID(rt, &id);
        auto class_name = std::format("qjs.{}", meta::type_name<Functor_move>());

        JSClassDef def{class_name.c_str(),
                       [](JSRuntime* rt, JSValue obj) {
                           auto* ptr = static_cast<Functor_move*>(JS_GetOpaque(obj, id));
                           delete ptr;
                       },
                       nullptr,
                       [](JSContext* ctx,
                          JSValueConst func_obj,
                          JSValueConst this_val,
                          int argc,
                          JSValueConst* argv,
                          int flags) -> JSValue {
                           auto* ptr = static_cast<Functor_move*>(JS_GetOpaque(func_obj, id));
                           if(!ptr) {
                               return JS_ThrowTypeError(ctx, "Internal error: C++ functor is null");
                           }
                           return (*ptr)(ctx, this_val, argc, argv);
                       },
                       nullptr};
        JS_NewClass(rt, id, &def);
    }

    Value result{this->ctx, JS_NewObjectClass(this->ctx, id)};
    JS_SetOpaque(result.value(), new Functor_move(std::move(func)));

    const_cast<CModule*>(this)->exports.push_back(kv{std::string(name), std::move(result)});

    JS_AddModuleExport(this->ctx, m, name.data());
    return;
}

const CModule* Context::cmodule(const std::string& name) const {
    if(auto it = this->raw->modules.find(name); it != this->raw->modules.end()) {
        return &it->second;
    } else {
        auto m =
            JS_NewCModule(this->js_context(), name.data(), [](JSContext* js_ctx, JSModuleDef* m) {
                auto* ctx = Context::get_opaque(js_ctx);

                auto atom = Atom(js_ctx, JS_GetModuleName(js_ctx, m));

                if(!ctx) {
                    return -1;
                }

                auto& mod = ctx->modules[atom.to_string()];

                for(const auto& kv: mod.exports) {
                    JS_SetModuleExport(js_ctx, m, kv.name.c_str(), kv.value.value());
                }
                return 0;
            });
        if(m == nullptr) {
            throw std::runtime_error("Failed to create new C module");
        }

        return &this->raw->modules.emplace(name, CModule(this->js_context(), m, name))
                    .first->second;
    }
}
}  // namespace catter::qjs
