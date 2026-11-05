#pragma once

#include "net/control_serializer.hpp"

#include <SDL2/SDL.h>
#include <cstdint>

namespace mirror::ui {

/**
 * Translates desktop SDL2 mouse, keyboard, and window events into packed Module 2
 * binary control messages with coordinate normalization, deadzone suppression,
 * touch-slop filtering, and focus-lost auto-release.
 */
class InputMapper {
public:
    explicit InputMapper(int control_fd, int canvas_w, int canvas_h);
    ~InputMapper() = default;

    InputMapper(const InputMapper&) = delete;
    InputMapper& operator=(const InputMapper&) = delete;
    InputMapper(InputMapper&&) noexcept = default;
    InputMapper& operator=(InputMapper&&) noexcept = default;

    /**
     * Translates window pixel coordinates to canvas pixel coordinates within the viewport.
     * Rejects coordinates falling into letterbox or pillarbox margins.
     */
    bool translate_coordinates(int win_x, int win_y, const SDL_Rect& viewport,
                               uint32_t& target_x, uint32_t& target_y) const noexcept;

    void handle_mouse_button(const SDL_MouseButtonEvent& e, const SDL_Rect& viewport);
    void handle_mouse_motion(const SDL_MouseMotionEvent& e, const SDL_Rect& viewport);
    void handle_mouse_wheel(const SDL_MouseWheelEvent& e, int mouse_x, int mouse_y, const SDL_Rect& viewport);
    void handle_keyboard_event(const SDL_KeyboardEvent& e);
    void handle_focus_lost();
    void handle_clipboard_paste();

    void set_control_fd(int fd) noexcept {
        control_fd_ = fd;
        serializer_.set_fd(fd);
    }

    [[nodiscard]] int control_fd() const noexcept { return control_fd_; }
    [[nodiscard]] bool is_mouse_down() const noexcept { return is_mouse_down_; }
    [[nodiscard]] bool has_exceeded_slop() const noexcept { return has_exceeded_slop_; }
    [[nodiscard]] uint32_t last_valid_x() const noexcept { return last_valid_x_; }
    [[nodiscard]] uint32_t last_valid_y() const noexcept { return last_valid_y_; }

private:
    int control_fd_{-1};
    int canvas_w_{0};
    int canvas_h_{0};
    bool is_mouse_down_{false};
    bool has_exceeded_slop_{false};
    int down_x_{0};
    int down_y_{0};
    uint32_t last_valid_x_{0};
    uint32_t last_valid_y_{0};

    net::ControlSerializer serializer_;
};

} // namespace mirror::ui

namespace mirror {
using mirror::ui::InputMapper;
}

using mirror::ui::InputMapper;
