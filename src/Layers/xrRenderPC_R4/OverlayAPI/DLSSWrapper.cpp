#include "stdafx.h"
#include "DLSSWrapper.h"

NVSDK_NGX_PerfQuality_Value CDLSSWrapper::ResolveQuality(u32 qualityPreset) const
{
    switch (qualityPreset)
    {
    case 4: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
    case 3: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
    case 2: return NVSDK_NGX_PerfQuality_Value_Balanced;
    case 1: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
    default: return NVSDK_NGX_PerfQuality_Value_DLAA;
    }
}

bool CDLSSWrapper::Probe(ID3D11Device* device)
{
    Shutdown();
    if (!device || HW.FeatureLevel < D3D_FEATURE_LEVEL_11_1)
        return false;

    NVSDK_NGX_Result result = NVSDK_NGX_D3D11_Init(1602, L"", device);
    if (result != NVSDK_NGX_Result_Success)
    {
        Msg("! [UPSCALER/DLSS] NGX initialization failed: 0x%08x", result);
        return false;
    }
    m_initialized = true;

    result = NVSDK_NGX_D3D11_GetCapabilityParameters(&m_parameters);
    if (result != NVSDK_NGX_Result_Success || !m_parameters)
    {
        Msg("! [UPSCALER/DLSS] capability query failed: 0x%08x", result);
        Shutdown();
        return false;
    }

    u32 needsDriver = 0;
    u32 available = 0;
    m_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsDriver);
    m_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    if (needsDriver)
        Msg("! [UPSCALER/DLSS] NVIDIA driver update is required");
    if (!available)
    {
        Msg("! [UPSCALER/DLSS] unavailable on this adapter; native fallback selected");
        Shutdown();
        return false;
    }

    m_available = true;
    return true;
}

bool CDLSSWrapper::Create(const ContextParameters& params, u32 qualityPreset)
{
    DestroyFeature();
    if (!m_available || !m_parameters || !params.context)
        return false;

    m_context = params.context;
    NVSDK_NGX_DLSS_Create_Params create = {};
    create.Feature.InWidth = params.renderWidth;
    create.Feature.InHeight = params.renderHeight;
    create.Feature.InTargetWidth = params.displayWidth;
    create.Feature.InTargetHeight = params.displayHeight;
    create.Feature.InPerfQualityValue = ResolveQuality(qualityPreset);
    // phase_upscale receives the already-tonemapped LDR scene. Advertising it
    // as HDR makes NGX apply the wrong luminance contract; only request the
    // automatic exposure path because no explicit exposure texture is bound.
    create.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
        NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

    const NVSDK_NGX_Result result = NGX_D3D11_CREATE_DLSS_EXT(m_context, &m_handle, m_parameters, &create);
    if (result != NVSDK_NGX_Result_Success)
    {
        Msg("! [UPSCALER/DLSS] feature creation failed: 0x%08x", result);
        m_handle = nullptr;
        return false;
    }
    return true;
}

bool CDLSSWrapper::Draw(const DrawParameters& params)
{
    if (!m_handle || !m_parameters || !m_context || !params.unresolvedColor || !params.motionVectors ||
        !params.depth || !params.output)
        return false;

    NVSDK_NGX_D3D11_DLSS_Eval_Params eval = {};
    eval.Feature.pInColor = params.unresolvedColor;
    eval.Feature.pInOutput = params.output;
    eval.Feature.InSharpness = 0.f;
    eval.pInDepth = params.depth;
    eval.pInMotionVectors = params.motionVectors;
    eval.InRenderSubrectDimensions.Width = params.renderWidth;
    eval.InRenderSubrectDimensions.Height = params.renderHeight;
    eval.InJitterOffsetX = params.jitterX;
    eval.InJitterOffsetY = params.jitterY;
    eval.InReset = params.reset;
	eval.InPreExposure = 1.f;
	eval.InExposureScale = 1.f;
	eval.InFrameTimeDeltaInMsec = _max(1.f, Device.fTimeDelta * 1000.f);
    // SSS stores currentUV - previousUV. NGX expects the displacement from
    // the current pixel to its previous-frame pixel, expressed in pixels.
    eval.InMVScaleX = -float(params.renderWidth);
    eval.InMVScaleY = -float(params.renderHeight);

    const NVSDK_NGX_Result result = NGX_D3D11_EVALUATE_DLSS_EXT(m_context, m_handle, m_parameters, &eval);
    if (result != NVSDK_NGX_Result_Success)
    {
        Msg("! [UPSCALER/DLSS] evaluation failed: 0x%08x", result);
        return false;
    }
    return true;
}

void CDLSSWrapper::DestroyFeature()
{
    if (m_handle)
        NVSDK_NGX_D3D11_ReleaseFeature(m_handle);
    m_handle = nullptr;
    m_context = nullptr;
}

void CDLSSWrapper::Shutdown()
{
    DestroyFeature();
    if (m_parameters)
        NVSDK_NGX_D3D11_DestroyParameters(m_parameters);
    m_parameters = nullptr;
    if (m_initialized)
        NVSDK_NGX_D3D11_Shutdown1(nullptr);
    m_initialized = false;
    m_available = false;
}

CDLSSWrapper::~CDLSSWrapper()
{
    Shutdown();
}
