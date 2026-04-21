#pragma once

#include <cstdlib>
#include <format>
#include <string>
#include <type_traits>
#include <utility>
#include <cpptrace/from_current.hpp>
#include <kota/async/async.h>

namespace catter::util {

namespace detail {

template <typename>
struct callable_argument;

template <typename R, typename Arg>
struct callable_argument<R(Arg)> {
    using type = Arg;
};

template <typename R>
struct callable_argument<R(...)> {
    using type = void;
};

template <typename R, typename Arg>
Arg get_callable_argument_helper(R (*)(Arg));

template <typename R>
void get_callable_argument_helper(R (*)());

template <typename R, typename F, typename Arg>
Arg get_callable_argument_helper(R (F::*)(Arg));

template <typename R, typename F>
void get_callable_argument_helper(R (F::*)());

template <typename R, typename F, typename Arg>
Arg get_callable_argument_helper(R (F::*)(Arg) const);

template <typename R, typename F>
void get_callable_argument_helper(R (F::*)() const);

template <typename F>
decltype(get_callable_argument_helper(&F::operator())) get_callable_argument_wrapper(F);

template <typename F>
using get_callable_argument = decltype(get_callable_argument_wrapper(std::declval<F>()));

template <typename>
constexpr bool is_kota_task_v = false;

template <typename T, typename E, typename C>
constexpr bool is_kota_task_v<kota::task<T, E, C>> = true;

template <typename Task, typename E, typename F, typename Catch>
    requires (!std::is_same_v<E, void>)
Task co_do_try_catch(F f, Catch catcher) {
    Task catch_task;
    bool caught = false;
    CPPTRACE_TRY {
        auto task = std::move(f)();
        if constexpr(std::is_void_v<typename Task::value_type>) {
            co_await std::move(task);
            co_return;
        } else {
            co_return co_await std::move(task);
        }
    }
    CPPTRACE_CATCH(E e) {
        catch_task = std::move(catcher)(std::forward<E>(e));
        caught = true;
    }
    if(caught) {
        if constexpr(std::is_void_v<typename Task::value_type>) {
            co_await std::move(catch_task);
            co_return;
        } else {
            co_return co_await std::move(catch_task);
        }
    }
    std::abort();
}

template <typename Task, typename E, typename F, typename Catch>
    requires std::is_same_v<E, void>
Task co_do_try_catch(F f, Catch catcher) {
    Task catch_task;
    bool caught = false;
    CPPTRACE_TRY {
        auto task = std::move(f)();
        if constexpr(std::is_void_v<typename Task::value_type>) {
            co_await std::move(task);
            co_return;
        } else {
            co_return co_await std::move(task);
        }
    }
    CPPTRACE_CATCH(...) {
        catch_task = std::move(catcher)();
        caught = true;
    }
    if(caught) {
        if constexpr(std::is_void_v<typename Task::value_type>) {
            co_await std::move(catch_task);
            co_return;
        } else {
            co_return co_await std::move(catch_task);
        }
    }
    std::abort();
}

template <typename Task, typename F>
Task co_try_catch_impl(F f) {
    auto task = std::move(f)();
    if constexpr(std::is_void_v<typename Task::value_type>) {
        co_await std::move(task);
        co_return;
    } else {
        co_return co_await std::move(task);
    }
}

template <typename Task, typename F, typename Catch, typename... Catches>
Task co_try_catch_impl(F f, Catch catcher, Catches... catches) {
    auto wrapped = [f = std::move(f), catcher = std::move(catcher)]() mutable -> Task {
        using E = get_callable_argument<Catch>;
        co_return co_await co_do_try_catch<Task, E>(std::move(f), std::move(catcher));
    };
    co_return co_await co_try_catch_impl<Task>(std::move(wrapped), std::move(catches)...);
}

}  // namespace detail

template <typename... Args>
inline std::string format_exception(std::format_string<Args...> fmt, Args&&... args) {
    auto trace = cpptrace::from_current_exception();

    return std::format("{}\nStack trace:\n{}",
                       std::format(fmt, std::forward<Args>(args)...),
                       trace.empty() ? "<no stack trace available>" : trace.to_string());
}

template <typename F, typename... Catches>
auto co_try_catch(F&& f, Catches&&... catches) {
    using Task = std::remove_cvref_t<std::invoke_result_t<std::decay_t<F>&>>;
    static_assert(detail::is_kota_task_v<Task>, "co_try_catch body must return kota::task");
    return detail::co_try_catch_impl<Task>(
        std::decay_t<F>(std::forward<F>(f)),
        std::decay_t<Catches>(std::forward<Catches>(catches))...);
}

}  // namespace catter::util
