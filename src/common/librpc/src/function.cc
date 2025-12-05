#include <cstdint>
#include <cstdlib>
#include <print>

#include <type_traits>
#include <uv.h>
#include <vector>

#include "librpc/function.h"
#include "librpc/data.h"
#include "librpc/uv.h"

// TODO
namespace catter::rpc::server {

template <typename Ivokable>
    requires std::invocable<Ivokable, int64_t, int>
int spawn_process(uv_loop_t* loop, data::command& cmd, Ivokable&& exit_cb) {
    std::vector<char*> args;
    args.push_back(cmd.executable.data());
    for(auto& arg: cmd.args) {
        args.push_back(arg.data());
    }
    args.push_back(nullptr);

    struct Data {
        uv_process_t process;
        uv_process_options_t options;
        uv_stdio_container_t child_stdio[3];
        Ivokable exit_cb;
    };

    auto data = new Data{
        .process = uv_process_t{},
        .options = uv_process_options_t{},
        .child_stdio =
            {
                                {.flags = UV_IGNORE},
                                {.flags = UV_INHERIT_FD, .data = {.fd = 1}},
                                {.flags = UV_INHERIT_FD, .data = {.fd = 2}},
                                },
        .exit_cb = std::forward<Ivokable>(exit_cb)
    };

    data->process.data = data;

    data->options.exit_cb = [](uv_process_t* process, int64_t exit_status, int term_signal) {
        auto d = static_cast<Data*>(process->data);
        d->exit_cb(exit_status, term_signal);
        delete d;
    };

    data->options.file = cmd.executable.c_str();
    data->options.args = args.data();
    data->options.flags = UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS | UV_PROCESS_WINDOWS_HIDE;
    data->options.stdio_count = 3;
    data->options.stdio = data->child_stdio;
    return uv_spawn(loop, &data->process, &data->options);
}

#ifdef _WIN32
constexpr char PIPE_NAME[] = R"(\\.\pipe\114514)";
#endif

uv_pipe_t& client() {
    static uv_pipe_t instance = [] {
        uv_pipe_t pipe;
        uv_pipe_init(uv::default_loop(), &pipe, 0);
        return pipe;
    }();
    return instance;
}

int connect(uv_pipe_t* client, const char* name) {
    struct Data {
        bool connected;
        int status;
    };

    Data data{
        .connected = false,
        .status = 0,
    };

    uv_connect_t connect{};
    connect.data = &data;

    uv_pipe_connect(&connect, client, name, [](uv_connect_t* connect, int status) {
        auto data = static_cast<Data*>(connect->data);
        data->connected = true;
        data->status = status;
    });

    while(!data.connected) {
        uv_run(client->loop, UV_RUN_ONCE);
    }
    return data.status;
}

void write(std::vector<char>& payload) {
    static bool is_connected = false;

    if(!is_connected) {
        int status = connect(&client(), PIPE_NAME);
        if(status < 0) {
            throw std::runtime_error(
                std::format("Failed to connect to RPC server pipe: {}", uv_strerror(status)));
        }
        is_connected = true;
    }
    uv::sync::write(uv::ptr_cast<uv_stream_t>(&client()), payload);
}

void read(char* dst, size_t len) {
    uv::sync::read(uv::ptr_cast<uv_stream_t>(&client()), dst, len);
}

data::decision_info make_decision(data::command_id_t parent_id, data::command cmd) {
    std::vector<char> data;
    data.append_range(data::Serde<data::Request>::serialize(data::Request::MAKE_DECISION));
    data.append_range(data::Serde<data::command>::serialize(cmd));
    data.append_range(data::Serde<data::command_id_t>::serialize(parent_id));
    write(data);
    return data::Serde<data::decision_info>::deserialize(read);
}

void report_error(data::command_id_t parent_id, std::string error_msg) noexcept {
    try {
        std::vector<char> data;
        data.append_range(data::Serde<data::Request>::serialize(data::Request::REPORT_ERROR));
        data.append_range(data::Serde<data::command_id_t>::serialize(parent_id));
        data.append_range(data::Serde<std::string>::serialize(error_msg));
        write(data);
    } catch(...) {
        // cannot do anything here
    }
    return;
};

void finish(int ret_code) {
    std::vector<char> data;
    data.append_range(data::Serde<data::Request>::serialize(data::Request::FINISH));
    data.append_range(data::Serde<int>::serialize(ret_code));
    write(data);
    return;
}
}  // namespace catter::rpc::server
