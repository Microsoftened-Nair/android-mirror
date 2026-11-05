#include "video/video_decoder.hpp"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/pixfmt.h>
}

namespace mirror::video {

VideoDecoder::VideoDecoder(AtomicFrameQueue& frame_queue)
    : frame_queue_(frame_queue) {
}

VideoDecoder::~VideoDecoder() {
    if (decode_frame_) {
        av_frame_free(&decode_frame_);
    }
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
    }
}

bool VideoDecoder::init(AVCodecID codec_id, int width, int height) {
    // 1. Find decoder
    codec_ = avcodec_find_decoder(codec_id);
    if (!codec_) {
        return false;
    }

    // 2. Allocate context
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        return false;
    }

    // 3. Configure target pixel format and dimensions (if known)
    if (width > 0 && height > 0) {
        codec_ctx_->width = width;
        codec_ctx_->height = height;
    }
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    // 4. Low-delay options
    codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codec_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;

    // 5. Disable multi-threaded frame queue latency
    codec_ctx_->thread_count = 1;
    codec_ctx_->thread_type = 0;

    // 6. Disable B-frames and codec delay
    codec_ctx_->max_b_frames = 0;
    codec_ctx_->delay = 0;

    // 7. Configure low-latency dictionary parameters
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "tune", "zerolatency", 0);
    av_dict_set(&opts, "flags", "low_delay", 0);

    // 8. Open codec
    int ret = avcodec_open2(codec_ctx_, codec_, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        avcodec_free_context(&codec_ctx_);
        return false;
    }

    // 9. Allocate intermediate decode frame
    decode_frame_ = av_frame_alloc();
    if (!decode_frame_) {
        avcodec_free_context(&codec_ctx_);
        return false;
    }

    return true;
}

bool VideoDecoder::decode_packet(const AVPacket* packet) {
    if (!codec_ctx_ || !decode_frame_) {
        return false;
    }

    // 1. Send packet to decoder
    int ret = avcodec_send_packet(codec_ctx_, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return false;
    }

    // 2. Non-blocking drain loop
    while (ret >= 0) {
        int recv_ret = avcodec_receive_frame(codec_ctx_, decode_frame_);
        if (recv_ret == AVERROR(EAGAIN) || recv_ret == AVERROR_EOF) {
            break;
        } else if (recv_ret < 0) {
            return false;
        }

        // Push frame to atomic triple-buffer queue
        frame_queue_.push_latest(decode_frame_);

        // Reset frame state for the next decode iteration
        av_frame_unref(decode_frame_);
    }

    return true;
}

void VideoDecoder::flush() {
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
    }
}

} // namespace mirror::video
