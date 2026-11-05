#include "video/video_pipeline.hpp"
#include "protocol/video_packet.hpp"

#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace mirror::video {

namespace {

bool read_exact(int fd, void* dst, size_t len, const std::stop_token& stop_token, const std::atomic<bool>& is_running) {
    uint8_t* ptr = static_cast<uint8_t*>(dst);
    size_t total = 0;
    while (total < len) {
        if (stop_token.stop_requested() || !is_running.load(std::memory_order_relaxed)) {
            return false;
        }
        ssize_t n = ::recv(fd, ptr + total, len - total, 0);
        if (n > 0) {
            total += static_cast<size_t>(n);
        } else if (n == 0) {
            return false; // Socket closed
        } else {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            return false; // Error or socket shutdown
        }
    }
    return true;
}

} // namespace

VideoPipeline::VideoPipeline(int socket_fd, int width, int height)
    : socket_fd_(socket_fd),
      width_(width),
      height_(height),
      frame_queue_(),
      decoder_(frame_queue_) {
}

VideoPipeline::~VideoPipeline() {
    stop();
}

bool VideoPipeline::start() {
    if (is_running_.load(std::memory_order_acquire)) {
        return true;
    }

    if (!decoder_.init(AV_CODEC_ID_H264, width_, height_)) {
        return false;
    }

    parser_ = av_parser_init(AV_CODEC_ID_H264);
    if (!parser_) {
        return false;
    }
    parser_->flags |= PARSER_FLAG_COMPLETE_FRAMES;

    is_running_.store(true, std::memory_order_release);
    worker_thread_ = std::jthread([this](std::stop_token st) {
        run_receiver(st);
    });

    return true;
}

void VideoPipeline::stop() {
    if (!is_running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.request_stop();
    }

    if (socket_fd_ >= 0) {
        ::shutdown(socket_fd_, SHUT_RDWR);
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    if (parser_) {
        av_parser_close(parser_);
        parser_ = nullptr;
    }
}

void VideoPipeline::run_receiver(std::stop_token stop_token) {
    constexpr size_t kInitialBufferSize = 256 * 1024;
    std::vector<uint8_t> buffer(kInitialBufferSize);

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        return;
    }

    protocol::VideoPacketHeader net_hdr{};

    while (is_running_.load(std::memory_order_relaxed) && !stop_token.stop_requested()) {
        // 1. Read exact 12-byte VideoPacketHeader
        if (!read_exact(socket_fd_, &net_hdr, sizeof(net_hdr), stop_token, is_running_)) {
            break;
        }

        // 2. Convert from network byte order
        auto host_hdr = protocol::from_network_order(net_hdr);
        uint64_t pts = host_hdr.pts_us;
        uint32_t payload_len = host_hdr.payload_len;

        if (payload_len == 0) {
            continue;
        }

        // 3. Ensure buffer capacity without re-allocation during steady state
        if (buffer.size() < payload_len) {
            buffer.resize(payload_len);
        }

        // Read payload bytes
        if (!read_exact(socket_fd_, buffer.data(), payload_len, stop_token, is_running_)) {
            break;
        }

        // 4. Parse stream in chunks via av_parser_parse2 and dispatch to decoder
        const uint8_t* in_data = buffer.data();
        int in_len = static_cast<int>(payload_len);

        while (in_len > 0 && is_running_.load(std::memory_order_relaxed) && !stop_token.stop_requested()) {
            uint8_t* out_data = nullptr;
            int out_len = 0;

            int bytes_used = av_parser_parse2(
                parser_,
                decoder_.get_context(),
                &out_data,
                &out_len,
                in_data,
                in_len,
                static_cast<int64_t>(pts),
                AV_NOPTS_VALUE,
                0
            );

            if (bytes_used <= 0) {
                break;
            }

            in_data += bytes_used;
            in_len -= bytes_used;

            if (out_len > 0) {
                packet->data = out_data;
                packet->size = out_len;
                packet->pts = parser_->pts;
                packet->dts = parser_->dts;

                decoder_.decode_packet(packet);
                av_packet_unref(packet);
            }
        }
    }

    av_packet_free(&packet);
}

} // namespace mirror::video
