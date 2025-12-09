#include <cstddef>
#include <vector>
#include <string>
#include <print>

#include "libutil/uv.h"
#include "uv.h"

#ifdef _WIN32
constexpr char PIPE_NAME[] = R"(\\.\pipe\114514)";
#endif

using namespace catter;

[[nodiscard]]
uv::async::LazyTask<void> foo() {
    auto pipe = co_await uv::async::Create<uv_pipe_t>();
    std::println("Created pipe: {}", reinterpret_cast<uintptr_t>(pipe));

    uv_pipe_init(uv::default_loop(), pipe, 0);
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
    auto task = foo();

    uv::run(UV_RUN_DEFAULT);
    std::println("Exiting...");
    return 0;
}
