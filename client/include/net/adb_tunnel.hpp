#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mirror::net {

/**
 * Result of a subprocess execution.
 */
struct ProcessResult {
    int exit_code{-1};
    std::string stdout_str;
    std::string stderr_str;

    [[nodiscard]] bool success() const noexcept {
        return exit_code == 0;
    }
};

using ProcessExecutor = std::function<ProcessResult(const std::vector<std::string>& args)>;

/**
 * Manages host-side ADB forwarding tunnel lifecycle, port allocation,
 * device discovery, and cleanup.
 */
class AdbTunnel {
public:
    explicit AdbTunnel(std::string serial, std::string socket_name);
    AdbTunnel(std::string serial, std::string socket_name, ProcessExecutor executor);
    ~AdbTunnel();

    // Non-copyable, movable
    AdbTunnel(const AdbTunnel&) = delete;
    AdbTunnel& operator=(const AdbTunnel&) = delete;
    AdbTunnel(AdbTunnel&& other) noexcept;
    AdbTunnel& operator=(AdbTunnel&& other) noexcept;

    /**
     * Executes: adb -s <serial> shell pkill -f com.mirror.Server || true
     */
    bool kill_zombie_servers();

    /**
     * Executes: adb -s <serial> push <local_jar_path> <remote_path>
     */
    bool push_server_jar(const std::filesystem::path& local_jar_path,
                         const std::string& remote_path = "/data/local/tmp/mirror-server.jar");

    /**
     * Creates an ephemeral TCP listening socket on 127.0.0.1:0 (or uses local_port if provided),
     * closes the probe socket, and runs:
     * adb -s <serial> forward tcp:<HOST_PORT> localabstract:<socket_name>
     * Returns the host port on success, or 0 on failure.
     */
    uint16_t setup_forward(const std::string& socket_name = "", uint16_t local_port = 0);

    /**
     * Executes: adb -s <serial> forward --remove tcp:<HOST_PORT>
     */
    bool remove_forward();

    /**
     * Checks if a given abstract Unix domain socket is actively listening on the target device.
     */
    bool is_socket_listening(const std::string& socket_name = "");

    /**
     * Executes: adb -s <serial> reverse tcp:<remote_port> tcp:<host_port>
     * Returns true on success.
     */
    bool setup_reverse(uint16_t remote_port, uint16_t host_port);

    /**
     * Executes: adb -s <serial> reverse --remove tcp:<remote_port>
     */
    bool remove_reverse(uint16_t remote_port = 0);

    [[nodiscard]] const std::string& serial() const noexcept { return serial_; }
    [[nodiscard]] const std::string& socket_name() const noexcept { return socket_name_; }
    [[nodiscard]] uint16_t local_port() const noexcept { return local_port_; }
    [[nodiscard]] uint16_t reverse_port() const noexcept { return reverse_port_; }
    [[nodiscard]] bool is_forwarded() const noexcept { return local_port_ != 0; }
    [[nodiscard]] bool is_reversed() const noexcept { return reverse_port_ != 0; }

    /**
     * Parses `adb devices -l` output to extract connected device serials.
     */
    static std::vector<std::string> list_connected_devices();
    static std::vector<std::string> parse_devices_output(std::string_view output);

    /**
     * Safely executes an external process capturing stdout/stderr with zero dangling file descriptors.
     */
    static ProcessResult default_execute(const std::vector<std::string>& args);

    /**
     * Probes kernel for an ephemeral port on 127.0.0.1:0 using RAII socket closure.
     */
    static uint16_t allocate_ephemeral_port();

private:
    std::string serial_;
    std::string socket_name_;
    uint16_t local_port_{0};
    uint16_t reverse_port_{0};
    ProcessExecutor executor_;
};

} // namespace mirror::net

namespace mirror {
using mirror::net::AdbTunnel;
using mirror::net::ProcessResult;
using mirror::net::ProcessExecutor;
}

using mirror::net::AdbTunnel;
using mirror::net::ProcessResult;
using mirror::net::ProcessExecutor;
