#include <coroutine>
#include <cstddef>
#include <vector>
#include <string>
#include <print>

#include "libutil/uv.h"

#ifdef _WIN32
constexpr char PIPE_NAME[] = R"(\\.\pipe\114514)";
#else
constexpr char PIPE_NAME[] = "/tmp/114514.sock";
#endif

using namespace catter;

class Listener {
public:
    Listener(uv_stream_t* server) : stream{server} {}

    int start_listen(int backlog = 128) {
        this->stream->data = this;
        return uv_listen(this->stream, backlog, [](uv_stream_t* server, int status) {
            static_cast<Listener*>(server->data)->connection_cb(server, status);
        });
    }

    void connection_cb(uv_stream_t* req, int status) {
        if(status < 0) {
            std::println("Listen error: {}", uv_strerror(status));
            return;
        }
        std::println("New client is connecting...");
        this->semaphore_count++;

        if(this->semaphore_count > 0) {
            this->handle.resume();
        }
    }

    struct awaiter {
        awaiter(Listener* listener) : listener{listener}, base{listener->stream->loop} {}

        bool await_ready() noexcept {
            return false;
        }

        uv_pipe_t* await_resume() noexcept {
            auto ret = base.await_resume();
            uv_accept(uv::ptr_cast<uv_stream_t>(listener->stream), uv::ptr_cast<uv_stream_t>(ret));
            listener->semaphore_count--;
            return ret;
        }

        template <typename Promise>
        bool await_suspend(std::coroutine_handle<Promise> h) noexcept {
            base.await_suspend(h);
            if(listener->semaphore_count == 0) {
                listener->handle = h;
                return true;
            } else {
                return false;
            }
        }

        Listener* listener;
        uv::async::Create<uv_pipe_t> base;
    };

    awaiter accept() {
        return awaiter{this};
    }

private:
    uv_stream_t* stream{nullptr};

    std::coroutine_handle<> handle{};
    size_t semaphore_count{0};
};

uv::async::Lazy<void> accept(Listener& listener) {
    co_await listener.accept();
    std::println("Client connected.");
    co_return;
}

[[nodiscard]]
uv::async::Lazy<void> loop() {
    auto server = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());

    uv_pipe_bind(server, PIPE_NAME);

    Listener listener{uv::ptr_cast<uv_stream_t>(server)};
    int ret = listener.start_listen();
    if(ret < 0) {
        std::println("Failed to start listening: {}", uv_strerror(ret));
        co_return;
    }
    co_await accept(listener);
}

[[nodiscard]]
uv::async::Lazy<void> foo() {
    auto pipe = co_await uv::async::Create<uv_pipe_t>(uv::default_loop());

    auto status = co_await uv::async::awaiter::PipeConnect(pipe, PIPE_NAME);
    if(status != 0) {
        std::println("Connect failed: {}", uv_strerror(status));
        co_return;
    }

    std::println("Connected to pipe.");
    char buffer;
    while(true) {
        ssize_t nread =
            co_await uv::async::awaiter::Read(uv::ptr_cast<uv_stream_t>(pipe), &buffer, 1);
        if(nread < 0) {
            std::println("Read error: {}", uv_strerror(nread));
            break;
        } else {
            std::print("{}", buffer);
        }
    }
    co_return;
}

int main(int argc, char* argv[], char* envp[]) {
    auto loop_task = loop();
    auto foo_task = foo();
    uv::run();
    return 0;
}
