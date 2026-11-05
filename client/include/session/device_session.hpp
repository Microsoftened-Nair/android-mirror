#pragma once

#include <cstdint>
#include <string>

namespace mirror::session {

/**
 * Pure abstract interface representing a device session lifecycle.
 * Decouples presentation and UI loops from device transport and ADB orchestration.
 */
class IDeviceSession {
public:
    virtual ~IDeviceSession() = default;

    /**
     * Initializes the device session: verifies ADB, resolves target device,
     * stages server artifact, configures tunnels, spawns daemon, and connects channels.
     * Returns true on success, false on failure.
     */
    virtual bool initialize() = 0;

    /**
     * Tears down the device session: disconnects sockets, terminates the remote
     * daemon, and removes all port forwarding/reverse tunnels.
     */
    virtual void teardown() = 0;

    [[nodiscard]] virtual int video_fd() const noexcept = 0;
    [[nodiscard]] virtual int control_fd() const noexcept = 0;
    [[nodiscard]] virtual uint32_t width() const noexcept = 0;
    [[nodiscard]] virtual uint32_t height() const noexcept = 0;
    [[nodiscard]] virtual const std::string& device_name() const noexcept = 0;
    [[nodiscard]] virtual bool is_active() const noexcept = 0;
};

} // namespace mirror::session

namespace mirror {
using mirror::session::IDeviceSession;
}
