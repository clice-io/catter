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

namespace meta {
template <typename T>
consteval std::string_view type_name() {
    std::string_view name =
#if defined(__clang__) || defined(__GNUC__)
        __PRETTY_FUNCTION__;  // Clang / GCC
#elif defined(_MSC_VER)
        __FUNCSIG__;  // MSVC
#else
        static_assert(false, "Unsupported compiler");
#endif

#if defined(__clang__)
    constexpr std::string_view prefix = "std::string_view meta::type_name() [T = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    constexpr std::string_view prefix = "consteval std::string_view meta::type_name() [with T = ";
    constexpr std::string_view suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
    constexpr std::string_view prefix =
        "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl meta::type_name<";
    constexpr std::string_view suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

}  // namespace meta

namespace catter::qjs {
namespace detail {

template <typename... Args>
struct type_list {

    template <size_t I>
    struct get {
        using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
    };

    template <typename T>
    struct contains {
        constexpr static bool value = (std::is_same_v<T, Args> || ...);
    };

    template <typename T>
    constexpr static bool contains_v = contains<T>::value;

    constexpr static size_t size = sizeof...(Args);
};

template <typename Ts, size_t I>
using type_get = typename Ts::template get<I>::type;

template <typename U>
struct value_trans {
    static_assert("Unsupported type for value_trans");
};

template <typename U>
struct object_trans {
    static_assert("Unsupported type for object_trans");
};

inline std::string dump(JSContext* ctx) {
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

/**
 * @brief An exception class for reporting errors from the qjs wrapper.
 * It contains details about the JavaScript exception, including name, message, and stack trace.
 * Also You can use it in qjs::Function to throw exceptions back to JavaScript.
 */
class Exception : public std::exception {
public:
    Exception(std::string&& details) : details(std::move(details)) {}

    const char* what() const noexcept override {
        return details.c_str();
    }

private:
    std::string details;
};

/**
 * @brief A wrapper around a QuickJS JSValue.
 * This class manages the lifecycle of a JSValue, providing methods for type conversion,
 * checking for exceptions, and interacting with the QuickJS engine.
 */
class Value {
public:
    // Maybe we can prohibit copy and only allow move?
    Value() = default;

    Value(const Value& other) noexcept : ctx(other.ctx), val(JS_DupValue(other.ctx, other.val)) {}

    Value(Value&& other) noexcept :
        ctx(std::exchange(other.ctx, nullptr)), val(std::exchange(other.val, JS_UNINITIALIZED)) {}

    Value& operator= (const Value& other) noexcept {
        if(this != &other) {
            if(this->ctx) {
                JS_FreeValue(this->ctx, this->val);
            }
            this->ctx = other.ctx;
            this->val = JS_DupValue(other.ctx, other.val);
        }
        return *this;
    }

    Value& operator= (Value&& other) noexcept {
        if(this != &other) {
            if(this->ctx) {
                JS_FreeValue(this->ctx, this->val);
            }
            ctx = std::exchange(other.ctx, nullptr);
            val = std::exchange(other.val, JS_UNINITIALIZED);
        }
        return *this;
    }

    ~Value() noexcept {
        if(this->ctx) {
            JS_FreeValue(ctx, val);
        }
    }

    Value(JSContext* ctx, const JSValue& val) noexcept : ctx(ctx), val(JS_DupValue(ctx, val)) {}

    Value(JSContext* ctx, JSValue&& val) noexcept : ctx(ctx), val(std::move(val)) {}

    template <typename T>
    static Value from(JSContext* ctx, T&& value) noexcept {
        return detail::value_trans<std::remove_cvref_t<T>>::from(ctx, std::forward<T>(value));
    }

    template <typename T>
    static Value from(T&& value) noexcept {
        return detail::value_trans<std::remove_cvref_t<T>>::from(std::forward<T>(value));
    }

    template <typename T>
    std::optional<T> to() noexcept {
        return detail::value_trans<T>::to(this->ctx, *this);
    }

    bool is_exception() const noexcept {
        return JS_IsException(this->val);
    }

    bool is_valid() const noexcept {
        return this->ctx != nullptr;
    }

    operator bool() const noexcept {
        return this->is_valid();
    }

    const JSValue& value() const noexcept {
        return this->val;
    }

    JSValue release() noexcept {
        JSValue temp = this->val;
        this->val = JS_UNINITIALIZED;
        this->ctx = nullptr;
        return temp;
    }

    JSContext* context() const noexcept {
        return this->ctx;
    }

private:
    JSContext* ctx = nullptr;
    JSValue val = JS_UNINITIALIZED;
};

/**
 * @brief A wrapper around a QuickJS JSAtom.
 * This class manages the lifecycle of a JSAtom, which is used for efficient string handling in
 * QuickJS.
 */
class Atom {
public:
    Atom() = default;

    Atom(JSContext* ctx, const JSAtom& atom) noexcept : ctx(ctx), atom(JS_DupAtom(ctx, atom)) {}

    Atom(JSContext* ctx, JSAtom&& atom) noexcept : ctx(ctx), atom(std::move(atom)) {}

    Atom(const Atom& other) noexcept : ctx(other.ctx), atom(JS_DupAtom(other.ctx, other.atom)) {}

    Atom(Atom&& other) noexcept :
        ctx(std::exchange(other.ctx, nullptr)), atom(std::exchange(other.atom, JS_ATOM_NULL)) {}

    Atom& operator= (const Atom& other) noexcept {
        if(this != &other) {
            if(this->ctx) {
                JS_FreeAtom(this->ctx, this->atom);
            }
            this->ctx = other.ctx;
            this->atom = JS_DupAtom(other.ctx, other.atom);
        }
        return *this;
    }

    Atom& operator= (Atom&& other) noexcept {
        if(this != &other) {
            if(this->ctx) {
                JS_FreeAtom(this->ctx, this->atom);
            }
            ctx = std::exchange(other.ctx, nullptr);
            atom = std::exchange(other.atom, JS_ATOM_NULL);
        }
        return *this;
    }

    ~Atom() noexcept {
        if(this->ctx) {
            JS_FreeAtom(this->ctx, this->atom);
        }
    }

    JSAtom value() const noexcept {
        return this->atom;
    }

    std::string to_string() const noexcept {
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

/**
 * @brief A specialized Value that represents a JavaScript object.
 * It inherits from Value and provides object-specific operations like property access.
 */
class Object : private Value {
public:
    using Value::Value;
    using Value::is_valid;
    using Value::value;
    using Value::context;
    using Value::operator bool;
    using Value::release;

    Object() = default;
    Object(const Object&) = default;
    Object(Object&& other) = default;
    Object& operator= (const Object&) = default;
    Object& operator= (Object&& other) = default;
    ~Object() = default;

    Value get_property(const std::string& prop_name) const {
        auto ret = Value{this->context(),
                         JS_GetPropertyStr(this->context(), this->value(), prop_name.c_str())};
        if(ret.is_exception()) {
            throw qjs::Exception(detail::dump(this->context()));
        }
        return ret;
    }

    template <typename T>
    static Value from(T&& value) noexcept {
        return detail::object_trans<std::remove_cvref_t<T>>::from(std::forward<T>(value));
    }

    template <typename T>
    std::optional<T> to() noexcept {
        return detail::object_trans<T>::to(this->context(), *this);
    }

    template <typename T>
    class Register {
    public:
        static auto find(JSRuntime* rt) noexcept {
            return class_ids.find(rt);
        }

        static auto end() noexcept {
            return class_ids.end();
        }

        static JSClassID get(JSRuntime* rt) noexcept {
            if(auto it = class_ids.find(rt); it != class_ids.end()) {
                return it->second;
            } else {
                return JS_INVALID_CLASS_ID;
            }
        }

        static JSClassID create(JSRuntime* rt, JSClassDef* def) noexcept {
            JSClassID id = 0;
            JS_NewClassID(rt, &id);
            JS_NewClass(rt, id, def);
            class_ids[rt] = id;
            return id;
        }

    private:
        inline static std::unordered_map<JSRuntime*, JSClassID> class_ids{};
    };
};

/**
 * @brief A typed wrapper for JavaScript functions.
 * This class allows calling JavaScript functions from C++ and creating C++ callbacks that can be
 * called from JavaScript.
 */
template <typename Signature>
class Function {
    static_assert("Function must be instantiated with a function type");
};

template <typename R, typename... Args>
class Function<R(Args...)> : private Object {
public:
    using AllowParamTypes = detail::type_list<bool, int64_t, std::string, Object>;

    static_assert((AllowParamTypes::contains_v<Args> && ...),
                  "Function parameter types must be one of the allowed types");
    static_assert(AllowParamTypes::contains_v<R> || std::is_void_v<R>,
                  "Function return type must be one of the allowed types");

    using Sign = R(Args...);
    using Params = detail::type_list<Args...>;

    using Object::Object;
    using Object::is_valid;
    using Object::value;
    using Object::context;
    using Object::operator bool;
    using Object::release;

    Function() = default;
    Function(const Function&) = default;
    Function(Function&& other) = default;
    Function& operator= (const Function&) = default;
    Function& operator= (Function&& other) = default;
    ~Function() = default;

    using Register = Object::Register<std::function<Sign>>;

    template <typename Invocable>
        requires std::is_invocable_r_v<R, Invocable, Args...>
    static Function from(JSContext* ctx, Invocable&& invocable) noexcept {
        using Opaque = std::remove_cvref_t<Invocable>;
        auto rt = JS_GetRuntime(ctx);
        JSClassID id = 0;
        if(auto it = Register::find(rt); it != Register::end()) {
            id = it->second;
        } else {
            auto class_name = std::format("qjs.{}", meta::type_name<Invocable&&>());
            if constexpr(std::is_convertible_v<Invocable, Sign*> ||
                         std::is_lvalue_reference_v<Invocable&&>) {
                JSClassDef def{class_name.c_str(), nullptr, nullptr, proxy<Opaque>, nullptr};
                id = Register::create(rt, &def);
            } else {
                JSClassDef def{class_name.c_str(),
                               [](JSRuntime* rt, JSValue obj) {
                                   auto* ptr =
                                       static_cast<Opaque*>(JS_GetOpaque(obj, Register::get(rt)));
                                   delete ptr;
                               },
                               nullptr,
                               proxy<Opaque>,
                               nullptr};

                id = Register::create(rt, &def);
            }
        }
        Function<Sign> result{ctx, JS_NewObjectClass(ctx, id)};

        if constexpr(std::is_convertible_v<Invocable, Sign*> ||
                     std::is_lvalue_reference_v<Invocable&&>) {
            JS_SetOpaque(result.value(), &invocable);
        } else {
            JS_SetOpaque(result.value(), new Opaque(std::forward<Invocable>(invocable)));
        }
        return result;
    }

    auto to() noexcept {
        return [self = *this](Args... args) -> R {
            return self(args...);
        };
    }

    R invoke(const Object& this_obj, Args... args) const {

#ifdef _MSC_VER
        // MSVC have some problem, so we need to use a lambda here
        auto argv = std::array<JSValue, sizeof...(Args)>{[&] {
            return qjs::Value::from(this->context(), args).release();
        }()...};
#else
        auto argv = std::array<JSValue, sizeof...(Args)>{
            qjs::Value::from(this->context(), args).release()...};
#endif
        auto value = qjs::Value{this->context(),
                                JS_Call(this->context(),
                                        this->value(),
                                        this_obj.value(),
                                        sizeof...(Args),
                                        argv.data())};
        for(auto& v: argv) {
            JS_FreeValue(this->context(), v);
        }

        if(value.is_exception()) {
            throw qjs::Exception(detail::dump(this->context()));
        }

        if constexpr(std::is_void_v<R>) {
            return;
        } else {
            auto result = value.to<R>();
            if(!result.has_value()) {
                JS_ThrowTypeError(this->context(), "Failed to convert function return value");
                throw qjs::Exception(detail::dump(this->context()));
            }

            return result.value();
        }
    }

    R operator() (Args... args) const {
        return this->invoke(Object{this->context(), JS_GetGlobalObject(this->context())}, args...);
    }

private:
    template <typename Opaque>
    static JSValue proxy(JSContext* ctx,
                         JSValueConst func_obj,
                         JSValueConst this_val,
                         int argc,
                         JSValueConst* argv,
                         int flags) {
        if(argc != sizeof...(Args)) {
            return JS_ThrowTypeError(ctx, "Incorrect number of arguments");
        }
        return [&]<size_t... Is>(std::index_sequence<Is...>) -> JSValue {
            auto transformer = [&]<size_t N>(std::in_place_index_t<N>) {
                using T = detail::type_get<Params, N>;
                if constexpr(std::is_same_v<T, Object>) {
                    if(JS_IsObject(argv[N])) {
                        return Object{ctx, argv[N]};
                    }
                } else {
                    if(auto opt = qjs::Value{ctx, argv[N]}.to<T>(); opt.has_value()) {
                        return opt.value();
                    }
                }
                throw qjs::Exception(
                    std::format("Failed to convert argument[{}] to {}", N, meta::type_name<T>()));
            };

            if(auto* ptr = static_cast<Opaque*>(
                   JS_GetOpaque(func_obj, Register::get(JS_GetRuntime(ctx))))) {
                try {
                    if constexpr(std::is_void_v<R>) {
                        (*ptr)(transformer(std::in_place_index<Is>)...);
                        return JS_UNDEFINED;
                    } else if constexpr(std::is_same_v<R, Object>) {
                        auto res = (*ptr)(transformer(std::in_place_index<Is>)...);
                        return res.release();
                    } else {
                        auto res = (*ptr)(transformer(std::in_place_index<Is>)...);
                        return qjs::Value::from(ctx, res).release();
                    }
                } catch(const qjs::Exception& e) {
                    return JS_ThrowInternalError(ctx, "Exception in C++ function: %s", e.what());
                } catch(const std::exception& e) {
                    return JS_ThrowInternalError(
                        ctx,
                        "This is an Unexpected exception. If you encounter this, please report a bug to the author: %s",
                        e.what());
                }
            } else {
                return JS_ThrowInternalError(ctx, "Internal error: C++ functor is null");
            }
        }(std::make_index_sequence<sizeof...(Args)>{});
    }
};

namespace detail {
template <>
struct value_trans<bool> {
    static Value from(JSContext* ctx, bool value) noexcept {
        return Value{ctx, JS_NewBool(ctx, value)};
    }

    static std::optional<bool> to(JSContext* ctx, const Value& val) noexcept {
        if(!JS_IsBool(val.value())) {
            return std::nullopt;
        }
        return JS_ToBool(ctx, val.value());
    }
};

template <>
struct value_trans<int64_t> {
    static Value from(JSContext* ctx, int64_t value) noexcept {
        return Value{ctx, JS_NewInt64(ctx, value)};
    }

    static std::optional<int64_t> to(JSContext* ctx, const Value& val) noexcept {
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
    static Value from(JSContext* ctx, const std::string& value) noexcept {
        return Value{ctx, JS_NewStringLen(ctx, value.data(), value.size())};
    }

    static std::optional<std::string> to(JSContext* ctx, const Value& val) noexcept {
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
    static Value from(const Object& value) noexcept {
        return Value{value.context(), value.value()};
    }

    static Value from(Object&& value) noexcept {
        auto ctx = value.context();
        return Value{ctx, value.release()};
    }

    static std::optional<Object> to(JSContext* ctx, const Value& val) noexcept {
        if(!JS_IsObject(val.value())) {
            return std::nullopt;
        }
        return Object{ctx, val.value()};
    }
};

template <typename R, typename... Args>
struct object_trans<Function<R(Args...)>> {
    using FuncType = Function<R(Args...)>;

    static Value from(const FuncType& value) noexcept {
        return Value{value.context(), value.value()};
    }

    static Value from(FuncType&& value) noexcept {
        auto ctx = value.context();
        return Value{ctx, value.release()};
    }

    static std::optional<FuncType> to(JSContext* ctx, const Object& val) noexcept {
        if(!JS_IsFunction(ctx, val.value())) {
            return std::nullopt;
        }

        if(val.get_property("length").to<int64_t>() != sizeof...(Args)) {
            return std::nullopt;
        }

        return FuncType{ctx, val.value()};
    }
};
}  // namespace detail

/**
 * @brief Represents a C module that can be imported into JavaScript.
 * This class allows exporting qjs::Function to be used as a module in QuickJS.
 * We're not providing creation functions here. Please use Context::cmodule to get a CModule
 * instance, it will ensure the lifecycle is properly managed.
 */
class CModule {
public:
    friend class Context;
    CModule() = default;
    CModule(const CModule&) = delete;
    CModule(CModule&&) = default;
    CModule& operator= (const CModule&) = delete;
    CModule& operator= (CModule&&) = default;
    ~CModule() = default;

    template <typename Sign>
    const CModule& export_functor(const std::string& name, const Function<Sign>& func) const {
        this->exports_list().push_back(kv{
            name,
            Value{this->ctx, func.value()}
        });
        if(JS_AddModuleExport(this->ctx, m, name.c_str()) < 0) {
            throw std::runtime_error(
                std::format("Failed to add export '{}' to module '{}'", name, this->name));
        }
        return *this;
    }

private:
    CModule(JSContext* ctx, JSModuleDef* m, const std::string& name) noexcept :
        ctx(ctx), m(m), name(name) {}

    struct kv {
        std::string name;
        Value value;
    };

    std::vector<kv>& exports_list() const noexcept {
        return *this->exports;
    }

    JSContext* ctx = nullptr;
    JSModuleDef* m = nullptr;
    std::string name{};
    std::unique_ptr<std::vector<kv>> exports{std::make_unique<std::vector<kv>>()};
};

/**
 * @brief A wrapper around a QuickJS JSContext.
 * It manages the lifecycle of a JSContext and provides an interface for evaluating scripts,
 * managing modules, and accessing the global object.
 * We're not providing creation functions here. Please use Runtime::context to get a Context
 * instance, it will ensure the lifecycle is properly managed.
 */
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
    const CModule& cmodule(const std::string& name) const {
        if(auto it = this->raw->modules.find(name); it != this->raw->modules.end()) {
            return it->second;
        } else {
            auto m = JS_NewCModule(
                this->js_context(),
                name.data(),
                [](JSContext* js_ctx, JSModuleDef* m) {
                    auto* ctx = Context::get_opaque(js_ctx);

                    auto atom = Atom(js_ctx, JS_GetModuleName(js_ctx, m));

                    if(!ctx) {
                        return -1;
                    }

                    auto& mod = ctx->modules[atom.to_string()];

                    for(const auto& kv: mod.exports_list()) {
                        JS_SetModuleExport(js_ctx, m, kv.name.c_str(), kv.value.value());
                    }
                    return 0;
                });
            if(m == nullptr) {
                throw std::runtime_error("Failed to create new C module");
            }

            return this->raw->modules.emplace(name, CModule(this->js_context(), m, name))
                .first->second;
        }
    }

    Value eval(const char* input, size_t input_len, const char* filename, int eval_flags) const {
        auto val = JS_Eval(this->js_context(), input, input_len, filename, eval_flags);

        if(this->has_exception()) {
            JS_FreeValue(this->js_context(), val);
            throw qjs::Exception(detail::dump(this->js_context()));
        }

        return Value{this->js_context(), std::move(val)};
    }

    Value eval(std::string_view input, const char* filename, int eval_flags) const {
        return this->eval(input.data(), input.size(), filename, eval_flags);
    }

    Object global_this() const noexcept {
        return Object{this->js_context(), JS_GetGlobalObject(this->js_context())};
    }

    bool has_exception() const noexcept {
        return JS_HasException(this->js_context());
    }

    JSContext* js_context() const noexcept {
        return this->raw->ctx.get();
    }

    operator bool() const noexcept {
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
            void operator() (JSContext* ctx) const noexcept {
                JS_FreeContext(ctx);
            }
        };

        std::unique_ptr<JSContext, JSContextDeleter> ctx = nullptr;
        std::unordered_map<std::string, CModule> modules{};
    };

    void set_opaque() noexcept {
        JS_SetContextOpaque(this->js_context(), this->raw.get());
    }

    static Raw* get_opaque(JSContext* ctx) noexcept {
        return static_cast<Raw*>(JS_GetContextOpaque(ctx));
    }

    Context(JSContext* js_ctx) noexcept : raw(std::make_unique<Raw>(js_ctx)) {
        this->set_opaque();
    }

    std::unique_ptr<Raw> raw = nullptr;
};

/**
 * @brief A wrapper around a QuickJS JSRuntime.
 * This class manages the lifecycle of a JSRuntime and is the top-level object for using QuickJS.
 * It can contain multiple contexts.
 */
class Runtime {
public:
    Runtime() = default;
    Runtime(const Runtime&) = delete;
    Runtime(Runtime&&) = default;
    Runtime& operator= (const Runtime&) = delete;
    Runtime& operator= (Runtime&&) = default;
    ~Runtime() = default;

    static Runtime create() {
        auto js_rt = JS_NewRuntime();
        if(!js_rt) {
            throw std::runtime_error("Failed to create new JS runtime");
        }
        return Runtime(js_rt);
    }

    // Get or create a context with the given name
    // @name: The name of the context. Just for identification purposes.
    const Context& context(const std::string& name = "default") const {
        if(auto it = this->raw->ctxs.find(name); it != this->raw->ctxs.end()) {
            return it->second;
        } else {
            auto js_ctx = JS_NewContext(this->js_runtime());
            if(!js_ctx) {
                throw std::runtime_error("Failed to create new JS context");
            }
            return this->raw->ctxs.emplace(name, Context(js_ctx)).first->second;
        }
    }

    JSRuntime* js_runtime() const noexcept {
        return this->raw->rt.get();
    }

    operator bool() const noexcept {
        return this->raw != nullptr;
    }

private:
    class Raw {
    public:
        Raw() = default;

        Raw(JSRuntime* rt) noexcept : rt(rt) {}

        Raw(const Raw&) = delete;
        Raw(Raw&&) = default;
        Raw& operator= (const Raw&) = delete;
        Raw& operator= (Raw&&) = default;

        ~Raw() = default;

    public:
        struct JSRuntimeDeleter {
            void operator() (JSRuntime* rt) const noexcept {
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
