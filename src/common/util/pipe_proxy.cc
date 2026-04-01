#include "pipe_proxy.h"

#include <cstdio>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>

namespace catter::util {

eventide::task<void> PipeProxy::monitor() {
    while(true) {
        auto chunk = co_await pipe.read_chunk();
        if(!chunk) {
            if(chunk.error() == eventide::error::end_of_file ||
               chunk.error() == eventide::error::operation_aborted ||
               chunk.error() == eventide::error::broken_pipe) {
                break;
            }

            throw std::runtime_error(
                std::format("{} pipe read failed: {}", name, chunk.error().message()));
        }

        if(chunk->empty()) {
            continue;
        }

        output_buffer.append(chunk->data(), chunk->size());

        if(sink != nullptr) {
            std::span<const char> bytes(chunk->data(), chunk->size());
            (void)std::fwrite(bytes.data(), 1, bytes.size(), sink);
            std::fflush(sink);
        }

        pipe.consume(chunk->size());
    }

    co_return;
}

}  // namespace catter::util
