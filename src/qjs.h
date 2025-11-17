#pragma once
#include <array>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>
#include <format>

#include <quickjs.h>

#include "type_name.h"

namespace catter::qjs {
namespace detail {
std::string dump(JSContext* ctx);

template <typename Sign>
class function_ref {
    static_assert(false, "Sign must be a function type");
};

template <typename R, typename... Args>
class function_ref<R(Args...)> {
public:
    using Sign = R(Args...);

    using Erased = union {
        void* ctx;
        Sign* fn;
    };

    constexpr function_ref(const function_ref&) = default;
    constexpr function_ref(function_ref&&) = default;

    constexpr function_ref& operator= (const function_ref&) = default;
    constexpr function_ref& operator= (function_ref&&) = default;

    template <typename invocable_t>
        requires std::is_invocable_r_v<R, invocable_t, Args...> &&
                     (!std::is_convertible_v<invocable_t, Sign*>) &&
                     (!std::is_same_v<function_ref<R(Args...)>, invocable_t>)
    constexpr function_ref(invocable_t& inv) :
        proxy{[](Erased c, Args... args) -> R {
            return std::invoke(*static_cast<invocable_t*>(c.ctx), static_cast<Args>(args)...);
        }},
        ctx{.ctx = static_cast<void*>(&inv)} {}

    template <typename invocable_t>
        requires std::is_invocable_r_v<R, invocable_t, Args...> &&
                     std::is_convertible_v<invocable_t, Sign*>
    constexpr function_ref(const invocable_t& inv) :
        proxy{[](Erased c, Args... args) -> R {
            return std::invoke(c.fn, static_cast<Args>(args)...);
        }},
        ctx{.fn = inv} {}

    constexpr R operator() (Args... args) const {
        return proxy(ctx, static_cast<Args>(args)...);
    }

private:
    R (*proxy)(Erased, Args...);
    Erased ctx;
};

}  // namespace detail

class exception : public std::exception {
public:
    exception(std::string&& details) : details(std::move(details)) {}

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

    Value(Value&& other) :
        ctx(std::exchange(other.ctx, nullptr)), val(std::exchange(other.val, JS_UNDEFINED)) {}

    Value& operator= (const Value& other) {
        if(this != &other) {
            if(this->ctx) {
                JS_FreeValue(this->ctx, this->val);
            }
            this->ctx = other.ctx;
            this->val = JS_DupValue(other.ctx, other.val);
        }
        return *this;
    }

    Value& operator= (Value&& other) {
        if(this != &other) {
            ctx = std::exchange(other.ctx, nullptr);
            val = std::exchange(other.val, JS_UNDEFINED);
        }
        return *this;
    }

    ~Value() {
        if(this->ctx) {
            JS_FreeValue(ctx, val);
        }
    }

    Value(JSContext* ctx, const JSValue& val) : ctx(ctx), val(JS_DupValue(ctx, val)) {}

    Value(JSContext* ctx, JSValue&& val) : ctx(ctx), val(std::move(val)) {}

    template <typename T>
    static Value from(JSContext* ctx, T value) {
        if constexpr(std::is_same_v<T, int32_t>) {
            return Value(ctx, JS_NewInt32(ctx, value));
        } else if constexpr(std::is_same_v<T, double>) {
            return Value(ctx, JS_NewFloat64(ctx, value));
        } else if constexpr(std::is_same_v<T, const char*>) {
            return Value(ctx, JS_NewString(ctx, value));
        } else if constexpr(std::is_same_v<T, std::string>) {
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

class Atom {
public:
    Atom() = default;

    Atom(JSContext* ctx, JSAtom atom) : ctx(ctx), atom(atom) {}

    Atom(const Atom& other) : ctx(other.ctx), atom(JS_DupAtom(other.ctx, other.atom)) {}

    Atom(Atom&& other) :
        ctx(std::exchange(other.ctx, nullptr)), atom(std::exchange(other.atom, JS_ATOM_NULL)) {}

    Atom& operator= (const Atom& other) {
        if(this != &other) {
            if(this->ctx) {
                JS_FreeAtom(this->ctx, this->atom);
            }
            this->ctx = other.ctx;
            this->atom = JS_DupAtom(other.ctx, other.atom);
        }
        return *this;
    }

    Atom& operator= (Atom&& other) {
        if(this != &other) {
            ctx = std::exchange(other.ctx, nullptr);
            atom = std::exchange(other.atom, JS_ATOM_NULL);
        }
        return *this;
    }

    ~Atom() {
        if(this->ctx) {
            JS_FreeAtom(this->ctx, this->atom);
        }
    }

    JSAtom value() const {
        return this->atom;
    }

    std::string to_string() const {
        const char* str = JS_AtomToCString(this->ctx, this->atom);
        if(str == nullptr) {
            return {};
        }
        std::string result{str};
        JS_FreeCString(this->ctx, str);
        return result;
    }

private:
    JSContext* ctx = nullptr;
    JSAtom atom = JS_ATOM_NULL;
};

class Object : public Value {
public:
    using Value::Value;

    Object(const Value& val) : Value(val) {}

    Object(Value&& val) : Value(std::move(val)) {}

    Object() = default;
    Object(const Object&) = default;
    Object(Object&& other) = default;
    Object& operator= (const Object&) = default;
    Object& operator= (Object&& other) = default;
    ~Object() = default;

    Value get_property(const std::string& prop_name) {
        auto ret = Value(this->context(),
                         JS_GetPropertyStr(this->context(), this->value(), prop_name.c_str()));
        if(ret.is_exception()) {
            throw exception(detail::dump(this->context()));
        }
        return ret;
    }
};

template <typename T>
std::optional<T> Value::to() {
    if(!this->is_valid()) {
        return std::nullopt;
    }
    if constexpr(std::is_same_v<T, std::string>) {
        if(!JS_IsString(this->val)) {
            return std::nullopt;
        }
        const char* str = JS_ToCString(this->ctx, this->val);
        if(str == nullptr) {
            return std::nullopt;
        }
        std::string result{str};
        JS_FreeCString(this->ctx, str);
        return result;
    } else if constexpr(std::is_same_v<T, Object>) {
        if(!JS_IsObject(this->val)) {
            return std::nullopt;
        }
        return Object(*this);
    } else {
        static_assert(false, "Unsupported type for Value::to()");
    }
}

template <typename Signature>
class Function {};

template <typename R, typename... Args>
class Function<R(Args...)> : public Object {
public:
    using Object::Object;

    Function(const Object& obj) : Object(obj) {}

    Function(Object&& obj) : Object(std::move(obj)) {}

    Function() = default;
    Function(const Function&) = default;
    Function(Function&& other) = default;
    Function& operator= (const Function&) = default;
    Function& operator= (Function&& other) = default;
    ~Function() = default;

    R invoke(const Object& this_obj, Args... args) {
        auto value = Value(this->context(),
                           JS_Call(this->context(),
                                   this->value(),
                                   this_obj.value(),
                                   sizeof...(Args),
                                   std::array<JSValue, sizeof...(Args)>{
                                       Value::from(this->context(), args).value()...}
                                       .data()));

        if(value.is_exception()) {
            throw exception(detail::dump(this->context()));
        }

        auto result = value.to<R>();
        if(!result.has_value()) {
            JS_ThrowTypeError(this->context(), "Failed to convert function return value");
            throw exception(detail::dump(this->context()));
        }

        return result.value();
    }

    R operator() (Args... args) {
        return this->invoke(Object{this->context(), JS_GetGlobalObject(this->context())}, args...);
    }
};

class CModule {
public:
    friend class Context;
    CModule() = default;
    CModule(const CModule&) = default;
    CModule(CModule&&) = default;
    CModule& operator= (const CModule&) = default;
    CModule& operator= (CModule&&) = default;

#ifdef __cpp_lib_move_only_function
    using Functor_move =
        std::move_only_function<JSValue(JSContext*, JSValueConst, int, JSValueConst*)>;
#else
    using Functor_move = std::function<JSValue(JSContext*, JSValueConst, int, JSValueConst*)>;
#endif

    void add_functor(std::string_view name, Functor_move&& func) const;

private:
    CModule(JSContext* ctx, JSModuleDef* m, std::string_view name) : ctx(ctx), m(m), name(name) {}

    struct kv {
        std::string name;
        Value value;
    };

    JSContext* ctx = nullptr;
    JSModuleDef* m = nullptr;
    std::string name{};
    std::vector<kv> exports{};
};

class Context {
public:
    friend class Runtime;

    Context() = default;
    Context(const Context&) = delete;
    Context(Context&&) = default;
    Context& operator= (const Context&) = delete;
    Context& operator= (Context&&) = default;
    ~Context() = default;

    // Get or create a CModule with the given name
    // @name: The name of the module.
    // Different from context, it is used for js module system.
    // In js, you can import it via `import * as mod from 'name';`
    const CModule* cmodule(const std::string& name) const;

    Value eval(const char* input, size_t input_len, const char* filename, int eval_flags) const {
        auto val = JS_Eval(this->js_context(), input, input_len, filename, eval_flags);

        if(this->has_exception()) {
            throw exception(detail::dump(this->js_context()));
        }

        return Value(this->js_context(), std::move(val));
    }

    Value eval(std::string_view input, const char* filename, int eval_flags) const {
        return this->eval(input.data(), input.size(), filename, eval_flags);
    }

    Object global_this() const {
        return Object(this->js_context(), JS_GetGlobalObject(this->js_context()));
    }

    bool has_exception() const {
        return JS_HasException(this->js_context());
    }

    JSContext* js_context() const {
        return this->raw->ctx.get();
    }

    operator bool() {
        return this->raw != nullptr;
    }

private:
    class Raw {
    public:
        Raw() = default;

        Raw(JSContext* ctx) : ctx(ctx) {}

        Raw(const Raw&) = delete;
        Raw(Raw&&) = default;
        Raw& operator= (const Raw&) = delete;
        Raw& operator= (Raw&&) = default;

        ~Raw() = default;

    public:
        struct JSContextDeleter {
            void operator() (JSContext* ctx) const {
                JS_FreeContext(ctx);
            }
        };

        std::unordered_map<std::string, CModule> modules{};
        std::unique_ptr<JSContext, JSContextDeleter> ctx = nullptr;
    };

    void set_opaque() {
        JS_SetContextOpaque(this->js_context(), this->raw.get());
    }

    static Raw* get_opaque(JSContext* ctx) {
        return static_cast<Raw*>(JS_GetContextOpaque(ctx));
    }

    Context(JSContext* js_ctx) : raw(std::make_unique<Raw>(js_ctx)) {
        this->set_opaque();
    }

    std::unique_ptr<Raw> raw = nullptr;
};

class Runtime {
public:
    Runtime() = default;
    Runtime(const Runtime&) = delete;
    Runtime(Runtime&&) = default;
    Runtime& operator= (const Runtime&) = delete;
    Runtime& operator= (Runtime&&) = default;
    ~Runtime() = default;

    static Runtime create() {
        Runtime r{JS_NewRuntime()};
        return r;
    }

    // Get or create a context with the given name
    // @name: The name of the context. Just for identification purposes.
    const Context* context(const std::string& name = "default") const {
        if(auto it = this->raw->ctxs.find(name); it != this->raw->ctxs.end()) {
            return &it->second;
        } else {
            auto js_ctx = JS_NewContext(this->js_runtime());
            if(js_ctx == nullptr) {
                throw std::runtime_error("Failed to create new JS context");
            }
            return &this->raw->ctxs.emplace(name, Context(js_ctx)).first->second;
        }
    }

    JSRuntime* js_runtime() const {
        return this->raw->rt.get();
    }

    operator bool() const {
        return this->raw != nullptr;
    }

private:
    class Raw {
    public:
        Raw() = default;

        Raw(JSRuntime* rt) : rt(rt) {}

        Raw(const Raw&) = delete;
        Raw(Raw&&) = default;
        Raw& operator= (const Raw&) = delete;
        Raw& operator= (Raw&&) = default;

        ~Raw() = default;

    public:
        struct JSRuntimeDeleter {
            void operator() (JSRuntime* rt) const {
                JS_FreeRuntime(rt);
            }
        };

        std::unordered_map<std::string, Context> ctxs{};
        std::unique_ptr<JSRuntime, JSRuntimeDeleter> rt = nullptr;
    };

    Runtime(JSRuntime* js_rt) : raw(std::make_unique<Raw>(js_rt)) {}

    std::unique_ptr<Raw> raw = nullptr;
};

}  // namespace catter::qjs
