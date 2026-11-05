#include "video/video_demuxer.hpp"
#include "net/socket_client.hpp"

#include <utility>

namespace mirror::video {

VideoDemuxer::VideoDemuxer(int socket_fd, size_t max_buffer_size)
    : socket_fd_(socket_fd),
      max_buffer_size_(max_buffer_size),
      buffer_(max_buffer_size > 0 ? max_buffer_size : 4 * 1024 * 1024) {}

VideoDemuxer::VideoDemuxer(VideoDemuxer&& other) noexcept
    : socket_fd_(other.socket_fd_),
      max_buffer_size_(other.max_buffer_size_),
      buffer_(std::move(other.buffer_)) {
    other.socket_fd_ = -1;
    other.max_buffer_size_ = 0;
}

VideoDemuxer& VideoDemuxer::operator=(VideoDemuxer&& other) noexcept {
    if (this != &other) {
        socket_fd_ = other.socket_fd_;
        max_buffer_size_ = other.max_buffer_size_;
        buffer_ = std::move(other.buffer_);

        other.socket_fd_ = -1;
        other.max_buffer_size_ = 0;
    }
    return *this;
}

bool VideoDemuxer::read_next_packet(protocol::VideoPacketHeader& out_header,
                                    std::span<const uint8_t>& out_payload) {
    if (socket_fd_ < 0) {
        return false;
    }

    // Step 1: Read exactly 12 bytes into VideoPacketHeader
    protocol::VideoPacketHeader raw_header{};
    if (!net::SocketClient::read_exact(socket_fd_,
                                       reinterpret_cast<uint8_t*>(&raw_header),
                                       sizeof(raw_header))) {
        return false;
    }

    // Step 2: Convert pts_us and payload_len from Big-Endian network byte order
    out_header = protocol::from_network_order(raw_header);

    // Step 3: Guard against buffer overrun
    if (out_header.payload_len > max_buffer_size_) {
        return false;
    }

    // Step 4: Guard against malformed packet lacking Annex B start code
    if (out_header.payload_len < sizeof(protocol::ANNEX_B_START_CODE)) {
        return false;
    }

    // Step 5: Read exactly payload_len bytes into pre-allocated buffer
    if (!net::SocketClient::read_exact(socket_fd_, buffer_.data(), out_header.payload_len)) {
        return false;
    }

    // Step 6: Validate 4-byte Annex B start code delimiter (0x00, 0x00, 0x00, 0x01)
    if (buffer_[0] != protocol::ANNEX_B_START_CODE[0] ||
        buffer_[1] != protocol::ANNEX_B_START_CODE[1] ||
        buffer_[2] != protocol::ANNEX_B_START_CODE[2] ||
        buffer_[3] != protocol::ANNEX_B_START_CODE[3]) {
        return false; // Stream corruption
    }

    // Step 7: Construct span view without dynamic heap allocation
    out_payload = std::span<const uint8_t>(buffer_.data(), out_header.payload_len);
    return true;
}

} // namespace mirror::video
