#pragma once
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

namespace catter::util {

template <auto V, typename T = decltype(V)>
struct mem_fn {
    static_assert(std::is_member_function_pointer_v<T>, "V must be a member function pointer");
};

template <auto V, typename Class, typename Ret, typename... Args>
    requires std::is_member_pointer_v<decltype(V)>
struct mem_fn<V, Ret (Class::*)(Args...)> {
    using ClassType = Class;
    using FunctionType = Ret (Class::*)(Args...);

    constexpr static FunctionType get() {
        return V;
    }
};

template <auto V, typename Class, typename Ret, typename... Args>
    requires std::is_member_function_pointer_v<decltype(V)>
struct mem_fn<V, Ret (Class::*)(Args...) const> {
    using ClassType = Class;
    using FunctionType = Ret (Class::*)(Args...) const;

    constexpr static FunctionType get() {
        return V;
    }
};

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

    function_ref(const function_ref&) = default;
    function_ref(function_ref&&) = default;

    function_ref& operator= (const function_ref&) = default;
    function_ref& operator= (function_ref&&) = default;

    constexpr function_ref(R (*proxy)(Erased, Args&...), Erased ctx) noexcept :
        proxy(proxy), ctx(ctx) {}

    constexpr function_ref(Sign* invokable) noexcept :
        function_ref(
            [](Erased ctx, Args&... args) -> R {
                Sign* fn = ctx.fn;
                return (*fn)(static_cast<Args>(args)...);
            },
            Erased{.fn = invokable}) {};

    template <typename Class, typename MemFn>
    constexpr function_ref(Class* invokable, MemFn) noexcept :
        function_ref(
            [](Erased ctx, Args&... args) -> R {
                return (static_cast<Class*>(ctx.ctx)->*MemFn::get())(static_cast<Args>(args)...);
            },
            Erased{.ctx = invokable}) {
        static_assert(std::is_same_v<Class, typename MemFn::ClassType>, "Class type mismatch!");
    }

    template <typename Class, typename MemFn>
        requires std::is_lvalue_reference_v<Class&&>
    constexpr function_ref(Class&& invokable, MemFn) noexcept : function_ref(&invokable, MemFn{}) {}

    template <typename Class>
        requires (!std::is_same_v<std::remove_cvref_t<Class>, function_ref>)
    constexpr function_ref(Class&& invokable) noexcept :
        function_ref(make(std::forward<Class>(invokable))) {}

    template <typename... CallArgs>
    constexpr R operator() (CallArgs&&... args) const {
        static_assert(
            requires(Sign* fn, CallArgs&&... args) { fn(std::forward<CallArgs>(args)...); },
            "Invokable object must be callable with the given arguments");
        return proxy(ctx, args...);
    }

private:
    template <typename Class>
        requires std::is_lvalue_reference_v<Class&&> && std::is_invocable_r_v<R, Class, Args...>
    constexpr static function_ref make(Class&& invokable) {
        if constexpr(std::is_convertible_v<Class&&, Sign*>) {
            return function_ref(static_cast<Sign*>(std::forward<Class>(invokable)));
        } else {
            return function_ref(std::forward<Class>(invokable),
                                mem_fn<&std::remove_cvref_t<Class>::operator()>{});
        }
    }

    R (*proxy)(Erased, Args&...);
    Erased ctx;
};

template <typename Sign>
class function {
    static_assert(false, "Sign must be a function type");
};

template <typename R, typename... Args>
class function<R(Args...)> {
public:
    using Sign = R(Args...);

    using Erased = union {
        void* ctx;
        Sign* fn;
    };

    using Deleter = void(function*);

    function(const function&) = delete;

    function(function&& other) noexcept {
        this->proxy = std::exchange(other.proxy, nullptr);
        this->ctx = std::exchange(other.ctx, Erased{});
        this->deleter = std::exchange(other.deleter, nullptr);
        std::memcpy(this->storage, other.storage, sizeof(this->storage));
    }

    function& operator= (const function&) = delete;

    function& operator= (function&& other) noexcept {
        this->~function();
        return *new (this) function(std::move(other));
    }

    ~function() {
        if(this->deleter) {
            this->deleter(this);
        }
    }

    constexpr function(R (*proxy)(Erased, Args&...), Erased ctx) noexcept :
        proxy(proxy), ctx(ctx), deleter(nullptr), storage() {}

    constexpr function(R (*proxy)(Erased, Args&...), Erased ctx, Deleter* deleter) noexcept :
        proxy(proxy), ctx(ctx), deleter(deleter), storage() {}

    constexpr function(Sign* invokable) noexcept :
        function(
            [](Erased ctx, Args&... args) -> R {
                Sign* fn = ctx.fn;
                return (*fn)(static_cast<Args>(args)...);
            },
            Erased{.fn = invokable}) {};

    template <typename Class, typename MemFn, typename ClassType = std::remove_cvref_t<Class>>
        requires (sizeof(Class) <= 16)
    constexpr function(Class&& invokable, MemFn) noexcept :
        function(
            [](Erased ctx, Args&... args) -> R {
                return (static_cast<ClassType*>(ctx.ctx)->*MemFn::get())(
                    static_cast<Args>(args)...);
            },
            Erased{.ctx = this->storage},
            [](function* self) { std::destroy_at(self->storage_as<ClassType>()); }) {
        static_assert(std::is_same_v<ClassType, typename MemFn::ClassType>, "Class type mismatch!");
        std::construct_at(this->storage_as<ClassType>(), std::forward<Class>(invokable));
    }

    template <typename Class, typename MemFn, typename ClassType = std::remove_cvref_t<Class>>
    constexpr function(Class&& invokable, MemFn) noexcept :
        function(
            [](Erased ctx, Args&... args) -> R {
                return (static_cast<ClassType*>(ctx.ctx)->*MemFn::get())(
                    static_cast<Args>(args)...);
            },
            Erased{.ctx = new ClassType(std::forward<Class>(invokable))},
            [](function* self) { delete static_cast<ClassType*>(self->ctx.ctx); }) {
        static_assert(std::is_same_v<ClassType, typename MemFn::ClassType>, "Class type mismatch!");
    }

    template <typename Class>
        requires (!std::is_same_v<std::remove_cvref_t<Class>, function>)
    constexpr function(Class&& invokable) noexcept :
        function(make(std::forward<Class>(invokable))) {}

    template <typename... CallArgs>
    constexpr R operator() (CallArgs&&... args) const {
        static_assert(
            requires(Sign* fn, CallArgs&&... args) { fn(std::forward<CallArgs>(args)...); },
            "Invokable object must be callable with the given arguments");
        return proxy(ctx, args...);
    }

private:
    template<typename Class>
    Class* storage_as() {
        return std::launder(reinterpret_cast<Class*>(this->storage));
    }

    template <typename Class>
        requires std::is_invocable_r_v<R, Class, Args...>
    constexpr static function make(Class&& invokable) {
        if constexpr(std::is_convertible_v<Class&&, Sign*>) {
            return function(static_cast<Sign*>(std::forward<Class>(invokable)));
        } else {
            return function(std::forward<Class>(invokable),
                            mem_fn<&std::remove_cvref_t<Class>::operator()>{});
        }
    }

    R (*proxy)(Erased, Args&...);
    Erased ctx;
    Deleter* deleter;
    std::byte storage[16];
};
}  // namespace catter::util
