#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
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

template <typename... args_t>
struct type_list {

    template <size_t I>
    struct get {
        using type = typename std::tuple_element<I, std::tuple<args_t...>>::type;
    };

    template <size_t I>
    using get_t = typename get<I>::type;

    template <typename T>
    struct contains {
        constexpr static bool value = (std::is_same_v<T, args_t> || ...);
    };

    template <typename T>
    constexpr static bool contains_v = contains<T>::value;

    constexpr static size_t size = sizeof...(args_t);
};

template <typename U>
struct value_trans;

std::string dump(JSContext* ctx);

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
    static Value from(JSContext* ctx, T&& value) {
        return detail::value_trans<std::remove_cvref_t<T>>::from(ctx, std::forward<T>(value));
    }

    template <typename T>
    std::optional<T> to() {
        return detail::value_trans<T>::to(this->ctx, *this);
    }

    bool is_exception() const {
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

namespace detail {
template <>
struct value_trans<bool> {
    static Value from(JSContext* ctx, bool value) {
        return Value(ctx, JS_NewBool(ctx, value));
    }

    static std::optional<bool> to(JSContext* ctx, const Value& val) {
        if(!JS_IsBool(val.value())) {
            return std::nullopt;
        }
        return JS_ToBool(ctx, val.value());
    }
};

template <>
struct value_trans<int64_t> {
    static Value from(JSContext* ctx, int64_t value) {
        return Value(ctx, JS_NewInt32(ctx, value));
    }

    static std::optional<int64_t> to(JSContext* ctx, const Value& val) {
        if(!JS_IsNumber(val.value())) {
            return std::nullopt;
        }
        int64_t result;
        if(JS_ToInt64(ctx, &result, val.value()) < 0) {
            return std::nullopt;
        }
        return result;
    }
};

template <>
struct value_trans<std::string> {
    static Value from(JSContext* ctx, const std::string& value) {
        return Value(ctx, JS_NewStringLen(ctx, value.data(), value.size()));
    }

    static std::optional<std::string> to(JSContext* ctx, const Value& val) {
        if(!JS_IsString(val.value())) {
            return std::nullopt;
        }
        size_t len;
        const char* str = JS_ToCStringLen(ctx, &len, val.value());
        if(str == nullptr) {
            return std::nullopt;
        }
        std::string result{str, len};
        JS_FreeCString(ctx, str);
        return result;
    }
};

template <>
struct value_trans<Object> {
    static Value from(JSContext* ctx, const Object& obj) {
        return Value(ctx, JS_DupValue(ctx, obj.value()));
    }

    static std::optional<Object> to(JSContext* ctx, const Value& val) {
        if(!JS_IsObject(val.value())) {
            return std::nullopt;
        }
        return Object(ctx, JS_DupValue(ctx, val.value()));
    }
};
}  // namespace detail

template <typename Signature>
class Function {};

template <typename R, typename... Args>
class Function<R(Args...)> : public Object {
public:
    using AllowParamTypes = detail::type_list<bool, int64_t, std::string, Object>;

    static_assert((AllowParamTypes::contains_v<Args> && ...),
                  "Function parameter types must be one of the allowed types");
    static_assert(AllowParamTypes::contains_v<R> || std::is_void_v<R>,
                  "Function return type must be one of the allowed types");

    using Sign = R(Args...);
    using Params = detail::type_list<Args...>;

    using Object::Object;

    Function() = default;
    Function(const Function&) = default;
    Function(Function&& other) = default;
    Function& operator= (const Function&) = default;
    Function& operator= (Function&& other) = default;
    ~Function() = default;

    static Function from(const Object& obj) {
        return Function{obj.context(), JS_DupValue(obj.context(), obj.value())};
    }

    static Function from(Object&& obj) {
        return Function{obj.context(), JS_DupValue(obj.context(), obj.value())};
    }

    static Function from(JSContext* ctx, std::function<Sign> func) {
        // Maybe we should require @func to receive this_obj as first parameter,
        // like std::function<R(const Object&, Args...)> ?

        static JSClassID id = 0;
        auto rt = JS_GetRuntime(ctx);
        if(id == 0) {
            JS_NewClassID(rt, &id);
            auto class_name = std::format("qjs.{}", meta::type_name<std::function<Sign>>());

            JSClassDef def{
                class_name.c_str(),
                [](JSRuntime* rt, JSValue obj) {
                    auto* ptr = static_cast<std::function<Sign>*>(JS_GetOpaque(obj, id));
                    delete ptr;
                },
                nullptr,
                [](JSContext* ctx,
                   JSValueConst func_obj,
                   JSValueConst this_val,
                   int argc,
                   JSValueConst* argv,
                   int flags) -> JSValue {
                    if(argc != sizeof...(Args)) {
                        return JS_ThrowTypeError(ctx, "Incorrect number of arguments");
                    }
                    return [&]<size_t... Is>(std::index_sequence<Is...>) -> JSValue {
                        auto transformed_args =
                            std::make_tuple(Value{ctx, JS_DupValue(ctx, argv[Is])}
                                                .to<Params::template get_t<Is>>()...);

                        int32_t arg_error = -1;
                        std::string_view type_name = "";
                        ((std::get<Is>(transformed_args).has_value()
                              ? -1
                              : (type_name = meta::type_name<Params::template get_t<Is>>(),
                                 arg_error = Is)),
                         ...);

                        if(arg_error != -1) {
                            return JS_ThrowTypeError(
                                ctx,
                                std::format("Failed to convert argument[{}] to {}",
                                            arg_error,
                                            type_name)
                                    .c_str());
                        }
                        if(auto* ptr =
                               static_cast<std::function<Sign>*>(JS_GetOpaque(func_obj, id))) {
                            if constexpr(std::is_void_v<R>) {
                                (*ptr)(std::get<Is>(transformed_args).value()...);
                                return JS_UNDEFINED;
                            } else {
                                return Value::from(
                                           ctx,
                                           (*ptr)(std::get<Is>(transformed_args).value()...))
                                    .value();
                            }
                        } else {
                            return JS_ThrowTypeError(ctx, "Internal error: C++ functor is null");
                        }
                    }(std::make_index_sequence<sizeof...(Args)>{});
                },
                nullptr};
            JS_NewClass(rt, id, &def);
        }
        Function<Sign> result{ctx, JS_NewObjectClass(ctx, id)};
        JS_SetOpaque(result.value(), new std::function<Sign>(std::move(func)));
        return result;
    }

    std::function<Sign> to() {
        return [self = *this](Args... args) -> R {
            return self(args...);
        };
    }

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

        if constexpr(std::is_void_v<R>) {
            return;
        } else {
            auto result = value.to<R>();
            if(!result.has_value()) {
                JS_ThrowTypeError(this->context(), "Failed to convert function return value");
                throw exception(detail::dump(this->context()));
            }

            return result.value();
        }
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
    ~CModule() = default;

    template <typename Sign>
    const CModule& add_functor(const std::string& name, const Function<Sign>& func) const {
        const_cast<CModule*>(this)->exports.push_back(kv{
            name,
            Value{this->ctx, JS_DupValue(this->ctx, func.value())}
        });
        JS_AddModuleExport(this->ctx, m, name.c_str());
        return *this;
    }

private:
    CModule(JSContext* ctx, JSModuleDef* m, const std::string& name) : ctx(ctx), m(m), name(name) {}

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

        std::unique_ptr<JSContext, JSContextDeleter> ctx = nullptr;
        std::unordered_map<std::string, CModule> modules{};
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

        std::unique_ptr<JSRuntime, JSRuntimeDeleter> rt = nullptr;
        std::unordered_map<std::string, Context> ctxs{};
    };

    Runtime(JSRuntime* js_rt) : raw(std::make_unique<Raw>(js_rt)) {}

    std::unique_ptr<Raw> raw = nullptr;
};

}  // namespace catter::qjs
