#pragma once

#include "protocol/control_packet.hpp"

#include <cstdint>
#include <string_view>

namespace mirror::net {

/**
 * Zero-copy serializer emitting packed binary control messages into an active control channel socket.
 */
class ControlSerializer {
public:
    explicit ControlSerializer(int fd = -1) noexcept : fd_(fd) {}
    ~ControlSerializer() = default;

    void set_fd(int fd) noexcept { fd_ = fd; }
    [[nodiscard]] int fd() const noexcept { return fd_; }

    // Static dispatch methods
    static bool send_touch(int fd, uint8_t action, uint64_t pointer_id,
                           uint32_t x, uint32_t y, uint16_t w, uint16_t h,
                           uint16_t pressure, uint16_t buttons);

    static bool send_key(int fd, uint8_t action, uint32_t keycode, uint32_t meta);

    static bool send_scroll(int fd, uint32_t x, uint32_t y, int32_t hscroll, int32_t vscroll);

    static bool send_clipboard(int fd, std::string_view text, bool paste_now);

    static bool send_screen_power(int fd, bool power_on);

    static bool send_keyframe_request(int fd);

    static bool send_heartbeat(int fd, uint32_t ping_id);

    // Instance convenience methods
    bool send_touch(uint8_t action, uint64_t pointer_id,
                    uint32_t x, uint32_t y, uint16_t w, uint16_t h,
                    uint16_t pressure, uint16_t buttons) const {
        return send_touch(fd_, action, pointer_id, x, y, w, h, pressure, buttons);
    }

    bool send_key(uint8_t action, uint32_t keycode, uint32_t meta) const {
        return send_key(fd_, action, keycode, meta);
    }

    bool send_scroll(uint32_t x, uint32_t y, int32_t hscroll, int32_t vscroll) const {
        return send_scroll(fd_, x, y, hscroll, vscroll);
    }

    bool send_clipboard(std::string_view text, bool paste_now) const {
        return send_clipboard(fd_, text, paste_now);
    }

    bool send_screen_power(bool power_on) const {
        return send_screen_power(fd_, power_on);
    }

    bool send_keyframe_request() const {
        return send_keyframe_request(fd_);
    }

    bool send_heartbeat(uint32_t ping_id) const {
        return send_heartbeat(fd_, ping_id);
    }

private:
    int fd_{-1};
};

} // namespace mirror::net

namespace mirror {
using mirror::net::ControlSerializer;
}

using mirror::net::ControlSerializer;
