#pragma once

#include "session/device_session.hpp"
#include "net/adb_tunnel.hpp"
#include "net/socket_client.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/types.h>

namespace mirror::session {

/**
 * Options configuring an Android ADB mirror session.
 */
struct AndroidSessionOptions {
    std::string serial;
    uint16_t port{27183};
    std::string server_jar_path;
    std::string socket_name{"mirror"};
    int max_size{1920};
    int bit_rate{8000000};
    bool stay_awake{true};
};

/**
 * Robust ADB lifecycle controller managing discovery, staging, reverse/forward
 * tunnels, remote app_process daemon execution, and cleanup.
 */
class AndroidAdbSession : public IDeviceSession {
public:
    explicit AndroidAdbSession(AndroidSessionOptions options = {});
    ~AndroidAdbSession() override;

    AndroidAdbSession(const AndroidAdbSession&) = delete;
    AndroidAdbSession& operator=(const AndroidAdbSession&) = delete;
    AndroidAdbSession(AndroidAdbSession&&) = delete;
    AndroidAdbSession& operator=(AndroidAdbSession&&) = delete;

    /**
     * Executes the full startup sequence:
     * 1. Validate adb in PATH.
     * 2. Verify target device connectivity.
     * 3. Kill stale mirror daemons.
     * 4. Discover and push server.jar to /data/local/tmp/server.jar.
     * 5. Set up reverse tunnel: adb reverse tcp:<port> tcp:<port>.
     * 6. Set up forward tunnel: adb forward tcp:<port> localabstract:<socket_name>.
     * 7. Spawn app_process daemon.
     * 8. Connect dual sockets and complete handshake.
     */
    bool initialize() override;

    /**
     * Executes the teardown sequence:
     * 1. Close local sockets.
     * 2. Terminate the local adb daemon process handle.
     * 3. Kill the remote daemon on device.
     * 4. Remove reverse and forward tunnels.
     */
    void teardown() override;

    [[nodiscard]] int video_fd() const noexcept override;
    [[nodiscard]] int control_fd() const noexcept override;
    [[nodiscard]] uint32_t width() const noexcept override;
    [[nodiscard]] uint32_t height() const noexcept override;
    [[nodiscard]] const std::string& device_name() const noexcept override;
    [[nodiscard]] bool is_active() const noexcept override;

    [[nodiscard]] const std::string& serial() const noexcept { return options_.serial; }
    [[nodiscard]] uint16_t port() const noexcept { return options_.port; }

    /**
     * Implements Server JAR discovery hierarchy:
     * 1. MIRROR_SERVER_PATH environment variable
     * 2. Directory of running binary
     * 3. System install paths (/usr/local/share/android-mirror/server.jar, /usr/share/android-mirror/server.jar)
     * 4. Relative development fallbacks (build/server.jar, bin/dist/server.jar, etc.)
     */
    static std::filesystem::path find_server_jar(const std::string& custom_override = "");

private:
    bool validate_adb();
    bool resolve_device();
    bool spawn_remote_daemon(const std::string& remote_jar_path);

    AndroidSessionOptions options_;
    std::unique_ptr<net::AdbTunnel> tunnel_;
    net::SocketClient client_;

    pid_t daemon_pid_{-1};
    std::atomic<bool> is_active_{false};
    bool tunnels_configured_{false};
};

} // namespace mirror::session

namespace mirror {
using mirror::session::AndroidSessionOptions;
using mirror::session::AndroidAdbSession;
}
