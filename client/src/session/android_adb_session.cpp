#include "session/android_adb_session.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace mirror::session {

AndroidAdbSession::AndroidAdbSession(AndroidSessionOptions options)
    : options_(std::move(options)) {}

AndroidAdbSession::~AndroidAdbSession() {
    try {
        teardown();
    } catch (...) {
        // Enforce no unhandled exceptions across module boundaries
    }
}

std::filesystem::path AndroidAdbSession::find_server_jar(const std::string& custom_override) {
    std::error_code ec;

    // 0. Explicit CLI or programmatic override
    if (!custom_override.empty()) {
        std::filesystem::path p(custom_override);
        if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) {
            return std::filesystem::canonical(p, ec);
        }
    }

    // 1. Environment variable: MIRROR_SERVER_PATH
    const char* env_path = std::getenv("MIRROR_SERVER_PATH");
    if (env_path && *env_path != '\0') {
        std::filesystem::path p(env_path);
        if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) {
            return std::filesystem::canonical(p, ec);
        }
    }

    // 2. Directory of running binary
    std::filesystem::path exe_path = std::filesystem::canonical("/proc/self/exe", ec);
    if (!ec && !exe_path.empty()) {
        std::filesystem::path binary_jar = exe_path.parent_path() / "server.jar";
        if (std::filesystem::exists(binary_jar, ec) && !std::filesystem::is_directory(binary_jar, ec)) {
            return std::filesystem::canonical(binary_jar, ec);
        }
    }

    // 3. System install paths
    const std::vector<std::filesystem::path> system_paths = {
        "/usr/local/share/android-mirror/server.jar",
        "/usr/share/android-mirror/server.jar"
    };
    for (const auto& p : system_paths) {
        if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) {
            return std::filesystem::canonical(p, ec);
        }
    }

    // 4. Relative development fallbacks
    const std::vector<std::filesystem::path> dev_paths = {
        "build/server.jar",
        "bin/dist/server.jar",
        "android-server/build/libs/server.jar",
        "../build/server.jar",
        "../bin/dist/server.jar",
        "../android-server/build/libs/server.jar",
        "./server.jar"
    };
    for (const auto& p : dev_paths) {
        if (std::filesystem::exists(p, ec) && !std::filesystem::is_directory(p, ec)) {
            return std::filesystem::canonical(p, ec);
        }
    }

    return {};
}

bool AndroidAdbSession::validate_adb() {
    auto res = net::AdbTunnel::default_execute({"adb", "version"});
    if (!res.success()) {
        std::cerr << "[Session] Error: 'adb' executable not found in PATH. Please install Android Platform Tools.\n";
        return false;
    }
    return true;
}

bool AndroidAdbSession::resolve_device() {
    auto devices = net::AdbTunnel::list_connected_devices();

    if (!options_.serial.empty()) {
        bool found = false;
        for (const auto& d : devices) {
            if (d == options_.serial) {
                found = true;
                break;
            }
        }
        if (!found && !devices.empty()) {
            std::cerr << "[Session] Warning: Specified serial '" << options_.serial
                      << "' not found in connected authorized devices.\n";
        }
        return true;
    }

    if (devices.empty()) {
        std::cerr << "[Session] Error: No authorized Android devices detected via adb. Ensure USB debugging is enabled.\n";
        return false;
    }

    if (devices.size() > 1) {
        std::cerr << "[Session] Error: Multiple devices connected (" << devices.size()
                  << "). Please specify target serial with -s or --serial:\n";
        for (const auto& d : devices) {
            std::cerr << "  - " << d << "\n";
        }
        return false;
    }

    options_.serial = devices[0];
    std::cout << "[Session] Auto-selected connected device: " << options_.serial << "\n";
    return true;
}

bool AndroidAdbSession::spawn_remote_daemon(const std::string& remote_jar_path) {
    std::string cmd = "CLASSPATH=" + remote_jar_path + " app_process / com.mirror.Server"
                      + " --socket-name=" + options_.socket_name
                      + " --stay-awake=" + (options_.stay_awake ? "true" : "false");
    if (options_.max_size > 0) {
        cmd += " --max-size=" + std::to_string(options_.max_size);
    }
    if (options_.bit_rate > 0) {
        cmd += " --bit-rate=" + std::to_string(options_.bit_rate);
    }

    std::cout << "[Session] Spawning remote daemon via adb shell...\n";

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[Session] Error: fork failed for adb daemon process.\n";
        return false;
    }

    if (pid == 0) {
        // Child process
        int dev_null = open("/dev/null", O_RDWR);
        if (dev_null >= 0) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            if (dev_null > STDERR_FILENO) {
                close(dev_null);
            }
        }

        std::vector<const char*> argv;
        argv.push_back("adb");
        if (!options_.serial.empty()) {
            argv.push_back("-s");
            argv.push_back(options_.serial.c_str());
        }
        argv.push_back("shell");
        argv.push_back(cmd.c_str());
        argv.push_back(nullptr);

        execvp("adb", const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    daemon_pid_ = pid;
    return true;
}

bool AndroidAdbSession::initialize() {
    std::cout << "[Session] Initializing Android ADB Session...\n";

    // 1. Validate adb is present in PATH
    if (!validate_adb()) {
        return false;
    }

    // 2. Verify target device connectivity
    if (!resolve_device()) {
        return false;
    }

    tunnel_ = std::make_unique<net::AdbTunnel>(options_.serial, options_.socket_name);

    // 3. Pre-cleanup on device
    std::cout << "[Session] Terminating any stale mirror daemon instances...\n";
    tunnel_->kill_zombie_servers();

    // 4. Discover and stage server.jar
    auto jar_path = find_server_jar(options_.server_jar_path);
    if (jar_path.empty()) {
        std::cerr << "[Session] Error: Could not locate server.jar via discovery hierarchy.\n"
                  << "  Please set MIRROR_SERVER_PATH or build via ./scripts/build_server.sh.\n";
        return false;
    }
    std::cout << "[Session] Found server.jar: " << jar_path << "\n";

    const std::string remote_jar = "/data/local/tmp/server.jar";
    std::cout << "[Session] Staging server.jar to " << remote_jar << " on device...\n";
    if (!tunnel_->push_server_jar(jar_path, remote_jar)) {
        std::cerr << "[Session] Error: Failed to push server.jar to device.\n";
        return false;
    }

    // 5. Setup reverse socket tunnel: adb reverse tcp:<port> tcp:<port>
    std::cout << "[Session] Configuring reverse socket tunnel (tcp:" << options_.port
              << " -> tcp:" << options_.port << ")...\n";
    tunnel_->setup_reverse(options_.port, options_.port);

    // 6. Setup forward socket tunnel: adb forward tcp:<port> localabstract:<socket_name>
    std::cout << "[Session] Configuring forward socket tunnel (tcp:" << options_.port
              << " -> localabstract:" << options_.socket_name << ")...\n";
    if (tunnel_->setup_forward(options_.socket_name) == 0) {
        std::cerr << "[Session] Error: Failed to establish ADB port forwarding.\n";
        teardown();
        return false;
    }
    tunnels_configured_ = true;

    // 7. Spawn background device daemon
    if (!spawn_remote_daemon(remote_jar)) {
        teardown();
        return false;
    }

    // 8. Connect dual channels and execute handshake with retry loop
    std::cout << "[Session] Connecting to daemon on 127.0.0.1:" << options_.port << "...\n";
    bool connected = false;
    for (int attempt = 1; attempt <= 20; ++attempt) {
        if (daemon_pid_ > 0) {
            int status = 0;
            pid_t died = waitpid(daemon_pid_, &status, WNOHANG);
            if (died > 0) {
                std::cerr << "[Session] Error: ADB daemon process exited prematurely (status " << status << ").\n";
                daemon_pid_ = -1;
                teardown();
                return false;
            }
        }

        if (client_.connect("127.0.0.1", options_.port)) {
            connected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    if (!connected) {
        std::cerr << "[Session] Error: Timed out waiting for connection to server daemon on 127.0.0.1:"
                  << options_.port << ".\n";
        teardown();
        return false;
    }

    is_active_.store(true);
    std::cout << "[Session] Handshake completed successfully!\n"
              << "[Session] Remote Device: " << client_.device_name() << " ("
              << client_.width() << "x" << client_.height() << ", Codec: H.264)\n"
              << "[Session] Video FD: " << client_.video_fd()
              << ", Control FD: " << client_.control_fd() << "\n";

    return true;
}

void AndroidAdbSession::teardown() {
    is_active_.store(false);

    // 1. Disconnect sockets
    client_.disconnect();

    // 2. Terminate local ADB daemon child process
    if (daemon_pid_ > 0) {
        kill(daemon_pid_, SIGTERM);
        int status = 0;
        waitpid(daemon_pid_, &status, 0);
        daemon_pid_ = -1;
    }

    // 3. Remove tunnels and kill remote daemon on device
    if (tunnel_) {
        tunnel_->kill_zombie_servers();
        if (options_.port != 0) {
            tunnel_->remove_reverse(options_.port);
        }
        if (tunnels_configured_) {
            tunnel_->remove_forward();
            tunnels_configured_ = false;
        }
    }
}

int AndroidAdbSession::video_fd() const noexcept {
    return client_.video_fd();
}

int AndroidAdbSession::control_fd() const noexcept {
    return client_.control_fd();
}

uint32_t AndroidAdbSession::width() const noexcept {
    return client_.width();
}

uint32_t AndroidAdbSession::height() const noexcept {
    return client_.height();
}

const std::string& AndroidAdbSession::device_name() const noexcept {
    return client_.device_name();
}

bool AndroidAdbSession::is_active() const noexcept {
    return is_active_.load();
}

} // namespace mirror::session
