#include "stdafx.h"
#include "r4_rendertarget.h"
#include "UpscalerRuntime.h"

void CRenderTarget::phase_upscale(bool temporal)
{
    bool resolved = false;
    if (temporal)
    {
		// Export the sampled D24 hardware depth into an R32_FLOAT target. The
		// vendor APIs consume device depth, not XRay's view-space position buffer
		// and not the packed typeless depth/stencil allocation itself.
		u_setrt(rt_UpscaleDepth, nullptr, nullptr, nullptr);
		RImplementation.rmNormal();
		RCache.set_CullMode(CULL_NONE);
		RCache.set_Stencil(FALSE);

		const float depthWidth = float(m_renderWidth);
		const float depthHeight = float(m_renderHeight);
		const u32 depthColor = color_rgba(255, 255, 255, 255);
		u32 depthOffset = 0;
		FVF::TL* depthVertices = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, depthOffset);
		depthVertices->set(0.f, depthHeight, EPS_S, 1.f, depthColor, 0.f, 1.f); ++depthVertices;
		depthVertices->set(0.f, 0.f, EPS_S, 1.f, depthColor, 0.f, 0.f); ++depthVertices;
		depthVertices->set(depthWidth, depthHeight, EPS_S, 1.f, depthColor, 1.f, 1.f); ++depthVertices;
		depthVertices->set(depthWidth, 0.f, EPS_S, 1.f, depthColor, 1.f, 0.f);
		RCache.Vertex.Unlock(4, g_combine->vb_stride);
		RCache.set_Element(s_upscale->E[3]);
		RCache.set_Geometry(g_combine);
		RCache.Render(D3DPT_TRIANGLELIST, depthOffset, 0, 4, 0, 2);

        RCache.set_RT(nullptr, 0);
        RCache.set_RT(nullptr, 1);
        RCache.set_RT(nullptr, 2);
        RCache.set_RT(nullptr, 3);
        RCache.set_ZB(nullptr);
        RCache.set_Textures(nullptr);
        // set_Textures updates the engine-side SRV cache. External NGX/FSR
        // dispatches bypass RCache, so commit the null bindings immediately.
        SRVSManager.Apply();
        const bool resetHistory = m_upscalerResetHistory || Device.dwPrecacheFrame > 0;
        resolved = g_AnthologyUpscaler.Dispatch(
            rt_UpscaleInput->pSurface,
            rt_ssfx_motion_vectors->pSurface,
			rt_UpscaleDepth->pSurface,
            rt_UpscaleOutput->pSurface,
            resetHistory);
		// Both integrations submit compute work outside RCache. Release every CS
		// resource/UAV slot before sampling the output as a pixel-shader SRV; this
		// also prevents stale vendor bindings from leaking into the next frame.
		ID3D11ShaderResourceView* nullSrvs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
		ID3D11UnorderedAccessView* nullUavs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
		HW.pContext->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSrvs);
		HW.pContext->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, nullUavs, nullptr);
		HW.pContext->CSSetShader(nullptr, nullptr, 0);
		// NGX and FidelityFX issue commands directly on the immediate context and
		// may replace viewport, shaders, input layout, buffers and pipeline state.
		// Force XRay to bind its complete fullscreen-present state again instead
		// of trusting stale backend caches left from before the vendor dispatch.
		RCache.InvalidateExternalState();
		if (resolved && Device.dwPrecacheFrame == 0)
			m_upscalerResetHistory = false;
    }

    // Temporal failure and PiP/SVP frames deliberately use a spatial present.
    // This never advances the main camera's FSR/DLSS history.
    u_setrt(Device.dwWidth, Device.dwHeight, HW.pBaseRT, nullptr, nullptr, nullptr);
    D3D_VIEWPORT viewport = {0.f, 0.f, float(Device.dwWidth), float(Device.dwHeight), 0.f, 1.f};
    HW.pContext->RSSetViewports(1, &viewport);
    RCache.set_CullMode(CULL_NONE);
    RCache.set_Stencil(FALSE);

    const float width = float(Device.dwWidth);
    const float height = float(Device.dwHeight);
    const u32 color = color_rgba(255, 255, 255, 255);
    u32 offset = 0;
    FVF::TL* vertices = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, offset);
    vertices->set(0.f, height, EPS_S, 1.f, color, 0.f, 1.f); ++vertices;
    vertices->set(0.f, 0.f, EPS_S, 1.f, color, 0.f, 0.f); ++vertices;
    vertices->set(width, height, EPS_S, 1.f, color, 1.f, 1.f); ++vertices;
    vertices->set(width, 0.f, EPS_S, 1.f, color, 1.f, 0.f);
    RCache.Vertex.Unlock(4, g_combine->vb_stride);

    RCache.set_Element(s_upscale->E[resolved ? 1 : 0]);
    RCache.set_Geometry(g_combine);
    RCache.Render(D3DPT_TRIANGLELIST, offset, 0, 4, 0, 2);
    dwWidth = Device.dwWidth;
    dwHeight = Device.dwHeight;
}
