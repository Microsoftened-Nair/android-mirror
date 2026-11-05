#pragma once

#include "protocol/endian.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mirror::protocol {

inline constexpr uint8_t ANNEX_B_START_CODE[4] = {0x00, 0x00, 0x00, 0x01};

#pragma pack(push, 1)
/**
 * 12-byte binary video stream packet header.
 * Transmitted in Network Byte Order (Big-Endian).
 */
struct VideoPacketHeader {
    uint64_t pts_us;        // Microsecond presentation timestamp
    uint32_t payload_len;   // Byte length of following Annex B payload
};
#pragma pack(pop)

static_assert(sizeof(VideoPacketHeader) == 12, "VideoPacketHeader must be 12 bytes");

enum class NalUnitType : uint8_t {
    Other = 0,
    NonIdr = 1,
    Idr = 5,
    Sps = 7,
    Pps = 8
};

[[nodiscard]] inline VideoPacketHeader to_network_order(VideoPacketHeader hdr) noexcept {
    hdr.pts_us = to_big_endian(hdr.pts_us);
    hdr.payload_len = to_big_endian(hdr.payload_len);
    return hdr;
}

[[nodiscard]] inline VideoPacketHeader from_network_order(VideoPacketHeader hdr) noexcept {
    hdr.pts_us = from_big_endian(hdr.pts_us);
    hdr.payload_len = from_big_endian(hdr.payload_len);
    return hdr;
}

/**
 * Parses the H.264 NAL unit type from an Annex B payload.
 * Expects a 4-byte (0x00, 0x00, 0x00, 0x01) or 3-byte (0x00, 0x00, 0x01) start code prefix.
 */
inline NalUnitType parse_nal_unit_type(const uint8_t* payload, size_t size) noexcept {
    if (!payload) {
        return NalUnitType::Other;
    }

    size_t header_offset = 0;
    if (size >= 5 && payload[0] == 0x00 && payload[1] == 0x00 && payload[2] == 0x00 && payload[3] == 0x01) {
        header_offset = 4;
    } else if (size >= 4 && payload[0] == 0x00 && payload[1] == 0x00 && payload[2] == 0x01) {
        header_offset = 3;
    } else {
        return NalUnitType::Other;
    }

    uint8_t nal_type_raw = payload[header_offset] & 0x1F;
    switch (nal_type_raw) {
        case 1:
            return NalUnitType::NonIdr;
        case 5:
            return NalUnitType::Idr;
        case 7:
            return NalUnitType::Sps;
        case 8:
            return NalUnitType::Pps;
        default:
            return NalUnitType::Other;
    }
}

} // namespace mirror::protocol

namespace mirror {
using mirror::protocol::VideoPacketHeader;
using mirror::protocol::NalUnitType;
using mirror::protocol::parse_nal_unit_type;
using mirror::protocol::to_network_order;
using mirror::protocol::from_network_order;
using mirror::protocol::ANNEX_B_START_CODE;
}

using mirror::protocol::VideoPacketHeader;
using mirror::protocol::NalUnitType;
using mirror::protocol::parse_nal_unit_type;
