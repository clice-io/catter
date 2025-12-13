#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <expected>
#include <optional>
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

#include "libutil/meta.h"

// namespace meta

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
        auto ret = Value{this->context(),
                         JS_GetPropertyStr(this->context(), this->value(), prop_name.c_str())};
        if(ret.is_exception() || ret.is_undefined()) {
            return std::nullopt;
        }
        return ret;
    }

    /**
     * @brief Set a property on the JavaScript object, it is noexcept due to using in `C`.
     *
     * @tparam T The type of the value to set.
     * @param prop_name The name of the property to set.
     * @param val The value to set.
     * @return std::optional<qjs::Exception> Returns an exception if setting the property fails;
     * std::nullopt on success.
     */
    template <typename T>
    std::optional<qjs::Exception> set_property(const std::string& prop_name, T&& val) noexcept {
        if constexpr(std::is_same_v<JSValue, std::remove_cv_t<T>>) {
            JSValue js_val = val;
            int ret = JS_SetPropertyStr(this->context(), this->value(), prop_name.c_str(), js_val);
            if(ret < 0) {
                return qjs::Exception(detail::dump(this->context()));
            }
        } else {
            auto js_val = Value::from<std::remove_cv_t<T>>(this->context(), std::forward<T>(val));
            int ret = JS_SetPropertyStr(this->context(),
                                        this->value(),
                                        prop_name.c_str(),
                                        js_val.value());
            if(ret < 0) {
                return qjs::Exception(detail::dump(this->context()));
            }
        }
        return std::nullopt;
    }

    template <typename T>
    static Object from(T&& value) noexcept {
        return detail::object_trans<std::remove_cvref_t<T>>::from(std::forward<T>(value));
    }

    static std::expected<Object, qjs::Exception> empty_one(JSContext* ctx) noexcept {
        auto obj = JS_NewObject(ctx);
        if(JS_IsException(obj)) {
            return std::unexpected(qjs::Exception(detail::dump(ctx)));
        }
        return Object{ctx, obj};
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
class Function<R(Args...)> : private Object {
public:
    using AllowParamTypes = detail::type_list<bool, int64_t, std::string, Object>;
    using AllowRetTypes = detail::type_list<bool, int64_t, std::string, Object>;

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
            auto class_name = std::format("qjs.{}", meta::type_name<Invocable&&>());
            if constexpr(std::is_convertible_v<Invocable, Sign*> ||
                         std::is_lvalue_reference_v<Invocable&&>) {
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

        if constexpr(std::is_convertible_v<Invocable, Sign*> ||
                     std::is_lvalue_reference_v<Invocable&&>) {
            JS_SetOpaque(result.value(), static_cast<void*>(invocable));
        } else {
            JS_SetOpaque(result.value(), new Opaque(std::forward<Invocable>(invocable)));
        }
        return result;
    }

    /**
     * @brief Create a Function from a C function pointer.
     * This method wraps a C function pointer so that it can be called from JavaScript.
     * You should ensure that FnPtr is a valid function pointer type.

     *
     * @tparam FnPtr The C function pointer to wrap, the first parameter can be JSContext*.
     * @param ctx The QuickJS context.
     * @return A Function object representing the wrapped C function.
     */
    template <auto FnPtr>
        requires std::is_pointer_v<decltype(FnPtr)> &&
                 std::is_function_v<std::remove_pointer_t<decltype(FnPtr)>>
    static Function from_raw(JSContext* ctx, const char* name) noexcept {
        using FuncType = std::remove_cvref_t<decltype(*FnPtr)>;
        Function<Sign> result{ctx, JS_NewCFunction(ctx, FnProxy<FnPtr>, name, sizeof...(Args))};
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
    template <typename Opaque, typename Register>
    static JSValue proxy(JSContext* ctx,
                         JSValueConst func_obj,
                         JSValueConst this_val,
                         int argc,
                         JSValueConst* argv,
                         int flags) noexcept {
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
                throw qjs::Exception("Failed to convert function parameter");
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

    /**
     * @brief A generic function proxy for C function pointers.
     * This function template allows wrapping C function pointers to be used as JavaScript
     * functions in QuickJS.
     * You should ensure that FnPtr is a valid function pointer type.
     * It supports the first parameter to be JSContext*, and it is optional.
     */
    template <auto FnPtr>
        requires std::is_pointer_v<decltype(FnPtr)> &&
                 std::is_function_v<std::remove_pointer_t<decltype(FnPtr)>>
    static auto FnProxy(JSContext* ctx,
                        JSValueConst this_val,
                        int argc,
                        JSValueConst* argv) noexcept -> JSValue {
        using RawFuncType = std::remove_cvref_t<decltype(*FnPtr)>;
        using RawParamTupleTy = catter::meta::FuncDecomposer<RawFuncType>::ParamTy;
        using RetTy = catter::meta::FuncDecomposer<RawFuncType>::RetTy;
        constexpr bool has_ctx = [&] {
            if constexpr(std::tuple_size_v<RawParamTupleTy> > 0) {
                using FirstParam = std::tuple_element_t<0, RawParamTupleTy>;
                if constexpr(std::is_same_v<FirstParam, JSContext*>) {
                    return true;
                }
            }
            return false;
        }();
        using ParamTupleTy = meta::tuple_slice_t<RawParamTupleTy, has_ctx ? 1 : 0>;

        constexpr size_t ParamCount = std::tuple_size_v<ParamTupleTy>;
        if(argc != ParamCount) {
            return JS_ThrowTypeError(ctx, "Incorrect number of arguments");
        }
        return [&]<size_t... Is>(std::index_sequence<Is...>) -> JSValue {
            auto transformer = [&]<size_t N>(std::in_place_index_t<N>) {
                using T = std::tuple_element_t<N, ParamTupleTy>;
                if constexpr(std::is_same_v<T, Object>) {
                    if(JS_IsObject(argv[N])) {
                        return Object{ctx, argv[N]};
                    }
                } else {
                    if(auto opt = qjs::Value{ctx, argv[N]}.to<T>(); opt.has_value()) {
                        return opt.value();
                    }
                }
                throw qjs::Exception("Failed to convert function parameter");
            };
            try {
                if constexpr(has_ctx) {
                    if constexpr(std::is_void_v<RetTy>) {
                        (*FnPtr)(ctx, transformer(std::in_place_index<Is>)...);
                        return JS_UNDEFINED;
                    } else if constexpr(std::is_same_v<RetTy, Object>) {
                        auto res = (*FnPtr)(ctx, transformer(std::in_place_index<Is>)...);
                        return res.release();
                    } else {
                        auto res = (*FnPtr)(ctx, transformer(std::in_place_index<Is>)...);
                        return qjs::Value::from(ctx, res).release();
                    }
                } else {
                    if constexpr(std::is_void_v<RetTy>) {
                        (*FnPtr)(transformer(std::in_place_index<Is>)...);
                        return JS_UNDEFINED;
                    } else if constexpr(std::is_same_v<RetTy, Object>) {
                        auto res = (*FnPtr)(transformer(std::in_place_index<Is>)...);
                        return res.release();
                    } else {
                        auto res = (*FnPtr)(transformer(std::in_place_index<Is>)...);
                        return qjs::Value::from(ctx, res).release();
                    }
                }

            } catch(const qjs::Exception& e) {
                return JS_ThrowInternalError(ctx, "Exception in C++ function: %s", e.what());
            } catch(const std::exception& e) {
                return JS_ThrowInternalError(
                    ctx,
                    "This is an Unexpected exception. If you encounter this, please report a bug to the author: %s",
                    e.what());
            }
        }(std::make_index_sequence<ParamCount>{});
    }
};

template <typename T>
    requires detail::type_list<bool, int64_t, std::string>::contains_v<T>
class Array : private Object {
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
        return len_val.to<uint32_t>().value();
    }

    T get(uint32_t index) const {
        auto val = catter::qjs::Value{this->context(),
                                      JS_GetPropertyUint32(this->context(), this->value(), index)};
        if(val.is_exception()) {
            throw qjs::Exception(detail::dump(this->context()));
        }
        auto result = val.to<T>();
        if(!result.has_value()) {
            throw qjs::Exception("Fail to convert a js value!");
        }
        return result.value();
    }

    void push(T&& value) {
        auto js_val = qjs::Value::from(this->context(), std::forward<T>(value));
        uint32_t len = this->length();
        auto res = JS_SetPropertyUint32(this->context(), this->value(), len, js_val.release());
        if(res < 0) {
            throw qjs::Exception(detail::dump(this->context()));
        }
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

    static std::optional<bool> to(JSContext* ctx, const Value& val) noexcept {
        if(!JS_IsBool(val.value())) {
            return std::nullopt;
        }
        return JS_ToBool(ctx, val.value());
    }
};

template <class Num>
    requires std::is_integral_v<Num>
struct value_trans<Num> {
    static Value from(JSContext* ctx, Num value) noexcept {
        if constexpr(std::is_unsigned_v<Num> && sizeof(Num) <= sizeof(uint32_t)) {
            return Value{ctx, JS_NewUint32(ctx, static_cast<uint32_t>(value))};
        } else if(std::is_signed_v<Num>) {
            return Value{ctx, JS_NewInt64(ctx, static_cast<int32_t>(value))};
        } else {
            static_assert(meta::dep_true<Num>, "Unsupported integral type for value");
        }
    }

    static std::optional<Num> to(JSContext* ctx, const Value& val) noexcept {
        if(!JS_IsNumber(val.value())) {
            return std::nullopt;
        }
        Num result;
        if constexpr(std::is_unsigned_v<Num> && sizeof(Num) <= sizeof(uint32_t)) {
            uint32_t temp;
            if(JS_ToUint32(ctx, &temp, val.value()) < 0) {
                return std::nullopt;
            }
            result = static_cast<Num>(temp);
        } else if(std::is_signed_v<Num>) {
            int64_t temp;
            if(JS_ToInt64(ctx, &temp, val.value()) < 0) {
                return std::nullopt;
            }
            result = static_cast<Num>(temp);
        } else {
            static_assert(meta::dep_true<Num>, "Unsupported integral type for value");
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

    static Object from(const FuncType& value) noexcept {
        return Object{value.context(), value.value()};
    }

    static Object from(FuncType&& value) noexcept {
        auto ctx = value.context();
        return Object{ctx, value.release()};
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

    static std::optional<ArrTy> to(JSContext* ctx, const Object& val) noexcept {
        if(!JS_IsArray(val.value())) {
            return std::nullopt;
        }
        return ArrTy{ctx, val.value()};
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

    const CModule& export_bare_functor(const std::string& name, JSCFunction func, int argc) const {
        this->exports_list().push_back(kv{
            name,
            Value{this->ctx, JS_NewCFunction(this->ctx, func, name.c_str(), argc)}
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
