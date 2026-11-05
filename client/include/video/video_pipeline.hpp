#pragma once

#include "protocol/video_packet.hpp"
#include "video/atomic_frame_queue.hpp"
#include "video/video_decoder.hpp"

#include <atomic>
#include <cstdint>
#include <stop_token>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace mirror::video {

/**
 * Manages the video ingestion worker thread, Annex B NAL unit stream reassembly via av_parser_parse2,
 * decoding via VideoDecoder, and presentation frame queuing.
 */
class VideoPipeline {
public:
    explicit VideoPipeline(int socket_fd, int width = 0, int height = 0);
    ~VideoPipeline();

    VideoPipeline(const VideoPipeline&) = delete;
    VideoPipeline& operator=(const VideoPipeline&) = delete;
    VideoPipeline(VideoPipeline&&) = delete;
    VideoPipeline& operator=(VideoPipeline&&) = delete;

    /**
     * Initializes codec parser and decoder, then launches the background receiver worker thread.
     */
    bool start();

    /**
     * Halts receiver thread, interrupts blocking reads, and closes parser resources cleanly.
     */
    void stop();

    [[nodiscard]] bool is_running() const noexcept {
        return is_running_.load(std::memory_order_acquire);
    }

    /**
     * Consumes the latest decoded frame from the atomic triple-buffer queue into dst_frame.
     */
    bool consume_rendered_frame(AVFrame* dst_frame) {
        return frame_queue_.consume(dst_frame);
    }

    [[nodiscard]] VideoDecoder& get_decoder() noexcept { return decoder_; }
    [[nodiscard]] const VideoDecoder& get_decoder() const noexcept { return decoder_; }
    [[nodiscard]] AtomicFrameQueue& get_frame_queue() noexcept { return frame_queue_; }

private:
    void run_receiver(std::stop_token stop_token);

    int socket_fd_{-1};
    int width_{0};
    int height_{0};

    std::atomic<bool> is_running_{false};
    AtomicFrameQueue frame_queue_;
    VideoDecoder decoder_;
    AVCodecParserContext* parser_{nullptr};
    std::jthread worker_thread_;
};

} // namespace mirror::video

namespace mirror {
using mirror::video::VideoPipeline;
}

using mirror::video::VideoPipeline;
