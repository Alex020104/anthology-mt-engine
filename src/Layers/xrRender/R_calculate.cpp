#include "stdafx.h"
#include "../../xrEngine/customhud.h"
#include "xrRender_console.h"

float g_fSCREEN;

extern float r_dtex_range;
extern float r_ssaDISCARD;
extern float r_ssaDONTSORT;
extern float r_ssaLOD_A;
extern float r_ssaLOD_B;
extern float r_ssaHZBvsTEX;
extern float r_ssaGLOD_start, r_ssaGLOD_end;

void CRender::Calculate()
{
	// Transfer to global space to avoid deep pointer access
	float fov_factor = _sqr(90.f / Device.fFOV);
	// The previous frame ends on the display-sized upscale target, while this
	// calculation runs before the next core render target is selected. Using the
	// mutable target dimensions here kept SSA/LOD submission at native resolution
	// even when DLSS/FSR rasterized the world at a lower resolution.
	const u32 renderWidth = GetMainRenderWidth();
	const u32 renderHeight = GetMainRenderHeight();
	g_fSCREEN = float(renderWidth * renderHeight) * fov_factor * (EPS_S + ps_r__LOD);
	// The SecondVP quality RT bank is selected after Calculate(), so its reduced
	// dimensions are not visible to this traversal. Apply a deliberately linear
	// PiP-only SSA/LOD budget here: the physical RT already supplies the quadratic
	// pixel saving, while scaling traversal by area caused visible geometry loss.
	// At 100% this path remains bit-for-bit identical to the normal calculation.
	if (Device.m_SecondViewport.IsSVPFrame() && ps_scope_lense_quality_percent < 100)
		g_fSCREEN *= ScopeLenseRenderScale();
	r_ssaDISCARD = _sqr(ps_r__ssaDISCARD) / g_fSCREEN;
	r_ssaDONTSORT = _sqr(ps_r__ssaDONTSORT / 3) / g_fSCREEN;
	r_ssaLOD_A = _sqr(ps_r2_ssaLOD_A / 3) / g_fSCREEN;
	r_ssaLOD_B = _sqr(ps_r2_ssaLOD_B / 3) / g_fSCREEN;
	r_ssaGLOD_start = _sqr(ps_r__GLOD_ssa_start / 3) / g_fSCREEN;
	r_ssaGLOD_end = _sqr(ps_r__GLOD_ssa_end / 3) / g_fSCREEN;
	r_ssaHZBvsTEX = _sqr(ps_r__ssaHZBvsTEX / 3) / g_fSCREEN;
	r_dtex_range = ps_r2_df_parallax_range * g_fSCREEN / (1024.f * 768.f);

	// Detect camera-sector
	if (!vLastCameraPos.similar(Device.vCameraPosition, EPS_S))
	{
		CSector* pSector = (CSector*)detectLastSector(Device.vCameraPosition);
		if (pSector && (pSector != pLastSector))
			g_pGamePersistent->OnSectorChanged(translateSector(pSector));

		if (0 == pSector) pSector = pLastSector;
		pLastSector = pSector;
		vLastCameraPos.set(Device.vCameraPosition);
	}

	Lights.Update();
}
