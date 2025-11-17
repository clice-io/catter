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

}  // namespace catter::qjs
