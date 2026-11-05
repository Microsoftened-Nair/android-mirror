#pragma once

#include "protocol/endian.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace mirror::protocol {

inline constexpr size_t MAX_CLIPBOARD_LENGTH = 262144; // 256 KB

inline constexpr uint8_t TOUCH_ACTION_DOWN = 0x00;
inline constexpr uint8_t TOUCH_ACTION_UP = 0x01;
inline constexpr uint8_t TOUCH_ACTION_MOVE = 0x02;
inline constexpr uint8_t TOUCH_ACTION_CANCEL = 0x03;

inline constexpr uint8_t KEY_ACTION_DOWN = 0x00;
inline constexpr uint8_t KEY_ACTION_UP = 0x01;

inline constexpr uint8_t SCREEN_POWER_MODE_OFF = 0x00;
inline constexpr uint8_t SCREEN_POWER_MODE_NORMAL = 0x02;

enum class ControlMessageType : uint8_t {
    InjectTouch = 0x00,
    InjectKey = 0x01,
    InjectScroll = 0x02,
    SetClipboard = 0x03,
    SetScreenPower = 0x04,
    RequestKeyframe = 0x05,
    HeartbeatPing = 0x06
};

#pragma pack(push, 1)

struct TouchEventPacket {
    uint8_t type;            // 0x00
    uint8_t action;          // 0x00=DOWN, 0x01=UP, 0x02=MOVE, 0x03=CANCEL
    uint16_t reserved;       // 0x0000
    uint64_t pointer_id;
    uint32_t pos_x;
    uint32_t pos_y;
    uint16_t canvas_width;
    uint16_t canvas_height;
    uint16_t pressure;       // 0x0000 to 0xFFFF
    uint16_t buttons;        // bitmask
};
static_assert(sizeof(TouchEventPacket) == 28, "TouchEventPacket must be 28 bytes");

struct KeyEventPacket {
    uint8_t type;            // 0x01
    uint8_t action;          // 0x00=DOWN, 0x01=UP
    uint16_t reserved;       // 0x0000
    uint32_t key_code;
    uint32_t meta_state;
};
static_assert(sizeof(KeyEventPacket) == 12, "KeyEventPacket must be 12 bytes");

struct ScrollEventPacket {
    uint8_t type;            // 0x02
    uint8_t reserved[3];     // {0, 0, 0}
    uint32_t pos_x;
    uint32_t pos_y;
    int32_t hscroll;
    int32_t vscroll;
};
static_assert(sizeof(ScrollEventPacket) == 20, "ScrollEventPacket must be 20 bytes");

struct ClipboardHeaderPacket {
    uint8_t type;            // 0x03
    uint8_t paste_flag;      // 0x00 or 0x01
    uint16_t reserved;       // 0x0000
    uint32_t length;
};
static_assert(sizeof(ClipboardHeaderPacket) == 8, "ClipboardHeaderPacket must be 8 bytes");

struct ScreenPowerPacket {
    uint8_t type;            // 0x04
    uint8_t power_mode;      // 0x00=OFF, 0x02=NORMAL
};
static_assert(sizeof(ScreenPowerPacket) == 2, "ScreenPowerPacket must be 2 bytes");

struct KeyframeRequestPacket {
    uint8_t type;            // 0x05
};
static_assert(sizeof(KeyframeRequestPacket) == 1, "KeyframeRequestPacket must be 1 byte");

struct HeartbeatPingPacket {
    uint8_t type;            // 0x06
    uint32_t ping_id;
};
static_assert(sizeof(HeartbeatPingPacket) == 5, "HeartbeatPingPacket must be 5 bytes");

#pragma pack(pop)

} // namespace mirror::protocol

namespace mirror {
using mirror::protocol::ControlMessageType;
using mirror::protocol::TouchEventPacket;
using mirror::protocol::KeyEventPacket;
using mirror::protocol::ScrollEventPacket;
using mirror::protocol::ClipboardHeaderPacket;
using mirror::protocol::ScreenPowerPacket;
using mirror::protocol::KeyframeRequestPacket;
using mirror::protocol::HeartbeatPingPacket;
using mirror::protocol::MAX_CLIPBOARD_LENGTH;
using mirror::protocol::TOUCH_ACTION_DOWN;
using mirror::protocol::TOUCH_ACTION_UP;
using mirror::protocol::TOUCH_ACTION_MOVE;
using mirror::protocol::TOUCH_ACTION_CANCEL;
using mirror::protocol::KEY_ACTION_DOWN;
using mirror::protocol::KEY_ACTION_UP;
using mirror::protocol::SCREEN_POWER_MODE_OFF;
using mirror::protocol::SCREEN_POWER_MODE_NORMAL;
}

using mirror::protocol::ControlMessageType;
using mirror::protocol::TouchEventPacket;
using mirror::protocol::KeyEventPacket;
using mirror::protocol::ScrollEventPacket;
using mirror::protocol::ClipboardHeaderPacket;
using mirror::protocol::ScreenPowerPacket;
using mirror::protocol::KeyframeRequestPacket;
using mirror::protocol::HeartbeatPingPacket;
