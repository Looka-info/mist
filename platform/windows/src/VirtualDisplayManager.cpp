#include "mist/platform/windows/VirtualDisplayManager.h"
#include "mist/common/Logger.h"
#include <numeric>
#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mist::platform::windows {

namespace {

// IddCx custom control IOCTL definitions
constexpr uint32_t IOCTL_IDD_PLUG_MONITOR   = 0x80002000;
constexpr uint32_t IOCTL_IDD_UNPLUG_MONITOR = 0x80002004;
[[maybe_unused]] constexpr uint32_t IOCTL_IDD_QUERY_STATUS = 0x80002008;

} // namespace

VirtualDisplayManager::VirtualDisplayManager(VirtualDisplayConfig config) noexcept
    : config_(std::move(config)) {
    if (config_.simulationOnly) {
        driverPresent_ = false;
        return;
    }

#if defined(_WIN32)
    // Check if the IddCx device driver is installed and accessible
    HANDLE hDevice = CreateFileA(
        config_.driverDevicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hDevice != INVALID_HANDLE_VALUE) {
        driverPresent_ = true;
        CloseHandle(hDevice);
        MIST_LOG_INFO(common::SubsystemId::Platform, "IddCx Virtual Display Driver detected and ready.");
    } else {
        driverPresent_ = false;
        MIST_LOG_INFO(common::SubsystemId::Platform, "IddCx driver device not present; running in virtual display simulation mode.");
    }
#else
    driverPresent_ = false;
#endif
}

VirtualDisplayManager::~VirtualDisplayManager() {
    (void)unplugAllVirtualDisplays();
}

bool VirtualDisplayManager::isDriverInstalled() const noexcept {
    return driverPresent_;
}

std::vector<uint8_t> VirtualDisplayManager::generateEdid14(
    uint32_t width,
    uint32_t height,
    uint32_t refreshRate,
    const std::string& monitorName
) noexcept {
    std::vector<uint8_t> edid(128, 0);

    // 1. Header (8 bytes)
    edid[0] = 0x00;
    edid[1] = 0xFF; edid[2] = 0xFF; edid[3] = 0xFF;
    edid[4] = 0xFF; edid[5] = 0xFF; edid[6] = 0xFF;
    edid[7] = 0x00;

    // 2. Manufacturer ID: "MST" in 5-bit packed ASCII ('M'=13, 'S'=19, 'T'=20)
    // 0001101 10011 10100 -> 0x36, 0x74
    edid[8] = 0x36;
    edid[9] = 0x74;

    // 3. Product code & Serial
    edid[10] = 0x01; edid[11] = 0x01;
    edid[12] = 0x01; edid[13] = 0x02; edid[14] = 0x03; edid[15] = 0x04;

    // 4. Manufacture week (1) and year (2026 - 1990 = 36 = 0x24)
    edid[16] = 0x01;
    edid[17] = 0x24;

    // 5. EDID Version 1.4
    edid[18] = 0x01;
    edid[19] = 0x04;

    // 6. Basic Display Parameters: Digital input, 8-bit, DisplayPort/HDMI compatible
    edid[20] = 0x80;
    edid[21] = 60; // 60 cm horizontal screen size
    edid[22] = 34; // 34 cm vertical screen size
    edid[23] = 0x78; // Gamma 2.2 ((120 + 100) / 100)
    edid[24] = 0x0A; // Standby, suspend, sRGB

    // 7. Color Characteristics (sRGB Rec.709 chromaticity standard)
    edid[25] = 0xEE; edid[26] = 0x91; edid[27] = 0xA3; edid[28] = 0x54;
    edid[29] = 0x4C; edid[30] = 0x99; edid[31] = 0x26; edid[32] = 0x0F;
    edid[33] = 0x50; edid[34] = 0x54;

    // 8. Established Timings (standard 640x480, 800x600, 1024x768 @ 60Hz)
    edid[35] = 0x21; edid[36] = 0x08; edid[37] = 0x00;

    // 9. Standard Timings (1920x1080 @ 60Hz: 0xD1, 0xC0)
    edid[38] = 0xD1; edid[39] = 0xC0;

    // 10. Detailed Timing Descriptor 1 (Target Native Mode)
    // Pixel clock in 10 kHz units
    uint32_t totalH = width + 160;
    uint32_t totalV = height + 45;
    uint32_t pixelClock10kHz = (totalH * totalV * refreshRate) / 10000;
    edid[54] = static_cast<uint8_t>(pixelClock10kHz & 0xFF);
    edid[55] = static_cast<uint8_t>((pixelClock10kHz >> 8) & 0xFF);

    // H active and blanking
    uint32_t hBlank = totalH - width;
    edid[56] = static_cast<uint8_t>(width & 0xFF);
    edid[57] = static_cast<uint8_t>(hBlank & 0xFF);
    edid[58] = static_cast<uint8_t>(((width >> 8) & 0x0F) << 4 | ((hBlank >> 8) & 0x0F));

    // V active and blanking
    uint32_t vBlank = totalV - height;
    edid[59] = static_cast<uint8_t>(height & 0xFF);
    edid[60] = static_cast<uint8_t>(vBlank & 0xFF);
    edid[61] = static_cast<uint8_t>(((height >> 8) & 0x0F) << 4 | ((vBlank >> 8) & 0x0F));

    // H/V sync offset and pulse
    edid[62] = 0x58; edid[63] = 0x2C; edid[64] = 0x45; edid[65] = 0x00;
    edid[66] = 0x58; edid[67] = 0x22; edid[68] = 0x20; edid[69] = 0x00;
    edid[70] = 0x00; edid[71] = 0x1E;

    // 11. Detailed Timing Descriptor 2 (Monitor Name)
    edid[72] = 0x00; edid[73] = 0x00; edid[74] = 0x00;
    edid[75] = 0xFC; // Monitor name descriptor tag
    edid[76] = 0x00;

    std::string name = monitorName.substr(0, 13);
    for (size_t i = 0; i < 13; ++i) {
        if (i < name.size()) {
            edid[77 + i] = static_cast<uint8_t>(name[i]);
        } else if (i == name.size()) {
            edid[77 + i] = 0x0A; // Newline termination
        } else {
            edid[77 + i] = 0x20; // Space padding
        }
    }

    // 12. Detailed Timing Descriptor 3 (Monitor Range Limits)
    edid[90] = 0x00; edid[91] = 0x00; edid[92] = 0x00;
    edid[93] = 0xFD; // Range limits tag
    edid[94] = 0x00;
    edid[95] = 48;   // Min vertical rate (48 Hz)
    edid[96] = 165;  // Max vertical rate (165 Hz)
    edid[97] = 30;   // Min horizontal rate (30 kHz)
    edid[98] = 255;  // Max horizontal rate (255 kHz)
    edid[99] = 60;   // Max pixel clock (600 MHz / 10)
    edid[100] = 0x00; edid[101] = 0x0A; edid[102] = 0x20;

    // 13. Detailed Timing Descriptor 4 (Serial String)
    edid[108] = 0x00; edid[109] = 0x00; edid[110] = 0x00;
    edid[111] = 0xFF; // Serial number tag
    edid[112] = 0x00;
    std::string serial = "MST-VIRT-001";
    for (size_t i = 0; i < 13; ++i) {
        edid[113 + i] = (i < serial.size()) ? static_cast<uint8_t>(serial[i]) : 0x20;
    }

    // 14. Extension flag
    edid[126] = 0x00;

    // 15. Checksum (byte 127) - ensures sum of all 128 bytes is 0 mod 256
    uint32_t sum = 0;
    for (size_t i = 0; i < 127; ++i) {
        sum += edid[i];
    }
    edid[127] = static_cast<uint8_t>((256 - (sum % 256)) % 256);

    return edid;
}

bool VirtualDisplayManager::validateEdidChecksum(const std::vector<uint8_t>& edid) noexcept {
    if (edid.size() != 128) {
        return false;
    }
    uint32_t sum = std::accumulate(edid.begin(), edid.end(), 0u);
    return (sum % 256) == 0;
}

common::Result<uint32_t> VirtualDisplayManager::plugVirtualDisplay(
    const VirtualDisplayMode& targetMode,
    const std::vector<VirtualDisplayMode>& additionalModes
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (targetMode.width == 0 || targetMode.height == 0 || targetMode.refreshRate == 0) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InvalidArgument,
                             "Invalid display mode resolution or refresh rate");
    }

    uint32_t monitorIndex = nextMonitorIndex_++;

    auto edid = generateEdid14(targetMode.width, targetMode.height, targetMode.refreshRate,
                               "Mist V-Disp " + std::to_string(monitorIndex));

    if (!validateEdidChecksum(edid)) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::InternalError,
                             "Failed to compute valid EDID 1.4 checksum");
    }

#if defined(_WIN32)
    if (!config_.simulationOnly && driverPresent_) {
        HANDLE hDevice = CreateFileA(
            config_.driverDevicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hDevice != INVALID_HANDLE_VALUE) {
            DWORD bytesReturned = 0;
            BOOL ok = DeviceIoControl(
                hDevice,
                IOCTL_IDD_PLUG_MONITOR,
                edid.data(),
                static_cast<DWORD>(edid.size()),
                nullptr,
                0,
                &bytesReturned,
                nullptr
            );
            CloseHandle(hDevice);

            if (!ok) {
                MIST_LOG_WARN(common::SubsystemId::Platform,
                              "DeviceIoControl IOCTL_IDD_PLUG_MONITOR returned error, continuing in virtualized mode");
            }
        }
    }
#endif

    VirtualDisplayInfo info{};
    info.monitorIndex = monitorIndex;
    info.currentMode = targetMode;
    info.supportedModes = additionalModes;
    if (std::find(info.supportedModes.begin(), info.supportedModes.end(), targetMode) == info.supportedModes.end()) {
        info.supportedModes.insert(info.supportedModes.begin(), targetMode);
    }
    info.friendlyName = "Mist Virtual Display " + std::to_string(monitorIndex);
    info.isPlugged = true;
    info.edidBlock = std::move(edid);

    displays_[monitorIndex] = info;

    MIST_LOG_INFO(common::SubsystemId::Platform,
                  "Plugged virtual display #" + std::to_string(monitorIndex) + " (" +
                  std::to_string(targetMode.width) + "x" + std::to_string(targetMode.height) +
                  " @" + std::to_string(targetMode.refreshRate) + "Hz)");

    return monitorIndex;
}

common::Result<void> VirtualDisplayManager::unplugVirtualDisplay(uint32_t monitorIndex) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = displays_.find(monitorIndex);
    if (it == displays_.end()) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NotFound,
                             "Virtual display index " + std::to_string(monitorIndex) + " not found");
    }

#if defined(_WIN32)
    if (!config_.simulationOnly && driverPresent_) {
        HANDLE hDevice = CreateFileA(
            config_.driverDevicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (hDevice != INVALID_HANDLE_VALUE) {
            DWORD bytesReturned = 0;
            DeviceIoControl(
                hDevice,
                IOCTL_IDD_UNPLUG_MONITOR,
                &monitorIndex,
                sizeof(monitorIndex),
                nullptr,
                0,
                &bytesReturned,
                nullptr
            );
            CloseHandle(hDevice);
        }
    }
#endif

    displays_.erase(it);
    MIST_LOG_INFO(common::SubsystemId::Platform, "Unplugged virtual display #" + std::to_string(monitorIndex));
    return common::Result<void>::success();
}

common::Result<void> VirtualDisplayManager::unplugAllVirtualDisplays() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& [idx, _] : displays_) {
#if defined(_WIN32)
        if (!config_.simulationOnly && driverPresent_) {
            HANDLE hDevice = CreateFileA(
                config_.driverDevicePath.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr
            );

            if (hDevice != INVALID_HANDLE_VALUE) {
                DWORD bytesReturned = 0;
                uint32_t monitorIndex = idx;
                DeviceIoControl(
                    hDevice,
                    IOCTL_IDD_UNPLUG_MONITOR,
                    &monitorIndex,
                    sizeof(monitorIndex),
                    nullptr,
                    0,
                    &bytesReturned,
                    nullptr
                );
                CloseHandle(hDevice);
            }
        }
#endif
    }

    displays_.clear();
    return common::Result<void>::success();
}

std::vector<VirtualDisplayInfo> VirtualDisplayManager::getActiveVirtualDisplays() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<VirtualDisplayInfo> list;
    list.reserve(displays_.size());
    for (const auto& [_, info] : displays_) {
        list.push_back(info);
    }
    return list;
}

common::Result<VirtualDisplayInfo> VirtualDisplayManager::getVirtualDisplay(uint32_t monitorIndex) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = displays_.find(monitorIndex);
    if (it == displays_.end()) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NotFound,
                             "Virtual display #" + std::to_string(monitorIndex) + " not found");
    }
    return it->second;
}

} // namespace mist::platform::windows
