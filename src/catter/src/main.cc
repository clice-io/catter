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
    Listener(uv_pipe_t* server) : stream{server} {
        this->stream->data = this;

        uv_listen(uv::ptr_cast<uv_stream_t>(this->stream),
                  128,
                  [](uv_stream_t* server, int status) {
                      auto self = static_cast<Listener*>(server->data);
                      if(status < 0) {
                          std::println("Listen error: {}", uv_strerror(status));
                          return;
                      }
                      std::println("New client is connecting...");
                      self->semaphore_count++;

                      if(self->semaphore_count > 0) {
                          self->handle.resume();
                      }
                  });
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
                std::println("Listener is waiting for client...");
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
    uv_pipe_t* stream{nullptr};

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
    auto server = co_await uv::async::Create<uv_pipe_t>(uv::default_loop(), 1);
    uv_pipe_bind(server, PIPE_NAME);
    Listener listener{server};
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
    uv::run();
    return 0;
}
