#include "stdafx.h"
#include "FSR3Wrapper.h"

namespace
{
void FsrMessage(FfxMsgType type, const wchar_t* message)
{
    string1024 text = {};
    const int written = WideCharToMultiByte(CP_UTF8, 0, message, -1, text, sizeof(text) - 1, nullptr, nullptr);
    text[written > 0 ? written - 1 : 0] = 0;
    Msg("%s [UPSCALER/FSR3] %s", type == FFX_MESSAGE_TYPE_ERROR ? "!" : "~", text);
}
}

bool CFSR3Wrapper::Create(const ContextParameters& params)
{
    Destroy();
    if (!params.device || !params.maxRenderSize.width || !params.maxRenderSize.height ||
        !params.displaySize.width || !params.displaySize.height || HW.FeatureLevel < D3D_FEATURE_LEVEL_11_0)
        return false;

    m_scratch.resize(ffxGetScratchMemorySizeDX11(1));
    FfxErrorCode code = ffxGetInterfaceDX11(&m_description.backendInterface, ffxGetDeviceDX11(params.device),
        m_scratch.data(), m_scratch.size(), 1);
    if (code != FFX_OK)
    {
        Msg("! [UPSCALER/FSR3] DX11 backend creation failed: %d", code);
        Destroy();
        return false;
    }

    auto makeTexture = [&](DXGI_FORMAT format, bool renderTarget, ID3D11Texture2D** output)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = params.maxRenderSize.width;
        desc.Height = params.maxRenderSize.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        if (renderTarget)
            desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        return SUCCEEDED(params.device->CreateTexture2D(&desc, nullptr, output));
    };

    if (!makeTexture(DXGI_FORMAT_R32_FLOAT, true, &m_dilatedDepth) ||
        !makeTexture(DXGI_FORMAT_R16G16_FLOAT, true, &m_dilatedMotion) ||
        !makeTexture(DXGI_FORMAT_R32_UINT, false, &m_reconstructedPrevDepth))
    {
        Msg("! [UPSCALER/FSR3] shared resource creation failed");
        Destroy();
        return false;
    }

    m_description.maxRenderSize = params.maxRenderSize;
    m_description.maxUpscaleSize = params.displaySize;
    m_description.fpMessage = FsrMessage;
#ifdef DEBUG
    m_description.flags |= FFX_FSR3UPSCALER_ENABLE_DEBUG_CHECKING;
#endif

    code = ffxFsr3UpscalerContextCreate(&m_context, &m_description);
    if (code != FFX_OK)
    {
        Msg("! [UPSCALER/FSR3] context creation failed: %d", code);
        Destroy();
        return false;
    }

    m_created = true;
    return true;
}

bool CFSR3Wrapper::Draw(const DrawParameters& params)
{
    if (!m_created || !params.deviceContext || !params.unresolvedColor || !params.motionVectors ||
        !params.depth || !params.output)
        return false;

    FfxFsr3UpscalerDispatchDescription desc = {};
    desc.commandList = ffxGetCommandListDX11(params.deviceContext);
    desc.color = ffxGetResourceDX11(params.unresolvedColor, GetFfxResourceDescriptionDX11(params.unresolvedColor), nullptr);
    desc.depth = ffxGetResourceDX11(params.depth, GetFfxResourceDescriptionDX11(params.depth), nullptr);
    desc.motionVectors = ffxGetResourceDX11(params.motionVectors, GetFfxResourceDescriptionDX11(params.motionVectors), nullptr);
    desc.exposure = ffxGetResourceDX11(nullptr, FfxResourceDescription{}, nullptr);
    desc.reactive = ffxGetResourceDX11(nullptr, FfxResourceDescription{}, nullptr);
    desc.transparencyAndComposition = ffxGetResourceDX11(nullptr, FfxResourceDescription{}, nullptr);
    desc.dilatedDepth = ffxGetResourceDX11(m_dilatedDepth, GetFfxResourceDescriptionDX11(m_dilatedDepth), nullptr,
        FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    desc.dilatedMotionVectors = ffxGetResourceDX11(m_dilatedMotion, GetFfxResourceDescriptionDX11(m_dilatedMotion), nullptr,
        FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    desc.reconstructedPrevNearestDepth = ffxGetResourceDX11(m_reconstructedPrevDepth,
        GetFfxResourceDescriptionDX11(m_reconstructedPrevDepth), nullptr, FFX_RESOURCE_STATE_UNORDERED_ACCESS);
    desc.output = ffxGetResourceDX11(params.output, GetFfxResourceDescriptionDX11(params.output), nullptr,
        FFX_RESOURCE_STATE_UNORDERED_ACCESS);

    desc.jitterOffset = {params.jitterX, params.jitterY};
    desc.motionVectorScale = {-float(params.renderWidth) * 0.5f, float(params.renderHeight) * 0.5f};
    desc.renderSize = {params.renderWidth, params.renderHeight};
    desc.upscaleSize = {params.displayWidth, params.displayHeight};
    desc.enableSharpening = params.sharpening;
    desc.sharpness = params.sharpness;
    desc.frameTimeDelta = params.frameTimeMs;
    desc.preExposure = 1.f;
    desc.reset = params.reset;
    desc.cameraNear = params.nearPlane;
    desc.cameraFar = params.farPlane;
    desc.cameraFovAngleVertical = params.verticalFov;
    desc.viewSpaceToMetersFactor = 1.f;

    const FfxErrorCode code = ffxFsr3UpscalerContextDispatch(&m_context, &desc);
    if (code != FFX_OK)
    {
        Msg("! [UPSCALER/FSR3] dispatch failed: %d", code);
        return false;
    }
    return true;
}

void CFSR3Wrapper::Destroy()
{
    if (m_created)
        ffxFsr3UpscalerContextDestroy(&m_context);
    m_created = false;
    _RELEASE(m_dilatedDepth);
    _RELEASE(m_dilatedMotion);
    _RELEASE(m_reconstructedPrevDepth);
    m_scratch.clear();
    ZeroMemory(&m_context, sizeof(m_context));
    ZeroMemory(&m_description, sizeof(m_description));
}

CFSR3Wrapper::~CFSR3Wrapper()
{
    Destroy();
}
