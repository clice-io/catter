#pragma once
#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
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

template <typename Class, typename MemFn>
concept is_mem_fn_of = requires {
    typename MemFn::ClassType;
    requires std::is_same_v<Class, typename MemFn::ClassType>;
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

private:
    constexpr function_ref(R (*proxy)(const function_ref*, Args&...), Erased ctx) noexcept :
        proxy(proxy), erased(ctx) {}

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

public:
    template <typename Class, typename MemFn>
        requires is_mem_fn_of<Class, MemFn>
    constexpr function_ref(Class* invokable, MemFn) noexcept :
        function_ref(
            [](const function_ref* self, Args&... args) -> R {
                return (static_cast<Class*>(self->erased.ctx)->*MemFn::get())(
                    static_cast<Args>(args)...);
            },
            Erased{.ctx = invokable}) {}

    constexpr function_ref(Sign* invokable) noexcept :
        function_ref(
            [](const function_ref* self, Args&... args) -> R {
                Sign* fn = self->erased.fn;
                return (*fn)(static_cast<Args>(args)...);
            },
            Erased{.fn = invokable}) {};

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
        return proxy(this, args...);
    }

private:
    R (*proxy)(const function_ref*, Args&...);
    Erased erased;
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

    constexpr static std::size_t sbo_size = 16;
    constexpr static std::size_t sbo_align = alignof(std::max_align_t);

    template <typename T>
    constexpr static bool sbo_eligible =
        sizeof(T) <= sbo_size && alignof(T) <= sbo_align && std::is_trivially_copyable_v<T>;

    function(const function&) = delete;

    function(function&& other) noexcept {
        this->proxy = std::exchange(other.proxy, nullptr);
        this->erased = std::exchange(other.erased, Erased{});
        this->deleter = std::exchange(other.deleter, nullptr);
        std::memcpy(this->storage, other.storage, sizeof(this->storage));
    }

    function& operator= (const function&) = delete;

    function& operator= (function&& other) noexcept {
        if(this == &other) {
            return *this;
        }
        this->~function();
        return *new (this) function(std::move(other));
    }

    ~function() {
        if(this->deleter) {
            this->deleter(this);
        }
    }

private:
    constexpr function(R (*proxy)(const function*, Args&...), Erased ctx) noexcept :
        proxy(proxy), erased(ctx), deleter(nullptr), storage() {}

    constexpr function(R (*proxy)(const function*, Args&...), Erased ctx, Deleter* deleter) noexcept
        : proxy(proxy), erased(ctx), deleter(deleter), storage() {}

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

public:
    constexpr function(Sign* invokable) noexcept :
        function(
            [](const function* self, Args&... args) -> R {
                Sign* fn = self->erased.fn;
                return (*fn)(static_cast<Args>(args)...);
            },
            Erased{.fn = invokable}) {};

    template <typename Class, typename MemFn, typename ClassType = std::remove_cvref_t<Class>>
        requires sbo_eligible<ClassType> && is_mem_fn_of<ClassType, MemFn>
    constexpr function(Class&& invokable, MemFn) noexcept :
        function(
            [](const function* self, Args&... args) -> R {
                return (self->storage_as<ClassType>()->*MemFn::get())(static_cast<Args>(args)...);
            },
            Erased{},
            [](function* self) { std::destroy_at(self->storage_as<ClassType>()); }) {
        std::construct_at(reinterpret_cast<ClassType*>(this->storage),
                          std::forward<Class>(invokable));
    }

    template <typename Class, typename MemFn, typename ClassType = std::remove_cvref_t<Class>>
        requires (!sbo_eligible<ClassType>) && is_mem_fn_of<ClassType, MemFn>
    constexpr function(Class&& invokable, MemFn) noexcept :
        function(
            [](const function* self, Args&... args) -> R {
                return (static_cast<ClassType*>(self->erased.ctx)->*MemFn::get())(
                    static_cast<Args>(args)...);
            },
            Erased{.ctx = new ClassType(std::forward<Class>(invokable))},
            [](function* self) { delete static_cast<ClassType*>(self->erased.ctx); }) {}

    template <typename Class>
        requires (!std::is_same_v<std::remove_cvref_t<Class>, function>)
    constexpr function(Class&& invokable) noexcept :
        function(make(std::forward<Class>(invokable))) {}

    template <typename... CallArgs>
    constexpr R operator() (CallArgs&&... args) const {
        static_assert(
            requires(Sign* fn, CallArgs&&... args) { fn(std::forward<CallArgs>(args)...); },
            "Invokable object must be callable with the given arguments");
        return proxy(this, args...);
    }

private:
    template <typename Class>
    const Class* storage_as() const {
        return std::launder(reinterpret_cast<const Class*>(this->storage));
    }

    template <typename Class>
    Class* storage_as() {
        return std::launder(reinterpret_cast<Class*>(this->storage));
    }

    alignas(sbo_align) std::byte storage[sbo_size];
    R (*proxy)(const function*, Args&...);
    Erased erased;
    Deleter* deleter;
};

}  // namespace catter::util
