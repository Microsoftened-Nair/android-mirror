#include "ui/video_renderer.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

static AVFrame* allocate_test_yuv_frame(int width, int height) {
    AVFrame* frame = av_frame_alloc();
    assert(frame != nullptr);
    frame->width = width;
    frame->height = height;
    frame->format = AV_PIX_FMT_YUV420P;
    int ret = av_frame_get_buffer(frame, 32);
    assert(ret == 0);

    // Initialize Y, U, V buffers
    for (int y = 0; y < height; ++y) {
        std::memset(frame->data[0] + y * frame->linesize[0], 128, width);
    }
    for (int y = 0; y < height / 2; ++y) {
        std::memset(frame->data[1] + y * frame->linesize[1], 64, width / 2);
        std::memset(frame->data[2] + y * frame->linesize[2], 192, width / 2);
    }
    return frame;
}

static void test_renderer_initialization_and_rendering() {
    constexpr int kCanvasW = 320;
    constexpr int kCanvasH = 240;

    mirror::ui::VideoRenderer renderer("Test Mirror Window", kCanvasW, kCanvasH);

    bool ok = renderer.init();
    assert(ok);
    assert(renderer.get_window() != nullptr);
    assert(renderer.get_renderer() != nullptr);
    assert(renderer.get_texture() != nullptr);
    assert(renderer.canvas_width() == kCanvasW);
    assert(renderer.canvas_height() == kCanvasH);

    // 1:1 initial viewport
    SDL_Rect vp = renderer.calculate_viewport_rect();
    assert(vp.w == kCanvasW);
    assert(vp.h == kCanvasH);
    assert(vp.x == 0);
    assert(vp.y == 0);

    // Test aspect ratio calculation on resize
    // Wider window: pillarboxing
    SDL_SetWindowSize(renderer.get_window(), 640, 240);
    SDL_Rect pillarbox = renderer.calculate_viewport_rect();
    assert(pillarbox.w == 320);
    assert(pillarbox.h == 240);
    assert(pillarbox.x == 160);
    assert(pillarbox.y == 0);

    // Taller window: letterboxing
    SDL_SetWindowSize(renderer.get_window(), 320, 480);
    SDL_Rect letterbox = renderer.calculate_viewport_rect();
    assert(letterbox.w == 320);
    assert(letterbox.h == 240);
    assert(letterbox.x == 0);
    assert(letterbox.y == 120);

    // Restore size
    SDL_SetWindowSize(renderer.get_window(), kCanvasW, kCanvasH);

    // Test planar frame rendering
    AVFrame* frame = allocate_test_yuv_frame(kCanvasW, kCanvasH);
    renderer.render_frame(frame);

    // Test minimized state
    assert(!renderer.is_minimized());
    renderer.set_minimized(true);
    assert(renderer.is_minimized());
    // Should skip render while minimized
    renderer.render_frame(frame);
    renderer.set_minimized(false);
    assert(!renderer.is_minimized());

    av_frame_free(&frame);
    std::cout << "[PASS] test_renderer_initialization_and_rendering" << std::endl;
}

int main(int argc, char* argv[]) {
    // Suppress unused parameter warnings for main signature matching SDL
    (void)argc;
    (void)argv;

    test_renderer_initialization_and_rendering();
    std::cout << "All VideoRenderer tests passed successfully!" << std::endl;
    return 0;
}
