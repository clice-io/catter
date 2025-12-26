#include <boost/ut.hpp>
#include <string_view>
#include <ranges>
#include "uv/uv.h"

using namespace boost;
using namespace catter;

ut::suite<"uv"> uv = [] {
    ut::test("default_loop") = [] {
        uv_loop_t* loop1 = uv::default_loop();
        uv_loop_t* loop2 = uv_default_loop();
        ut::expect(loop1 == loop2);
    };

    ut::test("run_loop") = [] {
        uv_loop_t* loop = uv::default_loop();
        int ret = uv::run_loop(loop, UV_RUN_NOWAIT);
        ut::expect(ret >= 0);
    };

    ut::test("run") = [] {
        int ret = uv::run(UV_RUN_NOWAIT);
        ut::expect(ret >= 0);
    };

    ut::test("is_base_of_v") = [] {
        ut::expect(uv::is_base_of_v<uv_stream_t, uv_tcp_t>);
        ut::expect(uv::is_base_of_v<uv_stream_t, uv_pipe_t>);
        ut::expect(uv::is_base_of_v<uv_handle_t, uv_stream_t>);

        ut::expect(!uv::is_base_of_v<uv_tcp_t, uv_stream_t>);
    };
};

ut::suite<"uv::async"> uv_async = [] {
    ut::test("Lazy<int>") = [] {
        uv::async::Lazy<int> task = []() -> uv::async::Lazy<int> {
            co_return 42;
        }();

        ut::expect(task.get() == 42);
    };

    ut::test("Lazy<void>") = [] {
        bool executed = false;

        uv::async::Lazy<void> task = [&]() -> uv::async::Lazy<void> {
            executed = true;
            co_return;
        }();

        ut::expect(executed);
        ut::expect(task.done());
    };

    ut::test("Lazy with exception") = [] {
        auto task = []() -> uv::async::Lazy<void> {
            throw std::runtime_error("Test exception");
            co_return;
        };

        ut::expect(ut::throws([&] { task().get(); }));

        auto task2 = [&]() -> uv::async::Lazy<void> {
            co_return co_await task();
        };

        ut::expect(ut::throws([&] { task2().get(); }));
    };

    ut::test("Lazy with suspend") = [] {
        uv::async::Lazy<void> task = []() -> uv::async::Lazy<void> {
            co_await std::suspend_always{};

            co_return;
        }();

        ut::expect(task.done() == false);
        task.resume();
        ut::expect(task.done() == true);
    };

    ut::test("Awaiting Lazy") = [] {
        auto simple_lazy = []() -> uv::async::Lazy<int> {
            co_return 42;
        }();

        auto await_lazy = [&]() -> uv::async::Lazy<int> {
            co_await std::suspend_always{};
            co_return 42;
        }();

        auto task = [&]() -> uv::async::Lazy<int> {
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

    ut::test("raii of uv_handle_t") = [] {
        uv::async::Lazy<void> task = []() -> uv::async::Lazy<void> {
            uv_pipe_t* pipe = co_await uv::async::Create<uv_pipe_t>(uv::default_loop(), 0);
            ut::expect(pipe != nullptr);
            co_return;
        }();

        ut::expect(!task.done());
        uv::wait(task);
    };

    ut::test("read and write") = [] {
        auto test_str = std::string_view{"Hello, uv async read and write!"} |
                        std::ranges::to<std::vector<char>>();

        auto accept_task = [&](uv_stream_t* server) -> uv::async::Lazy<void> {
            auto conn = co_await uv::async::Create<uv_tcp_t>(uv::default_loop());
            ut::expect(uv_accept(server, uv::cast<uv_stream_t>(conn)) == 0);

            ut::expect(co_await uv::async::write(uv::cast<uv_stream_t>(conn), test_str) == 0);

            std::vector<char> read_buf(test_str.size());
            auto nread = co_await uv::async::read(uv::cast<uv_stream_t>(conn),
                                                  read_buf.data(),
                                                  read_buf.size());
            ut::expect(nread == static_cast<int>(test_str.size()));
            ut::expect(read_buf == test_str);
        };

        auto task = [&]() -> uv::async::Lazy<void> {
            auto server = co_await uv::async::Create<uv_tcp_t>(uv::default_loop());

            sockaddr_in server_addr;
            ut::expect(uv_ip4_addr("127.0.0.1", 11451, &server_addr) == 0);
            ut::expect(uv_tcp_bind(server, reinterpret_cast<const sockaddr*>(&server_addr), 0) ==
                       0);

            uv::async::Lazy<void> acceptor;

            auto lscb = [&](uv_stream_t* server_stream, int status) {
                ut::expect(status == 0);
                acceptor = accept_task(server_stream);
            };

            ut::expect(uv::listen(uv::cast<uv_stream_t>(server), 128, lscb) == 0);
            auto client = co_await uv::async::Create<uv_tcp_t>(uv::default_loop());

            ut::expect(co_await uv::async::awaiter::TCPConnect(
                           client,
                           reinterpret_cast<const sockaddr*>(&server_addr)) == 0);

            std::vector<char> read_buf(test_str.size());
            auto nread = co_await uv::async::read(uv::cast<uv_stream_t>(client),
                                                  read_buf.data(),
                                                  read_buf.size());
            ut::expect(nread == static_cast<int>(test_str.size()));
            ut::expect(read_buf == test_str);

            ut::expect(co_await uv::async::write(uv::cast<uv_stream_t>(client), test_str) == 0);

            auto eof = co_await uv::async::read(uv::cast<uv_stream_t>(client),
                                                read_buf.data(),
                                                read_buf.size());
            ut::expect(eof == UV_EOF);

            co_return;
        };

        ut::expect(ut::nothrow([&] { uv::wait(task()); }));
    };

    ut::test("spawn process") = [] {
        auto task = []() -> uv::async::Lazy<void> {
#ifdef CATTER_WINDOWS
            int64_t exit_status = co_await uv::async::spawn("cmd.exe", {"/C", "exit", "123"}, true);
#else
            int64_t exit_status = co_await uv::async::spawn("/bin/sh", {"-c", "exit 123"}, true);
#endif
            ut::expect(exit_status == 123);
            co_return;
        }();

        ut::expect(ut::nothrow([&] { uv::wait(task); }));
    };
};
