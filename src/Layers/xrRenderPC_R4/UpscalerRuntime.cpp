#include "stdafx.h"
#include "UpscalerRuntime.h"
#include "../xrRender/xrRender_console.h"
#include "../../xrEngine/igame_persistent.h"
#include "../../xrEngine/EngineThreading.h"
#include <dxgi1_4.h>

CAnthologyUpscalerRuntime g_AnthologyUpscaler;

namespace
{
u32 ResolveEffectiveQuality(u32 mode)
{
	const u32 configured = clampr(ps_r4_upscaler_quality, 0u, 6u);
	if (mode != AnthologyUpscalerDLSS || configured != 5)
		return configured;

	// Direct NGX has no arbitrary custom-ratio quality mode. Snap Custom to the
	// nearest supported preset and use that preset's render size, so a low-res
	// input can never be paired with DLAA.
	static const float presetScales[] = {1.0f, 2.0f / 3.0f, 0.5882353f, 0.5f, 1.0f / 3.0f};
	const float customScale = clampr(ps_r4_upscaler_custom_scale, 0.33f, 1.0f);
	u32 nearest = 0;
	float nearestDistance = fabsf(customScale - presetScales[0]);
	for (u32 i = 1; i < _countof(presetScales); ++i)
	{
		const float distance = fabsf(customScale - presetScales[i]);
		if (distance < nearestDistance)
		{
			nearest = i;
			nearestDistance = distance;
		}
	}
	return nearest;
}

LPCSTR QualityName(u32 quality)
{
	switch (quality)
	{
	case 0: return "native";
	case 1: return "quality";
	case 2: return "balanced";
	case 3: return "performance";
	case 4: return "ultra_performance";
	case 5: return "custom";
    case 6: return "ultra_quality";
	default: return "unknown";
	}
}
}

float CAnthologyUpscalerRuntime::ResolveRenderScale() const
{
	const u32 quality = ResolveEffectiveQuality(m_mode);
    switch (quality)
    {
    case 0: return 1.f;
    case 1: return 2.f / 3.f;
    case 2: return 0.5882353f;
    case 3: return 0.5f;
    case 4: return 1.f / 3.f;
    case 6: return 5.f / 6.f;
    default: return clampr(ps_r4_upscaler_custom_scale, 0.33f, 1.f);
    }
}

namespace
{
void BackendLog(const char* stage, unsigned result)
{
    Msg("! [UPSCALER/brodrigz] %s: 0x%08x", stage, result);
}

dlss::Quality BackendQuality(u32 quality)
{
    return quality == 6 ? dlss::Quality::UltraQuality :
        static_cast<dlss::Quality>(clampr(quality, 0u, 4u) + 1u);
}
}

bool CAnthologyUpscalerRuntime::ProbeAndResolveMode()
{
    m_mode = clampr(ps_r4_upscaler, u32(AnthologyUpscalerOff), u32(AnthologyUpscalerDLSS));
    if (m_mode == AnthologyUpscalerDLSS)
    {
        string_path cache;
        FS.update_path(cache, "$app_data_root$", "");
        wchar_t wideCache[MAX_PATH] = {};
        const bool converted = MultiByteToWideChar(CP_ACP, 0, cache, -1, wideCache, MAX_PATH) > 0;
        if (!converted || !m_dlss.Initialize(HW.pDevice, wideCache, BackendLog))
            m_mode = AnthologyUpscalerOff;
    }
    if (m_mode == AnthologyUpscalerFSR3 && HW.FeatureLevel < D3D_FEATURE_LEVEL_11_1)
    {
        Msg("! [UPSCALER/FSR3] feature level 11.1 is required; native fallback selected");
        m_mode = AnthologyUpscalerOff;
    }
    if (!IsEnabled()) ps_r4_upscaler = AnthologyUpscalerOff;
    return IsEnabled();
}

bool CAnthologyUpscalerRuntime::ResolveRenderSize(u32& width, u32& height)
{
    const dlss::Size display{Device.dwWidth, Device.dwHeight};
    dlss::Size render;
    const u32 quality = ResolveEffectiveQuality(m_mode);
    bool valid = false;
    if (m_mode == AnthologyUpscalerDLSS)
        valid = m_dlss.OptimalSize(BackendQuality(quality), display, render);
    else if (m_mode == AnthologyUpscalerFSR3 && quality <= 4)
        valid = fsr3::D3D11Backend::OptimalSize(quality, display, render);
    else if (m_mode == AnthologyUpscalerFSR3)
    {
        const float scale = ResolveRenderScale();
        render = {_max(1u, u32(display.width * scale)), _max(1u, u32(display.height * scale))};
        valid = true;
    }
    if (valid) { width = render.width; height = render.height; }
    else
    {
        Msg("! [UPSCALER] backend rejected the display size; native fallback selected");
        m_mode = AnthologyUpscalerOff;
        ps_r4_upscaler = AnthologyUpscalerOff;
    }
    return valid;
}

bool CAnthologyUpscalerRuntime::Initialize(u32 renderWidth, u32 renderHeight, u32 displayWidth, u32 displayHeight)
{
	ResetTemporalState();
	ReleaseGpuProfile();
	// A failed/recreated backend must never leave its automatic sampler bias
	// behind. The user-owned r__tf_mipbias value itself is never modified.
	SetTemporalUpscalerMipBias(0.0f, false);
    m_renderWidth = renderWidth;
    m_renderHeight = renderHeight;
    m_displayWidth = displayWidth;
    m_displayHeight = displayHeight;
	m_dispatchLogged = false;
    if (!IsEnabled())
        return false;

    const dlss::Size render{renderWidth, renderHeight}, display{displayWidth, displayHeight};
    const bool created = m_mode == AnthologyUpscalerFSR3
        ? m_fsr3.Create(HW.pDevice, render, display, false, BackendLog, false)
        : m_dlss.Create(BackendQuality(ResolveEffectiveQuality(m_mode)), render, display, false,
            static_cast<dlss::Preset>(ps_r4_dlss_preset), false);

    if (!created)
    {
        Msg("! [UPSCALER] requested backend could not be created; renderer stays native");
        m_mode = AnthologyUpscalerOff;
        ps_r4_upscaler = AnthologyUpscalerOff;
        return false;
    }

	const float actualScale = displayWidth ? float(renderWidth) / float(displayWidth) : 1.0f;
	const bool scaled = actualScale < 0.999f;
	const float automaticMipBias = scaled ? clampr(log2f(actualScale) - 1.0f, -3.0f, 0.0f) : 0.0f;
	SetTemporalUpscalerMipBias(automaticMipBias, scaled);
	const float effectiveMipBias = GetEffectiveTextureMipBias();
	const u32 configuredQuality = clampr(ps_r4_upscaler_quality, 0u, 6u);
	const u32 effectiveQuality = ResolveEffectiveQuality(m_mode);
	string64 qualityLabel = {};
	if (configuredQuality == effectiveQuality)
		xr_sprintf(qualityLabel, "%s(%u)", QualityName(effectiveQuality), effectiveQuality);
	else
	{
		xr_sprintf(qualityLabel, "%s(%u)->%s(%u)", QualityName(configuredQuality), configuredQuality,
			QualityName(effectiveQuality), effectiveQuality);
	}

	Msg("* [UPSCALER] backend=%s quality=%s render=%ux%u display=%ux%u scale=%.3f "
		"mip_bias(user=%.3f auto=%.3f effective=%.3f) frame_generation=off integration=brodrigz-00093f6-anthology",
		m_mode == AnthologyUpscalerFSR3 ? "FSR3.1.2" : "DLSS", qualityLabel, renderWidth, renderHeight,
		displayWidth, displayHeight, actualScale, ps_r__tf_Mipbias, automaticMipBias, effectiveMipBias);
    return true;
}

void CAnthologyUpscalerRuntime::UpdateJitter(u32 frameIndex)
{
	if (!IsEnabled() || !m_renderWidth || !m_displayWidth || Device.m_SecondViewport.IsSVPFrame())
	{
		if (!Device.m_SecondViewport.IsSVPFrame())
			g_main_taa_jitter_pixels.set(0.f, 0.f);
		return;
	}
	if (m_jitterFrame == frameIndex)
		return;
	m_jitterFrame = frameIndex;
    m_previousJitter = g_main_taa_jitter_pixels;

    const auto jitter = dlss::Jitter(m_jitterPhase++,
        {m_renderWidth, m_renderHeight}, {m_displayWidth, m_displayHeight});
    g_main_taa_jitter_pixels.set(jitter.x, jitter.y);
    if (!m_historyValid) m_previousJitter = g_main_taa_jitter_pixels;
}

bool CAnthologyUpscalerRuntime::Dispatch(ID3D11Resource* color, ID3D11Resource* motion,
	ID3D11Resource* depth, ID3D11Resource* output, ID3D11Resource* reactive, bool resetHistory)
{
	// Motion history refers to the last main render. A failed resolve, loading,
	// menu gap or a camera cut cannot reuse a vendor history from an older view.
	const u32 frameGap = Device.dwFrame - m_lastDispatchFrame;
	const bool cameraCut = m_historyValid &&
		(Device.vCameraPosition.distance_to_sqr(m_previousCameraPosition) > 25.f ||
		 Device.vCameraDirection.dotproduct(m_previousCameraDirection) < 0.5f ||
		 fabsf(Device.fFOV - m_previousFov) > 10.f);
	resetHistory = resetHistory || !m_historyValid || frameGap > 2 || cameraCut ||
		ps_r2_heatvision != m_previousHeatvision;
	const float frameTimeMs = !resetHistory ?
		clampr(float(Device.dwTimeContinual - m_lastDispatchTime), 1.f, 250.f) :
		clampr(Device.fTimeDelta * 1000.f, 1.f, 250.f);

	auto finishDispatch = [this, color, motion, depth, output](bool success)
	{
		m_historyValid = success && Device.dwPrecacheFrame == 0;
		if (success)
		{
			m_lastDispatchFrame = Device.dwFrame;
			m_lastDispatchTime = Device.dwTimeContinual;
			m_previousCameraPosition.set(Device.vCameraPosition);
			m_previousCameraDirection.set(Device.vCameraDirection);
			m_previousFov = Device.fFOV;
			m_previousHeatvision = ps_r2_heatvision;
		}
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

    Microsoft::WRL::ComPtr<ID3D11Texture2D> colorTex, motionTex, depthTex, outputTex, reactiveTex;
    auto asTexture = [](ID3D11Resource* resource, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture)
    {
        return resource && SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&texture)));
    };
    if (!asTexture(color, colorTex) || !asTexture(motion, motionTex) ||
        !asTexture(depth, depthTex) || !asTexture(output, outputTex)) return finishDispatch(false);
    if (reactive && !asTexture(reactive, reactiveTex)) return finishDispatch(false);

    fsr3::D3D11Backend::Frame frame;
    frame.color = colorTex.Get(); frame.motion = motionTex.Get(); frame.depth = depthTex.Get();
    frame.output = outputTex.Get(); frame.reactive = reactiveTex.Get();
    frame.jitter = {g_main_taa_jitter_pixels.x, g_main_taa_jitter_pixels.y};
    frame.deltaMilliseconds = frameTimeMs;
    frame.frame = Device.dwFrame; frame.cameraCut = resetHistory;
    const float projectionNear = -Device.mProject._43 / Device.mProject._33;
    frame.cameraNear = projectionNear > EPS_S ? projectionNear : VIEWPORT_NEAR;
    frame.cameraFar = g_pGamePersistent && g_pGamePersistent->Environment().CurrentEnv
        ? g_pGamePersistent->Environment().CurrentEnv->far_plane : 500.f;
    frame.verticalFov = deg2rad(Device.fFOV);
    return finishDispatch(m_mode == AnthologyUpscalerFSR3 ? m_fsr3.Evaluate(frame) :
        m_mode == AnthologyUpscalerDLSS && m_dlss.Evaluate(frame));
}

void CAnthologyUpscalerRuntime::Shutdown()
{
	ReleaseGpuProfile();
	ResetTemporalState();
	SetTemporalUpscalerMipBias(0.0f, false);
    m_fsr3.Destroy();
    m_dlss.ReleaseFeature();
    m_mode = AnthologyUpscalerOff;
    m_renderWidth = m_renderHeight = m_displayWidth = m_displayHeight = 0;
	m_dispatchLogged = false;
	g_main_taa_jitter_pixels.set(0.f, 0.f);
}

void CAnthologyUpscalerRuntime::ShutdownDevice()
{
    Shutdown();
    m_dlss.Shutdown();
}

void CAnthologyUpscalerRuntime::ResetTemporalState()
{
	m_jitterFrame = u32(-1);
	m_jitterPhase = 0;
	m_lastDispatchFrame = m_lastDispatchTime = 0;
	m_historyValid = false;
}

void CAnthologyUpscalerRuntime::ReleaseGpuProfile()
{
	for (GpuProfileSample& sample : m_gpuSamples)
	{
		_RELEASE(sample.disjoint);
		for (ID3D11Query*& timestamp : sample.timestamps)
			_RELEASE(timestamp);
		sample.pending = false;
		sample.temporal = false;
	}
	m_gpuActiveSample = -1;
	m_gpuProfileUnavailable = false;
	m_gpuProfileCount = 0;
	m_gpuPrepareMs = m_gpuVendorMs = m_gpuSceneMs = m_gpuMaxMs = 0.0;
}

void CAnthologyUpscalerRuntime::BeginSceneGpuProfile()
{
	m_gpuActiveSample = -1;
	if (!mt_FrameProfile || !mt_FrameProfileDetailed || Device.dwPrecacheFrame ||
		Device.m_SecondViewport.IsSVPFrame() || m_gpuProfileUnavailable)
		return;

	for (GpuProfileSample& sample : m_gpuSamples)
	{
		if (!sample.pending)
			continue;
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
		HRESULT ready = HW.pContext->GetData(sample.disjoint, &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH);
		if (ready == S_FALSE)
			continue;
		if (FAILED(ready) || disjoint.Disjoint || !disjoint.Frequency)
		{
			sample.pending = false;
			continue;
		}
		UINT64 times[5] = {};
		bool complete = true;
		for (u32 i = 0; i < _countof(times); ++i)
		{
			if (!sample.temporal && i > 0 && i < 4)
				continue;
			ready = HW.pContext->GetData(sample.timestamps[i], &times[i], sizeof(times[i]), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			if (ready != S_OK)
			{
				complete = false;
				if (FAILED(ready))
					sample.pending = false;
				break;
			}
		}
		if (!complete)
			continue;
		sample.pending = false;
		if (times[4] < times[0] || (sample.temporal &&
			(times[4] < times[3] || times[3] < times[2] || times[2] < times[1] || times[1] < times[0])))
			continue;
		const double toMs = 1000.0 / double(disjoint.Frequency);
		if (sample.temporal)
		{
			m_gpuPrepareMs += double(times[2] - times[1]) * toMs;
			m_gpuVendorMs += double(times[3] - times[2]) * toMs;
		}
		m_gpuSceneMs += double(times[4] - times[0]) * toMs;
		m_gpuMaxMs = std::max(m_gpuMaxMs, double(times[4] - times[0]) * toMs);
		if (++m_gpuProfileCount >= 120)
		{
			IDXGIAdapter3* adapter = nullptr;
			if (HW.m_pAdapter && SUCCEEDED(HW.m_pAdapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter))))
			{
				DXGI_QUERY_VIDEO_MEMORY_INFO local = {}, shared = {};
				if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local)) &&
					SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &shared)))
				{
					Msg("* [render-memory/profile] local=%.1f budget=%.1f shared=%.1f MiB",
						double(local.CurrentUsage) / 1048576., double(local.Budget) / 1048576., double(shared.CurrentUsage) / 1048576.);
				}
				adapter->Release();
			}
			Msg("* [render-gpu/profile] mode=%s samples=%u render=%ux%u display=%ux%u main=%.3f max=%.3f prepare=%.3f vendor=%.3f upscale=%.3f ms; asynchronous GPU timestamps",
				!IsEnabled() ? "native" : m_mode == AnthologyUpscalerFSR3 ? "FSR3" : "DLSS",
				m_gpuProfileCount, IsEnabled() ? m_renderWidth : Device.dwWidth,
				IsEnabled() ? m_renderHeight : Device.dwHeight, Device.dwWidth, Device.dwHeight,
				m_gpuSceneMs / m_gpuProfileCount, m_gpuMaxMs,
				m_gpuPrepareMs / m_gpuProfileCount, m_gpuVendorMs / m_gpuProfileCount,
				(m_gpuPrepareMs + m_gpuVendorMs) / m_gpuProfileCount);
			m_gpuProfileCount = 0;
			m_gpuPrepareMs = m_gpuVendorMs = m_gpuSceneMs = m_gpuMaxMs = 0.0;
		}
	}

	for (u32 i = 0; i < _countof(m_gpuSamples); ++i)
	{
		GpuProfileSample& sample = m_gpuSamples[i];
		if (sample.pending)
			continue;
		D3D11_QUERY_DESC description = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
		HRESULT result = S_OK;
		if (!sample.disjoint)
			result = HW.pDevice->CreateQuery(&description, &sample.disjoint);
		description.Query = D3D11_QUERY_TIMESTAMP;
		for (ID3D11Query*& timestamp : sample.timestamps)
		{
			if (SUCCEEDED(result) && !timestamp)
				result = HW.pDevice->CreateQuery(&description, &timestamp);
		}
		if (FAILED(result))
		{
			m_gpuProfileUnavailable = true;
			Msg("! [upscaler/gpu] timestamp queries unavailable: 0x%08x", result);
			return;
		}
		m_gpuActiveSample = int(i);
		sample.temporal = false;
		HW.pContext->Begin(sample.disjoint);
		HW.pContext->End(sample.timestamps[0]);
		return;
	}
	// Every slot is pending: skip this measurement without waiting on the GPU.
}

void CAnthologyUpscalerRuntime::BeginGpuProfile()
{
	if (m_gpuActiveSample < 0)
		return;
	GpuProfileSample& sample = m_gpuSamples[m_gpuActiveSample];
	sample.temporal = true;
	HW.pContext->End(sample.timestamps[1]);
}

void CAnthologyUpscalerRuntime::MarkGpuDispatch()
{
	if (m_gpuActiveSample >= 0)
		HW.pContext->End(m_gpuSamples[m_gpuActiveSample].timestamps[2]);
}

void CAnthologyUpscalerRuntime::EndGpuProfile()
{
	if (m_gpuActiveSample >= 0)
		HW.pContext->End(m_gpuSamples[m_gpuActiveSample].timestamps[3]);
}

void CAnthologyUpscalerRuntime::EndSceneGpuProfile()
{
	if (m_gpuActiveSample < 0)
		return;
	GpuProfileSample& sample = m_gpuSamples[m_gpuActiveSample];
	HW.pContext->End(sample.timestamps[4]);
	HW.pContext->End(sample.disjoint);
	sample.pending = true;
	m_gpuActiveSample = -1;
}

// Shared engine callers use Anthology's active scene size; the swapchain stays native.
unsigned RenderScreenWidth() { return GetMainRenderWidth(); }
unsigned RenderScreenHeight() { return GetMainRenderHeight(); }
