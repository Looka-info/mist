#pragma once

#include "mist/common/Result.h"
#include <string>
#include <vector>
#include <cstdint>
#include <utility>

#if defined(_WIN32)
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#endif

namespace mist::platform::windows {

struct DisplayInfo {
    uint32_t index{0};
    uint32_t adapterIndex{0};
    std::string deviceName;
    std::string adapterName;
    uint32_t width{0};
    uint32_t height{0};
    uint32_t refreshRateNumerator{60};
    uint32_t refreshRateDenominator{1};
    int32_t rotationDegrees{0};
    bool isPrimary{false};
    bool isAttachedToDesktop{false};
};

struct AdapterInfo {
    uint32_t adapterIndex{0};
    std::string description;
    uint64_t dedicatedVideoMemoryBytes{0};
    uint32_t vendorId{0};
    uint32_t deviceId{0};
};

class D3D11DeviceManager {
public:
    [[nodiscard]] static std::vector<AdapterInfo> enumerateAdapters();
    [[nodiscard]] static std::vector<DisplayInfo> enumerateDisplays();

#if defined(_WIN32)
    [[nodiscard]] static common::Result<std::pair<Microsoft::WRL::ComPtr<ID3D11Device>, Microsoft::WRL::ComPtr<ID3D11DeviceContext>>>
    createDevice(uint32_t adapterIndex = 0);

    [[nodiscard]] static common::Result<std::pair<Microsoft::WRL::ComPtr<ID3D11Device>, Microsoft::WRL::ComPtr<ID3D11DeviceContext>>>
    createDeviceForDisplay(uint32_t displayIndex);

    [[nodiscard]] static common::Result<Microsoft::WRL::ComPtr<IDXGIOutput1>>
    getOutputForDisplay(uint32_t displayIndex);
#endif
};

} // namespace mist::platform::windows
