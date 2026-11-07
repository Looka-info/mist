#pragma once

#include "mist/common/Result.h"
#include "mist/decode/IVideoDecoder.h"
#include <cstdint>

namespace mist::render {

struct RenderConfig {
    void* nativeWindowHandle{nullptr}; // HWND on Windows
    uint32_t width{1920};
    uint32_t height{1080};
    bool vsync{false};
    bool enableHdr{false};
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    [[nodiscard]] virtual common::Result<void> initialize(const RenderConfig& config) = 0;
    [[nodiscard]] virtual common::Result<void> present(const decode::DecodedFrame& frame) = 0;
    [[nodiscard]] virtual common::Result<void> resize(uint32_t width, uint32_t height) = 0;
    [[nodiscard]] virtual common::Result<void> setVsync(bool enabled) = 0;
    [[nodiscard]] virtual common::Result<void> shutdown() = 0;
};

} // namespace mist::render
