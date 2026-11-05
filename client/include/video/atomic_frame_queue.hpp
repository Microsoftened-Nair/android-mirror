#pragma once

#include <atomic>

extern "C" {
#include <libavutil/frame.h>
}

namespace mirror::video {

/**
 * Thread-safe, lockless single-frame atomic swap queue using triple buffering
 * to pass AVFrames from the decoder worker thread to the SDL2 presentation thread
 * without buffer bloat, heap allocation, or pixel copying.
 */
class AtomicFrameQueue {
public:
    explicit AtomicFrameQueue();
    ~AtomicFrameQueue();

    AtomicFrameQueue(const AtomicFrameQueue&) = delete;
    AtomicFrameQueue& operator=(const AtomicFrameQueue&) = delete;
    AtomicFrameQueue(AtomicFrameQueue&&) = delete;
    AtomicFrameQueue& operator=(AtomicFrameQueue&&) = delete;

    /**
     * Swaps src_frame into the clean slot and publishes it as the latest write slot.
     * Non-blocking; drops the frame under arbitration contention.
     * Uses av_frame_move_ref to avoid copying pixel buffers.
     */
    void push_latest(AVFrame* src_frame);

    /**
     * Acquires the latest written frame and moves its reference into dst_frame.
     * Returns true if a new frame was consumed, false if no new frame is available.
     * Uses av_frame_move_ref to avoid copying pixel buffers.
     */
    bool consume(AVFrame* dst_frame);

    /**
     * Checks if a new unconsumed frame is currently available.
     */
    [[nodiscard]] bool has_new_frame() const noexcept {
        return new_frame_available_.load(std::memory_order_acquire);
    }

private:
    AVFrame* slots_[3]{nullptr, nullptr, nullptr};
    std::atomic<int> write_idx_{0};
    std::atomic<int> read_idx_{1};
    std::atomic<int> clean_idx_{2};
    std::atomic<bool> new_frame_available_{false};
};

} // namespace mirror::video

namespace mirror {
using mirror::video::AtomicFrameQueue;
}

using mirror::video::AtomicFrameQueue;
