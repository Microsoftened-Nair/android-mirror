#pragma once

#include "video/atomic_frame_queue.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace mirror::video {

/**
 * Zero-latency AVCodecContext wrapper configured for real-time H.264 decoding,
 * disabling frame reordering, buffer caching, and multi-threaded frame queue latency.
 */
class VideoDecoder {
public:
    explicit VideoDecoder(AtomicFrameQueue& frame_queue);
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    VideoDecoder(VideoDecoder&&) = delete;
    VideoDecoder& operator=(VideoDecoder&&) = delete;

    /**
     * Initializes the FFmpeg decoder context with ultra-low latency flags and YUV420P format.
     */
    bool init(AVCodecID codec_id = AV_CODEC_ID_H264, int width = 0, int height = 0);

    /**
     * Sends packet to the decoder and drains all produced frames directly into AtomicFrameQueue.
     * Returns true on success (or EOF/EAGAIN), false on unrecoverable decode error.
     */
    bool decode_packet(const AVPacket* packet);

    /**
     * Flushes internal decoder buffers (e.g. on stream reset or seek).
     */
    void flush();

    [[nodiscard]] AVCodecContext* get_context() const noexcept { return codec_ctx_; }
    [[nodiscard]] const AVCodec* get_codec() const noexcept { return codec_; }

private:
    AVCodecContext* codec_ctx_{nullptr};
    const AVCodec* codec_{nullptr};
    AVFrame* decode_frame_{nullptr};
    AtomicFrameQueue& frame_queue_;
};

} // namespace mirror::video

namespace mirror {
using mirror::video::VideoDecoder;
}

using mirror::video::VideoDecoder;
