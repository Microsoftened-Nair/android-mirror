#include "app/client_app.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

static void test_client_app_lifecycle() {
    mirror::app::ClientApp app(-1, -1, 320, 240, "App Lifecycle Test");
    assert(app.is_running());

    // Stop app after a short delay on a background thread
    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        app.request_stop();
    });

    int ret = app.run();
    stopper.join();

    assert(ret == 0);
    assert(!app.is_running());
    std::cout << "[PASS] test_client_app_lifecycle" << std::endl;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    test_client_app_lifecycle();
    std::cout << "All ClientApp tests passed successfully!" << std::endl;
    return 0;
}
