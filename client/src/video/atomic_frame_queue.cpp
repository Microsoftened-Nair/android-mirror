#include "video/atomic_frame_queue.hpp"

#include <new>

namespace mirror::video {

AtomicFrameQueue::AtomicFrameQueue() {
    for (int i = 0; i < 3; ++i) {
        slots_[i] = av_frame_alloc();
        if (!slots_[i]) {
            for (int j = 0; j < i; ++j) {
                av_frame_free(&slots_[j]);
            }
            throw std::bad_alloc();
        }
    }
}

AtomicFrameQueue::~AtomicFrameQueue() {
    for (int i = 0; i < 3; ++i) {
        if (slots_[i]) {
            av_frame_free(&slots_[i]);
        }
    }
}

void AtomicFrameQueue::push_latest(AVFrame* src_frame) {
    if (!src_frame) {
        return;
    }

    // 1. Claim the clean slot index
    int target = clean_idx_.exchange(-1, std::memory_order_acq_rel);

    // 2. Drop frame under arbitration contention
    if (target == -1) {
        return;
    }

    // 3. Unreference any existing buffer in the target slot
    av_frame_unref(slots_[target]);

    // 4. Move reference from src_frame into slots_[target] (zero pixel copying)
    av_frame_move_ref(slots_[target], src_frame);

    // 5. Publish target to write_idx_ and retrieve the previous write slot
    int prev_write = write_idx_.exchange(target, std::memory_order_acq_rel);

    // 6. Return the previous write slot to clean_idx_
    if (prev_write >= 0) {
        clean_idx_.store(prev_write, std::memory_order_release);
    }

    // 7. Signal that a new frame is available for consumption
    new_frame_available_.store(true, std::memory_order_release);
}

bool AtomicFrameQueue::consume(AVFrame* dst_frame) {
    if (!dst_frame) {
        return false;
    }

    // 1. Check if a new frame is available
    if (!new_frame_available_.load(std::memory_order_acquire)) {
        return false;
    }

    // 2. Claim write_idx_
    int target = write_idx_.exchange(-1, std::memory_order_acq_rel);

    // 3. If contention occurred or no slot was claimed, return false
    if (target == -1) {
        return false;
    }

    // 4. Unreference destination frame
    av_frame_unref(dst_frame);

    // 5. Move reference from slots_[target] to dst_frame (zero pixel copying)
    av_frame_move_ref(dst_frame, slots_[target]);

    // 6. Publish target to read_idx_ and retrieve the previous read slot
    int prev_read = read_idx_.exchange(target, std::memory_order_acq_rel);

    // 7. Recycle the previous read slot to clean_idx_
    if (prev_read >= 0) {
        clean_idx_.store(prev_read, std::memory_order_release);
    }

    // 8. Mark frame availability consumed
    new_frame_available_.store(false, std::memory_order_release);

    return true;
}

} // namespace mirror::video
