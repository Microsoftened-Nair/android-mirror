#include "net/adb_tunnel.hpp"

#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sstream>
#include <utility>

namespace mirror::net {

AdbTunnel::AdbTunnel(std::string serial, std::string socket_name)
    : serial_(std::move(serial)),
      socket_name_(std::move(socket_name)),
      local_port_(0),
      reverse_port_(0),
      executor_(&AdbTunnel::default_execute) {}

AdbTunnel::AdbTunnel(std::string serial, std::string socket_name, ProcessExecutor executor)
    : serial_(std::move(serial)),
      socket_name_(std::move(socket_name)),
      local_port_(0),
      reverse_port_(0),
      executor_(executor ? std::move(executor) : &AdbTunnel::default_execute) {}

AdbTunnel::~AdbTunnel() {
    try {
        if (local_port_ != 0) {
            remove_forward();
        }
        if (reverse_port_ != 0) {
            remove_reverse(reverse_port_);
        }
    } catch (...) {
        // Enforce no unhandled exceptions across module boundaries
    }
}

AdbTunnel::AdbTunnel(AdbTunnel&& other) noexcept
    : serial_(std::move(other.serial_)),
      socket_name_(std::move(other.socket_name_)),
      local_port_(other.local_port_),
      reverse_port_(other.reverse_port_),
      executor_(std::move(other.executor_)) {
    other.local_port_ = 0;
    other.reverse_port_ = 0;
}

AdbTunnel& AdbTunnel::operator=(AdbTunnel&& other) noexcept {
    if (this != &other) {
        try {
            if (local_port_ != 0) {
                remove_forward();
            }
            if (reverse_port_ != 0) {
                remove_reverse(reverse_port_);
            }
        } catch (...) {
        }
        serial_ = std::move(other.serial_);
        socket_name_ = std::move(other.socket_name_);
        local_port_ = other.local_port_;
        reverse_port_ = other.reverse_port_;
        executor_ = std::move(other.executor_);
        other.local_port_ = 0;
        other.reverse_port_ = 0;
    }
    return *this;
}

bool AdbTunnel::kill_zombie_servers() {
    try {
        std::vector<std::string> args;
        args.emplace_back("adb");
        if (!serial_.empty()) {
            args.emplace_back("-s");
            args.emplace_back(serial_);
        }
        args.emplace_back("shell");
        args.emplace_back("pkill -f com.mirror.Server || true");

        ProcessResult res = executor_(args);
        return res.success();
    } catch (...) {
        return false;
    }
}

bool AdbTunnel::push_server_jar(const std::filesystem::path& local_jar_path,
                                const std::string& remote_path) {
    try {
        std::vector<std::string> args;
        args.emplace_back("adb");
        if (!serial_.empty()) {
            args.emplace_back("-s");
            args.emplace_back(serial_);
        }
        args.emplace_back("push");
        args.emplace_back(local_jar_path.string());
        args.emplace_back(remote_path.empty() ? "/data/local/tmp/mirror-server.jar" : remote_path);

        ProcessResult res = executor_(args);
        return res.success();
    } catch (...) {
        return false;
    }
}

uint16_t AdbTunnel::setup_forward(const std::string& socket_name, uint16_t local_port) {
    try {
        std::string target_socket = socket_name.empty() ? socket_name_ : socket_name;
        if (target_socket.empty()) {
            return 0;
        }

        if (local_port_ != 0) {
            remove_forward();
        }

        uint16_t port = local_port;
        if (port == 0) {
            port = allocate_ephemeral_port();
            if (port == 0) {
                return 0;
            }
        }

        std::vector<std::string> args;
        args.emplace_back("adb");
        if (!serial_.empty()) {
            args.emplace_back("-s");
            args.emplace_back(serial_);
        }
        args.emplace_back("forward");
        args.emplace_back("tcp:" + std::to_string(port));
        args.emplace_back("localabstract:" + target_socket);

        ProcessResult res = executor_(args);
        if (res.success()) {
            local_port_ = port;
            socket_name_ = std::move(target_socket);
            return port;
        }
        return 0;
    } catch (...) {
        return 0;
    }
}

bool AdbTunnel::remove_forward() {
    try {
        if (local_port_ == 0) {
            return true;
        }

        std::vector<std::string> args;
        args.emplace_back("adb");
        if (!serial_.empty()) {
            args.emplace_back("-s");
            args.emplace_back(serial_);
        }
        args.emplace_back("forward");
        args.emplace_back("--remove");
        args.emplace_back("tcp:" + std::to_string(local_port_));

        ProcessResult res = executor_(args);
        local_port_ = 0;
        return res.success();
    } catch (...) {
        local_port_ = 0;
        return false;
    }
}

bool AdbTunnel::is_socket_listening(const std::string& socket_name) {
    try {
        std::string target_socket = socket_name.empty() ? socket_name_ : socket_name;
        if (target_socket.empty()) {
            return false;
        }

        std::vector<std::string> args;
        args.emplace_back("adb");
        if (!serial_.empty()) {
            args.emplace_back("-s");
            args.emplace_back(serial_);
        }
        args.emplace_back("shell");
        args.emplace_back("grep -q " + target_socket + " /proc/net/unix");

        ProcessResult res = executor_(args);
        return res.success();
    } catch (...) {
        return false;
    }
}

bool AdbTunnel::setup_reverse(uint16_t remote_port, uint16_t host_port) {
    try {
        if (remote_port == 0 || host_port == 0) {
            return false;
        }

        if (reverse_port_ != 0) {
            remove_reverse(reverse_port_);
        }

        std::vector<std::string> args;
        args.emplace_back("adb");
        if (!serial_.empty()) {
            args.emplace_back("-s");
            args.emplace_back(serial_);
        }
        args.emplace_back("reverse");
        args.emplace_back("tcp:" + std::to_string(remote_port));
        args.emplace_back("tcp:" + std::to_string(host_port));

        ProcessResult res = executor_(args);
        if (res.success()) {
            reverse_port_ = remote_port;
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

bool AdbTunnel::remove_reverse(uint16_t remote_port) {
    try {
        uint16_t port_to_remove = (remote_port != 0) ? remote_port : reverse_port_;
        if (port_to_remove == 0) {
            return true;
        }

        std::vector<std::string> args;
        args.emplace_back("adb");
        if (!serial_.empty()) {
            args.emplace_back("-s");
            args.emplace_back(serial_);
        }
        args.emplace_back("reverse");
        args.emplace_back("--remove");
        args.emplace_back("tcp:" + std::to_string(port_to_remove));

        ProcessResult res = executor_(args);
        if (port_to_remove == reverse_port_) {
            reverse_port_ = 0;
        }
        return res.success();
    } catch (...) {
        reverse_port_ = 0;
        return false;
    }
}

std::vector<std::string> AdbTunnel::list_connected_devices() {
    try {
        std::vector<std::string> args = {"adb", "devices", "-l"};
        ProcessResult res = default_execute(args);
        if (!res.success()) {
            return {};
        }
        return parse_devices_output(res.stdout_str);
    } catch (...) {
        return {};
    }
}

std::vector<std::string> AdbTunnel::parse_devices_output(std::string_view output) {
    std::vector<std::string> devices;
    size_t pos = 0;

    while (pos < output.size()) {
        size_t next_line = output.find('\n', pos);
        std::string_view line = (next_line == std::string_view::npos)
                                    ? output.substr(pos)
                                    : output.substr(pos, next_line - pos);
        pos = (next_line == std::string_view::npos) ? output.size() : next_line + 1;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.remove_prefix(1);
        }

        if (line.empty() || line.starts_with('*') || line.starts_with("List of devices")) {
            continue;
        }

        size_t token_end = 0;
        while (token_end < line.size() && line[token_end] != ' ' && line[token_end] != '\t') {
            ++token_end;
        }

        std::string_view serial = line.substr(0, token_end);

        size_t state_start = token_end;
        while (state_start < line.size() && (line[state_start] == ' ' || line[state_start] == '\t')) {
            ++state_start;
        }

        size_t state_end = state_start;
        while (state_end < line.size() && line[state_end] != ' ' && line[state_end] != '\t') {
            ++state_end;
        }

        std::string_view state = line.substr(state_start, state_end - state_start);

        if (state == "device") {
            devices.emplace_back(serial);
        }
    }

    return devices;
}

uint16_t AdbTunnel::allocate_ephemeral_port() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return 0;
    }

    struct RaiiCloser {
        int fd;
        ~RaiiCloser() {
            if (fd >= 0) {
                close(fd);
            }
        }
    } closer{fd};

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return 0;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        return 0;
    }

    return ntohs(addr.sin_port);
}

ProcessResult AdbTunnel::default_execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {-1, "", "No arguments provided"};
    }

    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) < 0) {
        return {-1, "", "Failed to create stdout pipe"};
    }
    if (pipe(stderr_pipe) < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return {-1, "", "Failed to create stderr pipe"};
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return {-1, "", "Failed to fork process"};
    }

    if (pid == 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

#if defined(__GLIBC__)
        close_range(3, 1024, 0);
#else
        for (int fd = 3; fd < 256; ++fd) {
            close(fd);
        }
#endif

        std::vector<char*> c_args;
        c_args.reserve(args.size() + 1);
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(c_args[0], c_args.data());
        _exit(127);
    }

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    std::string out_str;
    std::string err_str;

    struct pollfd fds[2];
    fds[0].fd = stdout_pipe[0];
    fds[0].events = POLLIN;
    fds[1].fd = stderr_pipe[0];
    fds[1].events = POLLIN;

    char buffer[4096];
    int open_fds = 2;

    while (open_fds > 0) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (fds[0].fd >= 0 && (fds[0].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t r = read(fds[0].fd, buffer, sizeof(buffer));
            if (r > 0) {
                out_str.append(buffer, static_cast<size_t>(r));
            } else {
                close(fds[0].fd);
                fds[0].fd = -1;
                --open_fds;
            }
        }

        if (fds[1].fd >= 0 && (fds[1].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t r = read(fds[1].fd, buffer, sizeof(buffer));
            if (r > 0) {
                err_str.append(buffer, static_cast<size_t>(r));
            } else {
                close(fds[1].fd);
                fds[1].fd = -1;
                --open_fds;
            }
        }
    }

    if (fds[0].fd >= 0) {
        close(fds[0].fd);
    }
    if (fds[1].fd >= 0) {
        close(fds[1].fd);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        break;
    }

    int exit_code = -1;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_code = 128 + WTERMSIG(status);
    }

    return {exit_code, std::move(out_str), std::move(err_str)};
}

} // namespace mirror::net
