#include "qjs.h"
#include <array>
#include <exception>
#include <quickjs.h>
#include <optional>
#include <format>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>
namespace catter::qjs {
namespace detail {
    std::string dump(JSContext* ctx) {
        JSValue exception_val = JS_GetException(ctx); 
        
        // Get the error name  
        JSValue name = JS_GetPropertyStr(ctx, exception_val, "name");  
        const char *error_name = JS_ToCString(ctx, name);  
        
        // Get the stack trace  
        JSValue stack = JS_GetPropertyStr(ctx, exception_val, "stack");  
        const char *stack_str = JS_ToCString(ctx, stack);  
        
        // Get the message  
        const char *msg = JS_ToCString(ctx, exception_val);  
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
}

class exception : public std::exception {
public:
    exception(std::string&& details)
        : details(std::move(details)) {}
    
    const char* what() const noexcept override {
        return details.c_str();
    }
private:
    std::string details;
};

class Value {
public:
    Value() = default;
    Value(const Value& other) : ctx(other.ctx), val(JS_DupValue(other.ctx, other.val)) {}
    Value(Value&& other) : ctx(std::exchange(other.ctx, nullptr)), val(std::exchange(other.val, JS_UNDEFINED)) {}
    Value& operator=(const Value& other) {
        if (this != &other) {
            if (this->ctx) {
                JS_FreeValue(this->ctx, this->val);
            }
            this->ctx = other.ctx;
            this->val = JS_DupValue(other.ctx, other.val);
        }
        return *this;
    }
    Value& operator=(Value&& other) {
        if (this != &other) {
            ctx = std::exchange(other.ctx, nullptr);
            val = std::exchange(other.val, JS_UNDEFINED);
        }
        return *this;
    }
    ~Value() {
        if (this->ctx) {
            JS_FreeValue(ctx, val);
        }
    }

    Value(JSContext* ctx, JSValue val): ctx(ctx), val(val) {}

    template<typename T>
    static Value from(JSContext* ctx, T value) {
        if constexpr (std::is_same_v<T, int32_t>) {
            return Value(ctx, JS_NewInt32(ctx, value));
        } else if constexpr (std::is_same_v<T, double>) {
            return Value(ctx, JS_NewFloat64(ctx, value));
        } else if constexpr (std::is_same_v<T, const char*>) {
            return Value(ctx, JS_NewString(ctx, value));
        } else if constexpr (std::is_same_v<T, std::string>) {
            return Value(ctx, JS_NewStringLen(ctx, value.c_str(), value.size()));
        } else {
            static_assert(false, "Unsupported type for Value::from()");
        }
    }

    template <typename T>
    std::optional<T> to();

    bool is_exception() {
        return JS_IsException(this->val);
    }

    bool is_valid() const {
        return this->ctx != nullptr;
    }

    operator bool() const {
        return this->is_valid();
    }

    JSValue value() const {
        return this->val;
    }

    JSContext* context() const {
        return this->ctx;
    }

private:
    JSContext* ctx = nullptr;
    JSValue val = JS_UNDEFINED;
};

class Object : public Value {
public:
    using Value::Value;
    Object(const Value& val) : Value(val) {}
    Object(Value&& val) : Value(std::move(val)) {}

    Object() = default;   
    Object(const Object&) = default;
    Object(Object&& other) = default;
    Object& operator=(const Object&) = default;
    Object& operator=(Object&& other) = default;
    ~Object() = default;


    Value get_property(const std::string& prop_name) {
        auto ret = Value(this->context(), JS_GetPropertyStr(this->context(), this->value(), prop_name.c_str()));
        if (ret.is_exception()) {
            throw exception(detail::dump(this->context()));
        }
        return ret;
    }
};

template <typename T>
std::optional<T> Value::to(){
    if(!this->is_valid()) {
        return std::nullopt;
    }
    if constexpr (std::is_same_v<T, std::string>) {
        if (!JS_IsString(this->val)){
            return std::nullopt;
        }
        const char* str = JS_ToCString(this->ctx, this->val);
        if(str == nullptr) {
            return std::nullopt;
        }
        std::string result{str};
        JS_FreeCString(this->ctx, str);
        return result;
    } else if constexpr (std::is_same_v<T, Object>) {
        if (!JS_IsObject(this->val)){
            return std::nullopt;
        }
        return Object(*this);
    } else {
        static_assert(false, "Unsupported type for Value::as()");
    }
}

template<typename Signature>
class Function{};

template<typename R, typename... Args>
class Function<R(Args...)> : public Object {
public:
    using Object::Object;
    Function(const Object& obj) : Object(obj) {}
    Function(Object&& obj) : Object(std::move(obj)) {}

    Function() = default;
    Function(const Function&) = default;
    Function(Function&& other) = default;
    Function& operator=(const Function&) = default;
    Function& operator=(Function&& other) = default;
    ~Function() = default;


    R invoke(const Object& this_obj, Args... args){
        auto value = Value(this->context(), JS_Call(this->context(), this->value(), 
            this_obj.value(), sizeof...(Args), std::array<JSValue, sizeof...(Args)>{Value::from(this->context(), args).value()...}.data()));

        if (value.is_exception()) {
            throw exception(detail::dump(this->context()));
        }

        auto result = value.to<R>();
        if (!result.has_value()) {
            JS_ThrowTypeError(this->context(), "Failed to convert function return value");
            throw exception(detail::dump(this->context()));
        }

        return result.value();
    }

    R operator()(Args... args) {
        return this->invoke(Object{}, args...);
    }
};


class Context {
public:    
    class CModule {
    public:
        friend class Context;
        CModule() = default;
        CModule(const CModule&) = default;
        CModule(CModule&&) = default;
        CModule& operator=(const CModule&) = default;
        CModule& operator=(CModule&&) = default;
        template<typename T>
        void add_functor(std::string_view name, T&& func) {
            using Functor = std::decay_t<T>;

            static JSClassID id = 0;

            auto rt = JS_GetRuntime(this->ctx);
            if (id == 0){
                JS_NewClassID(rt, &id);
            }

            JSClassDef def{
                name.data(),
                [](JSRuntime *rt, JSValue obj){
                    auto* ptr = static_cast<Functor*>(
                        JS_GetOpaque(obj, id));
                    delete ptr;
                }, 
                nullptr, 
                [](JSContext *ctx, JSValueConst func_obj,
                            JSValueConst this_val, int argc,
                            JSValueConst *argv, int flags) -> JSValue {
                    auto* ptr = static_cast<Functor*>(
                        JS_GetOpaque(func_obj, id));
                    if (!ptr) {
                        return JS_ThrowTypeError(ctx, "Internal error: C++ functor is null");
                    }
                    return (*ptr)(ctx, this_val, argc, argv);
                },
                nullptr
            };
            JS_NewClass(rt, id, &def);

            Value result = {ctx, JS_NewObjectClass(ctx, id)};
            JS_SetOpaque(result.value(), new Functor(std::forward<T>(func)));
            this->exports.push_back(kv{std::string(name), std::move(result)});

            JS_AddModuleExport(this->ctx->get(), m, name.data());
        }
    private:
        CModule(Context* ctx, JSModuleDef* m, std::string_view name) : ctx(ctx), m(m), name(name) {}
        struct kv {
            std::string name;
            Value value;
        };
        Context* ctx = nullptr;
        JSModuleDef* m = nullptr;
        std::string name{};
        std::vector<kv> exports{};
    };
    Context() = default;
    Context(const Context&) = delete;
    Context(Context&&) = default;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = default;
    ~Context() = default;

    static Context create(JSRuntime* rt) {
        Context c{JS_NewContext(rt)};
        return c;
    }

    CModule* new_cmodule(std::string_view name) {
        auto m = JS_NewCModule(this->get(), name.data(), [](JSContext* js_ctx, JSModuleDef* m){
            auto* ctx = Context::get_opaque(js_ctx);
            if (!ctx) {
                return -1;
            }
            const auto& mod = ctx->modules[m];

            for (const auto& kv : mod.exports) {
                JS_SetModuleExport(js_ctx, m, kv.name.c_str(), kv.value.value());
            }
            return 0;
        });
        auto it = this->raw->modules.emplace(m, CModule(this, m, name));
        return &it.first->second;
    }

    Value eval(const char *input, size_t input_len,
                        const char *filename, int eval_flags){
        auto val = JS_Eval(this->get(), input, input_len, filename, eval_flags);
        
        if (this->has_exception()) {
            throw exception(detail::dump(this->get()));
        }

        return Value(this->get(), std::move(val));
    }


    Value eval(std::string_view input,
                        const char *filename, int eval_flags){
        return this->eval(input.data(), input.size(), filename, eval_flags);
    }

    Object global() {
        JSValue global = JS_GetGlobalObject(this->get());
        return Object(this->get(), global);
    }

    bool has_exception() {
        return JS_HasException(this->get());
    }

    JSContext* get() {
        return this->raw->ctx.get();
    }

    operator bool() {
        return this->raw != nullptr;
    }

private:
    class Raw {
    public:
        Raw() = default;
        Raw(JSContext* ctx): ctx(ctx) {}
        Raw(const Raw&) = delete;
        Raw(Raw&&) = default;
        Raw& operator=(const Raw&) = delete;
        Raw& operator=(Raw&&) = default;

        ~Raw() = default;
    public:
        struct JSContextDeleter {
            void operator()(JSContext* ctx) const {
                JS_FreeContext(ctx);
            }
        };

        std::unordered_map<JSModuleDef*, CModule> modules{};
        std::unique_ptr<JSContext, JSContextDeleter> ctx = nullptr;
    };

    void set_opaque() {
        JS_SetContextOpaque(this->get(), this->raw.get());
    }

    static Raw* get_opaque(JSContext* ctx) {
        return static_cast<Raw*>(JS_GetContextOpaque(ctx));
    }

    Context(JSContext* raw_ctx) : raw(std::make_unique<Raw>(raw_ctx)) {
        this->set_opaque();
    }

    std::unique_ptr<Raw> raw = nullptr;
};

class Runtime {
public:
    Runtime() = default;
    Runtime(const Runtime&) = delete;
    Runtime(Runtime&&) = default;
    Runtime& operator=(const Runtime&) = delete;
    Runtime& operator=(Runtime&&) = default;
    ~Runtime() = default;

    static Runtime create() {
        Runtime r{JS_NewRuntime()};
        return r;
    }

    Context new_context() {
        return Context::create(this->get());
    }


    JSRuntime* get() {
        return this->rt.get();
    }

    operator bool() {
        return this->rt != nullptr;
    }

private:
    Runtime(JSRuntime* raw_rt): rt(raw_rt) {}

    struct JSRuntimeDeleter {
        void operator()(JSRuntime* rt) const {
            JS_FreeRuntime(rt);
        }
    };

    std::unique_ptr<JSRuntime, JSRuntimeDeleter> rt = nullptr;
};

}



std::string fix_cmd(std::string c) {
    auto rt = catter::qjs::Runtime::create();
    auto ctx = rt.new_context();

    auto mod = ctx.new_cmodule("catter");
    catter::qjs::Function<std::string(std::string)> addCallback{};
    mod->add_functor("addCallback", [&](JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) -> JSValue {
        if (argc != 1) {
            return JS_ThrowTypeError(ctx, "Expected exactly 1 argument");
        }

        addCallback = catter::qjs::Function<std::string(std::string)>{ctx, JS_DupValue(ctx, argv[0])};
        return JS_UNDEFINED;
    });

    std::string script = R"(
        import * as catter from 'catter';

        catter.addCallback((cmd) => {
            return {};
        });
    )";

    std::string cmd = "gcc --version";

    try {
        ctx.eval(script, nullptr, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT | JS_EVAL_TYPE_MODULE);

        if (!addCallback) {
            throw std::runtime_error("addCallback function was not set");
        }

        std::println("Invoke result: {}", addCallback.invoke(ctx.global(), cmd));
    } catch (const std::exception& e) {
        std::println("{}", e.what());
    }


    return {};
}