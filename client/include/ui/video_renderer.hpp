#pragma once

#include <SDL2/SDL.h>
#include <string>

extern "C" {
#include <libavutil/frame.h>
}

namespace mirror::ui {

/**
 * Native desktop presentation window with hardware-accelerated SDL2 renderer,
 * adaptive VSync configuration, aspect-ratio-preserving letterbox calculation,
 * and zero-copy planar YUV420P streaming texture updates.
 */
class VideoRenderer {
public:
    explicit VideoRenderer(std::string title, int canvas_w, int canvas_h);
    ~VideoRenderer();

    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;
    VideoRenderer(VideoRenderer&&) = delete;
    VideoRenderer& operator=(VideoRenderer&&) = delete;

    /**
     * Initializes Per-Monitor V2 DPI awareness (Windows), SDL video subsystem,
     * resizable high-DPI window, accelerated VSync renderer, and streaming IYUV texture.
     */
    bool init();

    /**
     * Renders a decoded YUV420P frame directly into the hardware texture without CPU pixel conversion.
     */
    void render_frame(AVFrame* frame);

    /**
     * Computes the aspect-ratio-preserving destination viewport rectangle
     * for letterboxing or pillarboxing within the current window bounds.
     */
    [[nodiscard]] SDL_Rect calculate_viewport_rect() const noexcept;

    void set_minimized(bool minimized) noexcept { is_minimized_ = minimized; }
    [[nodiscard]] bool is_minimized() const noexcept { return is_minimized_; }

    [[nodiscard]] SDL_Window* get_window() const noexcept { return window_; }
    [[nodiscard]] SDL_Renderer* get_renderer() const noexcept { return renderer_; }
    [[nodiscard]] SDL_Texture* get_texture() const noexcept { return texture_; }
    [[nodiscard]] int canvas_width() const noexcept { return canvas_w_; }
    [[nodiscard]] int canvas_height() const noexcept { return canvas_h_; }

private:
    std::string title_;
    int canvas_w_{0};
    int canvas_h_{0};
    bool is_minimized_{false};

    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    SDL_Texture* texture_{nullptr};
};

} // namespace mirror::ui

namespace mirror {
using mirror::ui::VideoRenderer;
}

using mirror::ui::VideoRenderer;
