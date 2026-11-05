#include "ui/input_mapper.hpp"
#include "protocol/control_packet.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace mirror::ui {

namespace {

uint32_t sdl_scancode_to_android(SDL_Scancode sc) noexcept {
    switch (sc) {
        case SDL_SCANCODE_HOME: return 3;       // AKEYCODE_HOME
        case SDL_SCANCODE_ESCAPE: return 4;     // AKEYCODE_BACK
        case SDL_SCANCODE_0: return 7;          // AKEYCODE_0
        case SDL_SCANCODE_1: return 8;
        case SDL_SCANCODE_2: return 9;
        case SDL_SCANCODE_3: return 10;
        case SDL_SCANCODE_4: return 11;
        case SDL_SCANCODE_5: return 12;
        case SDL_SCANCODE_6: return 13;
        case SDL_SCANCODE_7: return 14;
        case SDL_SCANCODE_8: return 15;
        case SDL_SCANCODE_9: return 16;
        case SDL_SCANCODE_UP: return 19;        // AKEYCODE_DPAD_UP
        case SDL_SCANCODE_DOWN: return 20;      // AKEYCODE_DPAD_DOWN
        case SDL_SCANCODE_LEFT: return 21;      // AKEYCODE_DPAD_LEFT
        case SDL_SCANCODE_RIGHT: return 22;     // AKEYCODE_DPAD_RIGHT
        case SDL_SCANCODE_SELECT: return 23;    // AKEYCODE_DPAD_CENTER
        case SDL_SCANCODE_VOLUMEUP: return 24;  // AKEYCODE_VOLUME_UP
        case SDL_SCANCODE_VOLUMEDOWN: return 25;// AKEYCODE_VOLUME_DOWN
        case SDL_SCANCODE_POWER: return 26;     // AKEYCODE_POWER
        case SDL_SCANCODE_A: return 29;
        case SDL_SCANCODE_B: return 30;
        case SDL_SCANCODE_C: return 31;
        case SDL_SCANCODE_D: return 32;
        case SDL_SCANCODE_E: return 33;
        case SDL_SCANCODE_F: return 34;
        case SDL_SCANCODE_G: return 35;
        case SDL_SCANCODE_H: return 36;
        case SDL_SCANCODE_I: return 37;
        case SDL_SCANCODE_J: return 38;
        case SDL_SCANCODE_K: return 39;
        case SDL_SCANCODE_L: return 40;
        case SDL_SCANCODE_M: return 41;
        case SDL_SCANCODE_N: return 42;
        case SDL_SCANCODE_O: return 43;
        case SDL_SCANCODE_P: return 44;
        case SDL_SCANCODE_Q: return 45;
        case SDL_SCANCODE_R: return 46;
        case SDL_SCANCODE_S: return 47;
        case SDL_SCANCODE_T: return 48;
        case SDL_SCANCODE_U: return 49;
        case SDL_SCANCODE_V: return 50;
        case SDL_SCANCODE_W: return 51;
        case SDL_SCANCODE_X: return 52;
        case SDL_SCANCODE_Y: return 53;
        case SDL_SCANCODE_Z: return 54;
        case SDL_SCANCODE_COMMA: return 55;
        case SDL_SCANCODE_PERIOD: return 56;
        case SDL_SCANCODE_LALT: return 57;
        case SDL_SCANCODE_RALT: return 58;
        case SDL_SCANCODE_LSHIFT: return 59;
        case SDL_SCANCODE_RSHIFT: return 60;
        case SDL_SCANCODE_TAB: return 61;
        case SDL_SCANCODE_SPACE: return 62;
        case SDL_SCANCODE_RETURN: return 66;
        case SDL_SCANCODE_BACKSPACE: return 67;
        case SDL_SCANCODE_GRAVE: return 68;
        case SDL_SCANCODE_MINUS: return 69;
        case SDL_SCANCODE_EQUALS: return 70;
        case SDL_SCANCODE_LEFTBRACKET: return 71;
        case SDL_SCANCODE_RIGHTBRACKET: return 72;
        case SDL_SCANCODE_BACKSLASH: return 73;
        case SDL_SCANCODE_SEMICOLON: return 74;
        case SDL_SCANCODE_APOSTROPHE: return 75;
        case SDL_SCANCODE_SLASH: return 76;
        case SDL_SCANCODE_PAGEUP: return 92;
        case SDL_SCANCODE_PAGEDOWN: return 93;
        case SDL_SCANCODE_DELETE: return 112;   // AKEYCODE_FORWARD_DEL
        case SDL_SCANCODE_LCTRL: return 113;
        case SDL_SCANCODE_RCTRL: return 114;
        case SDL_SCANCODE_CAPSLOCK: return 115;
        case SDL_SCANCODE_SCROLLLOCK: return 116;
        case SDL_SCANCODE_LGUI: return 117;
        case SDL_SCANCODE_RGUI: return 118;
        case SDL_SCANCODE_PRINTSCREEN: return 120;
        case SDL_SCANCODE_PAUSE: return 121;
        case SDL_SCANCODE_INSERT: return 124;
        case SDL_SCANCODE_F1: return 131;
        case SDL_SCANCODE_F2: return 132;
        case SDL_SCANCODE_F3: return 133;
        case SDL_SCANCODE_F4: return 134;
        case SDL_SCANCODE_F5: return 135;
        case SDL_SCANCODE_F6: return 136;
        case SDL_SCANCODE_F7: return 137;
        case SDL_SCANCODE_F8: return 138;
        case SDL_SCANCODE_F9: return 139;
        case SDL_SCANCODE_F10: return 140;
        case SDL_SCANCODE_F11: return 141;
        case SDL_SCANCODE_F12: return 142;
        default: return 0;
    }
}

uint32_t sdl_mod_to_android(uint16_t mod) noexcept {
    uint32_t meta = 0;
    if (mod & KMOD_SHIFT) meta |= 0x01;    // AMETA_SHIFT_ON
    if (mod & KMOD_LSHIFT) meta |= 0x40;   // AMETA_SHIFT_LEFT_ON
    if (mod & KMOD_RSHIFT) meta |= 0x80;   // AMETA_SHIFT_RIGHT_ON
    if (mod & KMOD_ALT) meta |= 0x02;      // AMETA_ALT_ON
    if (mod & KMOD_LALT) meta |= 0x10;     // AMETA_ALT_LEFT_ON
    if (mod & KMOD_RALT) meta |= 0x20;     // AMETA_ALT_RIGHT_ON
    if (mod & KMOD_CTRL) meta |= 0x1000;   // AMETA_CTRL_ON
    if (mod & KMOD_LCTRL) meta |= 0x2000;  // AMETA_CTRL_LEFT_ON
    if (mod & KMOD_RCTRL) meta |= 0x4000;  // AMETA_CTRL_RIGHT_ON
    if (mod & KMOD_GUI) meta |= 0x10000;   // AMETA_META_ON
    if (mod & KMOD_LGUI) meta |= 0x20000;  // AMETA_META_LEFT_ON
    if (mod & KMOD_RGUI) meta |= 0x40000;  // AMETA_META_RIGHT_ON
    if (mod & KMOD_CAPS) meta |= 0x100000; // AMETA_CAPS_LOCK_ON
    if (mod & KMOD_NUM) meta |= 0x200000;  // AMETA_NUM_LOCK_ON
    if (mod & KMOD_SCROLL) meta |= 0x400000;// AMETA_SCROLL_LOCK_ON
    return meta;
}

} // namespace

InputMapper::InputMapper(int control_fd, int canvas_w, int canvas_h)
    : control_fd_(control_fd),
      canvas_w_(canvas_w),
      canvas_h_(canvas_h),
      serializer_(control_fd) {
}

bool InputMapper::translate_coordinates(int win_x, int win_y, const SDL_Rect& viewport,
                                       uint32_t& target_x, uint32_t& target_y) const noexcept {
    int x_view = win_x - viewport.x;
    int y_view = win_y - viewport.y;

    // Reject interactions within letterbox / pillarbox margins
    if (x_view < 0 || x_view >= viewport.w || y_view < 0 || y_view >= viewport.h) {
        return false;
    }

    if (viewport.w <= 0 || viewport.h <= 0 || canvas_w_ <= 0 || canvas_h_ <= 0) {
        return false;
    }

    int cx = (x_view * canvas_w_) / viewport.w;
    int cy = (y_view * canvas_h_) / viewport.h;

    target_x = static_cast<uint32_t>(std::clamp(cx, 0, canvas_w_ - 1));
    target_y = static_cast<uint32_t>(std::clamp(cy, 0, canvas_h_ - 1));
    return true;
}

void InputMapper::handle_mouse_button(const SDL_MouseButtonEvent& e, const SDL_Rect& viewport) {
    if (e.button == SDL_BUTTON_RIGHT) {
        // Right-Click -> KEYCODE_BACK (4)
        uint8_t action = (e.type == SDL_MOUSEBUTTONDOWN) ? protocol::KEY_ACTION_DOWN : protocol::KEY_ACTION_UP;
        serializer_.send_key(action, 4, 0);
        return;
    }

    if (e.button == SDL_BUTTON_MIDDLE) {
        // Middle-Click -> KEYCODE_HOME (3)
        uint8_t action = (e.type == SDL_MOUSEBUTTONDOWN) ? protocol::KEY_ACTION_DOWN : protocol::KEY_ACTION_UP;
        serializer_.send_key(action, 3, 0);
        return;
    }

    if (e.button == SDL_BUTTON_LEFT) {
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            uint32_t tx = 0;
            uint32_t ty = 0;
            if (!translate_coordinates(e.x, e.y, viewport, tx, ty)) {
                return; // Click fell in margin deadzone
            }

            down_x_ = e.x;
            down_y_ = e.y;
            has_exceeded_slop_ = false;
            is_mouse_down_ = true;
            last_valid_x_ = tx;
            last_valid_y_ = ty;

            serializer_.send_touch(protocol::TOUCH_ACTION_DOWN, 0, tx, ty,
                                   static_cast<uint16_t>(canvas_w_), static_cast<uint16_t>(canvas_h_),
                                   0xFFFF, 0x0001);
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            if (is_mouse_down_) {
                uint32_t tx = last_valid_x_;
                uint32_t ty = last_valid_y_;
                translate_coordinates(e.x, e.y, viewport, tx, ty);

                serializer_.send_touch(protocol::TOUCH_ACTION_UP, 0, tx, ty,
                                       static_cast<uint16_t>(canvas_w_), static_cast<uint16_t>(canvas_h_),
                                       0x0000, 0x0000);
                is_mouse_down_ = false;
                has_exceeded_slop_ = false;
            }
        }
    }
}

void InputMapper::handle_mouse_motion(const SDL_MouseMotionEvent& e, const SDL_Rect& viewport) {
    if (!is_mouse_down_) {
        return;
    }

    // Touch-slop deadzone: filter mouse jitter under 8 pixels
    if (!has_exceeded_slop_) {
        if (std::abs(e.x - down_x_) > 8 || std::abs(e.y - down_y_) > 8) {
            has_exceeded_slop_ = true;
        } else {
            return;
        }
    }

    uint32_t tx = last_valid_x_;
    uint32_t ty = last_valid_y_;
    if (translate_coordinates(e.x, e.y, viewport, tx, ty)) {
        last_valid_x_ = tx;
        last_valid_y_ = ty;
    } else {
        // Clamp to edges if dragged into letterbox margins
        int x_view = std::clamp(e.x - viewport.x, 0, viewport.w - 1);
        int y_view = std::clamp(e.y - viewport.y, 0, viewport.h - 1);
        if (viewport.w > 0 && viewport.h > 0) {
            tx = static_cast<uint32_t>((x_view * canvas_w_) / viewport.w);
            ty = static_cast<uint32_t>((y_view * canvas_h_) / viewport.h);
            last_valid_x_ = tx;
            last_valid_y_ = ty;
        }
    }

    serializer_.send_touch(protocol::TOUCH_ACTION_MOVE, 0, tx, ty,
                           static_cast<uint16_t>(canvas_w_), static_cast<uint16_t>(canvas_h_),
                           0xFFFF, 0x0001);
}

void InputMapper::handle_mouse_wheel(const SDL_MouseWheelEvent& e, int mouse_x, int mouse_y, const SDL_Rect& viewport) {
    uint32_t tx = 0;
    uint32_t ty = 0;
    if (!translate_coordinates(mouse_x, mouse_y, viewport, tx, ty)) {
        return;
    }

    int32_t hscroll = e.x;
    int32_t vscroll = (e.direction == SDL_MOUSEWHEEL_FLIPPED) ? -e.y : e.y;

    serializer_.send_scroll(tx, ty, hscroll, vscroll);
}

void InputMapper::handle_keyboard_event(const SDL_KeyboardEvent& e) {
    uint8_t action = (e.type == SDL_KEYDOWN) ? protocol::KEY_ACTION_DOWN : protocol::KEY_ACTION_UP;
    uint32_t meta = sdl_mod_to_android(e.keysym.mod);

    // Ctrl+P / Cmd+P -> KEYCODE_POWER (26)
    if ((e.keysym.mod & (KMOD_CTRL | KMOD_GUI)) && e.keysym.scancode == SDL_SCANCODE_P) {
        serializer_.send_key(action, 26, meta);
        return;
    }

    // Ctrl+V / Cmd+V -> Clipboard Paste
    if (action == protocol::KEY_ACTION_DOWN &&
        (e.keysym.mod & (KMOD_CTRL | KMOD_GUI)) && e.keysym.scancode == SDL_SCANCODE_V) {
        handle_clipboard_paste();
        return;
    }

    // Escape -> KEYCODE_BACK (4)
    if (e.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        serializer_.send_key(action, 4, meta);
        return;
    }

    uint32_t android_kc = sdl_scancode_to_android(e.keysym.scancode);
    if (android_kc != 0) {
        serializer_.send_key(action, android_kc, meta);
    }
}

void InputMapper::handle_focus_lost() {
    if (is_mouse_down_) {
        serializer_.send_touch(protocol::TOUCH_ACTION_UP, 0, last_valid_x_, last_valid_y_,
                               static_cast<uint16_t>(canvas_w_), static_cast<uint16_t>(canvas_h_),
                               0x0000, 0x0000);
        is_mouse_down_ = false;
        has_exceeded_slop_ = false;
    }
}

void InputMapper::handle_clipboard_paste() {
    if (control_fd_ < 0) {
        return;
    }
    char* text = SDL_GetClipboardText();
    if (text) {
        if (text[0] != '\0') {
            serializer_.send_clipboard(text, true);
        }
        SDL_free(text);
    }
}

} // namespace mirror::ui
