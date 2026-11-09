#pragma once

#include "mist/common/Result.h"
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace mist::platform::windows {

struct VirtualDisplayMode {
    uint32_t width{1920};
    uint32_t height{1080};
    uint32_t refreshRate{60};

    [[nodiscard]] bool operator==(const VirtualDisplayMode& other) const noexcept {
        return width == other.width && height == other.height && refreshRate == other.refreshRate;
    }
};

struct VirtualDisplayInfo {
    uint32_t monitorIndex{0};
    VirtualDisplayMode currentMode;
    std::vector<VirtualDisplayMode> supportedModes;
    std::string friendlyName{"Mist Virtual Display"};
    bool isPlugged{false};
    std::vector<uint8_t> edidBlock;
};

struct VirtualDisplayConfig {
    bool simulationOnly{false};         // True in test/CI environments without kernel IddCx driver
    std::string driverDevicePath{"\\\\.\\IddDisplayDevice"};
};

class VirtualDisplayManager {
public:
    explicit VirtualDisplayManager(VirtualDisplayConfig config = {}) noexcept;
    ~VirtualDisplayManager();

    // Query if the native IddCx virtual display driver is installed and accessible
    [[nodiscard]] bool isDriverInstalled() const noexcept;

    // Plug in a virtual monitor matching client resolution & refresh rate
    // Returns assigned monitor index (e.g. 1, 2, 3...)
    [[nodiscard]] common::Result<uint32_t> plugVirtualDisplay(
        const VirtualDisplayMode& targetMode,
        const std::vector<VirtualDisplayMode>& additionalModes = {}
    );

    // Unplug an existing virtual monitor
    [[nodiscard]] common::Result<void> unplugVirtualDisplay(uint32_t monitorIndex);

    // Unplug all virtual monitors
    [[nodiscard]] common::Result<void> unplugAllVirtualDisplays();

    // Query active virtual monitors
    [[nodiscard]] std::vector<VirtualDisplayInfo> getActiveVirtualDisplays() const;
    [[nodiscard]] common::Result<VirtualDisplayInfo> getVirtualDisplay(uint32_t monitorIndex) const;

    // EDID 1.4 standard block generator (128 bytes)
    // Produces a compliant VESA EDID 1.4 block with correct checksum for the requested mode
    [[nodiscard]] static std::vector<uint8_t> generateEdid14(
        uint32_t width,
        uint32_t height,
        uint32_t refreshRate,
        const std::string& monitorName = "Mist Display"
    ) noexcept;

    // Helper: validate EDID checksum (sum of 128 bytes % 256 == 0)
    [[nodiscard]] static bool validateEdidChecksum(const std::vector<uint8_t>& edid) noexcept;

private:
    mutable std::mutex mutex_;
    VirtualDisplayConfig config_;
    uint32_t nextMonitorIndex_{1};
    std::unordered_map<uint32_t, VirtualDisplayInfo> displays_;
    bool driverPresent_{false};
};

} // namespace mist::platform::windows
