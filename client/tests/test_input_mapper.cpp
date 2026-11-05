#include "ui/input_mapper.hpp"
#include "protocol/control_packet.hpp"

#include <cassert>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

static void test_coordinate_translation_and_margins() {
    int fds[2];
    int ret = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(ret == 0);

    constexpr int kCanvasW = 1080;
    constexpr int kCanvasH = 1920;
    mirror::ui::InputMapper mapper(fds[0], kCanvasW, kCanvasH);

    // Viewport centered inside larger window with margins
    // Viewport: x=100, y=50, w=400, h=800
    SDL_Rect viewport{100, 50, 400, 800};

    uint32_t tx = 0;
    uint32_t ty = 0;

    // Inside deadzone margins
    assert(!mapper.translate_coordinates(50, 50, viewport, tx, ty));   // Left margin
    assert(!mapper.translate_coordinates(550, 50, viewport, tx, ty));  // Right margin
    assert(!mapper.translate_coordinates(200, 20, viewport, tx, ty));  // Top margin
    assert(!mapper.translate_coordinates(200, 900, viewport, tx, ty)); // Bottom margin

    // Top-left corner of viewport
    assert(mapper.translate_coordinates(100, 50, viewport, tx, ty));
    assert(tx == 0);
    assert(ty == 0);

    // Center of viewport
    assert(mapper.translate_coordinates(300, 450, viewport, tx, ty));
    assert(tx == kCanvasW / 2);
    assert(ty == kCanvasH / 2);

    // Bottom-right edge (clamped within [0, canvas-1])
    assert(mapper.translate_coordinates(499, 849, viewport, tx, ty));
    assert(tx <= static_cast<uint32_t>(kCanvasW - 1));
    assert(ty <= static_cast<uint32_t>(kCanvasH - 1));

    ::close(fds[0]);
    ::close(fds[1]);
    std::cout << "[PASS] test_coordinate_translation_and_margins" << std::endl;
}

static void test_touch_slop_and_focus_lost() {
    int fds[2];
    int ret = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(ret == 0);

    constexpr int kCanvasW = 1000;
    constexpr int kCanvasH = 1000;
    mirror::ui::InputMapper mapper(fds[0], kCanvasW, kCanvasH);
    SDL_Rect viewport{0, 0, 500, 500};

    // 1. Mouse down at (100, 100) -> canvas (200, 200)
    SDL_MouseButtonEvent down_ev{};
    down_ev.type = SDL_MOUSEBUTTONDOWN;
    down_ev.button = SDL_BUTTON_LEFT;
    down_ev.x = 100;
    down_ev.y = 100;
    mapper.handle_mouse_button(down_ev, viewport);
    assert(mapper.is_mouse_down());
    assert(!mapper.has_exceeded_slop());

    // Read emitted Touch DOWN packet
    mirror::protocol::TouchEventPacket pkt{};
    ssize_t n = ::recv(fds[1], &pkt, sizeof(pkt), 0);
    assert(n == static_cast<ssize_t>(sizeof(pkt)));
    assert(pkt.type == 0x00);
    assert(pkt.action == mirror::protocol::TOUCH_ACTION_DOWN);
    assert(mirror::protocol::from_big_endian(pkt.pos_x) == 200);
    assert(mirror::protocol::from_big_endian(pkt.pos_y) == 200);

    // 2. Mouse motion within 8px slop (e.g. delta = 3px) -> suppressed
    SDL_MouseMotionEvent move_small{};
    move_small.type = SDL_MOUSEMOTION;
    move_small.x = 103;
    move_small.y = 102;
    mapper.handle_mouse_motion(move_small, viewport);
    assert(!mapper.has_exceeded_slop());

    // Verify nothing sent across socket (non-blocking probe)
    char probe_buf[32];
    ssize_t probe = ::recv(fds[1], probe_buf, sizeof(probe_buf), MSG_DONTWAIT);
    assert(probe < 0); // No packet emitted

    // 3. Mouse motion exceeding 8px slop (e.g. delta = 15px) -> emitted
    SDL_MouseMotionEvent move_large{};
    move_large.type = SDL_MOUSEMOTION;
    move_large.x = 115;
    move_large.y = 100;
    mapper.handle_mouse_motion(move_large, viewport);
    assert(mapper.has_exceeded_slop());

    n = ::recv(fds[1], &pkt, sizeof(pkt), 0);
    assert(n == static_cast<ssize_t>(sizeof(pkt)));
    assert(pkt.action == mirror::protocol::TOUCH_ACTION_MOVE);
    assert(mirror::protocol::from_big_endian(pkt.pos_x) == 230); // 115 / 500 * 1000 = 230

    // 4. Focus lost -> emits synthetic UP and cancels touch
    mapper.handle_focus_lost();
    assert(!mapper.is_mouse_down());

    n = ::recv(fds[1], &pkt, sizeof(pkt), 0);
    assert(n == static_cast<ssize_t>(sizeof(pkt)));
    assert(pkt.action == mirror::protocol::TOUCH_ACTION_UP);
    assert(pkt.pressure == 0);

    ::close(fds[0]);
    ::close(fds[1]);
    std::cout << "[PASS] test_touch_slop_and_focus_lost" << std::endl;
}

static void test_key_and_special_button_mapping() {
    int fds[2];
    int ret = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(ret == 0);

    mirror::ui::InputMapper mapper(fds[0], 500, 500);
    SDL_Rect viewport{0, 0, 500, 500};

    // Right-Click -> KEYCODE_BACK (4)
    SDL_MouseButtonEvent right_click{};
    right_click.type = SDL_MOUSEBUTTONDOWN;
    right_click.button = SDL_BUTTON_RIGHT;
    mapper.handle_mouse_button(right_click, viewport);

    mirror::protocol::KeyEventPacket key_pkt{};
    ssize_t n = ::recv(fds[1], &key_pkt, sizeof(key_pkt), 0);
    assert(n == static_cast<ssize_t>(sizeof(key_pkt)));
    assert(key_pkt.type == 0x01);
    assert(key_pkt.action == mirror::protocol::KEY_ACTION_DOWN);
    assert(mirror::protocol::from_big_endian(key_pkt.key_code) == 4);

    // Middle-Click -> KEYCODE_HOME (3)
    SDL_MouseButtonEvent mid_click{};
    mid_click.type = SDL_MOUSEBUTTONDOWN;
    mid_click.button = SDL_BUTTON_MIDDLE;
    mapper.handle_mouse_button(mid_click, viewport);

    n = ::recv(fds[1], &key_pkt, sizeof(key_pkt), 0);
    assert(n == static_cast<ssize_t>(sizeof(key_pkt)));
    assert(mirror::protocol::from_big_endian(key_pkt.key_code) == 3);

    // Ctrl+P -> KEYCODE_POWER (26)
    SDL_KeyboardEvent ctrl_p{};
    ctrl_p.type = SDL_KEYDOWN;
    ctrl_p.keysym.scancode = SDL_SCANCODE_P;
    ctrl_p.keysym.mod = KMOD_LCTRL;
    mapper.handle_keyboard_event(ctrl_p);

    n = ::recv(fds[1], &key_pkt, sizeof(key_pkt), 0);
    assert(n == static_cast<ssize_t>(sizeof(key_pkt)));
    assert(mirror::protocol::from_big_endian(key_pkt.key_code) == 26);
    assert(mirror::protocol::from_big_endian(key_pkt.meta_state) != 0);

    // Escape -> KEYCODE_BACK (4)
    SDL_KeyboardEvent esc{};
    esc.type = SDL_KEYDOWN;
    esc.keysym.scancode = SDL_SCANCODE_ESCAPE;
    mapper.handle_keyboard_event(esc);

    n = ::recv(fds[1], &key_pkt, sizeof(key_pkt), 0);
    assert(n == static_cast<ssize_t>(sizeof(key_pkt)));
    assert(mirror::protocol::from_big_endian(key_pkt.key_code) == 4);

    ::close(fds[0]);
    ::close(fds[1]);
    std::cout << "[PASS] test_key_and_special_button_mapping" << std::endl;
}

int main() {
    test_coordinate_translation_and_margins();
    test_touch_slop_and_focus_lost();
    test_key_and_special_button_mapping();
    std::cout << "All InputMapper tests passed successfully!" << std::endl;
    return 0;
}
