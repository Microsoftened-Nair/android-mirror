#include "protocol/control_packet.hpp"
#include "net/control_serializer.hpp"
#include "net/socket_client.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace mirror;
using namespace mirror::protocol;
using namespace mirror::net;

void test_packet_sizes_and_layouts() {
    // 1. Size assertions
    static_assert(sizeof(TouchEventPacket) == 28, "TouchEventPacket must be 28 bytes");
    static_assert(sizeof(KeyEventPacket) == 12, "KeyEventPacket must be 12 bytes");
    static_assert(sizeof(ScrollEventPacket) == 20, "ScrollEventPacket must be 20 bytes");
    static_assert(sizeof(ClipboardHeaderPacket) == 8, "ClipboardHeaderPacket must be 8 bytes");
    static_assert(sizeof(ScreenPowerPacket) == 2, "ScreenPowerPacket must be 2 bytes");
    static_assert(sizeof(KeyframeRequestPacket) == 1, "KeyframeRequestPacket must be 1 byte");
    static_assert(sizeof(HeartbeatPingPacket) == 5, "HeartbeatPingPacket must be 5 bytes");

    // 2. Alignment assertions (#pragma pack(1))
    static_assert(alignof(TouchEventPacket) == 1, "TouchEventPacket alignment mismatch");
    static_assert(alignof(KeyEventPacket) == 1, "KeyEventPacket alignment mismatch");
    static_assert(alignof(ScrollEventPacket) == 1, "ScrollEventPacket alignment mismatch");
    static_assert(alignof(ClipboardHeaderPacket) == 1, "ClipboardHeaderPacket alignment mismatch");
    static_assert(alignof(ScreenPowerPacket) == 1, "ScreenPowerPacket alignment mismatch");
    static_assert(alignof(KeyframeRequestPacket) == 1, "KeyframeRequestPacket alignment mismatch");
    static_assert(alignof(HeartbeatPingPacket) == 1, "HeartbeatPingPacket alignment mismatch");

    // 3. Offset assertions
    static_assert(offsetof(TouchEventPacket, type) == 0);
    static_assert(offsetof(TouchEventPacket, action) == 1);
    static_assert(offsetof(TouchEventPacket, reserved) == 2);
    static_assert(offsetof(TouchEventPacket, pointer_id) == 4);
    static_assert(offsetof(TouchEventPacket, pos_x) == 12);
    static_assert(offsetof(TouchEventPacket, pos_y) == 16);
    static_assert(offsetof(TouchEventPacket, canvas_width) == 20);
    static_assert(offsetof(TouchEventPacket, canvas_height) == 22);
    static_assert(offsetof(TouchEventPacket, pressure) == 24);
    static_assert(offsetof(TouchEventPacket, buttons) == 26);

    static_assert(offsetof(KeyEventPacket, type) == 0);
    static_assert(offsetof(KeyEventPacket, action) == 1);
    static_assert(offsetof(KeyEventPacket, reserved) == 2);
    static_assert(offsetof(KeyEventPacket, key_code) == 4);
    static_assert(offsetof(KeyEventPacket, meta_state) == 8);

    static_assert(offsetof(ScrollEventPacket, type) == 0);
    static_assert(offsetof(ScrollEventPacket, reserved) == 1);
    static_assert(offsetof(ScrollEventPacket, pos_x) == 4);
    static_assert(offsetof(ScrollEventPacket, pos_y) == 8);
    static_assert(offsetof(ScrollEventPacket, hscroll) == 12);
    static_assert(offsetof(ScrollEventPacket, vscroll) == 16);

    static_assert(offsetof(ClipboardHeaderPacket, type) == 0);
    static_assert(offsetof(ClipboardHeaderPacket, paste_flag) == 1);
    static_assert(offsetof(ClipboardHeaderPacket, reserved) == 2);
    static_assert(offsetof(ClipboardHeaderPacket, length) == 4);

    static_assert(offsetof(ScreenPowerPacket, type) == 0);
    static_assert(offsetof(ScreenPowerPacket, power_mode) == 1);

    static_assert(offsetof(KeyframeRequestPacket, type) == 0);

    static_assert(offsetof(HeartbeatPingPacket, type) == 0);
    static_assert(offsetof(HeartbeatPingPacket, ping_id) == 1);

    std::cout << "[PASS] test_packet_sizes_and_layouts" << std::endl;
}

void test_serializer_dispatch() {
    int sv[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    assert(ret == 0);

    int write_fd = sv[0];
    int read_fd = sv[1];

    ControlSerializer serializer(write_fd);

    // 1. Test Touch Event
    bool ok = serializer.send_touch(TOUCH_ACTION_DOWN, 0x0102030405060708ULL, 100, 200, 1080, 2400, 0x8000, 0x0001);
    assert(ok);

    TouchEventPacket rec_touch{};
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_touch), sizeof(rec_touch));
    assert(ok);
    assert(rec_touch.type == static_cast<uint8_t>(ControlMessageType::InjectTouch));
    assert(rec_touch.action == TOUCH_ACTION_DOWN);
    assert(rec_touch.reserved == 0);
    assert(protocol::from_big_endian(rec_touch.pointer_id) == 0x0102030405060708ULL);
    assert(protocol::from_big_endian(rec_touch.pos_x) == 100);
    assert(protocol::from_big_endian(rec_touch.pos_y) == 200);
    assert(protocol::from_big_endian(rec_touch.canvas_width) == 1080);
    assert(protocol::from_big_endian(rec_touch.canvas_height) == 2400);
    assert(protocol::from_big_endian(rec_touch.pressure) == 0x8000);
    assert(protocol::from_big_endian(rec_touch.buttons) == 0x0001);

    // 2. Test Key Event
    ok = serializer.send_key(KEY_ACTION_UP, 66, 0x00000001); // KEYCODE_ENTER
    assert(ok);

    KeyEventPacket rec_key{};
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_key), sizeof(rec_key));
    assert(ok);
    assert(rec_key.type == static_cast<uint8_t>(ControlMessageType::InjectKey));
    assert(rec_key.action == KEY_ACTION_UP);
    assert(rec_key.reserved == 0);
    assert(protocol::from_big_endian(rec_key.key_code) == 66);
    assert(protocol::from_big_endian(rec_key.meta_state) == 1);

    // 3. Test Scroll Event
    ok = serializer.send_scroll(500, 600, -5, 10);
    assert(ok);

    ScrollEventPacket rec_scroll{};
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_scroll), sizeof(rec_scroll));
    assert(ok);
    assert(rec_scroll.type == static_cast<uint8_t>(ControlMessageType::InjectScroll));
    assert(protocol::from_big_endian(rec_scroll.pos_x) == 500);
    assert(protocol::from_big_endian(rec_scroll.pos_y) == 600);
    assert(protocol::from_big_endian(rec_scroll.hscroll) == -5);
    assert(protocol::from_big_endian(rec_scroll.vscroll) == 10);

    // 4. Test Clipboard Event
    std::string clip_text = "Copy/Paste Test String 123!";
    ok = serializer.send_clipboard(clip_text, true);
    assert(ok);

    ClipboardHeaderPacket rec_clip_hdr{};
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_clip_hdr), sizeof(rec_clip_hdr));
    assert(ok);
    assert(rec_clip_hdr.type == static_cast<uint8_t>(ControlMessageType::SetClipboard));
    assert(rec_clip_hdr.paste_flag == 1);
    assert(protocol::from_big_endian(rec_clip_hdr.length) == clip_text.size());

    std::vector<char> clip_buf(clip_text.size());
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(clip_buf.data()), clip_buf.size());
    assert(ok);
    assert(std::string(clip_buf.data(), clip_buf.size()) == clip_text);

    // 5. Test Screen Power Event
    ok = serializer.send_screen_power(false);
    assert(ok);
    ScreenPowerPacket rec_power{};
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_power), sizeof(rec_power));
    assert(ok);
    assert(rec_power.type == static_cast<uint8_t>(ControlMessageType::SetScreenPower));
    assert(rec_power.power_mode == SCREEN_POWER_MODE_OFF);

    ok = serializer.send_screen_power(true);
    assert(ok);
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_power), sizeof(rec_power));
    assert(ok);
    assert(rec_power.type == static_cast<uint8_t>(ControlMessageType::SetScreenPower));
    assert(rec_power.power_mode == SCREEN_POWER_MODE_NORMAL);

    // 6. Test Keyframe Request Event
    ok = serializer.send_keyframe_request();
    assert(ok);
    KeyframeRequestPacket rec_keyframe{};
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_keyframe), sizeof(rec_keyframe));
    assert(ok);
    assert(rec_keyframe.type == static_cast<uint8_t>(ControlMessageType::RequestKeyframe));

    // 7. Test Heartbeat Ping Event
    ok = serializer.send_heartbeat(12345678);
    assert(ok);
    HeartbeatPingPacket rec_heartbeat{};
    ok = SocketClient::read_exact(read_fd, reinterpret_cast<uint8_t*>(&rec_heartbeat), sizeof(rec_heartbeat));
    assert(ok);
    assert(rec_heartbeat.type == static_cast<uint8_t>(ControlMessageType::HeartbeatPing));
    assert(protocol::from_big_endian(rec_heartbeat.ping_id) == 12345678);

    close(write_fd);
    close(read_fd);
    std::cout << "[PASS] test_serializer_dispatch" << std::endl;
}

void test_clipboard_oversized_rejection() {
    int sv[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    assert(ret == 0);

    // Create string of MAX_CLIPBOARD_LENGTH + 1 bytes (256 KB + 1)
    std::string huge_text(MAX_CLIPBOARD_LENGTH + 1, 'X');
    bool ok = ControlSerializer::send_clipboard(sv[0], huge_text, false);
    assert(!ok); // Must be rejected

    // Test valid string within limit
    std::string max_valid_text(1024, 'Y');
    ok = ControlSerializer::send_clipboard(sv[0], max_valid_text, false);
    assert(ok);

    close(sv[0]);
    close(sv[1]);
    std::cout << "[PASS] test_clipboard_oversized_rejection" << std::endl;
}

int main() {
    test_packet_sizes_and_layouts();
    test_serializer_dispatch();
    test_clipboard_oversized_rejection();

    std::cout << "All control packet and serializer tests passed successfully!" << std::endl;
    return 0;
}
