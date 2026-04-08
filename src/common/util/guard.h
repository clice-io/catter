#pragma once
#include <concepts>
#include <utility>

namespace catter::util {

template <typename Invokable>
class Guard {
public:
    explicit Guard(Invokable invokable) : invokable(std::move(invokable)) {}

    Guard(const Guard&) = delete;
    Guard(Guard&&) = delete;
    Guard& operator= (const Guard&) = delete;
    Guard& operator= (Guard&&) = delete;

    ~Guard() {
        invokable();
    }

private:
    Invokable invokable;
};

template <typename Invokable, typename R = std::remove_reference_t<Invokable>>
    requires std::is_invocable_v<Invokable> && std::is_nothrow_invocable_v<Invokable>
Guard<R> make_guard(Invokable&& invokable) {
    return Guard<R>(std::forward<Invokable>(invokable));
}}