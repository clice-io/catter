#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <optional>
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

// namespace meta

namespace catter::qjs {

namespace refl = kota::meta;

namespace detail {

template <typename... Args>
struct type_list {
    template <typename T>
    constexpr static bool contains_v = (std::is_same_v<T, Args> || ...);
};

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
class Exception : public cpptrace::runtime_error {
public:
    Exception(const std::string& details);

    Exception(std::string&& details);

    template <typename... Args>
    Exception(std::format_string<Args...> fmt, Args&&... args) :
        Exception(std::format(fmt, std::forward<Args>(args)...)) {}
};

class TypeException : public Exception {
public:
    TypeException(const std::string& details);
};

class Error;
class Promise;

class JSException : public Exception {
public:
    JSException(const Error& error);
    static JSException dump(JSContext* ctx);
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

    Value(const Value& other) noexcept;

    Value(Value&& other) noexcept;

    Value& operator= (const Value& other) noexcept;

    Value& operator= (Value&& other) noexcept;

    ~Value() noexcept;

    Value(JSContext* ctx, const JSValue& val) noexcept;

    Value(JSContext* ctx, JSValue&& val) noexcept;

    template <typename T>
    static Value from(JSContext* ctx, T&& value) noexcept {
        return detail::value_trans<std::remove_cvref_t<T>>::from(ctx, std::forward<T>(value));
    }

    template <typename T>
    static Value from(T&& value) noexcept {
        return detail::value_trans<std::remove_cvref_t<T>>::from(std::forward<T>(value));
    }

    static Value undefined(JSContext* ctx) noexcept;

    static Value null(JSContext* ctx) noexcept;

    template <typename T>
    std::optional<T> to() const noexcept {
        return detail::value_trans<T>::to(*this);
    }

    template <typename T>
    T as() const {
        return detail::value_trans<T>::as(*this);
    }

    bool is_object() const noexcept;

    bool is_function() const noexcept;

    bool is_exception() const noexcept;

    bool is_undefined() const noexcept;

    bool is_null() const noexcept;

    bool is_nothing() const noexcept;

    bool is_valid() const noexcept;

    operator bool() const noexcept;

    const JSValue& value() const noexcept;

    JSValue release() noexcept;

    JSContext* context() const noexcept;

private:
    JSContext* ctx = nullptr;
    JSValue val = JS_UNINITIALIZED;
};

using Parameters = std::vector<Value>;

/**
 * @brief A wrapper around a QuickJS JSAtom.
 * This class manages the lifecycle of a JSAtom, which is used for efficient string handling in
 * QuickJS.
 */
class Atom {
public:
    Atom() = default;

    Atom(JSContext* ctx, const JSAtom& atom) noexcept;

    Atom(JSContext* ctx, JSAtom&& atom) noexcept;

    Atom(const Atom& other) noexcept;

    Atom(Atom&& other) noexcept;

    Atom& operator= (const Atom& other) noexcept;

    Atom& operator= (Atom&& other) noexcept;

    ~Atom() noexcept;

    JSAtom value() const noexcept;

    std::string to_string() const noexcept;

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

    Value get_property(const std::string& prop_name) const;

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
    std::optional<Value> get_optional_property(const std::string& prop_name) const noexcept;

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

    static Object empty_one(JSContext* ctx) noexcept;
};

class Error : protected Object {
public:
    using Object::Object;
    using Object::is_valid;
    using Object::value;
    using Object::context;
    using Object::operator bool;
    using Object::release;

    Error(JSContext* ctx, const JSValue& val);

    Error(JSContext* ctx, JSValue&& val);

    Error(const Error&) = default;
    Error(Error&& other) = default;
    Error& operator= (const Error&) = default;
    Error& operator= (Error&& other) = default;
    ~Error() = default;

    std::string message() const;

    std::string stack() const;

    std::string name() const;

    std::string format() const;
};

namespace detail {

template <typename... Args>
Parameters make_parameters(JSContext* ctx, Args&&... args) {
    auto make_parameter = [ctx]<typename T>(T&& value) -> Value {
        using ValueType = std::remove_cvref_t<T>;
        if constexpr(std::is_same_v<ValueType, Value>) {
            if constexpr(std::is_rvalue_reference_v<T&&>) {
                auto value_ctx = value.context();
                return Value{value_ctx, value.release()};
            } else {
                return Value{value.context(), value.value()};
            }
        } else if constexpr(std::is_same_v<ValueType, Object> || std::is_same_v<ValueType, Error> ||
                            std::is_same_v<ValueType, Promise>) {
            return Value::from(std::forward<T>(value));
        } else if constexpr(std::is_same_v<ValueType, std::string>) {
            return Value::from(ctx, std::forward<T>(value));
        } else if constexpr(std::is_constructible_v<std::string_view, T&&>) {
            return Value::from(ctx, std::string{std::string_view{std::forward<T>(value)}});
        } else {
            return Value::from(ctx, std::forward<T>(value));
        }
    };

    Parameters params{};
    params.reserve(sizeof...(Args));
    (params.push_back(make_parameter(std::forward<Args>(args))), ...);
    return params;
}

std::vector<JSValueConst> make_argv_view(const Parameters& params);

}  // namespace detail

/**
 * @brief A typed wrapper for JavaScript functions.
 * This class allows calling JavaScript functions from C++ and creating C++ callbacks that can be
 * called from JavaScript.
 */
template <typename Signature>
class Function {
    static_assert(kota::dependent_false<Signature>,
                  "Function must be instantiated with a function type");
};

/**
 * @brief A typed wrapper for JavaScript functions, it receive a c invocable `noexcept` due to
 * invoking in C. This class allows calling JavaScript functions from C++ and creating C++ callbacks
 * that can be called from JavaScript. Notice that it applies for closure, quickjs will manage the
 * closure's memory. If you want to pass a c ++ function pointer or functor that you manage the
 * memory, Please. It allows the first parameter to be JSContext*, and it is optional.
 *
 * The proxy function's params and return type must be supported by value_trans.
 */
template <typename R, typename... Args>
class Function<R(Args...)> : protected Object {
public:
    static_assert(
        (detail::type_list<bool, int32_t, uint32_t, int64_t, uint64_t, std::string, Object>::
             contains_v<Args> &&
         ...),
        "Function parameter types must be one of the allowed types");
    static_assert(detail::type_list<bool,
                                    int32_t,
                                    uint32_t,
                                    int64_t,
                                    uint64_t,
                                    std::string,
                                    Object,
                                    Promise>::contains_v<R> ||
                      std::is_void_v<R>,
                  "Function return type must be one of the allowed types");

    using Sign = R(Args...);

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
        auto params = detail::make_parameters(this->context(), std::move(args)...);
        auto argv = detail::make_argv_view(params);
        auto value = qjs::Value{
            this->context(),
            JS_Call(this->context(), this->value(), this_obj.value(), argv.size(), argv.data())};

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
                using T = std::tuple_element_t<N, std::tuple<Args...>>;
                return qjs::Value{ctx, argv[N]}.as<T>();
            };
            try {
                if constexpr(std::is_void_v<R>) {
                    fn(transformer(std::in_place_index<Is>)...);
                    return JS_UNDEFINED;
                } else {
                    auto res = fn(transformer(std::in_place_index<Is>)...);

                    if constexpr(std::is_same_v<R, Object>) {
                        return res.release();
                    } else {
                        return qjs::Value::from(ctx, res).release();
                    }
                }
            } catch(const qjs::Exception& e) {
                return JS_ThrowInternalError(ctx, "Exception in C++ function: %s", e.what());
            } catch(const std::exception& e) {
                return JS_ThrowInternalError(ctx, "Unexpected exception: %s", e.what());
            }
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

template <typename R>
class Function<R(Parameters)> : protected Object {
public:
    static_assert(detail::type_list<bool,
                                    int32_t,
                                    uint32_t,
                                    int64_t,
                                    uint64_t,
                                    std::string,
                                    Object,
                                    Promise>::contains_v<R> ||
                      std::is_void_v<R>,
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
        auto argv = detail::make_argv_view(args);
        auto value = qjs::Value{
            this->context(),
            JS_Call(this->context(), this->value(), this_obj.value(), argv.size(), argv.data())};

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

        try {
            if constexpr(std::is_void_v<R>) {
                fn(std::move(args));
                return JS_UNDEFINED;
            } else {
                auto res = fn(std::move(args));

                if constexpr(std::is_same_v<R, Object>) {
                    return res.release();
                } else {
                    return qjs::Value::from(ctx, res).release();
                }
            }
        } catch(const qjs::Exception& e) {
            return JS_ThrowInternalError(ctx, "Exception in C++ function: %s", e.what());
        } catch(const std::exception& e) {
            return JS_ThrowInternalError(ctx, "Unexpected exception: %s", e.what());
        }
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

struct PromiseCapability;

class Promise : protected Object {
public:
    using Object::Object;
    using Object::is_valid;
    using Object::value;
    using Object::context;
    using Object::operator bool;
    using Object::release;

    using ThenCallback = Function<void(Parameters)>;

    static PromiseCapability create(JSContext* ctx);
    static Promise from_value(Value&& value);

    bool is_pending() const;

    bool is_fulfilled() const;

    bool is_rejected() const;

    Value result() const;

    template <typename OnFulfilled>
    Promise then(const qjs::Function<OnFulfilled>& on_fulfilled) const {
        qjs::Parameters args;
        args.push_back(Value::from(on_fulfilled));
        return this->then_with_args(args);
    }

    template <typename OnFulfilled, typename OnRejected>
    Promise then(const qjs::Function<OnFulfilled>& on_fulfilled,
                 const qjs::Function<OnRejected>& on_rejected) const {
        qjs::Parameters args;
        args.push_back(Value::from(on_fulfilled));
        args.push_back(Value::from(on_rejected));
        return this->then_with_args(args);
    }

    template <typename OnRejected>
    Promise when_err(const qjs::Function<OnRejected>& on_rejected) const {
        qjs::Parameters args;
        args.push_back(Value::from(on_rejected));
        return this->call_promise_method("catch", args);
    }

private:
    Promise then_with_args(const qjs::Parameters& args) const;

    Promise call_promise_method(const char* method_name, const qjs::Parameters& args) const;
};

struct PromiseCapability {
    PromiseCapability(Promise promise, Value resolve_func, Value reject_func) noexcept;

    Promise promise;

    template <typename... Args>
    void resolve(Args&&... args) const {
        static_assert(sizeof...(Args) <= 1, "Promise resolve accepts at most one argument");
        this->call(resolve_func, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void reject(Args&&... args) const {
        static_assert(sizeof...(Args) <= 1, "Promise reject accepts at most one argument");
        this->call(reject_func, std::forward<Args>(args)...);
    }

    const Value& resolve_function() const noexcept;

    const Value& reject_function() const noexcept;

private:
    template <typename... Args>
    static void call(const Value& function, Args&&... args) {
        auto fn = function.as<qjs::Function<void(qjs::Parameters)>>();
        fn(detail::make_parameters(function.context(), std::forward<Args>(args)...));
    }

    Value resolve_func;
    Value reject_func;
};

std::string format_rejection(Parameters& args);

namespace detail {
template <>
struct value_trans<bool> {
    static Value from(JSContext* ctx, bool value) noexcept;

    static bool as(const Value& val);

    static std::optional<bool> to(const Value& val) noexcept;
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
    static Value from(JSContext* ctx, const std::string& value) noexcept;

    static std::string as(const Value& val);

    static std::optional<std::string> to(const Value& val) noexcept;
};

template <>
struct value_trans<Object> {
    static Value from(const Object& value) noexcept;

    static Value from(Object&& value) noexcept;

    static Object as(const Value& val);

    static std::optional<Object> to(const Value& val) noexcept;
};

template <>
struct value_trans<Error> {
    static Value from(const Error& value) noexcept;

    static Value from(Error&& value) noexcept;

    static Error as(const Value& val);

    static std::optional<Error> to(const Value& val) noexcept;
};

template <>
struct value_trans<Promise> {
    static Value from(JSContext*, const Promise& value) noexcept;

    static Value from(JSContext*, Promise&& value) noexcept;

    static Value from(const Promise& value) noexcept;

    static Value from(Promise&& value) noexcept;

    static Promise as(const Value& val);

    static std::optional<Promise> to(const Value& val) noexcept;
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
    static Object from(const Error& value) noexcept;

    static Object from(Error&& value) noexcept;

    static Error as(const Object& obj);

    static std::optional<Error> to(const Object& obj) noexcept;
};

template <>
struct object_trans<Promise> {
    static Object from(const Promise& value) noexcept;

    static Object from(Promise&& value) noexcept;

    static Promise as(const Object& obj);

    static std::optional<Promise> to(const Object& obj) noexcept;
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
        return export_functor_value(export_name, func.value());
    }

    const CModule& export_bare_functor(const std::string& export_name,
                                       JSCFunction func,
                                       int argc) const;

private:
    const CModule& export_functor_value(const std::string& export_name, JSValueConst value) const;

    CModule(JSContext* ctx, JSModuleDef* m, const std::string& name) noexcept;

    struct kv {
        std::string name;
        Value value;
    };

    std::vector<kv>& exports_list() const noexcept;

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
    const CModule& cmodule(const std::string& name) const;

    Value eval(const char* input, size_t input_len, const char* filename, int eval_flags) const;

    Value eval(std::string_view input, const char* filename, int eval_flags) const;

    Object global_this() const noexcept;

    bool has_exception() const noexcept;

    JSContext* js_context() const noexcept;

    operator bool() const noexcept;

private:
    class Raw {
    public:
        Raw() = default;

        Raw(JSContext* ctx);

        Raw(const Raw&) = delete;
        Raw(Raw&&) = default;
        Raw& operator= (const Raw&) = delete;
        Raw& operator= (Raw&&) = default;

        ~Raw() = default;

    public:
        struct JSContextDeleter {
            void operator() (JSContext* ctx) const noexcept;
        };

        std::unique_ptr<JSContext, JSContextDeleter> ctx = nullptr;
        std::unordered_map<std::string, CModule> modules{};
    };

    void set_opaque() noexcept;

    static Raw* get_opaque(JSContext* ctx) noexcept;

    Context(JSContext* js_ctx) noexcept;

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

    static Runtime create();

    // Get or create a context with the given name
    // @name: The name of the context. Just for identification purposes.
    const Context& context(const std::string& name = "default") const;

    JSRuntime* js_runtime() const noexcept;

    operator bool() const noexcept;

private:
    class Raw {
    public:
        Raw() = default;

        Raw(JSRuntime* rt) noexcept;

        Raw(const Raw&) = delete;
        Raw(Raw&&) = default;
        Raw& operator= (const Raw&) = delete;
        Raw& operator= (Raw&&) = default;

        ~Raw() = default;

    public:
        struct JSRuntimeDeleter {
            void operator() (JSRuntime* rt) const noexcept;
        };

        std::unique_ptr<JSRuntime, JSRuntimeDeleter> rt = nullptr;
        std::unordered_map<std::string, Context> ctxs{};
    };

    Runtime(JSRuntime* js_rt);

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

qjs::Value parse(const std::string& json_str, const Context& ctx);
}  // namespace json

}  // namespace catter::qjs
