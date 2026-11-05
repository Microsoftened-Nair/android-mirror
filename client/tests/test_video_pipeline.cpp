#include "video/video_pipeline.hpp"
#include "protocol/video_packet.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
}

static AVPacket* encode_synthetic_frame(int width, int height, int64_t pts) {
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

    AVFrame* src = av_frame_alloc();
    assert(src != nullptr);
    src->width = width;
    src->height = height;
    src->format = AV_PIX_FMT_YUV420P;
    ret = av_frame_get_buffer(src, 32);
    assert(ret == 0);

    for (int y = 0; y < height; ++y) {
        std::memset(src->data[0] + y * src->linesize[0], static_cast<int>(pts & 0xFF), width);
    }
    for (int y = 0; y < height / 2; ++y) {
        std::memset(src->data[1] + y * src->linesize[1], 128, width / 2);
        std::memset(src->data[2] + y * src->linesize[2], 128, width / 2);
    }
    src->pts = pts;

    ret = avcodec_send_frame(enc_ctx, src);
    assert(ret == 0);

    AVPacket* pkt = av_packet_alloc();
    assert(pkt != nullptr);
    ret = avcodec_receive_packet(enc_ctx, pkt);
    assert(ret == 0);

    av_frame_free(&src);
    avcodec_free_context(&enc_ctx);
    return pkt;
}

static void send_framed_packet(int fd, const AVPacket* pkt, uint64_t pts_us) {
    mirror::protocol::VideoPacketHeader hdr{};
    hdr.pts_us = pts_us;
    hdr.payload_len = static_cast<uint32_t>(pkt->size);
    hdr = mirror::protocol::to_network_order(hdr);

    ssize_t n1 = ::send(fd, &hdr, sizeof(hdr), 0);
    assert(n1 == static_cast<ssize_t>(sizeof(hdr)));

    ssize_t n2 = ::send(fd, pkt->data, pkt->size, 0);
    assert(n2 == static_cast<ssize_t>(pkt->size));
}

static void test_pipeline_streaming_and_consumption() {
    int fds[2];
    int ret = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(ret == 0);

    constexpr int kWidth = 320;
    constexpr int kHeight = 240;

    mirror::video::VideoPipeline pipeline(fds[0], kWidth, kHeight);
    assert(!pipeline.is_running());

    bool started = pipeline.start();
    assert(started);
    assert(pipeline.is_running());

    // Generate and send 3 synthetic frames
    for (int i = 1; i <= 3; ++i) {
        AVPacket* pkt = encode_synthetic_frame(kWidth, kHeight, i * 1000);
        send_framed_packet(fds[1], pkt, static_cast<uint64_t>(i * 33333));
        av_packet_free(&pkt);
    }

    // Wait and consume decoded frame
    AVFrame* dst = av_frame_alloc();
    assert(dst != nullptr);

    bool received = false;
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (pipeline.consume_rendered_frame(dst)) {
            received = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert(received);
    assert(dst->width == kWidth);
    assert(dst->height == kHeight);
    assert(dst->format == AV_PIX_FMT_YUV420P);
    assert(dst->data[0] != nullptr);

    // Stop pipeline and clean up
    pipeline.stop();
    assert(!pipeline.is_running());

    av_frame_free(&dst);
    ::close(fds[0]);
    ::close(fds[1]);

    std::cout << "[PASS] test_pipeline_streaming_and_consumption" << std::endl;
}

int main() {
    test_pipeline_streaming_and_consumption();
    std::cout << "All VideoPipeline tests passed successfully!" << std::endl;
    return 0;
}
