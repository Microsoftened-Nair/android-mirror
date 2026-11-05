#include "video/video_decoder.hpp"
#include "video/atomic_frame_queue.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
}

// Helper to encode a synthetic H.264 keyframe
static AVPacket* encode_synthetic_idr_frame(int width, int height, int64_t pts) {
    const AVCodec* encoder = avcodec_find_encoder_by_name("libx264");
    if (!encoder) {
        encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    assert(encoder != nullptr);

    AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
    assert(enc_ctx != nullptr);
    enc_ctx->width = width;
    enc_ctx->height = height;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx->time_base = AVRational{1, 30};
    enc_ctx->framerate = AVRational{30, 1};

    av_opt_set(enc_ctx->priv_data, "tune", "zerolatency", 0);
    int ret = avcodec_open2(enc_ctx, encoder, nullptr);
    assert(ret == 0);

    AVFrame* src_frame = av_frame_alloc();
    assert(src_frame != nullptr);
    src_frame->width = width;
    src_frame->height = height;
    src_frame->format = AV_PIX_FMT_YUV420P;
    ret = av_frame_get_buffer(src_frame, 32);
    assert(ret == 0);

    // Fill Y, U, V planes with synthetic pattern
    for (int y = 0; y < height; ++y) {
        std::memset(src_frame->data[0] + y * src_frame->linesize[0], static_cast<int>(pts & 0xFF), width);
    }
    for (int y = 0; y < height / 2; ++y) {
        std::memset(src_frame->data[1] + y * src_frame->linesize[1], 128, width / 2);
        std::memset(src_frame->data[2] + y * src_frame->linesize[2], 128, width / 2);
    }
    src_frame->pts = pts;

    ret = avcodec_send_frame(enc_ctx, src_frame);
    assert(ret == 0);

    AVPacket* pkt = av_packet_alloc();
    assert(pkt != nullptr);
    ret = avcodec_receive_packet(enc_ctx, pkt);
    assert(ret == 0);

    av_frame_free(&src_frame);
    avcodec_free_context(&enc_ctx);
    return pkt;
}

static void test_decoder_initialization_parameters() {
    mirror::video::AtomicFrameQueue queue;
    mirror::video::VideoDecoder decoder(queue);

    bool ok = decoder.init(AV_CODEC_ID_H264, 640, 480);
    assert(ok);

    AVCodecContext* ctx = decoder.get_context();
    assert(ctx != nullptr);
    assert((ctx->flags & AV_CODEC_FLAG_LOW_DELAY) != 0);
    assert((ctx->flags2 & AV_CODEC_FLAG2_FAST) != 0);
    assert(ctx->thread_count == 1);
    assert(ctx->thread_type == 0);
    assert(ctx->max_b_frames == 0);
    assert(ctx->delay == 0);
    assert(ctx->pix_fmt == AV_PIX_FMT_YUV420P);

    std::cout << "[PASS] test_decoder_initialization_parameters" << std::endl;
}

static void test_synthetic_packet_decoding() {
    mirror::video::AtomicFrameQueue queue;
    mirror::video::VideoDecoder decoder(queue);

    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    constexpr int64_t kPts = 1337;

    bool ok = decoder.init(AV_CODEC_ID_H264, kWidth, kHeight);
    assert(ok);

    AVPacket* pkt = encode_synthetic_idr_frame(kWidth, kHeight, kPts);
    assert(pkt != nullptr);
    assert(pkt->size > 0);

    // Decode packet
    bool decode_ok = decoder.decode_packet(pkt);
    assert(decode_ok);

    // Queue should now hold the decoded frame
    assert(queue.has_new_frame());

    AVFrame* dst_frame = av_frame_alloc();
    assert(dst_frame != nullptr);
    bool consumed = queue.consume(dst_frame);
    assert(consumed);

    assert(dst_frame->width == kWidth);
    assert(dst_frame->height == kHeight);
    assert(dst_frame->format == AV_PIX_FMT_YUV420P);
    assert(dst_frame->pts == kPts);
    assert(dst_frame->data[0] != nullptr);

    // Flushing decoder buffers
    decoder.flush();

    av_frame_free(&dst_frame);
    av_packet_free(&pkt);
    std::cout << "[PASS] test_synthetic_packet_decoding" << std::endl;
}

int main() {
    test_decoder_initialization_parameters();
    test_synthetic_packet_decoding();
    std::cout << "All VideoDecoder tests passed successfully!" << std::endl;
    return 0;
}
