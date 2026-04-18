#include "pipe_proxy.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>

namespace catter::util {

void PipeProxy::append_bounded_output(std::string& buffer,
                                      std::string_view chunk,
                                      bool& truncated,
                                      size_t limit) {
    if(chunk.empty()) {
        return;
    }

    if(!truncated && buffer.size() + chunk.size() <= limit) {
        buffer.append(chunk.data(), chunk.size());
        return;
    }

    truncated = true;

    if(limit <= truncation_marker.size()) {
        buffer.assign(truncation_marker.substr(0, limit));
        return;
    }

    const size_t payload_limit = limit - truncation_marker.size();
    std::string_view current_payload = buffer;
    if(current_payload.starts_with(truncation_marker)) {
        current_payload.remove_prefix(truncation_marker.size());
    }

    const size_t keep_from_chunk = std::min(payload_limit, chunk.size());
    const size_t keep_from_existing = payload_limit - keep_from_chunk;
    if(current_payload.size() > keep_from_existing) {
        current_payload.remove_prefix(current_payload.size() - keep_from_existing);
    }

    std::string next;
    next.reserve(limit);
    next.append(truncation_marker);
    next.append(current_payload);
    next.append(chunk.substr(chunk.size() - keep_from_chunk));
    buffer = std::move(next);
}

kota::task<void> PipeProxy::monitor() {
    while(true) {
        auto chunk = co_await pipe.read_chunk();
        if(!chunk) {
            if(chunk.error() == kota::error::end_of_file ||
               chunk.error() == kota::error::operation_aborted ||
               chunk.error() == kota::error::broken_pipe) {
                break;
            }

            throw std::runtime_error(
                std::format("{} pipe read failed: {}", name, chunk.error().message()));
        }

        if(chunk->empty()) {
            continue;
        }

        // Keep the tail because build failures usually surface the decisive diagnostics last,
        // while the full stream is still forwarded to the sink in real time.
        PipeProxy::append_bounded_output(output_buffer,
                                         std::string_view(chunk->data(), chunk->size()),
                                         output_truncated);

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
