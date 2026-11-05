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

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-s" || arg == "--serial") && i + 1 < argc) {
            serial = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--width" && i + 1 < argc) {
            canvas_w = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            canvas_h = std::atoi(argv[++i]);
        } else if (arg == "--title" && i + 1 < argc) {
            device_name = argv[++i];
        }
    }

    (void)serial;
    (void)port;

    std::cout << "Starting Android Mirror Client..." << std::endl;

    mirror::app::ClientApp app(-1, -1, canvas_w, canvas_h, device_name);
    return app.run();
}
