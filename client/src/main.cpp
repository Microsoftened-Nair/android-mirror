#include "app/client_app.hpp"
#include "net/adb_tunnel.hpp"
#include "net/socket_client.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

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

    std::cout << "Starting Android Mirror Client..." << std::endl;

    int video_fd = -1;
    int control_fd = -1;
    mirror::net::SocketClient client;
    if (!serial.empty() && !socket_name.empty()) {
        mirror::net::AdbTunnel tunnel(serial, socket_name);
        uint16_t forward_port = tunnel.setup_forward(socket_name);
        if (forward_port > 0) {
            if (client.connect("127.0.0.1", forward_port)) {
                video_fd = client.video_fd();
                control_fd = client.control_fd();
                canvas_w = static_cast<int>(client.width());
                canvas_h = static_cast<int>(client.height());
                if (!client.device_name().empty()) {
                    device_name = client.device_name();
                }
            }
        }
    }

    mirror::app::ClientApp app(video_fd, control_fd, canvas_w, canvas_h, device_name);
    return app.run();
}
