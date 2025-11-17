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

    constexpr function_ref& operator=(const function_ref&) = default;
    constexpr function_ref& operator=(function_ref&&) = default;


    template<typename invocable_t>
        requires std::is_invocable_r_v<R, invocable_t, Args...> && (!std::is_convertible_v<invocable_t, Sign*>)
                && (!std::is_same_v<function_ref<R(Args...)>, invocable_t>)
    constexpr function_ref(invocable_t& inv)
      : proxy{[](Erased c, Args... args) -> R {
            return std::invoke(*static_cast<invocable_t*>(c.ctx), static_cast<Args>(args)...);
        }},
        ctx{.ctx = static_cast<void*>(&inv)}
    {}

    template<typename invocable_t>
        requires std::is_invocable_r_v<R, invocable_t, Args...> && std::is_convertible_v<invocable_t, Sign*>
    constexpr function_ref(const invocable_t& inv)
      : proxy{[](Erased c, Args... args) -> R {
            return std::invoke(c.fn, static_cast<Args>(args)...);
        }},
        ctx{.fn = inv}
    {}

    constexpr R operator()(Args... args) const { return proxy(ctx, static_cast<Args>(args)...); }

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

class Context {
public:
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
        using Functor_move =
            std::function<JSValue(JSContext*, JSValueConst, int, JSValueConst*)>;
        
#endif

        void add_functor(std::string_view name, Functor_move&& func) {
            static JSClassID id = 0;

            auto rt = JS_GetRuntime(this->ctx->get());
            if(id == 0) {
                JS_NewClassID(rt, &id);
                auto class_name = std::format("{}.{}", this->name, meta::type_name<Functor_move>());

                JSClassDef def{
                    class_name.c_str(),
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

            Value result{this->ctx->get(), JS_NewObjectClass(this->ctx->get(), id)};
            JS_SetOpaque(result.value(), new Functor_move(std::move(func)));
            this->exports.push_back(kv{std::string(name), std::move(result)});

            JS_AddModuleExport(this->ctx->get(), m, name.data());
            return;
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
    Context& operator= (const Context&) = delete;
    Context& operator= (Context&&) = default;
    ~Context() = default;

    static Context create(JSRuntime* rt) {
        Context c{JS_NewContext(rt)};
        return c;
    }

    CModule* new_cmodule(const std::string& name) {
        auto m = JS_NewCModule(this->get(), name.data(), [](JSContext* js_ctx, JSModuleDef* m) {
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
        if(auto it = this->raw->modules.find(name); it != this->raw->modules.end()) {
            return &it->second;
        }
        return &this->raw->modules.emplace(name, CModule(this, m, name)).first->second;
    }

    Value eval(const char* input, size_t input_len, const char* filename, int eval_flags) {
        auto val = JS_Eval(this->get(), input, input_len, filename, eval_flags);

        if(this->has_exception()) {
            throw exception(detail::dump(this->get()));
        }

        return Value(this->get(), std::move(val));
    }

    Value eval(std::string_view input, const char* filename, int eval_flags) {
        return this->eval(input.data(), input.size(), filename, eval_flags);
    }

    Object global_this() {
        return Object(this->get(), JS_GetGlobalObject(this->get()));
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
    Runtime& operator= (const Runtime&) = delete;
    Runtime& operator= (Runtime&&) = default;
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
    Runtime(JSRuntime* raw_rt) : rt(raw_rt) {}

    struct JSRuntimeDeleter {
        void operator() (JSRuntime* rt) const {
            JS_FreeRuntime(rt);
        }
    };

    std::unique_ptr<JSRuntime, JSRuntimeDeleter> rt = nullptr;
};

}  // namespace catter::qjs
