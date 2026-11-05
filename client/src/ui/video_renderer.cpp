#include "ui/video_renderer.hpp"

#include <algorithm>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#endif

namespace mirror::ui {

VideoRenderer::VideoRenderer(std::string title, int canvas_w, int canvas_h)
    : title_(std::move(title)),
      canvas_w_(canvas_w),
      canvas_h_(canvas_h) {
}

VideoRenderer::~VideoRenderer() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

bool VideoRenderer::init() {
#if defined(_WIN32)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif

    // Initialize SDL video and timer subsystems
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Query desktop screen bounds to fit window comfortably without overflow
    SDL_Rect display_bounds{0, 0, 1920, 1080};
    SDL_GetDisplayUsableBounds(0, &display_bounds);

    int initial_w = canvas_w_;
    int initial_h = canvas_h_;
    int max_initial_w = (display_bounds.w > 0) ? (display_bounds.w * 8 / 10) : 1280;
    int max_initial_h = (display_bounds.h > 0) ? (display_bounds.h * 8 / 10) : 900;

    if (initial_w > max_initial_w || initial_h > max_initial_h) {
        double scale_w = static_cast<double>(max_initial_w) / static_cast<double>(initial_w);
        double scale_h = static_cast<double>(max_initial_h) / static_cast<double>(initial_h);
        double scale = std::min(scale_w, scale_h);
        initial_w = std::max(1, static_cast<int>(initial_w * scale));
        initial_h = std::max(1, static_cast<int>(initial_h * scale));
    }

    // Create resizable high-DPI window
    window_ = SDL_CreateWindow(
        title_.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        initial_w,
        initial_h,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN
    );
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create accelerated VSync renderer, falling back to software if necessary
    renderer_ = SDL_CreateRenderer(
        window_,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, 0);
    }
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Attempt adaptive VSync (-1), falling back to standard VSync (1)
    if (SDL_GL_SetSwapInterval(-1) != 0) {
        SDL_GL_SetSwapInterval(1);
    }

    // Create streaming IYUV (YUV420P) texture matching canonical canvas dimensions
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        canvas_w_,
        canvas_h_
    );
    if (!texture_) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Render an initial solid black frame so window is never left uninitialized/transparent
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderPresent(renderer_);

    return true;
}

SDL_Rect VideoRenderer::calculate_viewport_rect() const noexcept {
    if (!window_ || canvas_w_ <= 0 || canvas_h_ <= 0) {
        return SDL_Rect{0, 0, canvas_w_, canvas_h_};
    }

    int win_w = 0;
    int win_h = 0;
    SDL_GetWindowSize(window_, &win_w, &win_h);

    if (win_w <= 0 || win_h <= 0) {
        return SDL_Rect{0, 0, 0, 0};
    }

    double scale_x = static_cast<double>(win_w) / static_cast<double>(canvas_w_);
    double scale_y = static_cast<double>(win_h) / static_cast<double>(canvas_h_);
    double scale = std::min(scale_x, scale_y);

    int dst_w = static_cast<int>(canvas_w_ * scale);
    int dst_h = static_cast<int>(canvas_h_ * scale);

    int dst_x = (win_w - dst_w) / 2;
    int dst_y = (win_h - dst_h) / 2;

    return SDL_Rect{dst_x, dst_y, dst_w, dst_h};
}

void VideoRenderer::render_frame(AVFrame* frame) {
    if (!frame || is_minimized_ || !renderer_ || !texture_) {
        return;
    }

    // Direct planar upload to GPU VRAM: zero CPU pixel conversion
    int ret = SDL_UpdateYUVTexture(
        texture_,
        nullptr,
        frame->data[0], frame->linesize[0],
        frame->data[1], frame->linesize[1],
        frame->data[2], frame->linesize[2]
    );
    if (ret != 0) {
        return;
    }

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    SDL_Rect dst_rect = calculate_viewport_rect();
    SDL_RenderCopy(renderer_, texture_, nullptr, &dst_rect);

    SDL_RenderPresent(renderer_);
}

} // namespace mirror::ui
