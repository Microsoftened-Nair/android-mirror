#include "net/adb_tunnel.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace mirror::net;

void test_parse_devices() {
    // Empty output
    assert(AdbTunnel::parse_devices_output("").empty());

    // Only header
    assert(AdbTunnel::parse_devices_output("List of devices attached\n\n").empty());

    // Daemon start noise + multiple devices
    std::string sample_output =
        "* daemon not running; starting now at tcp:5037 *\n"
        "* daemon started successfully *\n"
        "List of devices attached\n"
        "emulator-5554          device product:sdk_gphone64_arm64 model:sdk_gphone64_arm64\n"
        "0123456789ABCDEF      unauthorized usb:1-1\n"
        "emulator-5556          offline\n"
        "988a1b424c534e484a     device usb:3-2 product:star2qltezc model:SM_G9650\n";

    std::vector<std::string> devices = AdbTunnel::parse_devices_output(sample_output);
    assert(devices.size() == 2);
    assert(devices[0] == "emulator-5554");
    assert(devices[1] == "988a1b424c534e484a");

    std::cout << "[PASS] test_parse_devices" << std::endl;
}

void test_command_construction() {
    std::vector<std::vector<std::string>> executed_commands;

    auto mock_executor = [&](const std::vector<std::string>& args) -> ProcessResult {
        executed_commands.push_back(args);
        return {0, "", ""};
    };

    AdbTunnel tunnel("device123", "test_socket", mock_executor);

    // 1. kill_zombie_servers
    assert(tunnel.kill_zombie_servers());
    assert(executed_commands.size() == 1);
    assert(executed_commands[0] == std::vector<std::string>({
        "adb", "-s", "device123", "shell", "pkill -f com.mirror.Server || true"
    }));

    // 2. push_server_jar with default path
    assert(tunnel.push_server_jar("/tmp/server.jar"));
    assert(executed_commands.size() == 2);
    assert(executed_commands[1] == std::vector<std::string>({
        "adb", "-s", "device123", "push", "/tmp/server.jar", "/data/local/tmp/mirror-server.jar"
    }));

    // 3. push_server_jar with custom path
    assert(tunnel.push_server_jar("/tmp/server.jar", "/data/local/tmp/custom.jar"));
    assert(executed_commands.size() == 3);
    assert(executed_commands[2] == std::vector<std::string>({
        "adb", "-s", "device123", "push", "/tmp/server.jar", "/data/local/tmp/custom.jar"
    }));

    // 4. setup_forward
    uint16_t port = tunnel.setup_forward("test_socket");
    assert(port > 0);
    assert(tunnel.local_port() == port);
    assert(tunnel.is_forwarded());
    assert(executed_commands.size() == 4);
    assert(executed_commands[3][0] == "adb");
    assert(executed_commands[3][1] == "-s");
    assert(executed_commands[3][2] == "device123");
    assert(executed_commands[3][3] == "forward");
    assert(executed_commands[3][4] == "tcp:" + std::to_string(port));
    assert(executed_commands[3][5] == "localabstract:test_socket");

    // 5. remove_forward
    assert(tunnel.remove_forward());
    assert(tunnel.local_port() == 0);
    assert(!tunnel.is_forwarded());
    assert(executed_commands.size() == 5);
    assert(executed_commands[4] == std::vector<std::string>({
        "adb", "-s", "device123", "forward", "--remove", "tcp:" + std::to_string(port)
    }));

    std::cout << "[PASS] test_command_construction" << std::endl;
}

void test_raii_teardown() {
    std::vector<std::vector<std::string>> executed_commands;
    uint16_t forwarded_port = 0;

    auto mock_executor = [&](const std::vector<std::string>& args) -> ProcessResult {
        executed_commands.push_back(args);
        return {0, "", ""};
    };

    {
        AdbTunnel tunnel("dev_raii", "raii_socket", mock_executor);
        forwarded_port = tunnel.setup_forward();
        assert(forwarded_port > 0);
        assert(tunnel.is_forwarded());
        // Tunnel goes out of scope here -> destructor must call remove_forward()
    }

    assert(executed_commands.size() == 2); // setup_forward + remove_forward in dtor
    assert(executed_commands[1] == std::vector<std::string>({
        "adb", "-s", "dev_raii", "forward", "--remove", "tcp:" + std::to_string(forwarded_port)
    }));

    std::cout << "[PASS] test_raii_teardown" << std::endl;
}

void test_ephemeral_port_allocation() {
    uint16_t port1 = AdbTunnel::allocate_ephemeral_port();
    assert(port1 > 0);
    uint16_t port2 = AdbTunnel::allocate_ephemeral_port();
    assert(port2 > 0);
    std::cout << "[PASS] test_ephemeral_port_allocation (probed ports: " << port1 << ", " << port2 << ")" << std::endl;
}

int main() {
    test_parse_devices();
    test_command_construction();
    test_raii_teardown();
    test_ephemeral_port_allocation();

    std::cout << "All AdbTunnel unit tests passed successfully!" << std::endl;
    return 0;
}
