#include "librpc/uv.h"

namespace catter::rpc::uv {

namespace sync {

void write(uv_stream_t* stream, std::vector<char>& payload) {
    struct Data {
        bool written;
        int status;
    };

    Data data{
        .written = false,
        .status = 0,
    };

    uv_write_t write_req{};
    write_req.data = &data;

    auto buf = uv_buf_init(payload.data(), static_cast<unsigned int>(payload.size()));

    uv_write(&write_req, stream, &buf, 1, [](uv_write_t* write_req, int status) {
        auto data = static_cast<Data*>(write_req->data);
        data->written = true;
        data->status = status;
    });

    while(!data.written) {
        uv::run(UV_RUN_ONCE);
    }

    if(data.status < 0) {
        throw std::runtime_error(
            std::format("Failed to write to RPC server pipe: {}", uv_strerror(data.status)));
    }
}

void read(uv_stream_t* stream, char* dst, size_t len) {
    char* buf_ptr = dst;
    size_t remaining = len;
    int status = 0;
    bool finished = false;

    ReadContext ctx{.alloc_cb =
                        [&](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
                            buf->base = buf_ptr;
                            buf->len = static_cast<unsigned int>(remaining);
                        },
                    .read_cb =
                        [&](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
                            if(nread >= 0) {
                                buf_ptr += nread;
                                remaining -= nread;
                                if(remaining == 0) {
                                    uv_read_stop(stream);
                                    finished = true;
                                    status = 0;
                                }
                            } else {
                                uv_read_stop(stream);
                                finished = true;
                                status = static_cast<int>(nread);
                            }
                        }};

    if(auto ret = read_start(stream, &ctx); ret < 0) {
        throw std::runtime_error(std::format("Failed to start read: {}", uv_strerror(ret)));
    }

    while(!finished) {
        uv::run(UV_RUN_ONCE);
    }

    if(status < 0) {
        throw std::runtime_error(std::format("Read error: {}", uv_strerror(status)));
    }
}

int spawn_process(uv_loop_t* loop, std::string& executable, std::vector<std::string>& args) {
    int ret = 0;
    bool finished = false;
    catter::rpc::uv::spawn_process(
        rpc::uv::default_loop(),
        [&](int64_t exit_status, int term_signal) {
            finished = true;
            ret = static_cast<int>(exit_status);
        },
        executable,
        args);
    while(!finished) {
        uv::run(UV_RUN_ONCE);
    }
    return ret;
}
}  // namespace sync

}  // namespace catter::rpc::uv
