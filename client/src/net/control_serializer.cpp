#include "net/control_serializer.hpp"
#include "net/socket_client.hpp"

namespace mirror::net {

using namespace mirror::protocol;

bool ControlSerializer::send_touch(int fd, uint8_t action, uint64_t pointer_id,
                                   uint32_t x, uint32_t y, uint16_t w, uint16_t h,
                                   uint16_t pressure, uint16_t buttons) {
    if (fd < 0) {
        return false;
    }

    TouchEventPacket pkt{};
    pkt.type = static_cast<uint8_t>(ControlMessageType::InjectTouch);
    pkt.action = action;
    pkt.reserved = 0;
    pkt.pointer_id = protocol::to_big_endian(pointer_id);
    pkt.pos_x = protocol::to_big_endian(x);
    pkt.pos_y = protocol::to_big_endian(y);
    pkt.canvas_width = protocol::to_big_endian(w);
    pkt.canvas_height = protocol::to_big_endian(h);
    pkt.pressure = protocol::to_big_endian(pressure);
    pkt.buttons = protocol::to_big_endian(buttons);

    return SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

bool ControlSerializer::send_key(int fd, uint8_t action, uint32_t keycode, uint32_t meta) {
    if (fd < 0) {
        return false;
    }

    KeyEventPacket pkt{};
    pkt.type = static_cast<uint8_t>(ControlMessageType::InjectKey);
    pkt.action = action;
    pkt.reserved = 0;
    pkt.key_code = protocol::to_big_endian(keycode);
    pkt.meta_state = protocol::to_big_endian(meta);

    return SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

bool ControlSerializer::send_scroll(int fd, uint32_t x, uint32_t y, int32_t hscroll, int32_t vscroll) {
    if (fd < 0) {
        return false;
    }

    ScrollEventPacket pkt{};
    pkt.type = static_cast<uint8_t>(ControlMessageType::InjectScroll);
    pkt.reserved[0] = 0;
    pkt.reserved[1] = 0;
    pkt.reserved[2] = 0;
    pkt.pos_x = protocol::to_big_endian(x);
    pkt.pos_y = protocol::to_big_endian(y);
    pkt.hscroll = protocol::to_big_endian(hscroll);
    pkt.vscroll = protocol::to_big_endian(vscroll);

    return SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

bool ControlSerializer::send_clipboard(int fd, std::string_view text, bool paste_now) {
    if (fd < 0 || text.size() > MAX_CLIPBOARD_LENGTH) {
        return false;
    }

    ClipboardHeaderPacket hdr{};
    hdr.type = static_cast<uint8_t>(ControlMessageType::SetClipboard);
    hdr.paste_flag = paste_now ? 1 : 0;
    hdr.reserved = 0;
    hdr.length = protocol::to_big_endian(static_cast<uint32_t>(text.size()));

    if (!SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(&hdr), sizeof(hdr))) {
        return false;
    }

    if (!text.empty()) {
        return SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }

    return true;
}

bool ControlSerializer::send_screen_power(int fd, bool power_on) {
    if (fd < 0) {
        return false;
    }

    ScreenPowerPacket pkt{};
    pkt.type = static_cast<uint8_t>(ControlMessageType::SetScreenPower);
    pkt.power_mode = power_on ? SCREEN_POWER_MODE_NORMAL : SCREEN_POWER_MODE_OFF;

    return SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

bool ControlSerializer::send_keyframe_request(int fd) {
    if (fd < 0) {
        return false;
    }

    KeyframeRequestPacket pkt{};
    pkt.type = static_cast<uint8_t>(ControlMessageType::RequestKeyframe);

    return SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

bool ControlSerializer::send_heartbeat(int fd, uint32_t ping_id) {
    if (fd < 0) {
        return false;
    }

    HeartbeatPingPacket pkt{};
    pkt.type = static_cast<uint8_t>(ControlMessageType::HeartbeatPing);
    pkt.ping_id = protocol::to_big_endian(ping_id);

    return SocketClient::write_exact(fd, reinterpret_cast<const uint8_t*>(&pkt), sizeof(pkt));
}

} // namespace mirror::net
