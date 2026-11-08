#pragma once

#include "mist/audio/AudioResampler.h"
#include <string>
#include <vector>

namespace mist::audio {

struct AudioDeviceInfo {
    std::wstring id;
    std::string friendlyName;
    bool isDefault{false};
    uint32_t sampleRate{48000};
    uint16_t channels{2};
    AudioSampleFormat format{AudioSampleFormat::Float32};
    uint16_t bitsPerSample{32};
};

class AudioDeviceEnumerator {
public:
    static common::Result<std::vector<AudioDeviceInfo>> enumerateRenderDevices();
    static common::Result<AudioDeviceInfo> getDefaultRenderDevice();
};

} // namespace mist::audio
