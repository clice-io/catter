#include <vector>
#include <string>
#include <print>

#include "librpc/uv.h"
#include "uv.h"

#ifdef _WIN32
constexpr char PIPE_NAME[] = R"(\\.\pipe\114514)";
#endif

int main(int argc, char* argv[], char* envp[]) {

    auto loop = catter::rpc::uv::default_loop();

    uv_pipe_t server{};

    uv_pipe_init(loop, &server, 0);

    uv_pipe_bind(&server, PIPE_NAME);

    uv_listen(catter::rpc::uv::ptr_cast<uv_stream_t>(&server),
              128,
              [](uv_stream_t* server, int status) {
                  if(status < 0) {
                      std::println("listen error: {}", uv_strerror(status));
                      return;
                  }

                  uv_pipe_t* client = new uv_pipe_t{};
                  uv_pipe_init(server->loop, client, 0);
                  if(uv_accept(server, catter::rpc::uv::ptr_cast<uv_stream_t>(client)) == 0) {
                      std::println("New client connected.");
                      // handle client

                  } else {
                      uv_close(catter::rpc::uv::ptr_cast<uv_handle_t>(client),
                               [](uv_handle_t* handle) { delete (uv_pipe_t*)handle; });
                  }
              });

    std::string exe = "catter-proxy";
    std::vector<std::string> args = {"-p", "12345", "--", "/bin/ls", "-l", "/"};

    auto exit_cb = [&](int64_t exit_status, int term_signal) {
        uv_close(catter::rpc::uv::ptr_cast<uv_handle_t>(&server), [](uv_handle_t* handle) {

        });
    };
    catter::rpc::uv::spawn_process(loop, exit_cb, exe, args);

    catter::rpc::uv::run(UV_RUN_DEFAULT);
    return 0;
}
