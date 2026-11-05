#include "net/socket_client.hpp"

#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <utility>

namespace mirror::net {

SocketClient::SocketClient() = default;

SocketClient::~SocketClient() {
    try {
        disconnect();
    } catch (...) {
        // Enforce no unhandled exceptions across module boundaries
    }
}

SocketClient::SocketClient(SocketClient&& other) noexcept
    : video_fd_(other.video_fd_),
      control_fd_(other.control_fd_),
      is_running_(other.is_running_.load()),
      device_name_(std::move(other.device_name_)),
      codec_id_(other.codec_id_),
      width_(other.width_),
      height_(other.height_) {
    other.video_fd_ = -1;
    other.control_fd_ = -1;
    other.is_running_.store(false);
}

SocketClient& SocketClient::operator=(SocketClient&& other) noexcept {
    if (this != &other) {
        disconnect();
        video_fd_ = other.video_fd_;
        control_fd_ = other.control_fd_;
        is_running_.store(other.is_running_.load());
        device_name_ = std::move(other.device_name_);
        codec_id_ = other.codec_id_;
        width_ = other.width_;
        height_ = other.height_;

        other.video_fd_ = -1;
        other.control_fd_ = -1;
        other.is_running_.store(false);
    }
    return *this;
}

int SocketClient::connect_tcp(const char* host, uint16_t port) {
    if (!host) {
        return -1;
    }

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (::inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        // Fallback for hostnames like "localhost"
        struct hostent* he = ::gethostbyname(host);
        if (!he || !he->h_addr_list[0]) {
            ::close(fd);
            return -1;
        }
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
    }

    // Set TCP_NODELAY to avoid Nagle's algorithm latency
    int flag = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

    if (::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

bool SocketClient::read_exact(int fd, uint8_t* buffer, size_t length) {
    if (fd < 0 || buffer == nullptr) {
        return false;
    }

    size_t total_read = 0;
    while (total_read < length) {
        ssize_t bytes_read = ::recv(fd, buffer + total_read, length - total_read, 0);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        total_read += static_cast<size_t>(bytes_read);
    }
    return true;
}

bool SocketClient::write_exact(int fd, const uint8_t* buffer, size_t length) {
    if (fd < 0 || buffer == nullptr) {
        return false;
    }

    size_t total_written = 0;
    while (total_written < length) {
#if defined(MSG_NOSIGNAL)
        ssize_t bytes_written = ::send(fd, buffer + total_written, length - total_written, MSG_NOSIGNAL);
#else
        ssize_t bytes_written = ::send(fd, buffer + total_written, length - total_written, 0);
#endif
        if (bytes_written <= 0) {
            if (bytes_written < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        total_written += static_cast<size_t>(bytes_written);
    }
    return true;
}

void SocketClient::close_socket(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

bool SocketClient::execute_handshake(uint32_t flags) {
    if (video_fd_ < 0 || control_fd_ < 0) {
        return false;
    }

    // Step 1: Read DeviceIdPacket (68 bytes) from video_fd_
    DeviceIdPacket dev_pkt{};
    if (!read_exact(video_fd_, reinterpret_cast<uint8_t*>(&dev_pkt), sizeof(dev_pkt))) {
        return false;
    }

    if (std::memcmp(dev_pkt.magic, MAGIC_VALUE, sizeof(MAGIC_VALUE)) != 0) {
        return false; // Magic validation failure
    }

    char name_buf[sizeof(dev_pkt.device_name) + 1] = {0};
    std::memcpy(name_buf, dev_pkt.device_name, sizeof(dev_pkt.device_name));
    device_name_ = std::string(name_buf);

    // Step 2: Read VideoGeometryPacket (12 bytes) from video_fd_
    VideoGeometryPacket geom_pkt{};
    if (!read_exact(video_fd_, reinterpret_cast<uint8_t*>(&geom_pkt), sizeof(geom_pkt))) {
        return false;
    }

    geom_pkt = from_network_order(geom_pkt);
    codec_id_ = geom_pkt.codec_id;
    width_ = geom_pkt.width;
    height_ = geom_pkt.height;

    // Step 3: Transmit ClientAckPacket (8 bytes) to control_fd_
    ClientAckPacket ack_pkt{
        .version = CLIENT_VERSION,
        .flags = flags
    };
    ack_pkt = to_network_order(ack_pkt);

    if (!write_exact(control_fd_, reinterpret_cast<const uint8_t*>(&ack_pkt), sizeof(ack_pkt))) {
        return false;
    }

    return true;
}

bool SocketClient::connect(const char* host, uint16_t port, uint32_t flags) {
    disconnect();

    video_fd_ = connect_tcp(host, port);
    if (video_fd_ < 0) {
        return false;
    }

    control_fd_ = connect_tcp(host, port);
    if (control_fd_ < 0) {
        close_socket(video_fd_);
        return false;
    }

    if (!execute_handshake(flags)) {
        disconnect();
        return false;
    }

    is_running_.store(true);
    return true;
}

void SocketClient::disconnect() {
    is_running_.store(false);

    if (video_fd_ >= 0) {
        ::shutdown(video_fd_, SHUT_RDWR);
        close_socket(video_fd_);
    }

    if (control_fd_ >= 0) {
        ::shutdown(control_fd_, SHUT_RDWR);
        close_socket(control_fd_);
    }
}

} // namespace mirror::net
