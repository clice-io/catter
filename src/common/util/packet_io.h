#pragma once
#include <cstddef>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <eventide/async/async.h>

#include "util/data.h"
#include "util/log.h"
#include "util/serde.h"

namespace catter {

class PacketChannel {
public:
    PacketChannel() = default;

    explicit PacketChannel(eventide::pipe&& pipe) : pipe(std::move(pipe)) {}

    PacketChannel(const PacketChannel&) = delete;
    PacketChannel& operator= (const PacketChannel&) = delete;
    PacketChannel(PacketChannel&&) = default;
    PacketChannel& operator= (PacketChannel&&) = default;

    eventide::task<std::optional<data::packet>> read_packet() {
        size_t packet_size = 0;
        if(!(co_await read_exact(reinterpret_cast<char*>(&packet_size),
                                 sizeof(packet_size),
                                 true))) {
            co_return std::nullopt;
        }

        data::packet payload(packet_size);
        if(packet_size > 0) {
            co_await read_exact(payload.data(), packet_size);
        }

        co_return payload;
    }

    eventide::task<eventide::error> write_packet(const std::vector<char>& payload) {
        auto serialized = Serde<data::packet>::serialize(payload);
        LOG_DEBUG("Writing {} bytes: {}", serialized.size(), log::to_hex(serialized));
        co_return co_await this->pipe.write(serialized);
    }

private:
    eventide::task<bool> read_exact(char* dst, size_t len, bool allow_eof = false) {
        size_t total_read = 0;
        while(total_read < len) {
            auto ret =
                co_await this->pipe.read_some(std::span<char>{dst + total_read, len - total_read});
            if(!ret) {
                throw std::runtime_error(std::format("ipc read failed: {}", ret.error().message()));
            }
            if(ret.value() == 0) {
                if(allow_eof && total_read == 0) {
                    co_return false;
                }
                throw std::runtime_error(
                    std::format("ipc read failed: unexpected EOF ({} of {} bytes read)",
                                total_read,
                                len));
            }
            total_read += ret.value();
        }
        LOG_DEBUG("Reading {} bytes: {}", len, log::to_hex(std::span<char>(dst, len)));
        co_return true;
    }

    eventide::pipe pipe{};
};

}  // namespace catter
