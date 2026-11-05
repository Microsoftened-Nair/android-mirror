#pragma once

#include "protocol/video_packet.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mirror::video {

/**
 * Stream reader handling socket-level buffer slicing, timestamp tracking,
 * and Annex B payload extraction with zero runtime allocations during steady state.
 */
class VideoDemuxer {
public:
    explicit VideoDemuxer(int socket_fd, size_t max_buffer_size = 4 * 1024 * 1024);
    ~VideoDemuxer() = default;

    // Non-copyable, movable
    VideoDemuxer(const VideoDemuxer&) = delete;
    VideoDemuxer& operator=(const VideoDemuxer&) = delete;
    VideoDemuxer(VideoDemuxer&& other) noexcept;
    VideoDemuxer& operator=(VideoDemuxer&& other) noexcept;

    /**
     * Reads the next video packet from the socket.
     * 1. Reads 12-byte VideoPacketHeader.
     * 2. Converts pts_us and payload_len from Big-Endian.
     * 3. Validates payload_len <= max_buffer_size.
     * 4. Reads payload_len bytes into pre-allocated memory.
     * 5. Validates 4-byte Annex B prefix (0x00, 0x00, 0x00, 0x01).
     *
     * Returns true on success, false on stream corruption, buffer overrun, or read failure.
     */
    bool read_next_packet(protocol::VideoPacketHeader& out_header, std::span<const uint8_t>& out_payload);

    [[nodiscard]] int socket_fd() const noexcept { return socket_fd_; }
    [[nodiscard]] size_t max_buffer_size() const noexcept { return max_buffer_size_; }

private:
    int socket_fd_{-1};
    size_t max_buffer_size_{4 * 1024 * 1024};
    std::vector<uint8_t> buffer_;
};

} // namespace mirror::video

namespace mirror {
using mirror::video::VideoDemuxer;
}

using mirror::video::VideoDemuxer;
