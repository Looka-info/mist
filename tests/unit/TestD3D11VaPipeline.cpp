#include <gtest/gtest.h>
#include "mist/decode/D3D11VideoDecoder.h"
#include "mist/render/D3D11Renderer.h"
#include "mist/platform/windows/D3D11Device.h"

using namespace mist::decode;
using namespace mist::render;
using namespace mist::platform::windows;

TEST(D3D11VaPipelineTest, SharedDeviceInitialization) {
    auto devRes = D3D11DeviceManager::createDevice(0);
    ASSERT_TRUE(devRes.hasValue());
    auto sharedDevice = devRes.value().first;
    auto sharedContext = devRes.value().second;
    ASSERT_NE(sharedDevice, nullptr);

    // Initialize Decoder with shared device
    DecodeConfig decConfig{
        .codec = mist::encode::VideoCodec::H264,
        .preferHardwareDecoding = true,
        .lowLatencyTuning = true
    };

    auto decRes = D3D11VideoDecoder::create();
    ASSERT_TRUE(decRes.hasValue());
    auto decoder = std::move(decRes.value());
    ASSERT_TRUE(decoder->initializeWithDevice(decConfig, sharedDevice, sharedContext).hasValue());

    // Initialize Renderer with the same shared device
    RenderConfig rndConfig{
        .width = 1280,
        .height = 720,
        .vsync = false,
        .headless = true
    };

    auto rndRes = D3D11Renderer::create();
    ASSERT_TRUE(rndRes.hasValue());
    auto renderer = std::move(rndRes.value());
    ASSERT_TRUE(renderer->initializeWithDevice(rndConfig, sharedDevice, sharedContext).hasValue());

    // Verify both subsystems share the exact same GPU device
    EXPECT_EQ(decoder->getD3D11Device(), sharedDevice.Get());
    EXPECT_EQ(renderer->getD3D11Device(), sharedDevice.Get());
    EXPECT_TRUE(decoder->isZeroCopyEnabled());
}

TEST(D3D11VaPipelineTest, ZeroCopyGpuSurfacePresentation) {
    auto devRes = D3D11DeviceManager::createDevice(0);
    ASSERT_TRUE(devRes.hasValue());
    auto device = devRes.value().first;
    auto context = devRes.value().second;

    RenderConfig rndConfig{
        .width = 1280,
        .height = 720,
        .vsync = false,
        .headless = true
    };

    auto rndRes = D3D11Renderer::create();
    ASSERT_TRUE(rndRes.hasValue());
    auto renderer = std::move(rndRes.value());
    ASSERT_TRUE(renderer->initializeWithDevice(rndConfig, device, context).hasValue());

    // Allocate GPU VRAM texture in NV12 format (standard hardware decoder output)
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 1280;
    texDesc.Height = 720;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_NV12;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> gpuTexture;
    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, gpuTexture.GetAddressOf());
    ASSERT_TRUE(SUCCEEDED(hr));
    ASSERT_NE(gpuTexture, nullptr);

    // Construct DecodedFrame referencing native GPU VRAM texture
    DecodedFrame frame{
        .width = 1280,
        .height = 720,
        .timestampNs = 16666666ULL,
        .frameNumber = 1,
        .nativeResource = gpuTexture.Get()
    };

    // Direct zero-copy presentation into renderer swapchain
    auto presRes = renderer->present(frame);
    EXPECT_TRUE(presRes.hasValue());
    EXPECT_EQ(renderer->getFramesPresented(), 1u);
    EXPECT_EQ(renderer->getFramesDropped(), 0u);

    // Present latency should be tracked
    EXPECT_GE(renderer->getPresentLatencyUs(), 0.0);
}

TEST(D3D11VaPipelineTest, SwapchainResizeAndVsyncControl) {
    auto devRes = D3D11DeviceManager::createDevice(0);
    ASSERT_TRUE(devRes.hasValue());

    RenderConfig rndConfig{
        .width = 1280,
        .height = 720,
        .vsync = false,
        .headless = true
    };

    auto rndRes = D3D11Renderer::create();
    ASSERT_TRUE(rndRes.hasValue());
    auto renderer = std::move(rndRes.value());
    ASSERT_TRUE(renderer->initializeWithDevice(rndConfig, devRes.value().first, devRes.value().second).hasValue());

    // Toggle VSync
    EXPECT_TRUE(renderer->setVsync(true).hasValue());
    EXPECT_TRUE(renderer->getConfig().vsync);
    EXPECT_TRUE(renderer->setVsync(false).hasValue());
    EXPECT_FALSE(renderer->getConfig().vsync);

    // Resize GPU swapchain
    EXPECT_TRUE(renderer->resize(1920, 1080).hasValue());
    EXPECT_EQ(renderer->getConfig().width, 1920u);
    EXPECT_EQ(renderer->getConfig().height, 1080u);

    EXPECT_TRUE(renderer->shutdown().hasValue());
    EXPECT_FALSE(renderer->isInitialized());
}
