#include "mist/platform/windows/D3D11Device.h"
#include "mist/platform/windows/PlatformWindows.h"
#include "mist/common/Logger.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mist::platform::windows {

#if defined(_WIN32)
static std::string wideToUtf8(const WCHAR* wideStr) {
    if (!wideStr || wideStr[0] == L'\0') return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) return "";
    std::string result(static_cast<size_t>(sizeNeeded - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, result.data(), sizeNeeded, nullptr, nullptr);
    return result;
}
#endif

std::vector<AdapterInfo> D3D11DeviceManager::enumerateAdapters() {
    std::vector<AdapterInfo> adapters;
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        MIST_LOG_ERROR(common::SubsystemId::Platform, "Failed to create DXGI factory 1");
        return adapters;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            // Ignore Microsoft Basic Render Driver software adapter if dedicated hardware is available
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && i > 0) {
                continue;
            }
            adapters.push_back(AdapterInfo{
                .adapterIndex = i,
                .description = wideToUtf8(desc.Description),
                .dedicatedVideoMemoryBytes = static_cast<uint64_t>(desc.DedicatedVideoMemory),
                .vendorId = desc.VendorId,
                .deviceId = desc.DeviceId
            });
        }
    }
#endif
    return adapters;
}

std::vector<DisplayInfo> D3D11DeviceManager::enumerateDisplays() {
    std::vector<DisplayInfo> displays;
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        MIST_LOG_ERROR(common::SubsystemId::Platform, "Failed to create DXGI factory for display enumeration");
        return displays;
    }

    uint32_t globalDisplayIndex = 0;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

    for (UINT adapterIdx = 0; factory->EnumAdapters1(adapterIdx, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++adapterIdx) {
        DXGI_ADAPTER_DESC1 adapterDesc{};
        std::string adapterName;
        if (SUCCEEDED(adapter->GetDesc1(&adapterDesc))) {
            adapterName = wideToUtf8(adapterDesc.Description);
        }

        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        for (UINT outputIdx = 0; adapter->EnumOutputs(outputIdx, output.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++outputIdx) {
            DXGI_OUTPUT_DESC outputDesc{};
            if (SUCCEEDED(output->GetDesc(&outputDesc))) {
                int rotationDeg = 0;
                switch (outputDesc.Rotation) {
                    case DXGI_MODE_ROTATION_ROTATE90:  rotationDeg = 90; break;
                    case DXGI_MODE_ROTATION_ROTATE180: rotationDeg = 180; break;
                    case DXGI_MODE_ROTATION_ROTATE270: rotationDeg = 270; break;
                    default: rotationDeg = 0; break;
                }

                uint32_t width = static_cast<uint32_t>(outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left);
                uint32_t height = static_cast<uint32_t>(outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top);

                bool isPrimary = (outputDesc.DesktopCoordinates.left == 0 && outputDesc.DesktopCoordinates.top == 0);

                displays.push_back(DisplayInfo{
                    .index = globalDisplayIndex++,
                    .adapterIndex = adapterIdx,
                    .deviceName = wideToUtf8(outputDesc.DeviceName),
                    .adapterName = adapterName,
                    .width = width,
                    .height = height,
                    .refreshRateNumerator = 60,
                    .refreshRateDenominator = 1,
                    .rotationDegrees = rotationDeg,
                    .isPrimary = isPrimary,
                    .isAttachedToDesktop = outputDesc.AttachedToDesktop != FALSE
                });
            }
        }
    }
#endif
    return displays;
}

#if defined(_WIN32)
common::Result<std::pair<Microsoft::WRL::ComPtr<ID3D11Device>, Microsoft::WRL::ComPtr<ID3D11DeviceContext>>>
D3D11DeviceManager::createDevice(uint32_t adapterIndex) {
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        return hresultToMistError(hr, common::SubsystemId::Platform, "CreateDXGIFactory1 failed");
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    hr = factory->EnumAdapters1(adapterIndex, adapter.GetAddressOf());
    if (FAILED(hr)) {
        return hresultToMistError(hr, common::SubsystemId::Platform, "Invalid adapter index");
    }

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1
    };
    D3D_FEATURE_LEVEL actualFeatureLevel;

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    // Uncomment for D3D11 debug layer when SDK layers are installed
    // creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

    hr = D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN, // Required when adapter is non-null
        nullptr,
        creationFlags,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        device.GetAddressOf(),
        &actualFeatureLevel,
        context.GetAddressOf()
    );

    if (FAILED(hr)) {
        return hresultToMistError(hr, common::SubsystemId::Platform, "D3D11CreateDevice failed");
    }

    return std::make_pair(std::move(device), std::move(context));
}

common::Result<std::pair<Microsoft::WRL::ComPtr<ID3D11Device>, Microsoft::WRL::ComPtr<ID3D11DeviceContext>>>
D3D11DeviceManager::createDeviceForDisplay(uint32_t displayIndex) {
    auto displays = enumerateDisplays();
    if (displayIndex >= displays.size()) {
        return common::Error(common::SubsystemId::Platform, common::ErrorCode::NotFound,
                             "Display index out of range: " + std::to_string(displayIndex));
    }
    return createDevice(displays[displayIndex].adapterIndex);
}

common::Result<Microsoft::WRL::ComPtr<IDXGIOutput1>>
D3D11DeviceManager::getOutputForDisplay(uint32_t displayIndex) {
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        return hresultToMistError(hr, common::SubsystemId::Platform, "CreateDXGIFactory1 failed");
    }

    uint32_t currentIndex = 0;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

    for (UINT adapterIdx = 0; factory->EnumAdapters1(adapterIdx, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++adapterIdx) {
        Microsoft::WRL::ComPtr<IDXGIOutput> output;
        for (UINT outputIdx = 0; adapter->EnumOutputs(outputIdx, output.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++outputIdx) {
            if (currentIndex == displayIndex) {
                Microsoft::WRL::ComPtr<IDXGIOutput1> output1;
                hr = output.As(&output1);
                if (FAILED(hr)) {
                    return hresultToMistError(hr, common::SubsystemId::Platform, "Failed to query IDXGIOutput1");
                }
                return output1;
            }
            ++currentIndex;
        }
    }

    return common::Error(common::SubsystemId::Platform, common::ErrorCode::NotFound,
                         "Display output index not found: " + std::to_string(displayIndex));
}
#endif

} // namespace mist::platform::windows
