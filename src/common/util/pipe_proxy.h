#pragma once

#include <cstdio>
#include <string>
#include <string_view>

#include <eventide/async/async.h>

namespace catter::util {

class PipeProxy {
public:
    PipeProxy(eventide::pipe&& pipe, FILE* sink, std::string_view name) :
        pipe(std::move(pipe)), sink(sink), name(name) {}

    PipeProxy(const PipeProxy&) = delete;
    PipeProxy& operator= (const PipeProxy&) = delete;
    PipeProxy(PipeProxy&&) = delete;
    PipeProxy& operator= (PipeProxy&&) = delete;

    ~PipeProxy() {
        try {
            stop();
        } catch(...) {}
    }

    eventide::task<void> monitor();

    void stop() {
        if(stopped) {
            return;
        }

        stopped = true;
        pipe.stop();
    }

    const std::string& output() const noexcept {
        return output_buffer;
    }

private:
    eventide::pipe pipe{};
    FILE* sink = nullptr;
    std::string name{};
    std::string output_buffer{};
    bool stopped = false;
};

}  // namespace catter::util
