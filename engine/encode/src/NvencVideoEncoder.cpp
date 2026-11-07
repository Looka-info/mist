#include "mist/encode/NvencVideoEncoder.h"
#include "mist/common/Logger.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
#include "nvEncodeAPI.h"
typedef NVENCSTATUS(NVENCAPI* PNVENCODEAPICREATEINSTANCE)(NV_ENCODE_API_FUNCTION_LIST*);
#endif

namespace mist::encode {

#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
static NV_ENCODE_API_FUNCTION_LIST g_nvenc{};
static bool g_nvencLoaded = false;
static HMODULE g_hNvencDll = nullptr;

static common::Result<void> ensureNvencLoaded() {
    if (g_nvencLoaded) {
        return common::Result<void>::success();
    }

    g_hNvencDll = LoadLibraryA("nvEncodeAPI64.dll");
    if (!g_hNvencDll) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::HardwareUnavailable,
                             "NVIDIA NVENC driver library (nvEncodeAPI64.dll) could not be loaded. Please verify NVIDIA driver installation.");
    }

    auto createInstance = reinterpret_cast<PNVENCODEAPICREATEINSTANCE>(
        reinterpret_cast<void*>(GetProcAddress(g_hNvencDll, "NvEncodeAPICreateInstance"))
    );
    if (!createInstance) {
        FreeLibrary(g_hNvencDll);
        g_hNvencDll = nullptr;
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::HardwareUnavailable,
                             "Symbol NvEncodeAPICreateInstance not found in nvEncodeAPI64.dll");
    }

    g_nvenc = NV_ENCODE_API_FUNCTION_LIST{};
    g_nvenc.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NVENCSTATUS status = createInstance(&g_nvenc);
    if (status != NV_ENC_SUCCESS) {
        FreeLibrary(g_hNvencDll);
        g_hNvencDll = nullptr;
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::HardwareUnavailable,
                             "NvEncodeAPICreateInstance failed with status " + std::to_string(status));
    }

    g_nvencLoaded = true;
    MIST_LOG_INFO(common::SubsystemId::Encode, "Successfully loaded NVIDIA NVENC API version " +
                  std::to_string(NVENCAPI_MAJOR_VERSION) + "." + std::to_string(NVENCAPI_MINOR_VERSION));
    return common::Result<void>::success();
}
#endif

NvencVideoEncoder::NvencVideoEncoder() noexcept = default;

NvencVideoEncoder::~NvencVideoEncoder() {
    cleanup();
#if defined(_WIN32)
    device_.Reset();
    context_.Reset();
#endif
}

common::Result<std::unique_ptr<NvencVideoEncoder>> NvencVideoEncoder::create() {
#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
    auto loadRes = ensureNvencLoaded();
    if (!loadRes) {
        return loadRes.error();
    }
    return std::make_unique<NvencVideoEncoder>();
#else
    return common::Error(common::SubsystemId::Encode, common::ErrorCode::HardwareUnavailable,
                         "NVENC support was not compiled in or NVIDIA Video Codec SDK headers were unavailable during build.");
#endif
}

NvencCapability NvencVideoEncoder::queryCapabilities(ID3D11Device* device) noexcept {
    NvencCapability cap{};
#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
    auto loadRes = ensureNvencLoaded();
    if (!loadRes) {
        return cap;
    }

    Microsoft::WRL::ComPtr<ID3D11Device> localDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> localContext;
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

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openParams{};
    openParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    openParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    openParams.device = device;
    openParams.apiVersion = NVENCAPI_VERSION;

    void* session = nullptr;
    NVENCSTATUS status = g_nvenc.nvEncOpenEncodeSessionEx(&openParams, &session);
    if (status != NV_ENC_SUCCESS || !session) {
        return cap;
    }

    cap.isSupported = true;

    // Check H.264 support
    uint32_t profileCount = 0;
    status = g_nvenc.nvEncGetEncodeProfileGUIDCount(session, NV_ENC_CODEC_H264_GUID, &profileCount);
    if (status == NV_ENC_SUCCESS && profileCount > 0) {
        cap.supportsH264 = true;
    }

    // Check HEVC support
    uint32_t hevcProfileCount = 0;
    status = g_nvenc.nvEncGetEncodeProfileGUIDCount(session, NV_ENC_CODEC_HEVC_GUID, &hevcProfileCount);
    if (status == NV_ENC_SUCCESS && hevcProfileCount > 0) {
        cap.supportsHevc = true;
    }

    // Check supported input formats
    uint32_t formatCount = 0;
    status = g_nvenc.nvEncGetInputFormatCount(session, NV_ENC_CODEC_H264_GUID, &formatCount);
    if (status == NV_ENC_SUCCESS && formatCount > 0) {
        std::vector<NV_ENC_BUFFER_FORMAT> formats(formatCount);
        uint32_t formatsFilled = 0;
        status = g_nvenc.nvEncGetInputFormats(session, NV_ENC_CODEC_H264_GUID, formats.data(), formatCount, &formatsFilled);
        if (status == NV_ENC_SUCCESS) {
            for (uint32_t i = 0; i < formatsFilled; ++i) {
                if (formats[i] == NV_ENC_BUFFER_FORMAT_ARGB || formats[i] == NV_ENC_BUFFER_FORMAT_ABGR) {
                    cap.supportsBgraInput = true;
                }
                if (formats[i] == NV_ENC_BUFFER_FORMAT_NV12) {
                    cap.supportsNv12Input = true;
                }
            }
        }
    }

    // Query max resolution
    NV_ENC_CAPS_PARAM capsParam{};
    capsParam.version = NV_ENC_CAPS_PARAM_VER;
    capsParam.capsToQuery = NV_ENC_CAPS_WIDTH_MAX;
    int maxWidth = 4096;
    g_nvenc.nvEncGetEncodeCaps(session, NV_ENC_CODEC_H264_GUID, &capsParam, &maxWidth);
    cap.maxResolutionWidth = static_cast<uint32_t>(maxWidth);

    capsParam.capsToQuery = NV_ENC_CAPS_HEIGHT_MAX;
    int maxHeight = 4096;
    g_nvenc.nvEncGetEncodeCaps(session, NV_ENC_CODEC_H264_GUID, &capsParam, &maxHeight);
    cap.maxResolutionHeight = static_cast<uint32_t>(maxHeight);

    g_nvenc.nvEncDestroyEncoder(session);
#endif
    return cap;
}

#if defined(_WIN32)
common::Result<void> NvencVideoEncoder::initializeWithDevice(
    const EncodeConfig& config,
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
) {
    setDevice(std::move(device), std::move(context));
    return initialize(config);
}

void NvencVideoEncoder::setDevice(
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    device_ = std::move(device);
    context_ = std::move(context);
}
#endif

common::Result<void> NvencVideoEncoder::initialize(const EncodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    cleanup();

    config_ = config;

#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
    auto loadRes = ensureNvencLoaded();
    if (!loadRes) {
        return loadRes;
    }

    // Create D3D11 device if not already provided
    if (!device_) {
        auto devRes = platform::windows::D3D11DeviceManager::createDevice(0);
        if (!devRes) {
            return devRes.error();
        }
        device_ = devRes.value().first;
        context_ = devRes.value().second;
    }

    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openParams{};
    openParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    openParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    openParams.device = device_.Get();
    openParams.apiVersion = NVENCAPI_VERSION;

    NVENCSTATUS status = g_nvenc.nvEncOpenEncodeSessionEx(&openParams, &encoderSession_);
    if (status != NV_ENC_SUCCESS || !encoderSession_) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::HardwareUnavailable,
                             "nvEncOpenEncodeSessionEx failed with status " + std::to_string(status));
    }

    // Configure modern low-latency preset: P4, Ultra-low-latency tuning
    GUID presetGuid = NV_ENC_PRESET_P4_GUID;
    GUID encodeCodecGuid = (config_.codec == VideoCodec::HEVC) ? NV_ENC_CODEC_HEVC_GUID : NV_ENC_CODEC_H264_GUID;

    NV_ENC_PRESET_CONFIG presetConfig{};
    presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;

    status = g_nvenc.nvEncGetEncodePresetConfigEx(
        encoderSession_,
        encodeCodecGuid,
        presetGuid,
        NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY,
        &presetConfig
    );
    if (status != NV_ENC_SUCCESS) {
        cleanup();
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InternalError,
                             "nvEncGetEncodePresetConfigEx failed with status " + std::to_string(status));
    }

    NV_ENC_CONFIG encodeConfig = presetConfig.presetCfg;
    uint32_t bitrateBps = config_.bitrateKbps * 1000;
    encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
    encodeConfig.rcParams.averageBitRate = bitrateBps;
    encodeConfig.rcParams.maxBitRate = bitrateBps;
    encodeConfig.rcParams.vbvBufferSize = bitrateBps / config_.targetFps;
    encodeConfig.rcParams.vbvInitialDelay = encodeConfig.rcParams.vbvBufferSize;
    encodeConfig.gopLength = config_.gopLength;
    encodeConfig.frameIntervalP = 1; // Pure IPPP pattern for minimum latency

    if (config_.codec == VideoCodec::H264) {
        encodeConfig.encodeCodecConfig.h264Config.idrPeriod = config_.gopLength;
        encodeConfig.encodeCodecConfig.h264Config.repeatSPSPPS = 1;
        encodeConfig.encodeCodecConfig.h264Config.outputAUD = 1;
    }

    NV_ENC_INITIALIZE_PARAMS initParams{};
    initParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
    initParams.encodeGUID = encodeCodecGuid;
    initParams.presetGUID = presetGuid;
    initParams.encodeWidth = config_.width;
    initParams.encodeHeight = config_.height;
    initParams.darWidth = config_.width;
    initParams.darHeight = config_.height;
    initParams.frameRateNum = config_.targetFps;
    initParams.frameRateDen = 1;
    initParams.enablePTD = 1;
    initParams.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
    initParams.encodeConfig = &encodeConfig;

    status = g_nvenc.nvEncInitializeEncoder(encoderSession_, &initParams);
    if (status != NV_ENC_SUCCESS) {
        cleanup();
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InternalError,
                             "nvEncInitializeEncoder failed with status " + std::to_string(status));
    }

    // Allocate bounded bitstream buffers (3 slots)
    constexpr size_t numSlots = 3;
    bufferSlots_.resize(numSlots);
    for (size_t i = 0; i < numSlots; ++i) {
        NV_ENC_CREATE_BITSTREAM_BUFFER createBuffer{};
        createBuffer.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
        createBuffer.size = 2 * 1024 * 1024; // 2MB bitstream buffer
        status = g_nvenc.nvEncCreateBitstreamBuffer(encoderSession_, &createBuffer);
        if (status != NV_ENC_SUCCESS) {
            cleanup();
            return common::Error(common::SubsystemId::Encode, common::ErrorCode::ResourceExhausted,
                                 "nvEncCreateBitstreamBuffer failed with status " + std::to_string(status));
        }
        bufferSlots_[i].outputBitstream = createBuffer.bitstreamBuffer;
    }

    isInitialized_ = true;
    frameCounter_ = 0;
    timer_.reset();
    MIST_LOG_INFO(common::SubsystemId::Encode, "NVENC H.264 Encoder initialized (" +
                  std::to_string(config_.width) + "x" + std::to_string(config_.height) + " @ " +
                  std::to_string(config_.targetFps) + " FPS, " + std::to_string(config_.bitrateKbps) + " kbps CBR)");
    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Encode, common::ErrorCode::HardwareUnavailable,
                         "NVENC hardware encoder unavailable");
#endif
}

common::Result<void> NvencVideoEncoder::encode(const capture::CapturedFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isInitialized_) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotInitialized, "Encoder not initialized");
    }

#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
    if (!frame.nativeResource) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InvalidArgument, "CapturedFrame nativeResource is null");
    }

    auto d3dTexture = static_cast<ID3D11Texture2D*>(frame.nativeResource);

    // Look up or register texture resource with NVENC
    auto it = registeredResources_.find(d3dTexture);
    if (it == registeredResources_.end()) {
        NV_ENC_REGISTER_RESOURCE regResource{};
        regResource.version = NV_ENC_REGISTER_RESOURCE_VER;
        regResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
        regResource.resourceToRegister = d3dTexture;
        regResource.width = frame.width;
        regResource.height = frame.height;
        regResource.pitch = 0;
        regResource.subResourceIndex = 0;
        regResource.bufferFormat = (frame.format == capture::PixelFormat::NV12) ? NV_ENC_BUFFER_FORMAT_NV12 : NV_ENC_BUFFER_FORMAT_ARGB;
        regResource.bufferUsage = NV_ENC_INPUT_IMAGE;

        NVENCSTATUS regStatus = g_nvenc.nvEncRegisterResource(encoderSession_, &regResource);
        if (regStatus != NV_ENC_SUCCESS) {
            return common::Error(common::SubsystemId::Encode, common::ErrorCode::InternalError,
                                 "nvEncRegisterResource failed with status " + std::to_string(regStatus));
        }

        RegisteredResource regEntry{
            .handle = regResource.registeredResource,
            .bufferFormat = static_cast<uint32_t>(regResource.bufferFormat)
        };
        it = registeredResources_.emplace(d3dTexture, regEntry).first;
    }

    void* registeredHandle = it->second.handle;
    auto bufferFormat = static_cast<NV_ENC_BUFFER_FORMAT>(it->second.bufferFormat);

    // Map input resource
    NV_ENC_MAP_INPUT_RESOURCE mapResource{};
    mapResource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    mapResource.registeredResource = registeredHandle;
    NVENCSTATUS status = g_nvenc.nvEncMapInputResource(encoderSession_, &mapResource);
    if (status != NV_ENC_SUCCESS) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InternalError,
                             "nvEncMapInputResource failed with status " + std::to_string(status));
    }

    auto& slot = bufferSlots_[currentSlot_];
    currentSlot_ = (currentSlot_ + 1) % bufferSlots_.size();

    // Prepare picture encode params
    NV_ENC_PIC_PARAMS picParams{};
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.inputWidth = frame.width;
    picParams.inputHeight = frame.height;
    picParams.inputPitch = 0;
    picParams.inputBuffer = mapResource.mappedResource;
    picParams.outputBitstream = slot.outputBitstream;
    picParams.bufferFmt = bufferFormat;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    picParams.inputTimeStamp = frame.timestampNs;

    if (forceKeyframeNext_ || frameCounter_ == 0) {
        picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
        forceKeyframeNext_ = false;
    }

    status = g_nvenc.nvEncEncodePicture(encoderSession_, &picParams);
    if (status != NV_ENC_SUCCESS && status != NV_ENC_ERR_NEED_MORE_INPUT) {
        (void)g_nvenc.nvEncUnmapInputResource(encoderSession_, mapResource.mappedResource);
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InternalError,
                             "nvEncEncodePicture failed with status " + std::to_string(status));
    }

    // Unmap input buffer immediately after submission
    (void)g_nvenc.nvEncUnmapInputResource(encoderSession_, mapResource.mappedResource);

    // Retrieve encoded bitstream
    NV_ENC_LOCK_BITSTREAM lockBitstream{};
    lockBitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
    lockBitstream.outputBitstream = slot.outputBitstream;
    lockBitstream.doNotWait = 0;

    status = g_nvenc.nvEncLockBitstream(encoderSession_, &lockBitstream);
    if (status != NV_ENC_SUCCESS) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::InternalError,
                             "nvEncLockBitstream failed with status " + std::to_string(status));
    }

    EncodedPacket packet{
        .data = std::vector<uint8_t>(static_cast<const uint8_t*>(lockBitstream.bitstreamBufferPtr),
                                     static_cast<const uint8_t*>(lockBitstream.bitstreamBufferPtr) + lockBitstream.bitstreamSizeInBytes),
        .isKeyframe = (lockBitstream.pictureType == NV_ENC_PIC_TYPE_IDR || lockBitstream.pictureType == NV_ENC_PIC_TYPE_I),
        .timestampNs = frame.timestampNs,
        .frameNumber = frameCounter_++
    };

    (void)g_nvenc.nvEncUnlockBitstream(encoderSession_, slot.outputBitstream);

    // Bounded queue: maintain maximum 2 pending frames, dropping oldest if queue fills
    while (outputQueue_.size() >= 2) {
        outputQueue_.pop();
    }
    outputQueue_.push(std::move(packet));

    return common::Result<void>::success();
#else
    return common::Error(common::SubsystemId::Encode, common::ErrorCode::HardwareUnavailable, "NVENC unavailable");
#endif
}

common::Result<EncodedPacket> NvencVideoEncoder::getNextPacket() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (outputQueue_.empty()) {
        return common::Error(common::SubsystemId::Encode, common::ErrorCode::NotFound, "No encoded packets in queue");
    }
    EncodedPacket packet = std::move(outputQueue_.front());
    outputQueue_.pop();
    return packet;
}

common::Result<void> NvencVideoEncoder::requestKeyframe() {
    std::lock_guard<std::mutex> lock(mutex_);
    forceKeyframeNext_ = true;
    return common::Result<void>::success();
}

common::Result<void> NvencVideoEncoder::setBitrate(uint32_t bitrateKbps) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.bitrateKbps = bitrateKbps;
#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
    if (isInitialized_ && encoderSession_) {
        NV_ENC_RECONFIGURE_PARAMS reconfig{};
        reconfig.version = NV_ENC_RECONFIGURE_PARAMS_VER;
        reconfig.reInitEncodeParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
        reconfig.reInitEncodeParams.encodeWidth = config_.width;
        reconfig.reInitEncodeParams.encodeHeight = config_.height;

        NV_ENC_CONFIG encodeConfig{};
        encodeConfig.version = NV_ENC_CONFIG_VER;
        uint32_t bitrateBps = config_.bitrateKbps * 1000;
        encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
        encodeConfig.rcParams.averageBitRate = bitrateBps;
        encodeConfig.rcParams.maxBitRate = bitrateBps;
        reconfig.reInitEncodeParams.encodeConfig = &encodeConfig;

        g_nvenc.nvEncReconfigureEncoder(encoderSession_, &reconfig);
    }
#endif
    return common::Result<void>::success();
}

common::Result<void> NvencVideoEncoder::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
    if (isInitialized_ && encoderSession_) {
        NV_ENC_PIC_PARAMS picParams{};
        picParams.version = NV_ENC_PIC_PARAMS_VER;
        picParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
        g_nvenc.nvEncEncodePicture(encoderSession_, &picParams);
    }
#endif
    return common::Result<void>::success();
}

void NvencVideoEncoder::cleanup() noexcept {
#if defined(MIST_HAS_NVENC_SDK) && defined(_WIN32)
    if (encoderSession_) {
        for (auto& [tex, reg] : registeredResources_) {
            if (reg.handle && g_nvenc.nvEncUnregisterResource) {
                (void)g_nvenc.nvEncUnregisterResource(encoderSession_, reg.handle);
            }
        }
        registeredResources_.clear();

        for (auto& slot : bufferSlots_) {
            if (slot.outputBitstream && g_nvenc.nvEncDestroyBitstreamBuffer) {
                g_nvenc.nvEncDestroyBitstreamBuffer(encoderSession_, slot.outputBitstream);
                slot.outputBitstream = nullptr;
            }
        }
        bufferSlots_.clear();
        if (g_nvenc.nvEncDestroyEncoder) {
            g_nvenc.nvEncDestroyEncoder(encoderSession_);
        }
        encoderSession_ = nullptr;
    }
#endif
    isInitialized_ = false;
    while (!outputQueue_.empty()) {
        outputQueue_.pop();
    }
}

} // namespace mist::encode
