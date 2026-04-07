#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

#include <eventide/async/async.h>

namespace catter::util {

constexpr inline size_t PIPE_PROXY_OUTPUT_LIMIT = 64 * 1024;
constexpr inline std::string_view PIPE_PROXY_TRUNCATION_MARKER =
    "[... truncated leading output ...]\n";

void append_bounded_output(std::string& buffer,
                           std::string_view chunk,
                           bool& truncated,
                           size_t limit = PIPE_PROXY_OUTPUT_LIMIT);

class PipeProxy {
public:
    PipeProxy(eventide::pipe&& pipe, FILE* sink, std::string_view name) :
        pipe(std::move(pipe)), sink(sink), name(name) {
        output_buffer.reserve(PIPE_PROXY_OUTPUT_LIMIT);
    }

    PipeProxy(const PipeProxy&) = delete;
    PipeProxy& operator= (const PipeProxy&) = delete;
    PipeProxy(PipeProxy&&) = delete;
    PipeProxy& operator= (PipeProxy&&) = delete;

    ~PipeProxy() {
        if(pipe.handle()) {
            pipe.stop();
        }
    }

    eventide::task<void> monitor();

    const std::string& output() const noexcept {
        return output_buffer;
    }

private:
    eventide::pipe pipe{};
    FILE* sink = nullptr;
    std::string name{};
    std::string output_buffer{};
    bool output_truncated = false;
};

}  // namespace catter::util
