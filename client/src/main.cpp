#include "app/client_app.hpp"
#include "session/android_adb_session.hpp"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::atomic<mirror::session::AndroidAdbSession*> g_active_session{nullptr};
std::atomic<mirror::app::ClientApp*> g_active_app{nullptr};

void signal_handler(int sig) {
    auto* app = g_active_app.load();
    if (app) {
        app->request_stop();
    }
    auto* session = g_active_session.load();
    if (session) {
        session->teardown();
    }
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void atexit_handler() {
    auto* session = g_active_session.load();
    if (session) {
        session->teardown();
    }
}

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  -s, --serial <serial>       Target Android device serial\n"
              << "  -p, --port <port>           Local TCP port for tunnel (default: 27183)\n"
              << "  --server-path <path>        Custom path to server.jar artifact\n"
              << "  --socket-name <name>        Abstract Unix domain socket name (default: mirror)\n"
              << "  --max-size <dimension>      Maximum streaming resolution bound (default: 1920)\n"
              << "  --bit-rate <bps>            Target H.264 video bitrate (default: 8000000)\n"
              << "  --stay-awake <true|false>   Keep Android device awake while connected (default: true)\n"
              << "  --title <title>             Window title override\n"
              << "  -h, --help                  Show this help message\n";
}

} // namespace

int main(int argc, char* argv[]) {
    mirror::session::AndroidSessionOptions session_opts;
    std::string custom_title;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg.rfind("--serial=", 0) == 0) {
            session_opts.serial = arg.substr(9);
        } else if ((arg == "-s" || arg == "--serial") && i + 1 < argc) {
            session_opts.serial = argv[++i];
        } else if (arg.rfind("--port=", 0) == 0) {
            session_opts.port = static_cast<uint16_t>(std::atoi(arg.substr(7).c_str()));
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            session_opts.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg.rfind("--server-path=", 0) == 0) {
            session_opts.server_jar_path = arg.substr(14);
        } else if (arg == "--server-path" && i + 1 < argc) {
            session_opts.server_jar_path = argv[++i];
        } else if (arg.rfind("--socket-name=", 0) == 0) {
            session_opts.socket_name = arg.substr(14);
        } else if (arg == "--socket-name" && i + 1 < argc) {
            session_opts.socket_name = argv[++i];
        } else if (arg.rfind("--max-size=", 0) == 0) {
            session_opts.max_size = std::atoi(arg.substr(11).c_str());
        } else if (arg == "--max-size" && i + 1 < argc) {
            session_opts.max_size = std::atoi(argv[++i]);
        } else if (arg.rfind("--bit-rate=", 0) == 0) {
            session_opts.bit_rate = std::atoi(arg.substr(11).c_str());
        } else if (arg == "--bit-rate" && i + 1 < argc) {
            session_opts.bit_rate = std::atoi(argv[++i]);
        } else if (arg.rfind("--stay-awake=", 0) == 0) {
            std::string val = arg.substr(13);
            session_opts.stay_awake = (val == "true" || val == "1");
        } else if (arg == "--stay-awake" && i + 1 < argc) {
            std::string val = argv[++i];
            session_opts.stay_awake = (val == "true" || val == "1");
        } else if (arg.rfind("--title=", 0) == 0) {
            custom_title = arg.substr(8);
        } else if (arg == "--title" && i + 1 < argc) {
            custom_title = argv[++i];
        }
    }

    // Register POSIX signal handlers and std::atexit for guaranteed cleanup
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::atexit(atexit_handler);

    std::cout << "=== Android Mirror Client ===" << std::endl;

    mirror::session::AndroidAdbSession session(session_opts);
    g_active_session.store(&session);

    if (!session.initialize()) {
        std::cerr << "[Client] Initialization failed. Exiting." << std::endl;
        g_active_session.store(nullptr);
        return 1;
    }

    std::string window_title = custom_title.empty() ? session.device_name() : custom_title;
    if (window_title.empty()) {
        window_title = "Android Mirror";
    }

    int result = 0;
    {
        mirror::app::ClientApp app(
            session.video_fd(),
            session.control_fd(),
            static_cast<int>(session.width()),
            static_cast<int>(session.height()),
            window_title
        );
        g_active_app.store(&app);

        std::cout << "[Client] Launching display and presentation loop..." << std::endl;
        result = app.run();
        g_active_app.store(nullptr);
    }

    std::cout << "[Client] Tearing down device session..." << std::endl;
    session.teardown();
    g_active_session.store(nullptr);

    std::cout << "[Client] Clean shutdown complete." << std::endl;
    return result;
}
