

#include <boost/ut.hpp>

#include "util/lazy.h"

using namespace boost;

ut::suite<"util::lazy"> util_lazy = [] {
    using namespace catter;

    ut::test("Lazy<int>") = [] {
        coro::Lazy<int> task = []() -> coro::Lazy<int> {
            co_return 42;
        }();

        ut::expect(task.get() == 42);
    };

    ut::test("Lazy<void>") = [] {
        bool executed = false;

        coro::Lazy<void> task = [&]() -> coro::Lazy<void> {
            executed = true;
            co_return;
        }();

        ut::expect(executed);
        ut::expect(task.done());
    };

    ut::test("Lazy with exception") = [] {
        auto task = []() -> coro::Lazy<void> {
            throw std::runtime_error("Test exception");
            co_return;
        };

        ut::expect(ut::throws([&] { task().get(); }));

        auto task2 = [&]() -> coro::Lazy<void> {
            co_return co_await task();
        };

        ut::expect(ut::throws([&] { task2().get(); }));
    };

    ut::test("Lazy with suspend") = [] {
        coro::Lazy<void> task = []() -> coro::Lazy<void> {
            co_await std::suspend_always{};

            co_return;
        }();

        ut::expect(task.done() == false);
        task.resume();
        ut::expect(task.done() == true);
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
