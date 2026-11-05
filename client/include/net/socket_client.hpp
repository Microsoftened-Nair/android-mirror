#pragma once

#include "protocol/handshake.hpp"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace mirror::net {

/**
 * Low-level TCP socket utilities, exact byte stream readers/writers,
 * and session lifecycle orchestration.
 */
class SocketClient {
public:
    SocketClient();
    ~SocketClient();

    // Move-only semantics
    SocketClient(const SocketClient&) = delete;
    SocketClient& operator=(const SocketClient&) = delete;
    SocketClient(SocketClient&& other) noexcept;
    SocketClient& operator=(SocketClient&& other) noexcept;

    // --- Low-level TCP Socket APIs ---

    /**
     * Connects to a remote host and TCP port. Returns file descriptor or -1 on error.
     */
    static int connect_tcp(const char* host, uint16_t port);

    /**
     * Reads exactly `length` bytes from `fd` into `buffer`, handling TCP fragmentation.
     * Zero dynamic allocations. Returns false on premature EOF or unrecoverable error.
     */
    static bool read_exact(int fd, uint8_t* buffer, size_t length);

    /**
     * Writes exactly `length` bytes from `buffer` to `fd`.
     * Zero dynamic allocations. Returns false on failure.
     */
    static bool write_exact(int fd, const uint8_t* buffer, size_t length);

    /**
     * Safely closes the socket and resets `fd` to -1.
     */
    static void close_socket(int& fd);

    // --- Session Transport Orchestration ---

    /**
     * Connects dual TCP sockets (video and control) to host:port and performs handshake.
     */
    bool connect(const char* host, uint16_t port, uint32_t flags = 0);

    /**
     * Executes the 3-step handshake over existing video_fd_ and control_fd_:
     * 1. Read DeviceIdPacket (68 bytes), validate "MIRR", extract device_name.
     * 2. Read VideoGeometryPacket (12 bytes), parse codec_id, width, height.
     * 3. Transmit ClientAckPacket (8 bytes) with version 1 and configured flags.
     */
    bool execute_handshake(uint32_t flags);

    /**
     * Unblocks reader/writer threads via shutdown(SHUT_RDWR), closes descriptors,
     * and sets is_running to false.
     */
    void disconnect();

    // Accessors
    [[nodiscard]] int video_fd() const noexcept { return video_fd_; }
    [[nodiscard]] int control_fd() const noexcept { return control_fd_; }
    [[nodiscard]] bool is_running() const noexcept { return is_running_.load(); }
    [[nodiscard]] const std::string& device_name() const noexcept { return device_name_; }
    [[nodiscard]] uint32_t codec_id() const noexcept { return codec_id_; }
    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }

    void set_fds(int video_fd, int control_fd) noexcept {
        video_fd_ = video_fd;
        control_fd_ = control_fd;
    }

private:
    int video_fd_{-1};
    int control_fd_{-1};
    std::atomic<bool> is_running_{false};

    std::string device_name_;
    uint32_t codec_id_{0};
    uint32_t width_{0};
    uint32_t height_{0};
};

// Standalone C-style / POSIX helper wrappers
inline int connect_tcp(const char* host, uint16_t port) {
    return SocketClient::connect_tcp(host, port);
}

inline bool read_exact(int fd, uint8_t* buffer, size_t length) {
    return SocketClient::read_exact(fd, buffer, length);
}

inline bool write_exact(int fd, const uint8_t* buffer, size_t length) {
    return SocketClient::write_exact(fd, buffer, length);
}

inline void close_socket(int& fd) {
    SocketClient::close_socket(fd);
}

} // namespace mirror::net

namespace mirror {
using mirror::net::SocketClient;
using mirror::net::connect_tcp;
using mirror::net::read_exact;
using mirror::net::write_exact;
using mirror::net::close_socket;
}

using mirror::net::SocketClient;
using mirror::net::connect_tcp;
using mirror::net::read_exact;
using mirror::net::write_exact;
using mirror::net::close_socket;
