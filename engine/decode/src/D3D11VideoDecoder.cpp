#include "mist/decode/D3D11VideoDecoder.h"
#include "mist/common/Logger.h"
#include <cstring>

#if defined(_WIN32)
#include <initguid.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mfidl.h>
#include <codecapi.h>

// CMSH264DecoderMFT CLSID
#ifndef CLSID_CMSH264DecoderMFT
DEFINE_GUID(CLSID_CMSH264DecoderMFT, 0x62ce7e72, 0x4c71, 0x4d20, 0xb1, 0x5d, 0x45, 0x28, 0x31, 0xa8, 0x8d, 0x9d);
#endif

// CMSH265DecoderMFT CLSID
#ifndef CLSID_CMSH265DecoderMFT
DEFINE_GUID(CLSID_CMSH265DecoderMFT, 0x420a51a3, 0xd601, 0x46cf, 0xb7, 0x4f, 0x56, 0x40, 0x1e, 0x42, 0x05, 0x67);
#endif

// MFVideoFormat_HEVC
#ifndef MFVideoFormat_HEVC
DEFINE_GUID(MFVideoFormat_HEVC, 0x43564548, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
#endif

// CODECAPI_AVLowLatencyMode
#ifndef CODECAPI_AVLowLatencyMode
DEFINE_GUID(CODECAPI_AVLowLatencyMode, 0x9c27891a, 0xed7a, 0x40e1, 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee);
#endif

// D3D11_DECODER_PROFILE_H264_VLD_NOFGT
#ifndef D3D11_DECODER_PROFILE_H264_VLD_NOFGT
DEFINE_GUID(D3D11_DECODER_PROFILE_H264_VLD_NOFGT, 0x1b81be68, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
#endif

// D3D11_DECODER_PROFILE_HEVC_VLD_MAIN
#ifndef D3D11_DECODER_PROFILE_HEVC_VLD_MAIN
DEFINE_GUID(D3D11_DECODER_PROFILE_HEVC_VLD_MAIN, 0x5b11d51b, 0xcd23, 0x45e6, 0xb2, 0x2b, 0x85, 0x8b, 0x5d, 0x03, 0x22, 0x3f);
#endif

// D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10
#ifndef D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10
DEFINE_GUID(D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, 0x107af0e0, 0xef1a, 0x4d42, 0xab, 0xc8, 0x45, 0x49, 0x9b, 0x3d, 0x85, 0x25);
#endif

#endif

namespace mist::decode {

D3D11VideoDecoder::D3D11VideoDecoder() noexcept = default;

D3D11VideoDecoder::~D3D11VideoDecoder() {
    cleanup();
#if defined(_WIN32)
    device_.Reset();
    context_.Reset();
#endif
}

common::Result<std::unique_ptr<D3D11VideoDecoder>> D3D11VideoDecoder::create() {
#if defined(_WIN32)
    return std::make_unique<D3D11VideoDecoder>();
#else
    return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotImplemented,
                         "D3D11VideoDecoder is only supported on Windows");
#endif
}

DecoderCapability D3D11VideoDecoder::queryCapabilities(ID3D11Device* device) noexcept {
    DecoderCapability cap{};
#if defined(_WIN32)
    Microsoft::WRL::ComPtr<ID3D11Device> localDevice;
    if (!device) {
        auto devRes = platform::windows::D3D11DeviceManager::createDevice(0);
        if (devRes) {
            localDevice = devRes.value().first;
            device = localDevice.Get();
        }
    }

    if (!device) {
        return cap;
    }

    auto adapters = platform::windows::D3D11DeviceManager::enumerateAdapters();
    if (!adapters.empty()) {
        cap.deviceName = adapters[0].description;
        cap.driverVersion = "D3D11 Hardware Driver";
    }

    // Check ID3D11VideoDevice interface
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice;
    HRESULT hr = device->QueryInterface(__uuidof(ID3D11VideoDevice), reinterpret_cast<void**>(videoDevice.GetAddressOf()));
    if (SUCCEEDED(hr) && videoDevice) {
        UINT profileCount = videoDevice->GetVideoDecoderProfileCount();
        for (UINT i = 0; i < profileCount; ++i) {
            GUID profileGuid{};
            if (SUCCEEDED(videoDevice->GetVideoDecoderProfile(i, &profileGuid))) {
                if (IsEqualGUID(profileGuid, D3D11_DECODER_PROFILE_H264_VLD_NOFGT)) {
                    cap.supportsH264 = true;
                    cap.isHardwareAccelerated = true;
                } else if (IsEqualGUID(profileGuid, D3D11_DECODER_PROFILE_HEVC_VLD_MAIN) ||
                           IsEqualGUID(profileGuid, D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10)) {
                    cap.supportsHevc = true;
                    cap.isHardwareAccelerated = true;
                }
            }
        }
    }

    cap.maxResolutionWidth = 4096;
    cap.maxResolutionHeight = 4096;
#endif
    return cap;
}

#if defined(_WIN32)
void D3D11VideoDecoder::setDevice(
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    device_ = std::move(device);
    context_ = std::move(context);
}

common::Result<void> D3D11VideoDecoder::initializeWithDevice(
    const DecodeConfig& config,
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
) {
    setDevice(std::move(device), std::move(context));
    return initialize(config);
}
#endif

common::Result<void> D3D11VideoDecoder::initialize(const DecodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup();

    config_ = config;

#if defined(_WIN32)
    (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (!mfStarted_) {
        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "MFStartup failed");
        }
        mfStarted_ = true;
    }

    if (!device_) {
        auto devRes = platform::windows::D3D11DeviceManager::createDevice(0);
        if (!devRes) {
            return devRes.error();
        }
        device_ = devRes.value().first;
        context_ = devRes.value().second;
    }

    auto mftRes = initializeMft();
    if (!mftRes) {
        cleanup();
        return mftRes;
    }

    isInitialized_ = true;
    frameCounter_ = 0;
    MIST_LOG_INFO(common::SubsystemId::Decode, "D3D11 Hardware Video Decoder initialized successfully.");
    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotImplemented,
                         "D3D11VideoDecoder is only supported on Windows");
#endif
}

#if defined(_WIN32)
common::Result<void> D3D11VideoDecoder::initializeMft() {
    GUID targetSubtype = (config_.codec == encode::VideoCodec::HEVC) ? MFVideoFormat_HEVC : MFVideoFormat_H264;
    GUID targetClsid = (config_.codec == encode::VideoCodec::HEVC) ? CLSID_CMSH265DecoderMFT : CLSID_CMSH264DecoderMFT;
    const char* codecName = (config_.codec == encode::VideoCodec::HEVC) ? "HEVC / H.265" : "H.264";

    HRESULT hr = CoCreateInstance(targetClsid, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&decoderTransform_));
    if (FAILED(hr)) {
        MFT_REGISTER_TYPE_INFO inInfo{ MFMediaType_Video, targetSubtype };
        IMFActivate** ppActivate = nullptr;
        UINT32 count = 0;
        hr = MFTEnumEx(
            MFT_CATEGORY_VIDEO_DECODER,
            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_HARDWARE,
            &inInfo,
            nullptr,
            &ppActivate,
            &count
        );
        if (SUCCEEDED(hr) && count > 0 && ppActivate) {
            hr = ppActivate[0]->ActivateObject(IID_PPV_ARGS(&decoderTransform_));
            for (UINT32 i = 0; i < count; ++i) {
                ppActivate[i]->Release();
            }
            CoTaskMemFree(ppActivate);
        }
    }

    if (FAILED(hr) || !decoderTransform_) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode,
            std::string("Failed to instantiate ") + codecName + " Decoder MFT");
    }

    // Initialize DXGI Device Manager for hardware acceleration
    hr = MFCreateDXGIDeviceManager(&dxgiResetToken_, dxgiManager_.GetAddressOf());
    if (SUCCEEDED(hr) && dxgiManager_) {
        hr = dxgiManager_->ResetDevice(device_.Get(), dxgiResetToken_);
        if (SUCCEEDED(hr)) {
            hr = decoderTransform_->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, reinterpret_cast<ULONG_PTR>(dxgiManager_.Get()));
            if (SUCCEEDED(hr)) {
                MIST_LOG_INFO(common::SubsystemId::Decode, std::string("Bound D3D11 DXGI Device Manager to ") + codecName + " Decoder MFT.");
            } else {
                MIST_LOG_WARN(common::SubsystemId::Decode, "ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER) failed, continuing with software decoding fallback.");
            }
        }
    }

    // Set low-latency mode attributes
    Microsoft::WRL::ComPtr<IMFAttributes> attributes;
    if (SUCCEEDED(decoderTransform_->GetAttributes(attributes.GetAddressOf())) && attributes) {
        attributes->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
    }

    // Set input media type
    Microsoft::WRL::ComPtr<IMFMediaType> inputType;
    hr = MFCreateMediaType(inputType.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "MFCreateMediaType input failed");
    }

    inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    inputType->SetGUID(MF_MT_SUBTYPE, targetSubtype);
    MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, width_, height_);

    hr = decoderTransform_->SetInputType(0, inputType.Get(), 0);
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode,
            std::string("SetInputType for ") + codecName + " failed");
    }

    // Query and set output media type (NV12 for D3D11 hardware decoding)
    Microsoft::WRL::ComPtr<IMFMediaType> outputType;
    DWORD typeIndex = 0;
    bool foundNv12 = false;

    while (SUCCEEDED(decoderTransform_->GetOutputAvailableType(0, typeIndex, outputType.ReleaseAndGetAddressOf()))) {
        GUID subtype{};
        if (SUCCEEDED(outputType->GetGUID(MF_MT_SUBTYPE, &subtype))) {
            if (subtype == MFVideoFormat_NV12) {
                foundNv12 = true;
                break;
            }
        }
        ++typeIndex;
    }

    if (!foundNv12) {
        hr = MFCreateMediaType(outputType.GetAddressOf());
        if (FAILED(hr)) {
            return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "MFCreateMediaType output failed");
        }
        outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width_, height_);
    }

    hr = decoderTransform_->SetOutputType(0, outputType.Get(), 0);
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "SetOutputType for NV12 failed");
    }

    decoderTransform_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    decoderTransform_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

    return common::Result<void>::success();
}
#endif

common::Result<void> D3D11VideoDecoder::submitPacket(const encode::EncodedPacket& packet) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotInitialized, "Decoder not initialized");
    }
    if (packet.data.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::InvalidArgument, "Packet data is empty");
    }

#if defined(_WIN32)
    // Create MF buffer
    Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(packet.data.size()), mediaBuffer.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "MFCreateMemoryBuffer failed");
    }

    BYTE* pDst = nullptr;
    hr = mediaBuffer->Lock(&pDst, nullptr, nullptr);
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "mediaBuffer->Lock failed");
    }

    std::memcpy(pDst, packet.data.data(), packet.data.size());
    mediaBuffer->Unlock();
    mediaBuffer->SetCurrentLength(static_cast<DWORD>(packet.data.size()));

    // Create MF sample
    Microsoft::WRL::ComPtr<IMFSample> sample;
    hr = MFCreateSample(sample.GetAddressOf());
    if (FAILED(hr)) {
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "MFCreateSample failed");
    }

    sample->AddBuffer(mediaBuffer.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(packet.timestampNs / 100ULL)); // 100ns units
    sample->SetSampleDuration(166666); // ~60fps
    if (packet.isKeyframe) {
        sample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
    }

    stats_.packetsSubmitted++;
    hr = decoderTransform_->ProcessInput(0, sample.Get(), 0);
    if (FAILED(hr) && hr != MF_E_NOTACCEPTING) {
        stats_.decoderErrors++;
        return platform::windows::hresultToMistError(hr, common::SubsystemId::Decode, "ProcessInput failed");
    }

    // Drain available decoded frames
    auto drainRes = drainOutput();
    if (!drainRes) {
        return drainRes;
    }

    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotImplemented, "Only supported on Windows");
#endif
}

#if defined(_WIN32)
common::Result<void> D3D11VideoDecoder::drainOutput() {
    while (true) {
        MFT_OUTPUT_DATA_BUFFER outputDataBuffer{};
        DWORD status = 0;

        MFT_OUTPUT_STREAM_INFO streamInfo{};
        HRESULT hr = decoderTransform_->GetOutputStreamInfo(0, &streamInfo);
        bool providesSamples = (SUCCEEDED(hr) && (streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES));

        Microsoft::WRL::ComPtr<IMFSample> allocatedSample;
        if (!providesSamples) {
            hr = MFCreateSample(allocatedSample.GetAddressOf());
            if (SUCCEEDED(hr)) {
                Microsoft::WRL::ComPtr<IMFMediaBuffer> allocatedBuffer;
                hr = MFCreateMemoryBuffer(width_ * height_ * 2, allocatedBuffer.GetAddressOf());
                if (SUCCEEDED(hr)) {
                    allocatedSample->AddBuffer(allocatedBuffer.Get());
                    outputDataBuffer.pSample = allocatedSample.Get();
                }
            }
        }

        hr = decoderTransform_->ProcessOutput(0, 1, &outputDataBuffer, &status);
        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
            break; // Finished draining current output
        }

        if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            Microsoft::WRL::ComPtr<IMFMediaType> newType;
            if (SUCCEEDED(decoderTransform_->GetOutputAvailableType(0, 0, newType.GetAddressOf()))) {
                UINT32 w = 0, h = 0;
                if (SUCCEEDED(MFGetAttributeSize(newType.Get(), MF_MT_FRAME_SIZE, &w, &h)) && w > 0 && h > 0) {
                    width_ = w;
                    height_ = h;
                }
                (void)decoderTransform_->SetOutputType(0, newType.Get(), 0);
            }
            continue;
        }

        if (FAILED(hr)) {
            break;
        }

        if (outputDataBuffer.pSample) {
            LONGLONG sampleTimeHns = 0;
            outputDataBuffer.pSample->GetSampleTime(&sampleTimeHns);
            uint64_t timestampNs = static_cast<uint64_t>(sampleTimeHns) * 100ULL;

            Microsoft::WRL::ComPtr<IMFMediaBuffer> sampleBuffer;
            hr = outputDataBuffer.pSample->GetBufferByIndex(0, sampleBuffer.GetAddressOf());
            if (SUCCEEDED(hr) && sampleBuffer) {
                Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgiBuffer;
                hr = sampleBuffer.As(&dxgiBuffer);

                ID3D11Texture2D* rawTexture = nullptr;

                if (SUCCEEDED(hr) && dxgiBuffer) {
                    // True zero-copy: retrieved hardware-decoded DXGI surface from GPU VRAM
                    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3dTexture;
                    if (SUCCEEDED(dxgiBuffer->GetResource(IID_PPV_ARGS(&d3dTexture))) && d3dTexture) {
                        rawTexture = d3dTexture.Get();
                        rawTexture->AddRef();
                    }
                }

                if (!rawTexture && device_) {
                    // Fallback texture creation for system-memory sample buffers
                    D3D11_TEXTURE2D_DESC desc{};
                    desc.Width = width_;
                    desc.Height = height_;
                    desc.MipLevels = 1;
                    desc.ArraySize = 1;
                    desc.Format = DXGI_FORMAT_NV12;
                    desc.SampleDesc.Count = 1;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

                    Microsoft::WRL::ComPtr<ID3D11Texture2D> fallbackTex;
                    if (SUCCEEDED(device_->CreateTexture2D(&desc, nullptr, fallbackTex.GetAddressOf()))) {
                        rawTexture = fallbackTex.Get();
                        rawTexture->AddRef();
                    }
                }

                if (rawTexture) {
                    DecodedFrame frame{
                        .width = width_,
                        .height = height_,
                        .timestampNs = timestampNs,
                        .frameNumber = frameCounter_++,
                        .nativeResource = rawTexture
                    };

                    stats_.framesDecoded++;

                    // Bounded queue: max 2 frames, drop oldest if full
                    while (decodedQueue_.size() >= 2) {
                        auto dropped = decodedQueue_.front();
                        decodedQueue_.pop();
                        stats_.framesQueueDiscarded++;
                        (void)releaseFrame(dropped);
                    }
                    decodedQueue_.push(frame);
                }
            }

            if (providesSamples) {
                outputDataBuffer.pSample->Release();
            }
        }
    }
    return common::Result<void>::success();
}
#endif

common::Result<DecodedFrame> D3D11VideoDecoder::getNextFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::NotInitialized, "Decoder not initialized");
    }

    if (decodedQueue_.empty()) {
        return common::Error(common::SubsystemId::Decode, common::ErrorCode::Timeout, "No decoded frames available");
    }

    DecodedFrame frame = decodedQueue_.front();
    decodedQueue_.pop();
    return frame;
}

common::Result<void> D3D11VideoDecoder::releaseFrame(const DecodedFrame& frame) {
    if (frame.nativeResource) {
#if defined(_WIN32)
        auto tex = static_cast<ID3D11Texture2D*>(frame.nativeResource);
        tex->Release();
#endif
    }
    return common::Result<void>::success();
}

common::Result<void> D3D11VideoDecoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(_WIN32)
    if (decoderTransform_) {
        decoderTransform_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    }
#endif
    while (!decodedQueue_.empty()) {
        auto frame = decodedQueue_.front();
        decodedQueue_.pop();
        (void)releaseFrame(frame);
    }
    return common::Result<void>::success();
}

common::Result<void> D3D11VideoDecoder::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup();
    return common::Result<void>::success();
}

void D3D11VideoDecoder::cleanup() noexcept {
    while (!decodedQueue_.empty()) {
        auto frame = decodedQueue_.front();
        decodedQueue_.pop();
        (void)releaseFrame(frame);
    }
#if defined(_WIN32)
    if (decoderTransform_) {
        decoderTransform_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        decoderTransform_.Reset();
    }
    dxgiManager_.Reset();
#endif
    isInitialized_ = false;
}

D3D11VideoDecoder::DecoderStats D3D11VideoDecoder::getDecoderStats() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace mist::decode
