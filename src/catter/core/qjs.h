#pragma once
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <quickjs.h>
#include <cpptrace/cpptrace.hpp>
#include <kota/support/functional.h>
#include <kota/support/type_traits.h>
#include <kota/meta/name.h>

#include "util/exception.h"

// namespace meta

namespace catter::qjs {

namespace refl = kota::meta;

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
struct value_trans;

template <typename U>
struct object_trans;
}  // namespace detail

/**
 * @brief An exception class for reporting errors from the qjs wrapper.
 * It contains details about the JavaScript exception, including name, message, and stack trace.
 * Also You can use it in qjs::Function to throw exceptions back to JavaScript.
 */
class Exception : public std::exception {
public:
    Exception(const std::string& details) : details(details) {}

    Exception(std::string&& details) : details(std::move(details)) {}

    template <typename... Args>
    Exception(std::format_string<Args...> fmt, Args&&... args) :
        Exception(std::format(fmt, std::forward<Args>(args)...)) {}

    const char* what() const noexcept override {
        return details.c_str();
    }

private:
    std::string details;
};

class TypeException : public Exception {
public:
    TypeException(const std::string& details) : Exception(std::format("TypeError: {}", details)) {}
};

class Error;

class JSException : public Exception {
public:
    inline JSException(const Error& error);
    inline static JSException dump(JSContext* ctx);
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

    static Value undefined(JSContext* ctx) noexcept {
        return Value{ctx, JS_UNDEFINED};
    }

    static Value null(JSContext* ctx) noexcept {
        return Value{ctx, JS_NULL};
    }

    template <typename T>
    std::optional<T> to() const noexcept {
        return detail::value_trans<T>::to(*this);
    }

    template <typename T>
    T as() const {
        return detail::value_trans<T>::as(*this);
    }

    bool is_object() const noexcept {
        return JS_IsObject(this->val);
    }

    bool is_function() const noexcept {
        return JS_IsFunction(this->ctx, this->val);
    }

    bool is_exception() const noexcept {
        return JS_IsException(this->val);
    }

    bool is_undefined() const noexcept {
        return JS_IsUndefined(this->val);
    }

    bool is_null() const noexcept {
        return JS_IsNull(this->val);
    }

    bool is_nothing() const noexcept {
        return this->is_null() || this->is_undefined();
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
class Object : protected Value {
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
            throw qjs::JSException::dump(this->context());
        }
        return ret;
    }

    /**
     * @brief Get the property object, noticed that property maybe undefined.
     */
    template <typename T>
        requires std::is_convertible_v<T, std::string>
    Value operator[] (const T& prop_name) const {
        return get_property(prop_name);
    }

    /**
     * @brief Get the optional property object; return nullopt when it is undefined or throw a
     * exception in js
     *
     * @param prop_name
     * @return std::optional<Value>
     */
    std::optional<Value> get_optional_property(const std::string& prop_name) const noexcept {
        try {
            if(auto ret = get_property(prop_name); ret.is_undefined()) {
                return std::nullopt;
            } else {
                return ret;
            }
        } catch(const qjs::Exception&) {
            return std::nullopt;
        }
    }

    /**
     * @brief Set a property on the JavaScript object, it is noexcept due to using in `C`.
     *
     * @tparam T The type of the value to set, if it is JSValue, will free inside.
     * @param prop_name The name of the property to set.
     * @param val The value to set.
     * @throws qjs::Exception if the operation fails, you can catch it in C++ and convert to JS
     * exception in `C` if needed.
     */
    template <typename T>
    void set_property(const std::string& prop_name, T&& val) {
        if constexpr(std::is_same_v<JSValue, std::remove_cvref_t<T>>) {
            JSValue js_val = JS_DupValue(this->context(), val);
            int ret = JS_SetPropertyStr(this->context(), this->value(), prop_name.c_str(), js_val);
            if(ret < 0) {
                throw qjs::JSException::dump(this->context());
            }
        } else if constexpr(requires {
                                { val.value() } -> std::convertible_to<JSValue>;
                            }) {
            JSValue js_val = JS_DupValue(this->context(), val.value());
            int ret = JS_SetPropertyStr(this->context(), this->value(), prop_name.c_str(), js_val);
            if(ret < 0) {
                throw qjs::JSException::dump(this->context());
            }
        } else {
            auto js_val = Value::from<std::remove_cv_t<T>>(this->context(), std::forward<T>(val));
            int ret = JS_SetPropertyStr(this->context(),
                                        this->value(),
                                        prop_name.c_str(),
                                        js_val.release());
            if(ret < 0) {
                throw qjs::JSException::dump(this->context());
            }
        }
    }

    template <typename T>
    static Object from(T&& value) noexcept {
        return detail::object_trans<std::remove_cvref_t<T>>::from(std::forward<T>(value));
    }

    template <typename T>
    std::optional<T> to() noexcept {
        return detail::object_trans<T>::to(*this);
    }

    template <typename T>
    T as() {
        return detail::object_trans<T>::as(*this);
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

    static Object empty_one(JSContext* ctx) noexcept {
        return Object{ctx, JS_NewObject(ctx)};
    }
};

class Error : protected Object {
public:
    using Object::Object;
    using Object::is_valid;
    using Object::value;
    using Object::context;
    using Object::operator bool;
    using Object::release;

    Error(JSContext* ctx, const JSValue& val) : Object(ctx, val) {}

    Error(JSContext* ctx, JSValue&& val) : Object(ctx, std::move(val)) {}

    Error(const Error&) = default;
    Error(Error&& other) = default;
    Error& operator= (const Error&) = default;
    Error& operator= (Error&& other) = default;
    ~Error() = default;

    std::string message() const {
        return this->get_property("message").as<std::string>();
    }

    std::string stack() const {
        return this->get_property("stack").as<std::string>();
    }

    std::string name() const {
        return this->get_property("name").as<std::string>();
    }

    std::string format() const {
        return std::format("{}: {}\nStack Trace:\n{}",
                           this->name(),
                           this->message(),
                           this->stack());
    }
};

inline JSException::JSException(const Error& error) : Exception(error.format()) {}

inline JSException JSException::dump(JSContext* ctx) {
    return JSException(Error(ctx, JS_GetException(ctx)));
}

/**
 * @brief A typed wrapper for JavaScript functions.
 * This class allows calling JavaScript functions from C++ and creating C++ callbacks that can be
 * called from JavaScript.
 */
template <typename Signature>
class Function {
    static_assert("Function must be instantiated with a function type");
};

/**
 * @brief A typed wrapper for JavaScript functions, it receive a c invocable `noexcept` due to
 * invoking in C. This class allows calling JavaScript functions from C++ and creating C++ callbacks
 * that can be called from JavaScript. Notice that it applies for closure, quickjs will manage the
 * closure's memory. If you want to pass a c ++ function pointer or functor that you manage the
 * memory, Please. It allows the first parameter to be JSContext*, and it is optional.
 *
 * The proxy function's param must be types in AllowParamTypes.
 * The return type must be void or types in AllowRetTypes, or JSValue.
 */
template <typename R, typename... Args>
class Function<R(Args...)> : protected Object {
public:
    using AllowParamTypes =
        detail::type_list<bool, int32_t, uint32_t, int64_t, uint64_t, std::string, Object>;
    using AllowRetTypes =
        detail::type_list<bool, int32_t, uint32_t, int64_t, uint64_t, std::string, Object>;

    static_assert((AllowParamTypes::contains_v<Args> && ...),
                  "Function parameter types must be one of the allowed types");
    static_assert(AllowRetTypes::contains_v<R> || std::is_void_v<R>,
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

    static Function from(JSContext* ctx, Sign*) {
        static_assert(
            kota::dependent_false<Sign*>,
            "Invocable type can't be function type, please use from_raw for function pointer");
    }

    template <typename Invocable>
        requires std::is_invocable_r_v<R, Invocable, Args...>
    static Function from(JSContext* ctx, Invocable&& invocable) noexcept {
        using Register = Object::Register<Invocable&&>;
        using Opaque = std::remove_cvref_t<Invocable>;
        auto rt = JS_GetRuntime(ctx);
        JSClassID id = 0;
        if(auto it = Register::find(rt); it != Register::end()) {
            id = it->second;
        } else {
            auto class_name =
                std::format("qjs.{}", std::string_view{refl::type_name<Invocable&&>()});
            if constexpr(std::is_lvalue_reference_v<Invocable&&>) {
                JSClassDef def{class_name.c_str(),
                               nullptr,
                               nullptr,
                               proxy<Opaque, Register>,
                               nullptr};
                id = Register::create(rt, &def);
            } else {
                JSClassDef def{class_name.c_str(),
                               [](JSRuntime* rt, JSValue obj) {
                                   auto* ptr =
                                       static_cast<Opaque*>(JS_GetOpaque(obj, Register::get(rt)));
                                   delete ptr;
                               },
                               nullptr,
                               proxy<Opaque, Register>,
                               nullptr};

                id = Register::create(rt, &def);
            }
        }
        Function<Sign> result{ctx, JS_NewObjectClass(ctx, id)};

        if constexpr(std::is_lvalue_reference_v<Invocable&&>) {
            JS_SetOpaque(result.value(), static_cast<void*>(std::addressof(invocable)));
        } else {
            JS_SetOpaque(result.value(), new Opaque(std::forward<Invocable>(invocable)));
        }
        return result;
    }

    using SignCtx = R(JSContext*, Args...);

    /**
     * @brief Create a Function from a C function pointer.
     * This method wraps a C function pointer so that it can be called from JavaScript.
     * You should ensure that FnPtr is a valid function pointer type.

     *
     * @tparam FnPtr The C function pointer to wrap, the first parameter can be JSContext*.
     * @param ctx The QuickJS context.
     * @return A Function object representing the wrapped C function.
     */
    template <SignCtx* FnPtr>
    static Function from_raw(JSContext* ctx, const char* name) noexcept {
        Function<Sign> result{ctx, JS_NewCFunction(ctx, proxy<FnPtr>, name, sizeof...(Args))};
        return result;
    }

    template <Sign* FnPtr>
    static Function from_raw(JSContext* ctx, const char* name) noexcept {
        Function<Sign> result{ctx, JS_NewCFunction(ctx, proxy<FnPtr>, name, sizeof...(Args))};
        return result;
    }

    auto as() noexcept {
        return [self = *this](Args... args) -> R {
            return self(args...);
        };
    }

    R invoke(const Object& this_obj, Args... args) const {
        auto tans = [&]<typename T>(T& value) -> JSValue {
            if constexpr(std::is_same_v<T, Object>) {
                return JS_DupValue(this->context(), value.value());
            } else {
                return qjs::Value::from(this->context(), value).release();
            }
        };

        auto argv = std::array<JSValue, sizeof...(Args)>{tans.template operator()<Args>(args)...};

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
            throw qjs::JSException::dump(this->context());
        }

        if constexpr(std::is_void_v<R>) {
            return;
        } else {
            return value.as<R>();
        }
    }

    R operator() (Args... args) const {
        return this->invoke(Object{this->context(), JS_GetGlobalObject(this->context())}, args...);
    }

private:
    template <typename Invocable>
    static JSValue invoke_helper(JSContext* ctx, int argc, JSValueConst* argv, Invocable&& fn) {
        if(argc != sizeof...(Args)) {
            return JS_ThrowTypeError(ctx, "Incorrect number of arguments");
        }

        return [&]<size_t... Is>(std::index_sequence<Is...>) -> JSValue {
            auto transformer = [&]<size_t N>(std::in_place_index_t<N>) {
                using T = detail::type_get<Params, N>;
                return qjs::Value{ctx, argv[N]}.as<T>();
            };
            JSValue result = JS_UNDEFINED;
            cpptrace::try_catch(
                [&] {
                    if constexpr(std::is_void_v<R>) {
                        fn(transformer(std::in_place_index<Is>)...);
                        result = JS_UNDEFINED;
                    } else {
                        auto res = fn(transformer(std::in_place_index<Is>)...);

                        if constexpr(std::is_same_v<R, Object>) {
                            result = res.release();
                        } else {
                            result = qjs::Value::from(ctx, res).release();
                        }
                    }
                },
                [&](const qjs::Exception& e) {
                    auto message =
                        util::format_exception("Exception in C++ function: {}", e.what());
                    result = JS_ThrowInternalError(ctx, "%s", message.c_str());
                },
                [&](const std::exception& e) {
                    auto message = util::format_exception("Unexpected exception: {}", e.what());
                    result = JS_ThrowInternalError(ctx, "%s", message.c_str());
                },
                [&] {
                    auto message = util::format_exception("Unknown exception in C++ function.");
                    result = JS_ThrowInternalError(ctx, "%s", message.c_str());
                });
            return result;
        }(std::make_index_sequence<sizeof...(Args)>{});
    }

    template <typename Opaque, typename Register>
    static JSValue proxy(JSContext* ctx,
                         JSValueConst func_obj,
                         [[maybe_unused]] JSValueConst this_val,
                         int argc,
                         JSValueConst* argv,
                         [[maybe_unused]] int flags) noexcept {

        auto* ptr = static_cast<Opaque*>(JS_GetOpaque(func_obj, Register::get(JS_GetRuntime(ctx))));

        if(!ptr) {
            return JS_ThrowInternalError(ctx, "Internal error: C++ functor is null");
        }

        return invoke_helper(ctx, argc, argv, [&]<typename... Ts>(Ts&&... args) -> decltype(auto) {
            return (*ptr)(std::forward<Ts>(args)...);
        });
    }

    template <Sign* FnPtr>
    static JSValue proxy(JSContext* ctx,
                         [[maybe_unused]] JSValueConst this_val,
                         int argc,
                         JSValueConst* argv) noexcept {

        return invoke_helper(ctx, argc, argv, [&]<typename... Ts>(Ts&&... args) -> decltype(auto) {
            return (*FnPtr)(std::forward<Ts>(args)...);
        });
    }

    template <SignCtx* FnPtr>
    static JSValue proxy(JSContext* ctx,
                         [[maybe_unused]] JSValueConst this_val,
                         int argc,
                         JSValueConst* argv) noexcept {
        return invoke_helper(ctx, argc, argv, [&]<typename... Ts>(Ts&&... args) -> decltype(auto) {
            return (*FnPtr)(ctx, std::forward<Ts>(args)...);
        });
    }
};

using Parameters = std::vector<Value>;

template <typename R>
class Function<R(Parameters)> : protected Object {
public:
    using AllowRetTypes =
        detail::type_list<bool, int32_t, uint32_t, int64_t, uint64_t, std::string, Object>;
    static_assert(AllowRetTypes::contains_v<R> || std::is_void_v<R>,
                  "Function return type must be one of the allowed types");

    using Params = Parameters;
    using Sign = R(Params);

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

    static Function from(JSContext* ctx, Sign*) {
        static_assert(
            kota::dependent_false<Sign*>,
            "Invocable type can't be function type, please use from_raw for function pointer");
    }

    template <typename Invocable>
        requires std::is_invocable_r_v<R, Invocable, Params>
    static Function from(JSContext* ctx, Invocable&& invocable) noexcept {
        using Register = Object::Register<Invocable&&>;
        using Opaque = std::remove_cvref_t<Invocable>;
        auto rt = JS_GetRuntime(ctx);
        JSClassID id = 0;
        if(auto it = Register::find(rt); it != Register::end()) {
            id = it->second;
        } else {
            auto class_name =
                std::format("qjs.{}", std::string_view{refl::type_name<Invocable&&>()});
            if constexpr(std::is_lvalue_reference_v<Invocable&&>) {
                JSClassDef def{class_name.c_str(),
                               nullptr,
                               nullptr,
                               proxy<Opaque, Register>,
                               nullptr};
                id = Register::create(rt, &def);
            } else {
                JSClassDef def{class_name.c_str(),
                               [](JSRuntime* rt, JSValue obj) {
                                   auto* ptr =
                                       static_cast<Opaque*>(JS_GetOpaque(obj, Register::get(rt)));
                                   delete ptr;
                               },
                               nullptr,
                               proxy<Opaque, Register>,
                               nullptr};

                id = Register::create(rt, &def);
            }
        }
        Function<Sign> result{ctx, JS_NewObjectClass(ctx, id)};

        if constexpr(std::is_lvalue_reference_v<Invocable&&>) {
            JS_SetOpaque(result.value(), static_cast<void*>(std::addressof(invocable)));
        } else {
            JS_SetOpaque(result.value(), new Opaque(std::forward<Invocable>(invocable)));
        }
        return result;
    }

    using SignCtx = R(JSContext*, Params);

    /**
     * @brief Create a Function from a C function pointer.
     * This method wraps a C function pointer so that it can be called from JavaScript.
     * You should ensure that FnPtr is a valid function pointer type.

     *
     * @tparam FnPtr The C function pointer to wrap, the first parameter can be JSContext*.
     * @param ctx The QuickJS context.
     * @return A Function object representing the wrapped C function.
     */
    template <SignCtx* FnPtr>
    static Function from_raw(JSContext* ctx, const char* name) noexcept {
        Function<Sign> result{ctx, JS_NewCFunction(ctx, proxy<FnPtr>, name, 0)};
        return result;
    }

    template <Sign* FnPtr>
    static Function from_raw(JSContext* ctx, const char* name) noexcept {
        Function<Sign> result{ctx, JS_NewCFunction(ctx, proxy<FnPtr>, name, 0)};
        return result;
    }

    auto as() noexcept {
        return [self = *this](Params args) -> R {
            return self(args);
        };
    }

    R invoke(const Object& this_obj, const Params& args) const {
        std::vector<JSValue> argv{};
        argv.reserve(args.size());

        for(const auto& arg: args) {
            if(!arg.is_valid()) {
                throw TypeException("Function argument contains an invalid value");
            }
            argv.push_back(JS_DupValue(this->context(), arg.value()));
        }

        auto value = qjs::Value{this->context(),
                                JS_Call(this->context(),
                                        this->value(),
                                        this_obj.value(),
                                        static_cast<int>(argv.size()),
                                        argv.data())};
        for(auto& v: argv) {
            JS_FreeValue(this->context(), v);
        }

        if(value.is_exception()) {
            throw qjs::JSException::dump(this->context());
        }

        if constexpr(std::is_void_v<R>) {
            return;
        } else {
            return value.as<R>();
        }
    }

    R operator() (Params args) const {
        return this->invoke(Object{this->context(), JS_GetGlobalObject(this->context())}, args);
    }

private:
    template <typename Invocable>
    static JSValue invoke_helper(JSContext* ctx, int argc, JSValueConst* argv, Invocable&& fn) {
        Params args{};
        args.reserve(argc);
        for(int i = 0; i < argc; ++i) {
            args.emplace_back(ctx, argv[i]);
        }

        JSValue result = JS_UNDEFINED;
        cpptrace::try_catch(
            [&] {
                if constexpr(std::is_void_v<R>) {
                    fn(std::move(args));
                    result = JS_UNDEFINED;
                } else {
                    auto res = fn(std::move(args));

                    if constexpr(std::is_same_v<R, Object>) {
                        result = res.release();
                    } else {
                        result = qjs::Value::from(ctx, res).release();
                    }
                }
            },
            [&](const qjs::Exception& e) {
                auto message = util::format_exception("Exception in C++ function: {}", e.what());
                result = JS_ThrowInternalError(ctx, "%s", message.c_str());
            },
            [&](const std::exception& e) {
                auto message = util::format_exception("Unexpected exception: {}", e.what());
                result = JS_ThrowInternalError(ctx, "%s", message.c_str());
            },
            [&] {
                auto message = util::format_exception("Unknown exception in C++ function.");
                result = JS_ThrowInternalError(ctx, "%s", message.c_str());
            });
        return result;
    }

    template <typename Opaque, typename Register>
    static JSValue proxy(JSContext* ctx,
                         JSValueConst func_obj,
                         [[maybe_unused]] JSValueConst this_val,
                         int argc,
                         JSValueConst* argv,
                         [[maybe_unused]] int flags) noexcept {

        auto* ptr = static_cast<Opaque*>(JS_GetOpaque(func_obj, Register::get(JS_GetRuntime(ctx))));

        if(!ptr) {
            return JS_ThrowInternalError(ctx, "Internal error: C++ functor is null");
        }

        return invoke_helper(ctx, argc, argv, [&]<typename... Ts>(Ts&&... args) -> decltype(auto) {
            return (*ptr)(std::forward<Ts>(args)...);
        });
    }

    template <Sign* FnPtr>
    static JSValue proxy(JSContext* ctx,
                         [[maybe_unused]] JSValueConst this_val,
                         int argc,
                         JSValueConst* argv) noexcept {

        return invoke_helper(ctx, argc, argv, [&]<typename... Ts>(Ts&&... args) -> decltype(auto) {
            return (*FnPtr)(std::forward<Ts>(args)...);
        });
    }

    template <SignCtx* FnPtr>
    static JSValue proxy(JSContext* ctx,
                         [[maybe_unused]] JSValueConst this_val,
                         int argc,
                         JSValueConst* argv) noexcept {
        return invoke_helper(ctx, argc, argv, [&]<typename... Ts>(Ts&&... args) -> decltype(auto) {
            return (*FnPtr)(ctx, std::forward<Ts>(args)...);
        });
    }
};

template <typename T>
    requires detail::type_list<int32_t, uint32_t, int64_t, uint64_t, std::string>::contains_v<T>
class Array : protected Object {
public:
    using Object::Object;
    using Object::is_valid;
    using Object::value;
    using Object::context;
    using Object::operator bool;
    using Object::release;

    Array() = default;
    Array(const Array&) = default;
    Array(Array&& other) = default;
    Array& operator= (const Array&) = default;
    Array& operator= (Array&& other) = default;
    ~Array() = default;

    uint32_t length() const {
        qjs::Value len_val = this->get_property("length");
        if(len_val.is_exception()) {
            throw qjs::JSException::dump(this->context());
        }
        return len_val.as<uint32_t>();
    }

    T operator[] (uint32_t index) const {
        auto elem = catter::qjs::Value{this->context(),
                                       JS_GetPropertyUint32(this->context(), this->value(), index)};
        if(elem.is_exception()) {
            throw qjs::JSException::dump(this->context());
        }
        return elem.as<T>();
    }

    std::optional<T> get(uint32_t index) const noexcept {
        try {
            return this->operator[] (index);
        } catch(const qjs::TypeException&) {
            return std::nullopt;
        }
    }

    template <typename V>
    void push(V&& item) {
        auto js_val = qjs::Value::from(this->context(), T{std::forward<V>(item)});
        uint32_t len = this->length();
        auto res = JS_SetPropertyUint32(this->context(), this->value(), len, js_val.release());
        if(res < 0) {
            throw qjs::JSException::dump(this->context());
        }
    }

    template <typename TT>
    struct array_trans {
        static_assert(kota::dependent_false<TT>, "Unsupported array element type for array_trans");
    };

    template <typename TT>
        requires std::same_as<TT, T>
    struct array_trans<std::vector<TT>> {
        static Array<T> from(JSContext* ctx, const std::vector<T>& vec) noexcept {
            std::vector<JSValue> js_values;
            js_values.reserve(vec.size());
            for(auto&& item: vec) {
                js_values.push_back(qjs::Value::from(ctx, item).release());
            }
            return Array<T>{ctx,
                            JS_NewArrayFrom(ctx, static_cast<int>(vec.size()), js_values.data())};
        }

        static std::vector<T> as(const Array<T>& arr) {
            std::vector<T> result;
            uint32_t len = arr.length();
            for(uint32_t i = 0; i < len; ++i) {
                result.push_back(arr[i]);
            }
            return result;
        }

        static std::optional<std::vector<T>> to(const Array<T>& arr) noexcept {
            try {
                std::vector<T> result;
                uint32_t len = arr.length();
                for(uint32_t i = 0; i < len; ++i) {
                    result.push_back(arr[i]);
                }
                return result;
            } catch(const qjs::TypeException&) {
                return std::nullopt;
            }
        }
    };

    template <typename R>
    static Array<T> from(JSContext* ctx, R&& range) noexcept {
        return array_trans<std::remove_cvref_t<R>>::from(ctx, std::forward<R>(range));
    }

    template <typename R>
    R as() {
        return array_trans<std::remove_cvref_t<R>>::as(*this);
    }

    static qjs::Array<T> empty_one(JSContext* ctx) noexcept {
        return Array{ctx, JS_NewArray(ctx)};
    }
};

namespace detail {
template <>
struct value_trans<bool> {
    static Value from(JSContext* ctx, bool value) noexcept {
        return Value{ctx, JS_NewBool(ctx, value)};
    }

    static bool as(const Value& val) {
        if(!JS_IsBool(val.value())) {
            throw TypeException("Value is not a boolean");
        }
        return JS_ToBool(val.context(), val.value());
    }

    static std::optional<bool> to(const Value& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <class Num>
    requires std::is_integral_v<Num>
struct value_trans<Num> {
    static Value from(JSContext* ctx, Num value) noexcept {
        if constexpr(std::is_unsigned_v<Num> && sizeof(Num) <= sizeof(uint32_t)) {
            return Value{ctx, JS_NewUint32(ctx, static_cast<uint32_t>(value))};
        } else if constexpr(std::is_signed_v<Num>) {
            return Value{ctx, JS_NewInt64(ctx, static_cast<int64_t>(value))};
        } else {
            static_assert(kota::dependent_false<Num>, "Unsupported integral type for value");
        }
    }

    static Num as(const Value& val) {
        if(!JS_IsNumber(val.value())) {
            throw TypeException("Value is not a number");
        }
        if constexpr(std::is_unsigned_v<Num>) {
            if constexpr(sizeof(Num) <= sizeof(uint32_t)) {
                uint32_t temp;
                if(JS_ToUint32(val.context(), &temp, val.value()) < 0) {
                    throw TypeException("Failed to convert value to uint32_t");
                }
                return static_cast<Num>(temp);
            } else {
                uint64_t temp;
                if(JS_ToIndex(val.context(), &temp, val.value()) < 0) {
                    throw TypeException("Failed to convert value to uint32_t");
                }
                return static_cast<Num>(temp);
            }
        } else if constexpr(std::is_signed_v<Num>) {
            if constexpr(sizeof(Num) <= sizeof(int32_t)) {
                int32_t temp;
                if(JS_ToInt32(val.context(), &temp, val.value()) < 0) {
                    throw TypeException("Failed to convert value to uint32_t");
                }
                return static_cast<Num>(temp);
            } else {
                int64_t temp;
                if(JS_ToInt64(val.context(), &temp, val.value()) < 0) {
                    throw TypeException("Failed to convert value to int64_t");
                }
                return static_cast<Num>(temp);
            }

        } else {
            static_assert(kota::dependent_false<Num>, "Unsupported integral type for value");
        }
    }

    static std::optional<Num> to(const Value& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <>
struct value_trans<std::string> {
    static Value from(JSContext* ctx, const std::string& value) noexcept {
        return Value{ctx, JS_NewStringLen(ctx, value.data(), value.size())};
    }

    static std::string as(const Value& val) {
        if(!JS_IsString(val.value())) {
            throw TypeException("Value is not a string");
        }
        size_t len;
        const char* str = JS_ToCStringLen(val.context(), &len, val.value());
        if(str == nullptr) {
            throw TypeException("Failed to convert value to string");
        }
        std::string result{str, len};
        JS_FreeCString(val.context(), str);
        return result;
    }

    static std::optional<std::string> to(const Value& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
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

    static Object as(const Value& val) {
        if(!JS_IsObject(val.value())) {
            throw TypeException("Value is not an object");
        }
        return Object{val.context(), val.value()};
    }

    static std::optional<Object> to(const Value& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <>
struct value_trans<Error> {
    static Value from(const Error& value) noexcept {
        return Value{value.context(), value.value()};
    }

    static Value from(Error&& value) noexcept {
        auto ctx = value.context();
        return Value{ctx, value.release()};
    }

    static Error as(const Value& val) {
        return val.as<Object>().as<Error>();
    }

    static std::optional<Error> to(const Value& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <typename T>
struct value_trans<Array<T>> {
    static Value from(const Array<T>& value) noexcept {
        return Value{value.context(), value.value()};
    }

    static Value from(Array<T>&& value) noexcept {
        auto ctx = value.context();
        return Value{ctx, value.release()};
    }

    static Array<T> as(const Value& val) {
        return val.as<Object>().as<Array<T>>();
    }

    static std::optional<Array<T>> to(const Value& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <typename R, typename... Args>
struct value_trans<Function<R(Args...)>> {
    using FuncType = Function<R(Args...)>;

    static Value from(const FuncType& value) noexcept {
        return Value{value.context(), value.value()};
    }

    static Value from(FuncType&& value) noexcept {
        auto ctx = value.context();
        return Value{ctx, value.release()};
    }

    static FuncType as(const Value& val) {
        return val.as<Object>().as<FuncType>();
    }

    static std::optional<FuncType> to(const Value& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <>
struct object_trans<Error> {
    static Object from(const Error& value) noexcept {
        return Object{value.context(), value.value()};
    }

    static Object from(Error&& value) noexcept {
        auto ctx = value.context();
        return Object{ctx, value.release()};
    }

    static Error as(const Object& obj) {
        if(!JS_IsError(obj.value())) {
            throw TypeException("Object is not an error");
        }
        return Error{obj.context(), obj.value()};
    }

    static std::optional<Error> to(const Object& obj) noexcept {
        try {
            return as(obj);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <typename T>
struct object_trans<Array<T>> {
    using ArrTy = Array<T>;

    static Object from(const ArrTy& value) noexcept {
        return Object{value.context(), value.value()};
    }

    static Object from(ArrTy&& value) noexcept {
        auto ctx = value.context();
        return Object{ctx, value.release()};
    }

    static ArrTy as(const Object& obj) {
        if(!JS_IsArray(obj.value())) {
            throw TypeException("Object is not an array");
        }
        return ArrTy{obj.context(), obj.value()};
    }

    static std::optional<ArrTy> to(const Object& obj) noexcept {
        try {
            return as(obj);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <typename R, typename... Args>
struct object_trans<Function<R(Args...)>> {
    using FuncType = Function<R(Args...)>;

    static Object from(const FuncType& value) noexcept {
        return Object{value.context(), value.value()};
    }

    static Object from(FuncType&& value) noexcept {
        auto ctx = value.context();
        return Object{ctx, value.release()};
    }

    static FuncType as(const Object& obj) {
        if(!JS_IsFunction(obj.context(), obj.value())) {
            throw TypeException("Object is not a function");
        }

        if(obj.get_property("length").as<int64_t>() != sizeof...(Args)) {
            throw TypeException("Function has incorrect number of arguments");
        }

        return FuncType{obj.context(), obj.value()};
    }

    static std::optional<FuncType> to(const Object& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
    }
};

template <typename R>
struct object_trans<Function<R(Parameters)>> {
    using FuncType = Function<R(Parameters)>;

    static Object from(const FuncType& value) noexcept {
        return Object{value.context(), value.value()};
    }

    static Object from(FuncType&& value) noexcept {
        auto ctx = value.context();
        return Object{ctx, value.release()};
    }

    static FuncType as(const Object& obj) {
        if(!JS_IsFunction(obj.context(), obj.value())) {
            throw TypeException("Object is not a function");
        }

        return FuncType{obj.context(), obj.value()};
    }

    static std::optional<FuncType> to(const Object& val) noexcept {
        try {
            return as(val);
        } catch(const TypeException&) {
            return std::nullopt;
        }
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
    const CModule& export_functor(const std::string& export_name,
                                  const Function<Sign>& func) const {
        this->exports_list().push_back(kv{
            export_name,
            Value{this->ctx, func.value()}
        });
        if(JS_AddModuleExport(this->ctx, m, export_name.c_str()) < 0) {
            throw qjs::Exception("Failed to add export '{}' to module '{}'",
                                 export_name,
                                 this->name);
        }
        return *this;
    }

    const CModule& export_bare_functor(const std::string& export_name,
                                       JSCFunction func,
                                       int argc) const {
        this->exports_list().push_back(kv{
            export_name,
            Value{this->ctx, JS_NewCFunction(this->ctx, func, export_name.c_str(), argc)}
        });
        if(JS_AddModuleExport(this->ctx, m, export_name.c_str()) < 0) {
            throw qjs::Exception("Failed to add export '{}' to module '{}'",
                                 export_name,
                                 this->name);
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

    /** Get or create a CModule with the given name
     * @name: The name of the module.
     * Different from context, it is used for js module system.
     * In js, you can import it via `import * as mod from 'name';`
     **/
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

                    for(auto& kv: mod.exports_list()) {
                        JS_SetModuleExport(js_ctx, m, kv.name.c_str(), kv.value.release());
                    }
                    return 0;
                });
            if(m == nullptr) {
                throw qjs::Exception("Failed to create new C module");
            }

            return this->raw->modules.emplace(name, CModule(this->js_context(), m, name))
                .first->second;
        }
    }

    Value eval(const char* input, size_t input_len, const char* filename, int eval_flags) const {
        auto val = JS_Eval(this->js_context(), input, input_len, filename, eval_flags);

        if(this->has_exception()) {
            JS_FreeValue(this->js_context(), val);
            throw qjs::JSException::dump(this->js_context());
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
            throw qjs::Exception("Failed to create new JS runtime");
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
                throw qjs::Exception("Failed to create new JS context");
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

namespace json {

template <typename T>
    requires requires(T&& t) {
        { t.value() } -> std::convertible_to<JSValue>;
        { t.context() } -> std::convertible_to<JSContext*>;
    }
std::string stringify(T&& v) {
    auto ctx = v.context();
    auto val = v.value();
    auto json_str_val = qjs::Value{ctx, JS_JSONStringify(ctx, val, JS_UNDEFINED, JS_UNDEFINED)};
    if(json_str_val.is_exception()) {
        throw qjs::JSException::dump(ctx);
    }

    const char* json_cstr = JS_ToCString(ctx, json_str_val.value());
    if(json_cstr) {
        std::string result{json_cstr};
        JS_FreeCString(ctx, json_cstr);
        return result;
    }
    throw qjs::TypeException("Failed to convert value to JSON string");
};  // namespace json

inline qjs::Value parse(const std::string& json_str, const Context& ctx) {

    auto ret = qjs::Value{
        ctx.js_context(),
        JS_ParseJSON(ctx.js_context(), json_str.data(), json_str.size(), "<json input>")};

    if(ret.is_exception()) {
        throw qjs::JSException::dump(ctx.js_context());
    }
    return ret;
}
}  // namespace json

}  // namespace catter::qjs
