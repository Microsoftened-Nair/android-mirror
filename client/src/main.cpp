#include "app/client_app.hpp"
#include "net/adb_tunnel.hpp"
#include "net/socket_client.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    std::string serial;
    uint16_t port = 27183;
    int canvas_w = 1080;
    int canvas_h = 2400;
    std::string device_name = "Android Device";

    std::string socket_name;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--serial=", 0) == 0) {
            serial = arg.substr(9);
        } else if ((arg == "-s" || arg == "--serial") && i + 1 < argc) {
            serial = argv[++i];
        } else if (arg.rfind("--socket-name=", 0) == 0) {
            socket_name = arg.substr(14);
        } else if (arg == "--socket-name" && i + 1 < argc) {
            socket_name = argv[++i];
        } else if (arg.rfind("--port=", 0) == 0) {
            port = static_cast<uint16_t>(std::atoi(arg.substr(7).c_str()));
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg.rfind("--width=", 0) == 0) {
            canvas_w = std::atoi(arg.substr(8).c_str());
        } else if (arg == "--width" && i + 1 < argc) {
            canvas_w = std::atoi(argv[++i]);
        } else if (arg.rfind("--height=", 0) == 0) {
            canvas_h = std::atoi(arg.substr(9).c_str());
        } else if (arg == "--height" && i + 1 < argc) {
            canvas_h = std::atoi(argv[++i]);
        } else if (arg.rfind("--title=", 0) == 0) {
            device_name = arg.substr(8);
        } else if (arg == "--title" && i + 1 < argc) {
            device_name = argv[++i];
        }
    }

    (void)port;

    std::cout << "[Client] Starting Android Mirror Client..." << std::endl;

    int video_fd = -1;
    int control_fd = -1;
    mirror::net::SocketClient client;
    std::unique_ptr<mirror::net::AdbTunnel> tunnel;

    if (!serial.empty() && !socket_name.empty()) {
        std::cout << "[Client] Establishing ADB tunnel for serial: " << serial
                  << " (abstract socket: " << socket_name << ")..." << std::endl;
        tunnel = std::make_unique<mirror::net::AdbTunnel>(serial, socket_name);
        uint16_t forward_port = tunnel->setup_forward(socket_name);
        if (forward_port == 0) {
            std::cerr << "[Client] Error: Failed to set up ADB port forwarding." << std::endl;
            return 1;
        }
        std::cout << "[Client] ADB tunnel established: tcp:127.0.0.1:" << forward_port
                  << " -> localabstract:" << socket_name << std::endl;

        bool connected = false;
        for (int attempt = 1; attempt <= 15; ++attempt) {
            std::cout << "[Client] Connecting to daemon (attempt " << attempt << "/15)..." << std::endl;
            if (client.connect("127.0.0.1", forward_port)) {
                connected = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }

        if (!connected) {
            std::cerr << "[Client] Error: Could not connect to Android server daemon on 127.0.0.1:"
                      << forward_port << std::endl;
            return 1;
        }

        video_fd = client.video_fd();
        control_fd = client.control_fd();
        canvas_w = static_cast<int>(client.width());
        canvas_h = static_cast<int>(client.height());
        if (!client.device_name().empty()) {
            device_name = client.device_name();
        }
        std::cout << "[Client] Handshake completed successfully!" << std::endl;
        std::cout << "[Client] Stream metadata: " << device_name << " ("
                  << canvas_w << "x" << canvas_h << ", Codec: H.264)" << std::endl;
        std::cout << "[Client] Video FD: " << video_fd << ", Control FD: " << control_fd << std::endl;
    }

    std::cout << "[Client] Launching display and input event loop..." << std::endl;
    mirror::app::ClientApp app(video_fd, control_fd, canvas_w, canvas_h, device_name);
    return app.run();
}
