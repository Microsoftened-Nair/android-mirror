#include "protocol/video_packet.hpp"
#include "video/video_demuxer.hpp"
#include "net/socket_client.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace mirror;
using namespace mirror::protocol;
using namespace mirror::video;
using namespace mirror::net;

void test_packet_layout_and_nal_parsing() {
    // 1. Static assertions
    static_assert(sizeof(VideoPacketHeader) == 12, "VideoPacketHeader size mismatch");
    static_assert(offsetof(VideoPacketHeader, pts_us) == 0, "pts_us offset mismatch");
    static_assert(offsetof(VideoPacketHeader, payload_len) == 8, "payload_len offset mismatch");

    // 2. NAL classification with 4-byte start codes
    const uint8_t sps_payload[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1F}; // 0x67 & 0x1F == 7
    assert(parse_nal_unit_type(sps_payload, sizeof(sps_payload)) == NalUnitType::Sps);

    const uint8_t pps_payload[] = {0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x38, 0x80}; // 0x68 & 0x1F == 8
    assert(parse_nal_unit_type(pps_payload, sizeof(pps_payload)) == NalUnitType::Pps);

    const uint8_t idr_payload[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00}; // 0x65 & 0x1F == 5
    assert(parse_nal_unit_type(idr_payload, sizeof(idr_payload)) == NalUnitType::Idr);

    const uint8_t non_idr_payload[] = {0x00, 0x00, 0x00, 0x01, 0x41, 0x9A}; // 0x41 & 0x1F == 1
    assert(parse_nal_unit_type(non_idr_payload, sizeof(non_idr_payload)) == NalUnitType::NonIdr);

    // 3. NAL classification with 3-byte start code
    const uint8_t idr_3byte[] = {0x00, 0x00, 0x01, 0x65, 0x88};
    assert(parse_nal_unit_type(idr_3byte, sizeof(idr_3byte)) == NalUnitType::Idr);

    // 4. Invalid or unknown
    const uint8_t bad_payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    assert(parse_nal_unit_type(bad_payload, sizeof(bad_payload)) == NalUnitType::Other);
    assert(parse_nal_unit_type(nullptr, 0) == NalUnitType::Other);

    // 5. Byte order roundtrip
    VideoPacketHeader hdr{
        .pts_us = 0x1122334455667788ULL,
        .payload_len = 0xAABBCCDD
    };
    VideoPacketHeader net_hdr = to_network_order(hdr);
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&net_hdr);
    assert(raw[0] == 0x11 && raw[1] == 0x22 && raw[7] == 0x88);
    assert(raw[8] == 0xAA && raw[9] == 0xBB && raw[11] == 0xDD);

    VideoPacketHeader roundtrip = from_network_order(net_hdr);
    assert(roundtrip.pts_us == hdr.pts_us);
    assert(roundtrip.payload_len == hdr.payload_len);

    std::cout << "[PASS] test_packet_layout_and_nal_parsing" << std::endl;
}

void test_demuxer_streaming() {
    int sv[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    assert(ret == 0);

    // Write a stream of packets on sv[0]
    std::thread sender([writer_fd = sv[0]]() {
        // Frame 1: SPS (20 bytes)
        std::vector<uint8_t> sps(20, 0xAA);
        sps[0] = 0x00; sps[1] = 0x00; sps[2] = 0x00; sps[3] = 0x01; sps[4] = 0x67;
        VideoPacketHeader h1{ .pts_us = 1000000ULL, .payload_len = static_cast<uint32_t>(sps.size()) };
        VideoPacketHeader net_h1 = to_network_order(h1);
        SocketClient::write_exact(writer_fd, reinterpret_cast<const uint8_t*>(&net_h1), sizeof(net_h1));
        SocketClient::write_exact(writer_fd, sps.data(), sps.size());

        // Frame 2: IDR (100 bytes)
        std::vector<uint8_t> idr(100, 0xBB);
        idr[0] = 0x00; idr[1] = 0x00; idr[2] = 0x00; idr[3] = 0x01; idr[4] = 0x65;
        VideoPacketHeader h2{ .pts_us = 1033333ULL, .payload_len = static_cast<uint32_t>(idr.size()) };
        VideoPacketHeader net_h2 = to_network_order(h2);
        SocketClient::write_exact(writer_fd, reinterpret_cast<const uint8_t*>(&net_h2), sizeof(net_h2));
        SocketClient::write_exact(writer_fd, idr.data(), idr.size());

        close(writer_fd);
    });

    VideoDemuxer demuxer(sv[1], 1024 * 1024);

    VideoPacketHeader out_hdr{};
    std::span<const uint8_t> out_payload{};

    // Read packet 1
    bool ok = demuxer.read_next_packet(out_hdr, out_payload);
    assert(ok);
    assert(out_hdr.pts_us == 1000000ULL);
    assert(out_hdr.payload_len == 20);
    assert(out_payload.size() == 20);
    assert(parse_nal_unit_type(out_payload.data(), out_payload.size()) == NalUnitType::Sps);

    // Read packet 2
    ok = demuxer.read_next_packet(out_hdr, out_payload);
    assert(ok);
    assert(out_hdr.pts_us == 1033333ULL);
    assert(out_hdr.payload_len == 100);
    assert(out_payload.size() == 100);
    assert(parse_nal_unit_type(out_payload.data(), out_payload.size()) == NalUnitType::Idr);

    // End of stream
    ok = demuxer.read_next_packet(out_hdr, out_payload);
    assert(!ok);

    sender.join();
    close(sv[1]);
    std::cout << "[PASS] test_demuxer_streaming" << std::endl;
}

void test_demuxer_error_handling() {
    int sv[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    assert(ret == 0);

    // 1. Buffer overrun protection
    VideoDemuxer demuxer(sv[1], 512); // Small max buffer size

    std::thread sender([writer_fd = sv[0]]() {
        // Packet claiming payload_len of 1000 bytes (> max_buffer_size of 512)
        VideoPacketHeader h{ .pts_us = 100, .payload_len = 1000 };
        VideoPacketHeader net_h = to_network_order(h);
        SocketClient::write_exact(writer_fd, reinterpret_cast<const uint8_t*>(&net_h), sizeof(net_h));

        close(writer_fd);
    });

    VideoPacketHeader out_hdr{};
    std::span<const uint8_t> out_payload{};
    bool ok = demuxer.read_next_packet(out_hdr, out_payload);
    assert(!ok); // Must fail due to buffer overrun guard

    sender.join();
    close(sv[1]);

    // 2. Stream corruption: Invalid Annex B start code
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    assert(ret == 0);

    VideoDemuxer demuxer2(sv[1], 512);

    std::thread sender2([writer_fd = sv[0]]() {
        // Valid header, but payload does not begin with 00 00 00 01
        VideoPacketHeader h{ .pts_us = 200, .payload_len = 8 };
        VideoPacketHeader net_h = to_network_order(h);
        SocketClient::write_exact(writer_fd, reinterpret_cast<const uint8_t*>(&net_h), sizeof(net_h));

        uint8_t bad_annex_b[] = {0x00, 0x00, 0x02, 0x01, 0x65, 0x00, 0x00, 0x00};
        SocketClient::write_exact(writer_fd, bad_annex_b, sizeof(bad_annex_b));

        close(writer_fd);
    });

    ok = demuxer2.read_next_packet(out_hdr, out_payload);
    assert(!ok); // Must fail due to invalid start code delimiter

    sender2.join();
    close(sv[1]);

    std::cout << "[PASS] test_demuxer_error_handling" << std::endl;
}

int main() {
    test_packet_layout_and_nal_parsing();
    test_demuxer_streaming();
    test_demuxer_error_handling();

    std::cout << "All video demuxer tests passed successfully!" << std::endl;
    return 0;
}
