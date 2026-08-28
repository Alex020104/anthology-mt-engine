#include "stdafx.h"
#include "UpscalerRuntime.h"
#include "../xrRender/xrRender_console.h"
#include "../../xrEngine/igame_persistent.h"

CAnthologyUpscalerRuntime g_AnthologyUpscaler;

float CAnthologyUpscalerRuntime::ResolveRenderScale() const
{
    switch (ps_r4_upscaler_quality)
    {
    case 0: return 1.f;
    case 1: return 2.f / 3.f;
    case 2: return 0.5882353f;
    case 3: return 0.5f;
    case 4: return 1.f / 3.f;
    default: return clampr(ps_r4_upscaler_custom_scale, 0.33f, 1.f);
    }
}

bool CAnthologyUpscalerRuntime::ProbeAndResolveMode()
{
    m_mode = clampr(ps_r4_upscaler, u32(AnthologyUpscalerOff), u32(AnthologyUpscalerDLSS));
    if (m_mode == AnthologyUpscalerDLSS && !m_dlss.Probe(HW.pDevice))
    {
        m_mode = AnthologyUpscalerOff;
        ps_r4_upscaler = AnthologyUpscalerOff;
    }
	if (m_mode == AnthologyUpscalerFSR3 && HW.FeatureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        Msg("! [UPSCALER/FSR3] feature level 11.0 is required; native fallback selected");
        m_mode = AnthologyUpscalerOff;
		ps_r4_upscaler = AnthologyUpscalerOff;
	}
	if (m_mode == AnthologyUpscalerFSR3)
	{
		static HMODULE backendModule = nullptr;
		static HMODULE upscalerModule = nullptr;
		if (!backendModule)
			backendModule = LoadLibraryA("ffx_backend_dx11_x64.dll");
		if (!upscalerModule)
			upscalerModule = LoadLibraryA("ffx_fsr3upscaler_x64.dll");
		if (!backendModule || !upscalerModule)
		{
			Msg("! [UPSCALER/FSR3] runtime DLLs are missing; native fallback selected");
			m_mode = AnthologyUpscalerOff;
			ps_r4_upscaler = AnthologyUpscalerOff;
		}
	}
    return IsEnabled();
}

bool CAnthologyUpscalerRuntime::Initialize(u32 renderWidth, u32 renderHeight, u32 displayWidth, u32 displayHeight)
{
    m_renderWidth = renderWidth;
    m_renderHeight = renderHeight;
    m_displayWidth = displayWidth;
    m_displayHeight = displayHeight;
	m_dispatchLogged = false;
    if (!IsEnabled())
        return false;

    bool created = false;
    if (m_mode == AnthologyUpscalerFSR3)
    {
        CFSR3Wrapper::ContextParameters params;
        params.maxRenderSize = {renderWidth, renderHeight};
        params.displaySize = {displayWidth, displayHeight};
        params.device = HW.pDevice;
        created = m_fsr3.Create(params);
    }
    else if (m_mode == AnthologyUpscalerDLSS)
    {
        CDLSSWrapper::ContextParameters params;
        params.renderWidth = renderWidth;
        params.renderHeight = renderHeight;
        params.displayWidth = displayWidth;
        params.displayHeight = displayHeight;
        params.device = HW.pDevice;
        params.context = HW.pContext;
        created = m_dlss.Create(params, ps_r4_upscaler_quality);
    }

    if (!created)
    {
        Msg("! [UPSCALER] requested backend could not be created; renderer stays native");
        m_mode = AnthologyUpscalerOff;
        ps_r4_upscaler = AnthologyUpscalerOff;
        return false;
    }

    Msg("* [UPSCALER] backend=%s render=%ux%u display=%ux%u scale=%.3f frame_generation=off",
        m_mode == AnthologyUpscalerFSR3 ? "FSR3.1.2" : "DLSS", renderWidth, renderHeight,
        displayWidth, displayHeight, ResolveRenderScale());
    return true;
}

void CAnthologyUpscalerRuntime::UpdateJitter(u32 frameIndex)
{
	if (!IsEnabled() || !m_renderWidth || !m_displayWidth)
	{
		g_main_taa_jitter_pixels.set(0.f, 0.f);
		return;
	}

	const int phaseCount = _max(1, ffxFsr3UpscalerGetJitterPhaseCount(
		int(m_renderWidth), int(m_displayWidth)));
	float jitterX = 0.f;
	float jitterY = 0.f;
	if (ffxFsr3UpscalerGetJitterOffset(&jitterX, &jitterY, int(frameIndex % phaseCount), phaseCount) != FFX_OK)
	{
		jitterX = 0.f;
		jitterY = 0.f;
	}
	g_main_taa_jitter_pixels.set(jitterX, jitterY);
}

bool CAnthologyUpscalerRuntime::Dispatch(ID3D11Resource* color, ID3D11Resource* motion,
	ID3D11Resource* depth, ID3D11Resource* output, bool resetHistory)
{
	auto finishDispatch = [this, color, motion, depth, output](bool success)
	{
		if (success && !m_dispatchLogged)
		{
			auto textureFormat = [](ID3D11Resource* resource)
			{
				ID3D11Texture2D* texture = nullptr;
				if (!resource || FAILED(resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture))))
					return DXGI_FORMAT_UNKNOWN;
				D3D11_TEXTURE2D_DESC desc = {};
				texture->GetDesc(&desc);
				texture->Release();
				return desc.Format;
			};
			Msg("* [UPSCALER] first vendor dispatch succeeded: color_fmt=%u motion_fmt=%u depth_fmt=%u output_fmt=%u",
				u32(textureFormat(color)), u32(textureFormat(motion)), u32(textureFormat(depth)),
				u32(textureFormat(output)));
			m_dispatchLogged = true;
		}
		return success;
	};

    if (m_mode == AnthologyUpscalerFSR3)
    {
        CFSR3Wrapper::DrawParameters params;
        params.deviceContext = HW.pContext;
        params.unresolvedColor = color;
        params.motionVectors = motion;
        params.depth = depth;
        params.output = output;
        params.renderWidth = m_renderWidth;
        params.renderHeight = m_renderHeight;
        params.displayWidth = m_displayWidth;
        params.displayHeight = m_displayHeight;
        params.reset = resetHistory;
		// Run RCAS inside the vendor dispatch. The old mandatory display 9-tap pass
		// erased much of the GPU saving and reprocessed an already reconstructed image.
		params.sharpening = ps_r4_upscaler_sharpness > EPS;
		params.sharpness = clampr(ps_r4_upscaler_sharpness, 0.f, 1.f);
        params.frameTimeMs = _max(1.f, Device.fTimeDelta * 1000.f);
        params.nearPlane = VIEWPORT_NEAR;
        params.farPlane = g_pGamePersistent && g_pGamePersistent->Environment().CurrentEnv ?
            g_pGamePersistent->Environment().CurrentEnv->far_plane : 500.f;
        params.verticalFov = deg2rad(Device.fFOV);
        params.jitterX = g_main_taa_jitter_pixels.x;
        params.jitterY = g_main_taa_jitter_pixels.y;
		return finishDispatch(m_fsr3.Draw(params));
    }
    if (m_mode == AnthologyUpscalerDLSS)
    {
        CDLSSWrapper::DrawParameters params;
        params.unresolvedColor = color;
        params.motionVectors = motion;
        params.depth = depth;
        params.output = output;
        params.renderWidth = m_renderWidth;
        params.renderHeight = m_renderHeight;
        params.reset = resetHistory;
		params.jitterX = g_main_taa_jitter_pixels.x;
		params.jitterY = g_main_taa_jitter_pixels.y;
		return finishDispatch(m_dlss.Draw(params));
    }
    return false;
}

void CAnthologyUpscalerRuntime::Shutdown()
{
    m_fsr3.Destroy();
    m_dlss.Shutdown();
    m_mode = AnthologyUpscalerOff;
    m_renderWidth = m_renderHeight = m_displayWidth = m_displayHeight = 0;
	m_dispatchLogged = false;
	g_main_taa_jitter_pixels.set(0.f, 0.f);
}
