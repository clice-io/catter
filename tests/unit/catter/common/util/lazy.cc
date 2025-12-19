

#include <boost/ut.hpp>

#include "util/lazy.h"

using namespace boost;

ut::suite<"util::lazy"> util_lazy = [] {
    using namespace catter;

    ut::test("Lazy<int>") = [] {
        coro::Lazy<int> lazy_value = []() -> coro::Lazy<int> {
            co_return 42;
        }();

        ut::expect(lazy_value.get() == 42);
    };

    ut::test("Lazy<void>") = [] {
        bool executed = false;

        coro::Lazy<void> lazy_value = [&]() -> coro::Lazy<void> {
            executed = true;
            co_return;
        }();

        ut::expect(executed);
        lazy_value.get();
    };

    ut::test("Lazy with exception") = [] {
        coro::Lazy<int> lazy_value = []() -> coro::Lazy<int> {
            throw std::runtime_error("Test exception");
            co_return 0;
        }();

        ut::expect(ut::throws([&] { lazy_value.get(); }));
    };

    ut::test("Lazy with suspend") = [] {
        coro::Lazy<void> lazy_value = []() -> coro::Lazy<void> {
            co_await std::suspend_always{};

            co_return;
        }();
#if __has_include(<unistd.h>) and __has_include(<sys/wait.h>)
        ut::expect(ut::aborts([&] { lazy_value.check_done(); }));
#else
        ut::expect(lazy_value.done() == false);
#endif
        lazy_value.resume();
        ut::expect(lazy_value.done() == true);
    };

    ut::test("Awaiting Lazy") = [] {
        auto simple_lazy = []() -> coro::Lazy<int> {
            co_return 42;
        }();

        auto await_lazy = [&]() -> coro::Lazy<int> {
            co_await std::suspend_always{};
            co_return 42;
        }();

        auto task = [&]() -> coro::Lazy<int> {
            int value = co_await simple_lazy;
            ut::expect(value == 42);
            value = co_await await_lazy;
            ut::expect(value == 42);
            co_return value;
        }();
        ut::expect(simple_lazy.done());
        ut::expect(!await_lazy.done());

        await_lazy.resume();
        ut::expect(await_lazy.done());
        ut::expect(task.get() == 42);
    };
};
