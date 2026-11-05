#include "net/socket_client.hpp"
#include "protocol/handshake.hpp"

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace mirror;
using namespace mirror::net;

// Helper to bind an ephemeral loopback TCP server
int create_server_socket(uint16_t& out_port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(server_fd >= 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // ephemeral

    int ret = bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    assert(ret == 0);

    socklen_t len = sizeof(addr);
    ret = getsockname(server_fd, reinterpret_cast<sockaddr*>(&addr), &len);
    assert(ret == 0);
    out_port = ntohs(addr.sin_port);

    ret = listen(server_fd, 2);
    assert(ret == 0);

    return server_fd;
}

void test_handshake_exchange() {
    uint16_t port = 0;
    int server_fd = create_server_socket(port);
    assert(port > 0);

    std::thread server_thread([server_fd]() {
        // Accept video connection
        int video_fd = accept(server_fd, nullptr, nullptr);
        assert(video_fd >= 0);

        // Accept control connection
        int control_fd = accept(server_fd, nullptr, nullptr);
        assert(control_fd >= 0);

        // Step 1: Send DeviceIdPacket
        DeviceIdPacket dev_pkt{};
        std::memcpy(dev_pkt.magic, MAGIC_VALUE, 4);
        std::strncpy(dev_pkt.device_name, "Google Pixel 8 Pro", sizeof(dev_pkt.device_name) - 1);
        bool ok = write_exact(video_fd, reinterpret_cast<const uint8_t*>(&dev_pkt), sizeof(dev_pkt));
        assert(ok);

        // Step 2: Send VideoGeometryPacket (in network byte order)
        VideoGeometryPacket geom_pkt{
            .codec_id = CODEC_H264,
            .width = 1440,
            .height = 3120
        };
        VideoGeometryPacket net_geom = to_network_order(geom_pkt);
        ok = write_exact(video_fd, reinterpret_cast<const uint8_t*>(&net_geom), sizeof(net_geom));
        assert(ok);

        // Step 3: Receive ClientAckPacket from control_fd
        ClientAckPacket ack_pkt{};
        ok = read_exact(control_fd, reinterpret_cast<uint8_t*>(&ack_pkt), sizeof(ack_pkt));
        assert(ok);
        ack_pkt = from_network_order(ack_pkt);

        assert(ack_pkt.version == CLIENT_VERSION);
        assert(ack_pkt.flags == (FLAG_SCREEN_OFF | FLAG_STAY_AWAKE));

        // Keep server sockets open for subsequent shutdown test
        char dummy[16];
        recv(video_fd, dummy, sizeof(dummy), 0);

        close(video_fd);
        close(control_fd);
        close(server_fd);
    });

    SocketClient client;
    bool connected = client.connect("127.0.0.1", port, FLAG_SCREEN_OFF | FLAG_STAY_AWAKE);
    assert(connected);
    assert(client.is_running());
    assert(client.device_name() == "Google Pixel 8 Pro");
    assert(client.codec_id() == CODEC_H264);
    assert(client.width() == 1440);
    assert(client.height() == 3120);
    assert(client.video_fd() >= 0);
    assert(client.control_fd() >= 0);

    // Test atomic shutdown unblocks blocking reader thread
    std::atomic<bool> thread_unblocked{false};
    std::thread reader_thread([&client, &thread_unblocked]() {
        uint8_t buf[16];
        // This read will block until client.disconnect() shuts down the descriptor
        bool res = SocketClient::read_exact(client.video_fd(), buf, sizeof(buf));
        assert(!res); // Must fail due to shutdown/close
        thread_unblocked.store(true);
    });

    // Brief delay to ensure reader thread is blocked in recv()
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(!thread_unblocked.load());

    // Disconnect client
    client.disconnect();
    assert(!client.is_running());
    assert(client.video_fd() == -1);
    assert(client.control_fd() == -1);

    reader_thread.join();
    assert(thread_unblocked.load());

    server_thread.join();
    std::cout << "[PASS] test_handshake_exchange and atomic shutdown" << std::endl;
}

void test_fragmented_read() {
    uint16_t port = 0;
    int server_fd = create_server_socket(port);
    assert(port > 0);

    const size_t total_payload = 1000;
    std::vector<uint8_t> expected_data(total_payload);
    for (size_t i = 0; i < total_payload; ++i) {
        expected_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    std::thread server_thread([server_fd, expected_data]() {
        int client_sock = accept(server_fd, nullptr, nullptr);
        assert(client_sock >= 0);

        // Send payload in small fragments of 37 bytes with micro delays
        size_t sent = 0;
        while (sent < expected_data.size()) {
            size_t chunk = std::min<size_t>(37, expected_data.size() - sent);
            send(client_sock, expected_data.data() + sent, chunk, 0);
            sent += chunk;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }

        close(client_sock);
        close(server_fd);
    });

    int client_sock = SocketClient::connect_tcp("127.0.0.1", port);
    assert(client_sock >= 0);

    std::vector<uint8_t> received_data(total_payload, 0);
    bool ok = SocketClient::read_exact(client_sock, received_data.data(), total_payload);
    assert(ok);
    assert(received_data == expected_data);

    SocketClient::close_socket(client_sock);
    server_thread.join();
    std::cout << "[PASS] test_fragmented_read across TCP boundaries" << std::endl;
}

void test_invalid_magic_rejection() {
    uint16_t port = 0;
    int server_fd = create_server_socket(port);

    std::thread server_thread([server_fd]() {
        int v_fd = accept(server_fd, nullptr, nullptr);
        int c_fd = accept(server_fd, nullptr, nullptr);

        // Send corrupt magic
        DeviceIdPacket bad_dev{};
        std::memcpy(bad_dev.magic, "BAD!", 4);
        write_exact(v_fd, reinterpret_cast<const uint8_t*>(&bad_dev), sizeof(bad_dev));

        close(v_fd);
        close(c_fd);
        close(server_fd);
    });

    SocketClient client;
    bool connected = client.connect("127.0.0.1", port, 0);
    assert(!connected);
    assert(!client.is_running());

    server_thread.join();
    std::cout << "[PASS] test_invalid_magic_rejection" << std::endl;
}

int main() {
    test_handshake_exchange();
    test_fragmented_read();
    test_invalid_magic_rejection();

    std::cout << "All SocketClient transport tests passed successfully!" << std::endl;
    return 0;
}
