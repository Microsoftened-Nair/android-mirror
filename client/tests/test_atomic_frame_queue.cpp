#include "video/atomic_frame_queue.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

static AVFrame* allocate_test_frame(int width, int height, int64_t pts) {
    AVFrame* frame = av_frame_alloc();
    assert(frame != nullptr);
    frame->width = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_YUV420P;
    frame->pts = pts;
    int ret = av_frame_get_buffer(frame, 32);
    assert(ret == 0);
    return frame;
}

static void test_basic_push_and_consume() {
    mirror::video::AtomicFrameQueue queue;
    assert(!queue.has_new_frame());

    AVFrame* src = allocate_test_frame(1920, 1080, 42);
    AVFrame* dst = av_frame_alloc();
    assert(dst != nullptr);

    // Initial consume on empty queue must return false
    assert(!queue.consume(dst));

    // Push latest frame
    queue.push_latest(src);
    assert(queue.has_new_frame());
    // src frame buffer ownership must have been moved (unref'd)
    assert(src->data[0] == nullptr);

    // Consume frame
    bool ok = queue.consume(dst);
    assert(ok);
    assert(!queue.has_new_frame());
    assert(dst->width == 1920);
    assert(dst->height == 1080);
    assert(dst->pts == 42);
    assert(dst->data[0] != nullptr);

    // Subsequent consume must return false
    assert(!queue.consume(dst));

    av_frame_free(&src);
    av_frame_free(&dst);
    std::cout << "[PASS] test_basic_push_and_consume" << std::endl;
}

static void test_latest_frame_overwrite() {
    mirror::video::AtomicFrameQueue queue;

    AVFrame* f1 = allocate_test_frame(1280, 720, 100);
    AVFrame* f2 = allocate_test_frame(1280, 720, 200);
    AVFrame* f3 = allocate_test_frame(1280, 720, 300);

    // Push 3 frames rapidly without consuming in-between
    queue.push_latest(f1);
    queue.push_latest(f2);
    queue.push_latest(f3);

    AVFrame* dst = av_frame_alloc();
    bool ok = queue.consume(dst);
    assert(ok);
    // Triple buffering ensures latest frame (pts 300) is consumed
    assert(dst->pts == 300);

    // Next consume should be empty
    assert(!queue.consume(dst));

    av_frame_free(&f1);
    av_frame_free(&f2);
    av_frame_free(&f3);
    av_frame_free(&dst);
    std::cout << "[PASS] test_latest_frame_overwrite" << std::endl;
}

static void test_multithreaded_producer_consumer() {
    mirror::video::AtomicFrameQueue queue;

    constexpr int kTotalFrames = 50000;
    std::atomic<bool> producer_done{false};
    std::atomic<int> frames_produced{0};
    std::atomic<int> frames_consumed{0};
    int64_t last_consumed_pts = -1;

    std::thread producer([&]() {
        AVFrame* f = allocate_test_frame(640, 480, 0);
        for (int i = 0; i < kTotalFrames; ++i) {
            f->pts = i;
            queue.push_latest(f);
            frames_produced.fetch_add(1, std::memory_order_relaxed);

            // If buffer was moved, re-allocate underlying buffer for next push
            if (f->data[0] == nullptr) {
                f->width = 640;
                f->height = 480;
                f->format = AV_PIX_FMT_YUV420P;
                int ret = av_frame_get_buffer(f, 32);
                assert(ret == 0);
            }
        }
        av_frame_free(&f);
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        AVFrame* dst = av_frame_alloc();
        while (!producer_done.load(std::memory_order_acquire) || queue.has_new_frame()) {
            if (queue.consume(dst)) {
                frames_consumed.fetch_add(1, std::memory_order_relaxed);
                // PTS must monotonically increase (latest frame semantics)
                assert(dst->pts >= last_consumed_pts);
                last_consumed_pts = dst->pts;
            }
        }
        av_frame_free(&dst);
    });

    producer.join();
    consumer.join();

    assert(frames_produced.load() == kTotalFrames);
    assert(frames_consumed.load() > 0);
    std::cout << "[PASS] test_multithreaded_producer_consumer (produced "
              << frames_produced.load() << ", consumed " << frames_consumed.load() << ")" << std::endl;
}

int main() {
    test_basic_push_and_consume();
    test_latest_frame_overwrite();
    test_multithreaded_producer_consumer();
    std::cout << "All AtomicFrameQueue tests passed successfully!" << std::endl;
    return 0;
}
