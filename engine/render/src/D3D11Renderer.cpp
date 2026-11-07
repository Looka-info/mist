#include "mist/render/D3D11Renderer.h"
#include "mist/common/Logger.h"
#include <algorithm>

namespace mist::render {

#if defined(_WIN32)
namespace {

LRESULT CALLBACK RendererWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace
#endif

D3D11Renderer::D3D11Renderer() noexcept = default;

D3D11Renderer::~D3D11Renderer() {
    (void)shutdown();
}

common::Result<std::unique_ptr<D3D11Renderer>> D3D11Renderer::create() {
#if defined(_WIN32)
    return std::make_unique<D3D11Renderer>();
#else
    return common::Error(common::SubsystemId::Render, common::ErrorCode::NotImplemented, "D3D11Renderer is only supported on Windows");
#endif
}

#if defined(_WIN32)
common::Result<void> D3D11Renderer::initializeWithDevice(
    const RenderConfig& config,
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        device_ = std::move(device);
        context_ = std::move(context);
    }
    return initialize(config);
}
#endif

common::Result<void> D3D11Renderer::initialize(const RenderConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup();

    config_ = config;

#if defined(_WIN32)
    if (!device_) {
        auto devRes = platform::windows::D3D11DeviceManager::createDevice(0);
        if (!devRes) {
            return devRes.error();
        }
        device_ = devRes.value().first;
        context_ = devRes.value().second;
    }

    if (!config_.headless) {
        if (config_.nativeWindowHandle) {
            hwnd_ = static_cast<HWND>(config_.nativeWindowHandle);
            ownsHwnd_ = false;
        } else {
            auto winRes = createRendererWindow();
            if (!winRes) {
                return winRes;
            }
        }

        auto swapRes = createSwapChain();
        if (!swapRes) {
            return swapRes;
        }
    } else {
        // Headless render target allocation (GPU-resident offscreen backbuffer)
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = config_.width;
        desc.Height = config_.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device_->CreateTexture2D(&desc, nullptr, backBuffer_.GetAddressOf());
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "CreateTexture2D for headless backbuffer failed");
        }

        hr = device_->CreateRenderTargetView(reinterpret_cast<ID3D11Resource*>(backBuffer_.Get()), nullptr, rtv_.GetAddressOf());
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "CreateRenderTargetView for headless backbuffer failed");
        }
    }

    (void)initVideoProcessor();

    isInitialized_.store(true);
    framesPresented_ = 0;
    framesDropped_ = 0;
    timer_.reset();

    MIST_LOG_INFO(common::SubsystemId::Render, "D3D11 Presentation Renderer initialized (" +
                  std::to_string(config_.width) + "x" + std::to_string(config_.height) +
                  (config_.headless ? ", Headless" : ", Flip-Model Window") +
                  (config_.vsync ? ", VSync ON" : ", VSync OFF") + ")");
    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Render, common::ErrorCode::NotImplemented, "D3D11Renderer is only supported on Windows");
#endif
}

#if defined(_WIN32)
common::Result<void> D3D11Renderer::createRendererWindow() {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"MistRendererWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = RendererWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = className;

    RegisterClassExW(&wc);

    RECT rc = { 0, 0, static_cast<LONG>(config_.width), static_cast<LONG>(config_.height) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExW(
        0, className, L"Mist Client — Direct3D 11 Stream Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd_) {
        return platform::windows::win32ErrorToMistError(GetLastError(), common::SubsystemId::Render, "CreateWindowExW failed");
    }

    ShowWindow(hwnd_, SW_SHOWNORMAL);
    UpdateWindow(hwnd_);
    ownsHwnd_ = true;
    return common::Result<void>::success();
}

common::Result<void> D3D11Renderer::createSwapChain() {
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.GetAddressOf()));
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "QueryInterface IDXGIDevice failed");
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "GetAdapter failed");
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "GetParent IDXGIFactory2 failed");
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = config_.width;
    desc.Height = config_.height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2; // Double-buffering for Flip-Model
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = 0;

    // Check tearing support for low-latency variable refresh rate
    BOOL allowTearing = FALSE;
    Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory.As(&factory5))) {
        if (SUCCEEDED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
            tearingSupported_ = (allowTearing == TRUE);
        }
    }
    if (tearingSupported_) {
        desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    hr = factory->CreateSwapChainForHwnd(
        reinterpret_cast<IUnknown*>(device_.Get()),
        hwnd_,
        &desc,
        nullptr,
        nullptr,
        swapChain_.GetAddressOf()
    );

    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "CreateSwapChainForHwnd failed");
    }

    (void)factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);

    hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer_));
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "swapChain_->GetBuffer failed");
    }

    hr = device_->CreateRenderTargetView(reinterpret_cast<ID3D11Resource*>(backBuffer_.Get()), nullptr, rtv_.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "CreateRenderTargetView failed");
    }

    return common::Result<void>::success();
}

common::Result<void> D3D11Renderer::initVideoProcessor() {
    HRESULT hr = device_->QueryInterface(__uuidof(ID3D11VideoDevice), reinterpret_cast<void**>(videoDevice_.GetAddressOf()));
    if (FAILED(hr)) {
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    hr = context_->QueryInterface(__uuidof(ID3D11VideoContext), reinterpret_cast<void**>(videoContext_.GetAddressOf()));
    if (FAILED(hr)) {
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc{};
    contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    contentDesc.InputFrameRate.Numerator = 60;
    contentDesc.InputFrameRate.Denominator = 1;
    contentDesc.InputWidth = config_.width;
    contentDesc.InputHeight = config_.height;
    contentDesc.OutputWidth = config_.width;
    contentDesc.OutputHeight = config_.height;
    contentDesc.OutputFrameRate.Numerator = 60;
    contentDesc.OutputFrameRate.Denominator = 1;
    contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = videoDevice_->CreateVideoProcessorEnumerator(&contentDesc, vpEnum_.GetAddressOf());
    if (FAILED(hr)) {
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    hr = videoDevice_->CreateVideoProcessor(vpEnum_.Get(), 0, videoProcessor_.GetAddressOf());
    if (FAILED(hr)) {
        useVideoProcessor_ = false;
        return common::Result<void>::success();
    }

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outDesc{};
    outDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outDesc.Texture2D.MipSlice = 0;

    hr = videoDevice_->CreateVideoProcessorOutputView(
        reinterpret_cast<ID3D11Resource*>(backBuffer_.Get()),
        vpEnum_.Get(),
        &outDesc,
        vpOutputView_.GetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        useVideoProcessor_ = true;
    }

    return common::Result<void>::success();
}

void D3D11Renderer::renderLetterboxed(ID3D11Texture2D* sourceTexture, uint32_t srcWidth, uint32_t srcHeight) {
    if (!sourceTexture || !videoProcessor_ || !vpOutputView_) return;

    // Aspect ratio letterboxing calculation
    double srcAspect = (srcHeight > 0) ? static_cast<double>(srcWidth) / srcHeight : 1.7777;
    double dstAspect = (config_.height > 0) ? static_cast<double>(config_.width) / config_.height : 1.7777;

    RECT targetRect{};
    if (srcAspect > dstAspect) {
        // Pillarboxing / Letterboxing top & bottom
        LONG targetH = static_cast<LONG>(config_.width / srcAspect);
        LONG marginY = (static_cast<LONG>(config_.height) - targetH) / 2;
        targetRect = { 0, marginY, static_cast<LONG>(config_.width), marginY + targetH };
    } else {
        // Pillarboxing left & right
        LONG targetW = static_cast<LONG>(config_.height * srcAspect);
        LONG marginX = (static_cast<LONG>(config_.width) - targetW) / 2;
        targetRect = { marginX, 0, marginX + targetW, static_cast<LONG>(config_.height) };
    }

    videoContext_->VideoProcessorSetOutputTargetRect(videoProcessor_.Get(), TRUE, &targetRect);

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inDesc{};
    inDesc.FourCC = 0;
    inDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inDesc.Texture2D.MipSlice = 0;
    inDesc.Texture2D.ArraySlice = 0;

    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> inView;
    HRESULT hr = videoDevice_->CreateVideoProcessorInputView(
        reinterpret_cast<ID3D11Resource*>(sourceTexture),
        vpEnum_.Get(),
        &inDesc,
        inView.GetAddressOf()
    );

    if (SUCCEEDED(hr) && inView) {
        D3D11_VIDEO_PROCESSOR_STREAM stream{};
        stream.Enable = TRUE;
        stream.pInputSurface = inView.Get();

        (void)videoContext_->VideoProcessorBlt(videoProcessor_.Get(), vpOutputView_.Get(), 0, 1, &stream);
    }
}
#endif

common::Result<void> D3D11Renderer::present(const decode::DecodedFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_.load()) {
        return common::Error(common::SubsystemId::Render, common::ErrorCode::NotInitialized, "Renderer not initialized");
    }

    platform::windows::HighResolutionTimer qpc;

#if defined(_WIN32)
    // Clear solid black background for clean letterboxing
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (rtv_) {
        context_->ClearRenderTargetView(rtv_.Get(), clearColor);
    }

    if (frame.nativeResource) {
        auto d3dTexture = static_cast<ID3D11Texture2D*>(frame.nativeResource);
        if (useVideoProcessor_) {
            renderLetterboxed(d3dTexture, frame.width, frame.height);
        } else if (backBuffer_) {
            context_->CopyResource(reinterpret_cast<ID3D11Resource*>(backBuffer_.Get()),
                                   reinterpret_cast<ID3D11Resource*>(d3dTexture));
        }
    }

    if (swapChain_) {
        UINT syncInterval = config_.vsync ? 1 : 0;
        UINT flags = (syncInterval == 0 && tearingSupported_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        HRESULT hr = swapChain_->Present(syncInterval, flags);
        if (FAILED(hr)) {
            if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
                MIST_LOG_ERROR(common::SubsystemId::Render, "Direct3D device removed or reset during Present");
                return common::Error(common::SubsystemId::Render, common::ErrorCode::HardwareUnavailable, "D3D11 device lost");
            }
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "Present failed");
        }
    }
#endif

    lastPresentLatencyUs_ = qpc.elapsedMicroseconds();
    framesPresented_.fetch_add(1, std::memory_order_relaxed);
    return common::Result<void>::success();
}

common::Result<void> D3D11Renderer::resize(uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_.load()) return common::Result<void>::success();

    config_.width = width;
    config_.height = height;

#if defined(_WIN32)
    if (context_) {
        context_->OMSetRenderTargets(0, nullptr, nullptr);
    }

    rtv_.Reset();
    vpOutputView_.Reset();
    backBuffer_.Reset();

    if (swapChain_) {
        UINT flags = tearingSupported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, flags);
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "ResizeBuffers failed");
        }

        hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer_));
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "GetBuffer after resize failed");
        }

        hr = device_->CreateRenderTargetView(reinterpret_cast<ID3D11Resource*>(backBuffer_.Get()), nullptr, rtv_.GetAddressOf());
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Render, "CreateRenderTargetView after resize failed");
        }
    }

    (void)initVideoProcessor();
#endif

    MIST_LOG_INFO(common::SubsystemId::Render, "D3D11 Renderer resized to " + std::to_string(width) + "x" + std::to_string(height));
    return common::Result<void>::success();
}

common::Result<void> D3D11Renderer::setVsync(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.vsync = enabled;
    return common::Result<void>::success();
}

common::Result<void> D3D11Renderer::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup();
    return common::Result<void>::success();
}

void D3D11Renderer::cleanup() noexcept {
#if defined(_WIN32)
    if (context_) {
        context_->OMSetRenderTargets(0, nullptr, nullptr);
    }
    vpOutputView_.Reset();
    videoProcessor_.Reset();
    vpEnum_.Reset();
    videoContext_.Reset();
    videoDevice_.Reset();

    rtv_.Reset();
    backBuffer_.Reset();
    swapChain_.Reset();

    if (ownsHwnd_ && hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        ownsHwnd_ = false;
    }
#endif
    isInitialized_.store(false);
}

} // namespace mist::render
