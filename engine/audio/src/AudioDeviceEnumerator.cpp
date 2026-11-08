#include "mist/audio/AudioDeviceEnumerator.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace mist::audio {

namespace {

// KSDATAFORMAT_SUBTYPE_IEEE_FLOAT: {00000003-0000-0010-8000-00aa00389b71}
static constexpr GUID GUID_IEEE_FLOAT = {
    0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};

std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

AudioDeviceInfo parseDevice(IMMDevice* device, bool isDefault) {
    AudioDeviceInfo info;
    info.isDefault = isDefault;

    LPWSTR strId = nullptr;
    if (SUCCEEDED(device->GetId(&strId)) && strId) {
        info.id = strId;
        CoTaskMemFree(strId);
    }

    ComPtr<IPropertyStore> props;
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
        PROPVARIANT var;
        PropVariantInit(&var);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var))) {
            if (var.vt == VT_LPWSTR && var.pwszVal) {
                info.friendlyName = wideToUtf8(var.pwszVal);
            }
            PropVariantClear(&var);
        }
    }

    if (info.friendlyName.empty()) {
        info.friendlyName = "Generic Audio Device";
    }

    ComPtr<IAudioClient> audioClient;
    if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient))) {
        WAVEFORMATEX* mixFormat = nullptr;
        if (SUCCEEDED(audioClient->GetMixFormat(&mixFormat)) && mixFormat) {
            info.sampleRate = mixFormat->nSamplesPerSec;
            info.channels = mixFormat->nChannels;
            info.bitsPerSample = mixFormat->wBitsPerSample;

            if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
                if (ext->SubFormat == GUID_IEEE_FLOAT) {
                    info.format = AudioSampleFormat::Float32;
                } else {
                    info.format = AudioSampleFormat::Int16;
                }
            } else if (mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
                info.format = AudioSampleFormat::Float32;
            } else {
                info.format = AudioSampleFormat::Int16;
            }

            CoTaskMemFree(mixFormat);
        }
    }

    return info;
}

} // namespace

common::Result<std::vector<AudioDeviceInfo>> AudioDeviceEnumerator::enumerateRenderDevices() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool coInitialized = SUCCEEDED(hr);

    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), &enumerator);
    if (FAILED(hr) || !enumerator) {
        if (coInitialized) CoUninitialize();
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create MMDeviceEnumerator");
    }

    std::wstring defaultId;
    ComPtr<IMMDevice> defaultDevice;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice)) && defaultDevice) {
        LPWSTR strId = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&strId)) && strId) {
            defaultId = strId;
            CoTaskMemFree(strId);
        }
    }

    ComPtr<IMMDeviceCollection> collection;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr) || !collection) {
        if (coInitialized) CoUninitialize();
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to enumerate audio endpoints");
    }

    UINT count = 0;
    collection->GetCount(&count);

    std::vector<AudioDeviceInfo> devices;
    devices.reserve(count);

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> dev;
        if (SUCCEEDED(collection->Item(i, &dev)) && dev) {
            LPWSTR strId = nullptr;
            bool isDef = false;
            if (SUCCEEDED(dev->GetId(&strId)) && strId) {
                isDef = (defaultId == strId);
                CoTaskMemFree(strId);
            }
            devices.push_back(parseDevice(dev.Get(), isDef));
        }
    }

    if (coInitialized) CoUninitialize();
    return devices;
}

common::Result<AudioDeviceInfo> AudioDeviceEnumerator::getDefaultRenderDevice() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool coInitialized = SUCCEEDED(hr);

    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), &enumerator);
    if (FAILED(hr) || !enumerator) {
        if (coInitialized) CoUninitialize();
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::InternalError, "Failed to create MMDeviceEnumerator");
    }

    ComPtr<IMMDevice> defaultDevice;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
    if (FAILED(hr) || !defaultDevice) {
        if (coInitialized) CoUninitialize();
        return common::Error(common::SubsystemId::Audio, common::ErrorCode::NotFound, "No default render endpoint available");
    }

    AudioDeviceInfo info = parseDevice(defaultDevice.Get(), true);
    if (coInitialized) CoUninitialize();
    return info;
}

} // namespace mist::audio
