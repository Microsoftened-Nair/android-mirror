#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <bit>

namespace mirror {

// Magic identifier "MIRR"
inline constexpr uint8_t MAGIC_VALUE[4] = {0x4D, 0x49, 0x52, 0x52};

// Supported video codec identifiers
inline constexpr uint32_t CODEC_H264 = 1;

// Client handshake protocol version
inline constexpr uint32_t CLIENT_VERSION = 1;

// Client ACK flag bitfields
inline constexpr uint32_t FLAG_SCREEN_OFF = (1u << 0);
inline constexpr uint32_t FLAG_STAY_AWAKE = (1u << 1);

#pragma pack(push, 1)

/**
 * 68-byte Device Identification Block sent by Server -> Client over Video Socket.
 */
struct DeviceIdPacket {
    uint8_t magic[4];
    char device_name[64];
};
static_assert(sizeof(DeviceIdPacket) == 68, "DeviceIdPacket must be exactly 68 bytes");

/**
 * 12-byte Stream Geometry Block sent by Server -> Client over Video Socket.
 * All uint32 fields are transmitted in Network Byte Order (Big-Endian).
 */
struct VideoGeometryPacket {
    uint32_t codec_id;
    uint32_t width;
    uint32_t height;
};
static_assert(sizeof(VideoGeometryPacket) == 12, "VideoGeometryPacket must be exactly 12 bytes");

/**
 * 8-byte Client Acknowledgment Block sent by Client -> Server over Control Socket.
 * All uint32 fields are transmitted in Network Byte Order (Big-Endian).
 */
struct ClientAckPacket {
    uint32_t version;
    uint32_t flags;
};
static_assert(sizeof(ClientAckPacket) == 8, "ClientAckPacket must be exactly 8 bytes");

#pragma pack(pop)

// Endian conversion helper functions
[[nodiscard]] constexpr uint32_t to_big_endian(uint32_t val) noexcept {
    if constexpr (std::endian::native == std::endian::big) {
        return val;
    } else {
#if __cpp_lib_byteswap >= 202110L
        return std::byteswap(val);
#else
        return __builtin_bswap32(val);
#endif
    }
}

[[nodiscard]] constexpr uint32_t from_big_endian(uint32_t val) noexcept {
    return to_big_endian(val);
}

// Zero-allocation serialization/deserialization helpers
[[nodiscard]] inline VideoGeometryPacket to_network_order(VideoGeometryPacket pkt) noexcept {
    pkt.codec_id = to_big_endian(pkt.codec_id);
    pkt.width = to_big_endian(pkt.width);
    pkt.height = to_big_endian(pkt.height);
    return pkt;
}

[[nodiscard]] inline VideoGeometryPacket from_network_order(VideoGeometryPacket pkt) noexcept {
    pkt.codec_id = from_big_endian(pkt.codec_id);
    pkt.width = from_big_endian(pkt.width);
    pkt.height = from_big_endian(pkt.height);
    return pkt;
}

[[nodiscard]] inline ClientAckPacket to_network_order(ClientAckPacket pkt) noexcept {
    pkt.version = to_big_endian(pkt.version);
    pkt.flags = to_big_endian(pkt.flags);
    return pkt;
}

[[nodiscard]] inline ClientAckPacket from_network_order(ClientAckPacket pkt) noexcept {
    pkt.version = from_big_endian(pkt.version);
    pkt.flags = from_big_endian(pkt.flags);
    return pkt;
}

} // namespace mirror

// Unqualified aliases for direct global namespace usage
using mirror::MAGIC_VALUE;
using mirror::CODEC_H264;
using mirror::CLIENT_VERSION;
using mirror::FLAG_SCREEN_OFF;
using mirror::FLAG_STAY_AWAKE;
using mirror::DeviceIdPacket;
using mirror::VideoGeometryPacket;
using mirror::ClientAckPacket;
using mirror::to_big_endian;
using mirror::from_big_endian;
using mirror::to_network_order;
using mirror::from_network_order;
