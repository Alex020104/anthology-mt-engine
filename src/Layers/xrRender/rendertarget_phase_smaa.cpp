#include "stdafx.h"


void CRenderTarget::phase_smaa()
{
	//Constants
	u32 Offset = 0;
	u32 C = color_rgba(0, 0, 0, 255);
	FLOAT ColorRGBA[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	float d_Z = EPS_S;
	float d_W = 1.0f;
	float w = float(Device.dwWidth);
	float h = float(Device.dwHeight);

	Fvector2 p0, p1;
#if defined(USE_DX10) || defined(USE_DX11)	
	p0.set(0.0f, 0.0f);
	p1.set(1.0f, 1.0f);
#else
	p0.set(0.5f / w, 0.5f / h);
	p1.set((w + 0.5f) / w, (h + 0.5f) / h);
#endif

	//////////////////////////////////////////////////////////////////////////
	//Edge detection
	u_setrt(rt_smaa_edgetex, nullptr, nullptr, nullptr);
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x1, 0, 0, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
#if defined(USE_DX10) || defined(USE_DX11)	
	HW.pContext->ClearRenderTargetView(rt_smaa_edgetex->pRT, ColorRGBA);
#else
	CHK_DX( HW.pDevice->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0L) );
#endif

	// Fill vertex buffer
	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	// Draw COLOR
	RCache.set_Element(s_smaa->E[0]);
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);

	//////////////////////////////////////////////////////////////////////////
	//Blending
	u_setrt(rt_smaa_blendtex, nullptr, nullptr, nullptr);
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(TRUE, D3DCMP_EQUAL, 0x1, 0, 0, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
#if defined(USE_DX10) || defined(USE_DX11)	
	HW.pContext->ClearRenderTargetView(rt_smaa_blendtex->pRT, ColorRGBA);
#else
	CHK_DX( HW.pDevice->Clear(0L, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0L) );	
#endif

	// Fill vertex buffer
	pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	// Draw COLOR
	RCache.set_Element(s_smaa->E[1]);
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);

	//////////////////////////////////////////////////////////////////////////
	//Set MSAA/NonMSAA rendertarget
#if defined(USE_DX10) || defined(USE_DX11)
	ref_rt& dest_rt = RImplementation.o.dx10_msaa ? rt_Generic : rt_Color;
	u_setrt(dest_rt, nullptr, nullptr, nullptr);
#else
	u_setrt(rt_Generic_0, nullptr, nullptr, nullptr);
#endif	

	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	// Fill vertex buffer
	pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	// Draw COLOR
	RCache.set_Element(s_smaa->E[2]);
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);

#if defined(USE_DX10) || defined(USE_DX11)
	HW.pContext->CopyResource(rt_Generic_0->pTexture->surface_get(), dest_rt->pTexture->surface_get());
#endif
}

#if defined(USE_DX11)

void CRenderTarget::phase_ssfx_taa()
{
	u32 Offset = 0;
	Fvector2 p0, p1;
	const bool svp_frame = Device.m_SecondViewport.IsSVPFrame();
	ref_rt& color_history = svp_frame ? rt_ssfx_prev_frame_svp : rt_ssfx_prev_frame_main;
	ref_rt& depth_history = svp_frame ? rt_ssfx_prevPos_svp : rt_ssfx_prevPos_main;
	bool& history_valid = svp_frame ? m_taaHistorySVPValid : m_taaHistoryMainValid;

	if (svp_frame)
	{
		const u32 frame_delay = std::max<u8>(Device.m_SecondViewport.GetSVPFrameDelay(), 2);
		const bool interrupted = m_taaSVPLastFrame == 0 || Device.dwFrame > m_taaSVPLastFrame + frame_delay;
		const bool fov_changed = !fsimilar(Device.fFOV, m_taaSVPLastFov, 0.01f);
		if (interrupted || fov_changed)
			history_valid = false;

		m_taaSVPLastFrame = Device.dwFrame;
		m_taaSVPLastFov = Device.fFOV;
	}

	if (history_valid)
	{
		HW.pContext->CopyResource(rt_ssfx_prev_frame->pTexture->surface_get(), color_history->pTexture->surface_get());
		HW.pContext->CopyResource(rt_ssfx_prevPos->pTexture->surface_get(), depth_history->pTexture->surface_get());
	}
	else
	{
		HW.pContext->CopyResource(rt_ssfx_prev_frame->pTexture->surface_get(), rt_Generic_0->pTexture->surface_get());
		HW.pContext->CopyResource(rt_ssfx_prevPos->pTexture->surface_get(), rt_Position->pTexture->surface_get());
	}

	u32 C = color_rgba(255, 255, 255, 255);
	float w = float(Device.dwWidth);
	float h = float(Device.dwHeight);
	float d_Z = EPS_S;
	float d_W = 1.f;

	p0.set(0.0f, 0.0f);
	p1.set(1.0f, 1.0f);

	// Temp motion vectors
	u_setrt(rt_ssfx_accum, nullptr, nullptr, nullptr);
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	// Draw COLOR
	RCache.set_Element(s_ssfx_taa->E[0]);

	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
	
	HW.pContext->CopyResource(rt_ssfx_taa->pTexture->surface_get(), rt_ssfx_accum->pTexture->surface_get());

	// TAA
	ref_rt& dest_rt = RImplementation.o.dx10_msaa ? rt_Generic : rt_Color;

	u_setrt(dest_rt, nullptr, nullptr, nullptr); // rt_ssfx_taa
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	// Draw COLOR
	RCache.set_Element(s_ssfx_taa->E[1]);
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);

	// Accumulate
	HW.pContext->CopyResource(rt_ssfx_prev_frame->pTexture->surface_get(), dest_rt->pTexture->surface_get());
	HW.pContext->CopyResource(color_history->pTexture->surface_get(), rt_ssfx_prev_frame->pTexture->surface_get());
	HW.pContext->CopyResource(depth_history->pTexture->surface_get(), rt_Position->pTexture->surface_get());
	history_valid = true;

	// Sharpening phase
	u_setrt(rt_Generic_0, nullptr, nullptr, nullptr);
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	// Draw COLOR
	RCache.set_Element(s_ssfx_taa->E[2]);
	
	Fvector4 taa_setup = ps_ssfx_taa;
	if (svp_frame)
		taa_setup.z = _max(taa_setup.z, 0.75f);
	RCache.set_c("taa_setup", taa_setup);

	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
}

#endif
