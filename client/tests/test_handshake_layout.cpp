#include "protocol/handshake.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>

int main() {
    // 1. Compile-time size assertions
    static_assert(sizeof(DeviceIdPacket) == 68, "DeviceIdPacket size mismatch");
    static_assert(sizeof(VideoGeometryPacket) == 12, "VideoGeometryPacket size mismatch");
    static_assert(sizeof(ClientAckPacket) == 8, "ClientAckPacket size mismatch");

    // 2. Alignment assertions (#pragma pack(1))
    static_assert(alignof(DeviceIdPacket) == 1, "DeviceIdPacket alignment mismatch");
    static_assert(alignof(VideoGeometryPacket) == 1, "VideoGeometryPacket alignment mismatch");
    static_assert(alignof(ClientAckPacket) == 1, "ClientAckPacket alignment mismatch");

    // 3. Offset assertions
    static_assert(offsetof(DeviceIdPacket, magic) == 0, "DeviceIdPacket.magic offset mismatch");
    static_assert(offsetof(DeviceIdPacket, device_name) == 4, "DeviceIdPacket.device_name offset mismatch");

    static_assert(offsetof(VideoGeometryPacket, codec_id) == 0, "VideoGeometryPacket.codec_id offset mismatch");
    static_assert(offsetof(VideoGeometryPacket, width) == 4, "VideoGeometryPacket.width offset mismatch");
    static_assert(offsetof(VideoGeometryPacket, height) == 8, "VideoGeometryPacket.height offset mismatch");

    static_assert(offsetof(ClientAckPacket, version) == 0, "ClientAckPacket.version offset mismatch");
    static_assert(offsetof(ClientAckPacket, flags) == 4, "ClientAckPacket.flags offset mismatch");

    // 4. Runtime assertions for constants
    assert(MAGIC_VALUE[0] == 0x4D);
    assert(MAGIC_VALUE[1] == 0x49);
    assert(MAGIC_VALUE[2] == 0x52);
    assert(MAGIC_VALUE[3] == 0x52);
    assert(CODEC_H264 == 1);
    assert(CLIENT_VERSION == 1);
    assert(FLAG_SCREEN_OFF == (1u << 0));
    assert(FLAG_STAY_AWAKE == (1u << 1));

    // 5. Runtime test: Network byte order for VideoGeometryPacket
    VideoGeometryPacket geom{
        .codec_id = CODEC_H264,
        .width = 1920,
        .height = 1080
    };

    VideoGeometryPacket net_geom = to_network_order(geom);
    const uint8_t* raw_geom = reinterpret_cast<const uint8_t*>(&net_geom);

    // Expected big-endian byte representations
    // codec_id: 1 -> 0x00, 0x00, 0x00, 0x01
    assert(raw_geom[0] == 0x00 && raw_geom[1] == 0x00 && raw_geom[2] == 0x00 && raw_geom[3] == 0x01);
    // width: 1920 (0x0780) -> 0x00, 0x00, 0x07, 0x80
    assert(raw_geom[4] == 0x00 && raw_geom[5] == 0x00 && raw_geom[6] == 0x07 && raw_geom[7] == 0x80);
    // height: 1080 (0x0438) -> 0x00, 0x00, 0x04, 0x38
    assert(raw_geom[8] == 0x00 && raw_geom[9] == 0x00 && raw_geom[10] == 0x04 && raw_geom[11] == 0x38);

    VideoGeometryPacket roundtrip_geom = from_network_order(net_geom);
    assert(roundtrip_geom.codec_id == geom.codec_id);
    assert(roundtrip_geom.width == geom.width);
    assert(roundtrip_geom.height == geom.height);

    // 6. Runtime test: Network byte order for ClientAckPacket
    ClientAckPacket ack{
        .version = CLIENT_VERSION,
        .flags = FLAG_SCREEN_OFF | FLAG_STAY_AWAKE
    };

    ClientAckPacket net_ack = to_network_order(ack);
    const uint8_t* raw_ack = reinterpret_cast<const uint8_t*>(&net_ack);

    // version: 1 -> 0x00, 0x00, 0x00, 0x01
    assert(raw_ack[0] == 0x00 && raw_ack[1] == 0x00 && raw_ack[2] == 0x00 && raw_ack[3] == 0x01);
    // flags: 3 -> 0x00, 0x00, 0x00, 0x03
    assert(raw_ack[4] == 0x00 && raw_ack[5] == 0x00 && raw_ack[6] == 0x00 && raw_ack[7] == 0x03);

    ClientAckPacket roundtrip_ack = from_network_order(net_ack);
    assert(roundtrip_ack.version == ack.version);
    assert(roundtrip_ack.flags == ack.flags);

    std::cout << "All handshake layout assertions and tests passed successfully!" << std::endl;
    return 0;
}
