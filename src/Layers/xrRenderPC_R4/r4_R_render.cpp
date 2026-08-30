#include "stdafx.h"
#include "../../xrEngine/igame_persistent.h"
#include "../xrRender/FBasicVisual.h"
#include "../../xrEngine/customhud.h"
#include "../../xrEngine/xr_object.h"
#include "../../xrEngine/EngineThreading.h"
#include "../xrRender/SkeletonCustom.h"
#include "../../xrParticles/ParticlesAsyncManager.h"

#include "../xrRender/QueryHelper.h"
#include "UpscalerRuntime.h"

namespace
{
enum ERenderPhaseProfile : u32
{
	RenderPhaseVisibility,
	RenderPhaseGBuffer,
	RenderPhaseLightVisibility,
	RenderPhaseSSS,
	RenderPhaseSun,
	RenderPhaseLocalLights,
	RenderPhaseCombine,
	RenderPhaseHud,
	RenderPhaseCount
};

struct SRenderPhaseAccumulator
{
	u32 frames = 0;
	u64 ticks[RenderPhaseCount] = {};
	u64 maxTicks[RenderPhaseCount] = {};
	u64 drawCalls[RenderPhaseCount] = {};
	u64 frameDrawCalls = 0;
	u64 staticDips = 0;
	u64 dynamicDips = 0;
	u64 detailDips = 0;
	u64 localLights = 0;
};

struct SRenderPhaseToken
{
	ERenderPhaseProfile phase;
	u64 startedAt;
	u32 drawCalls;
	bool enabled;
};

SRenderPhaseAccumulator g_renderPhaseProfile;

bool RenderPhaseProfileEnabled()
{
	return mt_FrameProfile && mt_FrameProfileDetailed && CPU::qpc_freq &&
		!Device.dwPrecacheFrame && !Device.m_SecondViewport.IsSVPFrame();
}

SRenderPhaseToken BeginRenderPhase(ERenderPhaseProfile phase)
{
	const bool enabled = RenderPhaseProfileEnabled();
	return {phase, enabled ? CPU::QPC() : 0, enabled ? RCache.stat.calls : 0, enabled};
}

void EndRenderPhase(const SRenderPhaseToken& token)
{
	if (!token.enabled)
		return;

	const u64 elapsed = CPU::QPC() - token.startedAt;
	g_renderPhaseProfile.ticks[token.phase] += elapsed;
	g_renderPhaseProfile.maxTicks[token.phase] =
		std::max(g_renderPhaseProfile.maxTicks[token.phase], elapsed);
	g_renderPhaseProfile.drawCalls[token.phase] += RCache.stat.calls - token.drawCalls;
}

void FinishRenderPhaseFrame(u32 localLights)
{
	if (!RenderPhaseProfileEnabled())
		return;

	SRenderPhaseAccumulator& profile = g_renderPhaseProfile;
	++profile.frames;
	profile.frameDrawCalls += RCache.stat.calls;
	profile.staticDips += RCache.stat.r.s_static.dips;
	profile.dynamicDips += RCache.stat.r.s_dynamic.dips;
	profile.detailDips += RCache.stat.r.s_details.dips;
	profile.localLights += localLights;

	if (profile.frames < 300)
		return;

	const double averageMs = 1000.0 / (double(CPU::qpc_freq) * double(profile.frames));
	const double maximumMs = 1000.0 / double(CPU::qpc_freq);
	Msg("* [render-phase/profile] avg-ms visibility/gbuffer/light-vis/sss/sun/local/combine/hud="
		"%.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f",
		profile.ticks[RenderPhaseVisibility] * averageMs,
		profile.ticks[RenderPhaseGBuffer] * averageMs,
		profile.ticks[RenderPhaseLightVisibility] * averageMs,
		profile.ticks[RenderPhaseSSS] * averageMs,
		profile.ticks[RenderPhaseSun] * averageMs,
		profile.ticks[RenderPhaseLocalLights] * averageMs,
		profile.ticks[RenderPhaseCombine] * averageMs,
		profile.ticks[RenderPhaseHud] * averageMs);
	Msg("* [render-phase/profile] max-ms visibility/gbuffer/light-vis/sss/sun/local/combine/hud="
		"%.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f",
		profile.maxTicks[RenderPhaseVisibility] * maximumMs,
		profile.maxTicks[RenderPhaseGBuffer] * maximumMs,
		profile.maxTicks[RenderPhaseLightVisibility] * maximumMs,
		profile.maxTicks[RenderPhaseSSS] * maximumMs,
		profile.maxTicks[RenderPhaseSun] * maximumMs,
		profile.maxTicks[RenderPhaseLocalLights] * maximumMs,
		profile.maxTicks[RenderPhaseCombine] * maximumMs,
		profile.maxTicks[RenderPhaseHud] * maximumMs);
	Msg("* [render-phase/profile] avg-work draws/static/dynamic/details/local-lights="
		"%.1f/%.1f/%.1f/%.1f/%.1f phase-draws(gbuffer/sun/local/combine)=%.1f/%.1f/%.1f/%.1f",
		double(profile.frameDrawCalls) / profile.frames,
		double(profile.staticDips) / profile.frames,
		double(profile.dynamicDips) / profile.frames,
		double(profile.detailDips) / profile.frames,
		double(profile.localLights) / profile.frames,
		double(profile.drawCalls[RenderPhaseGBuffer]) / profile.frames,
		double(profile.drawCalls[RenderPhaseSun]) / profile.frames,
		double(profile.drawCalls[RenderPhaseLocalLights]) / profile.frames,
		double(profile.drawCalls[RenderPhaseCombine]) / profile.frames);
	profile = {};
}

class SvpQualityPassScope
{
	CRenderTarget* target;
public:
	explicit SvpQualityPassScope(CRenderTarget* value) : target(value && value->begin_svp_quality_pass() ? value : nullptr) {}
	~SvpQualityPassScope() { restore(); }
	void restore()
	{
		if (target)
		{
			target->end_svp_quality_pass();
			target = nullptr;
		}
	}
};
}

void CRender::render_menu()
{
	PIX_EVENT(render_menu);
	//	Globals
	RCache.set_CullMode(CULL_CCW);
	RCache.set_Stencil(FALSE);
	RCache.set_ColorWriteEnable();

	const bool nativeMenu = Target->upscaler_active();
	const ref_rt& menuColor = nativeMenu ? Target->rt_UpscalePost : Target->rt_Generic_0;
	const ref_rt& menuDistortion = nativeMenu ? Target->rt_ui_pda : Target->rt_Generic_1;
	ID3DDepthStencilView* menuDepth = nativeMenu ? nullptr : Target->main_depth();
	if (nativeMenu)
	{
		// Both native menu targets were SRVs during the previous composition.
		// Commit their unbind before either resource becomes an RTV again.
		RCache.set_Textures(nullptr);
		SRVSManager.Apply();
	}

	// Main Render
	{
		// The world buffers are intentionally low resolution with DLSS/FSR, but
		// menu text and controls must remain at the display resolution. Reuse the
		// full-size upscale output as a private menu color target.
		Target->u_setrt(menuColor, 0, 0, menuDepth);
		if (nativeMenu)
			rmNormal();
		g_pGamePersistent->OnRenderPPUI_main(); // PP-UI
	}

	// Distort
	{
		FLOAT ColorRGBA[4] = {127.0f / 255.0f, 127.0f / 255.0f, 0.0f, 127.0f / 255.0f};
		// rt_ui_pda is also display-sized and is idle while the main menu is
		// rendered, so it can hold the native distortion/magnifier mask without
		// allocating another permanent full-resolution render target.
		Target->u_setrt(menuDistortion, 0, 0, menuDepth);
		if (nativeMenu)
			rmNormal();
		HW.pContext->ClearRenderTargetView(menuDistortion->pRT, ColorRGBA);
		g_pGamePersistent->OnRenderPPUI_PP(); // PP-UI
	}

	// Actual Display
	Target->u_setrt(Device.dwWidth, Device.dwHeight, HW.pBaseRT,NULL,NULL, HW.pBaseZB);
	rmNormal();
	if (nativeMenu)
		RCache.set_Element(Target->upscaler_menu_element());
	else
		RCache.set_Shader(Target->s_menu);
	RCache.set_Geometry(Target->g_menu);

	Fvector2 p0, p1;
	u32 Offset;
	auto C = color_rgba(255, 255, 255, 255);
	float _w = float(Device.dwWidth);
	float _h = float(Device.dwHeight);
	float d_Z = EPS_S;
	float d_W = 1.f;
	p0.set(.5f / _w, .5f / _h);
	p1.set((_w + .5f) / _w, (_h + .5f) / _h);

	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, Target->g_menu->vb_stride, Offset);
	pv->set(EPS, float(_h + EPS), d_Z, d_W, C, p0.x, p1.y);
	pv++;
	pv->set(EPS, EPS, d_Z, d_W, C, p0.x, p0.y);
	pv++;
	pv->set(float(_w + EPS), float(_h + EPS), d_Z, d_W, C, p1.x, p1.y);
	pv++;
	pv->set(float(_w + EPS), EPS, d_Z, d_W, C, p1.x, p0.y);
	pv++;
	RCache.Vertex.Unlock(4, Target->g_menu->vb_stride);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
}

extern u32 g_r;

void CRender::Render()
{
	PIX_EVENT(CRender_Render);

	rmNormal();

	bool _menu_pp = g_pGamePersistent ? g_pGamePersistent->OnRenderPPUI_query() : false;
	if (_menu_pp)
	{
		render_menu();
		return;
	};

	IMainMenu* pMainMenu = g_pGamePersistent ? g_pGamePersistent->m_pMainMenu : 0;
	bool bMenu = pMainMenu ? pMainMenu->CanSkipSceneRendering() : false;

	if (!(g_pGameLevel && g_hud)
		|| bMenu)
	{
		Target->u_setrt(Device.dwWidth, Device.dwHeight, HW.pBaseRT,NULL,NULL, HW.pBaseZB);
		return;
	}

	if (m_bFirstFrameAfterReset)
	{
		for (light* L : v_all_lights)//critical!!!
			L->m_moving_frames = 0;

		m_bFirstFrameAfterReset = false;
		return;
	}

	if (Target->upscaler_active() && !Device.m_SecondViewport.IsSVPFrame())
		g_AnthologyUpscaler.UpdateJitter(Device.dwFrame);

	SvpQualityPassScope svpQualityScope(Target);

	//.	VERIFY					(g_pGameLevel && g_pGameLevel->pHUD);

	// Configure
	RImplementation.o.distortion = FALSE; // disable distorion
	Fcolor sun_color = ((light*)Lights.sun_adapted._get())->color;
	BOOL bSUN = ps_r2_ls_flags.test(R2FLAG_SUN) && (u_diffuse2s(sun_color.r, sun_color.g, sun_color.b)>EPS) && !Core.ParamsData.test(ECoreParams::r4_dev);
	if (o.sunstatic) bSUN = FALSE;
	// Msg						("sstatic: %s, sun: %s",o.sunstatic?;"true":"false", bSUN?"true":"false");

	const SRenderPhaseToken visibilityPhase = BeginRenderPhase(RenderPhaseVisibility);
	// HOM
	ViewBase.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);
	HOM.Enable();
	HOM.Render(ViewBase);

	Target->phase_scene_prepare();

	//******* Main calc - DEFERRER RENDERER
	phase = PHASE_NORMAL;
	// phase_upscale leaves a display-sized viewport for the native HUD. The
	// scene targets above are core-sized, so restore their viewport before the
	// first world draw of the next frame.
	rmNormal();
	
	/*if (RImplementation.o.ssfx_core) // SSS23: DEPRECATED
	{
		// HUD Masking rendering
		FLOAT ColorRGBA[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		HW.pContext->ClearRenderTargetView(Target->rt_ssfx_hud->pRT, ColorRGBA);

		Target->u_setrt(Target->rt_ssfx_hud, NULL, NULL, HW.pBaseZB);
		r_dsgraph_render_hud(true);

		// Reset Depth
		HW.pContext->ClearDepthStencilView(HW.pBaseZB, D3D_CLEAR_DEPTH, 1.0f, 0);
	}*/

    GMBase.traverse(RImplementation.pLastSector, ViewBase, Device.vCameraPosition, Device.mFullTransform);
    GMBase.r_dsgraph_capture_static();
    GMBase.r_dsgraph_capture_dynamic();
	EndRenderPhase(visibilityPhase);

	const SRenderPhaseToken firstGBufferPhase = BeginRenderPhase(RenderPhaseGBuffer);
    if (RImplementation.o.ssfx_motionvectors)
    {
		Target->u_setrt(Target->get_core_width(), Target->get_core_height(), 0, 0, Target->rt_ssfx_motion_vectors->pRT, 0);

        FLOAT ColorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        HW.pContext->ClearRenderTargetView(Target->rt_ssfx_motion_vectors->pRT, ColorRGBA);

        RCache.set_Stencil(FALSE);
        g_pGamePersistent->Environment().RenderSky(true);

        RCache.Index.Flush();
        RCache.Vertex.Flush();

        RCache.set_xform_world(Fidentity);
    }

	if (ps_r2_ls_flags.test(R2FLAG_TERRAIN_PREPASS))
	{
		Target->u_setrt(Target->get_core_width(), Target->get_core_height(), NULL, NULL, NULL,
			!RImplementation.o.dx10_msaa ? Target->main_depth() : Target->rt_MSAADepth->pZRT);
	}

	//******* Main render :: PART-0	-- first
	{
		PIX_EVENT(DEFER_PART0_SPLIT);
		// level, SPLIT
		Target->phase_scene_begin();
		GMBase.r_dsgraph_render_static(0);
		GMBase.r_dsgraph_render_dynamic(0);
		Target->disable_aniso();
	}
	EndRenderPhase(firstGBufferPhase);

	//  Redotix99: for 3D Shader Based Scopes 	
	if (scope_3D_fake_enabled)
	{
		ID3D11Resource* zbuffer_res;
		Target->main_depth()->GetResource(&zbuffer_res);
		HW.pContext->CopyResource(RImplementation.Target->rt_tempzb->pSurface, zbuffer_res);
	}

	if (RImplementation.o.dx10_msaa)
		RCache.set_ZB(RImplementation.Target->rt_MSAADepth->pZRT);

	{
		PIX_EVENT(DEFER_TEST_LIGHT_VIS);
		const SRenderPhaseToken lightVisibilityPhase = BeginRenderPhase(RenderPhaseLightVisibility);
		//******* Occlusion testing of volume-limited light-sources
		Target->phase_occq();
		LP_normal.clear();
		LP_pending.clear();
		GMBase.r_dsgraph_capture_lights();
		EndRenderPhase(lightVisibilityPhase);
	}

	const SRenderPhaseToken secondGBufferPhase = BeginRenderPhase(RenderPhaseGBuffer);
	//******* Main render :: PART-1 (second)
	{
		PIX_EVENT(DEFER_PART1_SPLIT);
		// level
		Target->phase_scene_begin();
		GMBase.r_dsgraph_capture_hud();
		GMBase.r_dsgraph_render_hud();
		GMBase.r_dsgraph_render_lods(true,true);
		// Details/grass are one of the largest avoidable draw-list costs in the
		// second camera.  Balanced and Performance deliberately omit them while
		// preserving the world geometry, lighting and the native PiP cadence.
		const bool reduced_svp_details = Device.m_SecondViewport.IsSVPFrame() &&
			ScopeLenseQualityTier() >= 2;
		if (Details && !reduced_svp_details)
			Details->Render();
		Target->phase_scene_end();
	}

	// Wall marks
	const bool reduced_svp_wallmarks = Device.m_SecondViewport.IsSVPFrame() &&
		ScopeLenseQualityTier() >= 3;
	if (Wallmarks && !reduced_svp_wallmarks)
	{
		PIX_EVENT(DEFER_WALLMARKS);
		Target->phase_wallmarks();

		Wallmarks->Render(); // wallmarks has priority as normal geometry
	}

	// full screen pass to mark msaa-edge pixels in highest stencil bit
	if (RImplementation.o.dx10_msaa)
	{
		PIX_EVENT(MARK_MSAA_EDGES);
		Target->mark_msaa_edges();
	}

	//	TODO: DX10: Implement DX10 rain.
	if (ps_r2_ls_flags.test(R3FLAG_DYN_WET_SURF))
	{
		PIX_EVENT(DEFER_RAIN);
		render_rain();
	}
	EndRenderPhase(secondGBufferPhase);

	const SRenderPhaseToken sssPhase = BeginRenderPhase(RenderPhaseSSS);
	{
		// Save previus and current matrices
		{
			static Fmatrix mm_saved_viewproj;

			if (!Device.m_SecondViewport.IsSVPFrame())
			{
				Target->Matrix_previous.mul(mm_saved_viewproj, Device.mInvView);
				Target->Matrix_current.set(Device.mProject);
				mm_saved_viewproj.set(Device.mFullTransform);
			}
		}

		if (RImplementation.o.ssfx_sss && !Device.m_SecondViewport.IsSVPFrame())
		{
			static bool sss_rendered, sss_extended_rendered;

			// SSS Shadows
			if (ps_ssfx_sss_quality.z > 0)
			{
				Target->phase_ssfx_sss();
				sss_rendered = true;
			}
			else
			{
				if (sss_rendered) // Clear buffer
				{
					sss_rendered = false;
					FLOAT ColorRGBA[4] = { 1,1,1,1 };
					HW.pContext->ClearRenderTargetView(Target->rt_ssfx_sss->pRT, ColorRGBA);
				}
			}

			if (ps_ssfx_sss_quality.w > 0)
			{
				// Extra lights
				Target->phase_ssfx_sss_ext(RImplementation.LP_normal);
				sss_extended_rendered = true;
			}
			else
			{
				if (sss_extended_rendered) // Clear buffer
				{
					sss_extended_rendered = false;
					FLOAT ColorRGBA[4] = { 1,1,1,1 };
					HW.pContext->ClearRenderTargetView(Target->rt_ssfx_sss_tmp->pRT, ColorRGBA);
				}
			}
		}
		else if (RImplementation.o.ssfx_sss)
		{
			// The local-light composite is not a temporal owner and must be neutral
			// for the lens camera. Keep rt_ssfx_sss intact: it is the presented
			// main view's directional-shadow history and an SVP clear corrupts it.
			FLOAT NeutralLocalSSS[4] = { 1, 1, 1, 1 };
			HW.pContext->ClearRenderTargetView(Target->rt_ssfx_sss_tmp->pRT, NeutralLocalSSS);
		}
	}
	EndRenderPhase(sssPhase);

	// Directional light - fucking sun
	const SRenderPhaseToken sunPhase = BeginRenderPhase(RenderPhaseSun);
	if (bSUN) //bSUN && Device.dwFrame & 1 --Delayed sun update. Worth to check it in future
	{
		PIX_EVENT(DEFER_SUN);
		RImplementation.stats.l_visible ++;
		// render_sun_cascades also performs direct-light accumulation; skipping it
		// would leave reduced-quality PiP without direct sun even when cached shadow
		// maps exist. Keep this coherent until a separate cached-cascade accumulation
		// path is implemented.
		render_sun_cascades();
		Target->increment_light_marker();
		Target->accum_direct_blend();
	}
	EndRenderPhase(sunPhase);

	phase = PHASE_NORMAL;
	const SRenderPhaseToken localLightPhase = BeginRenderPhase(RenderPhaseLocalLights);

	{
		PIX_EVENT(DEFER_SELF_ILLUM);
		Target->phase_accumulator();
		// Render emissive geometry, stencil - write 0x0 at pixel pos
		RCache.set_xform_project(Device.mProject);
		RCache.set_xform_view(Device.mView);
		// Stencil - write 0x1 at pixel pos - 
		if (!RImplementation.o.dx10_msaa)
			RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0xff, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE,
			                   D3DSTENCILOP_KEEP);
		else
			RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0x7f, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE,
			                   D3DSTENCILOP_KEEP);
		//RCache.set_Stencil				(TRUE,D3DCMP_ALWAYS,0x00,0xff,0xff,D3DSTENCILOP_KEEP,D3DSTENCILOP_REPLACE,D3DSTENCILOP_KEEP);
		RCache.set_CullMode(CULL_CCW);
		RCache.set_ColorWriteEnable();
		GMBase.r_dsgraph_render_emissive(RImplementation.o.ssfx_bloom ? false : true);
	}

	// Tiers 2/3 deliberately skip SSFX bloom in phase_combine() and clear its
	// output. Do not submit the bloom-only emissive geometry when it has no
	// consumer; the regular emissive pass above is left unchanged.
	const bool reduced_svp_bloom = Device.m_SecondViewport.IsSVPFrame() &&
		ScopeLenseQualityTier() >= 2;
	if (RImplementation.o.ssfx_bloom && !reduced_svp_bloom)
	{
		// Render Emissive on `rt_ssfx_bloom_emissive`
		FLOAT ColorRGBA[4] = { 0,0,0,0 };
		HW.pContext->ClearRenderTargetView(Target->rt_ssfx_bloom_emissive->pRT, ColorRGBA);
		Target->u_setrt(Target->rt_ssfx_bloom_emissive, NULL, NULL,
			!RImplementation.o.dx10_msaa ? Target->main_depth() : Target->rt_MSAADepth->pZRT);
		GMBase.r_dsgraph_render_emissive(true, true);
	}

	// Lighting, non dependant on OCCQ
	{
		PIX_EVENT(DEFER_LIGHT_NO_OCCQ);
		Target->phase_accumulator();
		render_lights(LP_normal);
	}

	// Lighting, dependant on OCCQ
	{
		PIX_EVENT(DEFER_LIGHT_OCCQ);
		render_lights(LP_pending);
	}

	{
		const bool reduced_svp_volumetrics = Device.m_SecondViewport.IsSVPFrame() &&
			ScopeLenseQualityTier() >= 2;
		if (RImplementation.o.ssfx_volumetric && !reduced_svp_volumetrics)
			Target->phase_ssfx_volumetric_blur();
	}
	EndRenderPhase(localLightPhase);

	phase = PHASE_NORMAL;

	// Postprocess
	{
		PIX_EVENT(DEFER_LIGHT_COMBINE);
		const SRenderPhaseToken combinePhase = BeginRenderPhase(RenderPhaseCombine);
		Target->phase_combine();
		EndRenderPhase(combinePhase);
	}
	svpQualityScope.restore();

	if (Details)
		Details->details_clear();

	if (g_hud)
	{
		const SRenderPhaseToken hudPhase = BeginRenderPhase(RenderPhaseHud);
		if (g_hud->RenderActiveItemUIQuery())
			GMBase.r_dsgraph_render_hud_ui();
		if (g_hud->RenderCamAttachedUIQuery())
			GMBase.r_dsgraph_render_cam_ui();
		EndRenderPhase(hudPhase);
	}
	FinishRenderPhaseFrame(u32(LP_normal.v_point.size() + LP_normal.v_spot.size() +
		LP_normal.v_shadowed.size() + LP_pending.v_point.size() +
		LP_pending.v_spot.size() + LP_pending.v_shadowed.size()));

}
#include "../xrRender/CHudInitializer.h"

void CRender::render_forward()
{
	RImplementation.o.distortion = RImplementation.o.distortion_enabled; // enable distorion

	//******* Main render - second order geometry (the one, that doesn't support deffering)
	//.todo: should be done inside "combine" with estimation of of luminance, tone-mapping, etc.
	{
		// level
		phase = PHASE_NORMAL;
		//	Igor: we don't want to render old lods on next frame.
		GMBase.r_dsgraph_render_static(1); // normal level, secondary priority
		CParticlesAsync::Wait();
		GMBase.r_dsgraph_render_dynamic(1);
		GMBase.fade_render(); // faded-portals
		GMBase.r_dsgraph_render_sorted(false); // strict-sorted geoms
		g_pGamePersistent->Environment().RenderLast(); // rain/thunder-bolts
		GMBase.r_dsgraph_render_sorted_hud();
	}

	RImplementation.o.distortion = FALSE; // disable distorion
}

// Redotix99: for 3D Shader Based Scopes
void CRender::render_Reticle()
{
	VERIFY(0 == GMBase.RGraph.mapHUDSorted.Distort.size() + GMBase.RGraph.mapStaticSorted.Distort.size() + GMBase.RGraph.mapDynamicSorted.Distort.size());
	RImplementation.o.distortion = RImplementation.o.distortion_enabled;

	GMBase.r_dsgraph_render_ScopeSorted();

	RImplementation.o.distortion = FALSE;
}

void CRenderTarget::phase_svp_quality(ID3D11Texture2D* source)
{
	HW.pContext->CopyResource(rt_secondVP_capture->pTexture->surface_get(), source);

	u32 offset = 0;
	const u32 color = color_rgba(255, 255, 255, 255);
	const float width = float(rt_secondVP->dwWidth);
	const float height = float(rt_secondVP->dwHeight);

	u_setrt(rt_secondVP, nullptr, nullptr, nullptr);
	RImplementation.rmNormal();
	// The scene is rendered into the upper-left scaled viewport. The resolve
	// itself must cover the complete persistent lens texture.
	D3D_VIEWPORT resolve_viewport = {0.0f, 0.0f, width, height, 0.0f, 1.0f};
	HW.pContext->RSSetViewports(1, &resolve_viewport);
	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	FVF::TL* vertices = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, offset);
	vertices->set(0.0f, height, EPS_S, 1.0f, color, 0.0f, 1.0f); ++vertices;
	vertices->set(0.0f, 0.0f, EPS_S, 1.0f, color, 0.0f, 0.0f); ++vertices;
	vertices->set(width, height, EPS_S, 1.0f, color, 1.0f, 1.0f); ++vertices;
	vertices->set(width, 0.0f, EPS_S, 1.0f, color, 1.0f, 0.0f);
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	RCache.set_Element(s_svp_quality->E[0]);
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, offset, 0, 4, 0, 2);
}

void CRender::RenderToTarget(RRT target)
{
	ref_rt* RT = nullptr;

	switch (target)
	{
	case rtPDA:
		RT = &Target->rt_ui_pda;
		break;
	case rtSVP:
		RT = &Target->rt_secondVP;
		break;
	default:
		Debug.fatal(DEBUG_INFO, "None or wrong Target specified: %i", target);
		break;
	}

	ID3DTexture2D* pBuffer = nullptr;
	HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBuffer);
	HW.pContext->CopyResource((*RT)->pSurface, pBuffer);
	pBuffer->Release();

	if (target == rtSVP && RImplementation.o.ssfx_water)
	{
		HW.pContext->CopyResource(Target->rt_ssfx_water->pTexture->surface_get(), Target->rt_ssfx_water_main->pTexture->surface_get());
		HW.pContext->CopyResource(Target->rt_ssfx_temp->pTexture->surface_get(), Target->rt_ssfx_water_blur_main->pTexture->surface_get());
	}
}
