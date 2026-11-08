#include "mist/capture/SyntheticFrameGenerator.h"
#include "mist/common/Logger.h"
#include <cmath>
#include <vector>
#include <cstring>

namespace mist::capture {

SyntheticFrameGenerator::SyntheticFrameGenerator() noexcept = default;

#if defined(_WIN32)
common::Result<void> SyntheticFrameGenerator::initialize(ID3D11Device* device, ID3D11DeviceContext* context, uint32_t width, uint32_t height) {
    if (!device || !context) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::InvalidArgument, "Device or context is null");
    }

    device_ = device;
    context_ = context;
    width_ = width;
    height_ = height;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width_;
    desc.Height = height_;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, texture_.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateTexture2D for synthetic frame failed");
    }

    hr = device_->CreateRenderTargetView(reinterpret_cast<ID3D11Resource*>(texture_.Get()), nullptr, rtv_.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Capture, "CreateRenderTargetView failed");
    }

    // Milestone 7: Initialize high-entropy animated pattern texture with rich spatial details
    // to accurately saturate encoder target bitrates (8-15 Mbps) instead of idling at minimum QP.
    const uint32_t extWidth = width_ + 256;
    const uint32_t extHeight = height_ + 256;
    std::vector<uint32_t> patternPixels(extWidth * extHeight);

    // SMPTE-style primary and secondary color palette in BGRA format
    constexpr uint32_t kPalette[8] = {
        0xFFE0E0E0, // White
        0xFF00E0E0, // Yellow (B=0, G=224, R=224)
        0xFFE0E000, // Cyan   (B=224, G=224, R=0)
        0xFF00E000, // Green  (B=0, G=224, R=0)
        0xFFE000E0, // Magenta(B=224, G=0, R=224)
        0xFF0000E0, // Red    (B=0, G=0, R=224)
        0xFFE00000, // Blue   (B=224, G=0, R=0)
        0xFF101010  // Black
    };

    for (uint32_t y = 0; y < extHeight; ++y) {
        uint32_t* row = &patternPixels[y * extWidth];
        for (uint32_t x = 0; x < extWidth; ++x) {
            uint32_t pixel = kPalette[(x * 8 / extWidth) % 8];

            // Overlay 16x16 checkerboard in upper half
            if (y < (extHeight / 2)) {
                bool checker = (((x / 16) + (y / 16)) % 2) != 0;
                if (checker) {
                    pixel ^= 0x00A0A0A0;
                }
            } else {
                // High-frequency pseudo-random noise and diagonal hatch lines in lower half
                uint32_t hash = ((x * 1597334677U) ^ (y * 3812015801U));
                uint32_t noise = (hash >> 24) & 0xFFU;
                bool hatch = ((x + y) % 12) < 2;
                if (hatch) {
                    pixel = 0xFFFFFFFF;
                } else if (noise > 180U) {
                    pixel = 0xFF000000U | (noise << 16U) | (noise << 8U) | noise;
                }
            }
            row[x] = pixel;
        }
    }

    D3D11_TEXTURE2D_DESC patDesc = desc;
    patDesc.Width = extWidth;
    patDesc.Height = extHeight;
    patDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = patternPixels.data();
    initData.SysMemPitch = extWidth * sizeof(uint32_t);

    hr = device_->CreateTexture2D(&patDesc, &initData, patternTexture_.GetAddressOf());
    if (FAILED(hr)) {
        MIST_LOG_WARN(common::SubsystemId::Capture, "Failed to create patternTexture, falling back to clear color.");
    }

    timer_.reset();
    MIST_LOG_INFO(common::SubsystemId::Capture, "Synthetic GPU frame generator initialized (" +
                  std::to_string(width_) + "x" + std::to_string(height_) + " with high-entropy pattern)");
    return common::Result<void>::success();
}

common::Result<CapturedFrame> SyntheticFrameGenerator::generateFrame(uint64_t frameNumber, uint64_t timestampNs) {
    if (!context_ || !texture_) {
        return common::Error(common::SubsystemId::Capture, common::ErrorCode::NotInitialized, "SyntheticFrameGenerator not initialized");
    }

    if (patternTexture_) {
        // Continuous diagonal translation (4 px/frame horizontally, 2 px/frame vertically)
        // completely in GPU VRAM via CopySubresourceRegion with 0 CPU overhead.
        UINT srcX = static_cast<UINT>((frameNumber * 4) % 256);
        UINT srcY = static_cast<UINT>((frameNumber * 2) % 256);
        D3D11_BOX box{ srcX, srcY, 0, srcX + width_, srcY + height_, 1 };
        context_->CopySubresourceRegion(texture_.Get(), 0, 0, 0, 0, patternTexture_.Get(), 0, &box);
    } else if (rtv_) {
        // Dynamic color wave fallback
        float t = static_cast<float>(frameNumber % 120) / 120.0f;
        float r = 0.5f + 0.5f * std::sin(t * 6.2831853f);
        float g = 0.5f + 0.5f * std::sin(t * 6.2831853f + 2.0943951f);
        float b = 0.5f + 0.5f * std::sin(t * 6.2831853f + 4.1887902f);
        const float clearColor[4] = { r, g, b, 1.0f };
        context_->ClearRenderTargetView(rtv_.Get(), clearColor);
    }

    CapturedFrame frame{
        .width = width_,
        .height = height_,
        .format = PixelFormat::B8G8R8A8_UNORM,
        .timestampNs = timestampNs > 0 ? timestampNs : timer_.elapsedNanoseconds(),
        .frameNumber = frameNumber,
        .nativeResource = texture_.Get()
    };

    return frame;
}
#endif

} // namespace mist::capture
