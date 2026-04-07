#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

#include <eventide/async/async.h>

namespace catter::util {

class PipeProxy {
public:
    constexpr static size_t output_limit = 64 * 1024;
    constexpr static std::string_view truncation_marker = "[... truncated leading output ...]\n";

    PipeProxy(eventide::pipe&& pipe, FILE* sink, std::string_view name) :
        pipe(std::move(pipe)), sink(sink), name(name) {
        output_buffer.reserve(1024);
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

    static void append_bounded_output(std::string& buffer,
                                      std::string_view chunk,
                                      bool& truncated,
                                      size_t limit = output_limit);

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
