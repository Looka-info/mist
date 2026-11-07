#include <gtest/gtest.h>
#include "mist/render/D3D11Renderer.h"
#include "mist/platform/windows/D3D11Device.h"

using namespace mist::render;
using namespace mist::decode;
using namespace mist::platform::windows;

TEST(RendererTest, CreateAndInitializeHeadless) {
#if defined(_WIN32)
    auto createRes = D3D11Renderer::create();
    ASSERT_TRUE(createRes.hasValue());
    auto renderer = std::move(createRes.value());

    RenderConfig config{
        .nativeWindowHandle = nullptr,
        .width = 1280,
        .height = 720,
        .vsync = false,
        .headless = true
    };

    auto initRes = renderer->initialize(config);
    ASSERT_TRUE(initRes.hasValue()) << initRes.error().format();
    EXPECT_TRUE(renderer->isInitialized());
    EXPECT_EQ(renderer->getConfig().width, 1280u);
    EXPECT_EQ(renderer->getConfig().height, 720u);

    // Test resize
    auto resizeRes = renderer->resize(1920, 1080);
    EXPECT_TRUE(resizeRes.hasValue());
    EXPECT_EQ(renderer->getConfig().width, 1920u);

    // Test vsync
    auto vsyncRes = renderer->setVsync(true);
    EXPECT_TRUE(vsyncRes.hasValue());
    EXPECT_TRUE(renderer->getConfig().vsync);

    // Test shutdown
    auto shutRes = renderer->shutdown();
    EXPECT_TRUE(shutRes.hasValue());
    EXPECT_FALSE(renderer->isInitialized());
#endif
}

TEST(RendererTest, PresentSyntheticFrameHeadless) {
#if defined(_WIN32)
    auto devRes = D3D11DeviceManager::createDevice(0);
    if (!devRes.hasValue()) {
        GTEST_SKIP() << "Direct3D 11 device creation unavailable in test environment";
    }

    auto createRes = D3D11Renderer::create();
    ASSERT_TRUE(createRes.hasValue());
    auto renderer = std::move(createRes.value());

    RenderConfig config{
        .nativeWindowHandle = nullptr,
        .width = 1920,
        .height = 1080,
        .vsync = false,
        .headless = true
    };

    auto initRes = renderer->initializeWithDevice(config, devRes.value().first, devRes.value().second);
    ASSERT_TRUE(initRes.hasValue()) << initRes.error().format();

    // Create a synthetic NV12 texture
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 1920;
    desc.Height = 1080;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> testTexture;
    HRESULT hr = devRes.value().first->CreateTexture2D(&desc, nullptr, &testTexture);
    ASSERT_TRUE(SUCCEEDED(hr));

    DecodedFrame frame{
        .width = 1920,
        .height = 1080,
        .timestampNs = 1000000,
        .frameNumber = 1,
        .nativeResource = testTexture.Get()
    };

    auto presRes = renderer->present(frame);
    EXPECT_TRUE(presRes.hasValue()) << presRes.error().format();
    EXPECT_EQ(renderer->getFramesPresented(), 1u);
    EXPECT_EQ(renderer->getFramesDropped(), 0u);

    auto shutRes = renderer->shutdown();
    EXPECT_TRUE(shutRes.hasValue());
#endif
}
