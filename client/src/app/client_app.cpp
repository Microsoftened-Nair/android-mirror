#include "app/client_app.hpp"

#include <iostream>

namespace mirror::app {

ClientApp::ClientApp(int video_fd, int control_fd, int canvas_w, int canvas_h, const std::string& device_name)
    : video_fd_(video_fd),
      control_fd_(control_fd),
      canvas_w_(canvas_w),
      canvas_h_(canvas_h),
      device_name_(device_name) {
}

ClientApp::~ClientApp() {
    request_stop();
    if (video_pipeline_) {
        video_pipeline_->stop();
        video_pipeline_.reset();
    }
    renderer_.reset();
    input_mapper_.reset();
}

int ClientApp::run() {
    // 1. Initialize VideoRenderer and InputMapper
    std::string window_title = device_name_.empty() ? "Android Mirror" : ("Android Mirror - " + device_name_);
    renderer_ = std::make_unique<ui::VideoRenderer>(window_title, canvas_w_, canvas_h_);
    if (!renderer_->init()) {
        std::cerr << "Failed to initialize VideoRenderer" << std::endl;
        return 1;
    }

    input_mapper_ = std::make_unique<ui::InputMapper>(control_fd_, canvas_w_, canvas_h_);

    // Initialize video pipeline if network socket is active
    if (video_fd_ >= 0) {
        video_pipeline_ = std::make_unique<video::VideoPipeline>(video_fd_, canvas_w_, canvas_h_);
        if (!video_pipeline_->start()) {
            std::cerr << "Failed to start VideoPipeline worker thread" << std::endl;
            return 1;
        }
    }

    // 2. Allocate reusable frame for rendering
    AVFrame* render_frame = av_frame_alloc();
    if (!render_frame) {
        std::cerr << "Failed to allocate render frame" << std::endl;
        return 1;
    }

    SDL_Event event;

    // 3. Main OS message and presentation loop on Thread 0
    while (is_running_.load(std::memory_order_relaxed)) {
        // a. Event draining loop
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                is_running_.store(false, std::memory_order_release);
                break;
            }

            if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    renderer_->set_minimized(true);
                } else if (event.window.event == SDL_WINDOWEVENT_RESTORED ||
                           event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                    renderer_->set_minimized(false);
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    input_mapper_->handle_focus_lost();
                }
            } else {
                SDL_Rect viewport = renderer_->calculate_viewport_rect();
                if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                    input_mapper_->handle_mouse_button(event.button, viewport);
                } else if (event.type == SDL_MOUSEMOTION) {
                    input_mapper_->handle_mouse_motion(event.motion, viewport);
                } else if (event.type == SDL_MOUSEWHEEL) {
                    int mx = 0;
                    int my = 0;
                    SDL_GetMouseState(&mx, &my);
                    input_mapper_->handle_mouse_wheel(event.wheel, mx, my, viewport);
                } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                    input_mapper_->handle_keyboard_event(event.key);
                }
            }
        }

        // b. Frame rendering step
        bool frame_consumed = false;
        if (video_pipeline_) {
            frame_consumed = video_pipeline_->consume_rendered_frame(render_frame);
        } else {
            frame_consumed = frame_queue_.consume(render_frame);
        }

        if (frame_consumed) {
            renderer_->render_frame(render_frame);
        } else {
            // Yield CPU slice when no new frame is available to eliminate 100% busy-spin
            SDL_Delay(1);
        }
    }

    // 4. Teardown
    av_frame_free(&render_frame);

    if (video_pipeline_) {
        video_pipeline_->stop();
        video_pipeline_.reset();
    }

    renderer_.reset();
    input_mapper_.reset();

    return 0;
}

} // namespace mirror::app
