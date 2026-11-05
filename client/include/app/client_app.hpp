#pragma once

#include "ui/input_mapper.hpp"
#include "ui/video_renderer.hpp"
#include "video/atomic_frame_queue.hpp"
#include "video/video_pipeline.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace mirror::app {

/**
 * Top-level desktop application orchestrator coordinating network transport,
 * video decoding, input handling, and the SDL event loop on Thread 0.
 */
class ClientApp {
public:
    explicit ClientApp(int video_fd, int control_fd, int canvas_w, int canvas_h, const std::string& device_name);
    ~ClientApp();

    ClientApp(const ClientApp&) = delete;
    ClientApp& operator=(const ClientApp&) = delete;
    ClientApp(ClientApp&&) = delete;
    ClientApp& operator=(ClientApp&&) = delete;

    /**
     * Executes the primary OS message and presentation loop on Thread 0.
     * Drains SDL events, translates inputs, consumes decoded video frames,
     * and presents planar frames to the GPU renderer.
     */
    int run();

    void request_stop() noexcept {
        is_running_.store(false, std::memory_order_release);
    }

    [[nodiscard]] bool is_running() const noexcept {
        return is_running_.load(std::memory_order_acquire);
    }

    [[nodiscard]] video::AtomicFrameQueue& get_frame_queue() noexcept { return frame_queue_; }
    [[nodiscard]] ui::VideoRenderer* get_renderer() const noexcept { return renderer_.get(); }
    [[nodiscard]] ui::InputMapper* get_input_mapper() const noexcept { return input_mapper_.get(); }

private:
    std::atomic<bool> is_running_{true};
    std::unique_ptr<ui::VideoRenderer> renderer_;
    std::unique_ptr<ui::InputMapper> input_mapper_;
    video::AtomicFrameQueue frame_queue_;
    std::unique_ptr<video::VideoPipeline> video_pipeline_;

    int video_fd_{-1};
    int control_fd_{-1};
    int canvas_w_{0};
    int canvas_h_{0};
    std::string device_name_;
};

} // namespace mirror::app

namespace mirror {
using mirror::app::ClientApp;
}

using mirror::app::ClientApp;
