#include "stdafx.h"
#include "flod.h"

#ifdef _EDITOR
#include "igame_persistent.h"
#include "environment.h"
#else
#include "../../xrEngine/igame_persistent.h"
#include "../../xrEngine/environment.h"
#endif

extern float r_ssaLOD_A;
extern float r_ssaLOD_B;

xr_vector<int> lstLODgroups;
void CDSGraphManager::r_dsgraph_render_lods(bool _setup_zb, bool _clear)
{
	PROF_EVENT("LODS: Render");
	if (RGraph.mapLOD.empty())
		return;

	if (RGraph.mapLOD.size() > 1)
	{
		static auto sortFuncReverse = [](const auto& a, const auto& b) { return b < a; };
		if (_setup_zb)
			std::sort(RGraph.mapLOD.begin(), RGraph.mapLOD.end());
		else
			std::sort(RGraph.mapLOD.begin(), RGraph.mapLOD.end(), sortFuncReverse);
	}

	// *** Fill VB and generate groups
	u32 shid = _setup_zb ? SE_R1_LMODELS : SE_R1_NORMAL_LQ;
	FLOD* firstV = (FLOD*)RGraph.mapLOD[0].pVisual;
	float ssaRange = r_ssaLOD_A - r_ssaLOD_B;
	if (ssaRange < EPS_S) ssaRange = EPS_S;

	const u32 uiVertexPerImposter = 4;
	const u32 uiImpostersFit = _max(1u, RCache.Vertex.GetSize()
		/ (firstV->geom->vb_stride * uiVertexPerImposter));

	//Msg						("dbg_lods: shid[%d],firstV[%X]",shid,u32((void*)firstV));
	//Msg						("dbg_lods: shader[%X]",u32((void*)firstV->shader._get()));
	//Msg						("dbg_lods: shader_E[%X]",u32((void*)cur_S._get()));

	for (u32 i = 0; i < RGraph.mapLOD.size();)
	{
		const u32 batch_start = i;
		const u32 iBatchSize = _min((u32)RGraph.mapLOD.size() - i, uiImpostersFit);
		ref_selement cur_S = RGraph.mapLOD[batch_start].pVisual->shader->E[shid];
		int cur_count = 0;
		u32 vOffset;
		FLOD::_hw* V = (FLOD::_hw*)RCache.Vertex.Lock(iBatchSize * uiVertexPerImposter, firstV->geom->vb_stride,
		                                              vOffset);

		for (u32 j = 0; j < iBatchSize; ++j, ++i)
		{
			// sort out redundancy
			auto& P = RGraph.mapLOD[i];

			if (P.pVisual->shader->E[shid] == cur_S)
				cur_count++;
			else
			{
				lstLODgroups.push_back(cur_count);
				cur_S = P.pVisual->shader->E[shid];
				cur_count = 1;
			}

			// calculate alpha
			float ssaDiff = P.ssa - r_ssaLOD_B;
			float scale = ssaDiff / ssaRange;
			int iA = iFloor((1 - scale) * 255.f);
			u32 uA = u32(clampr(iA, 0, 255));

			// calculate direction and shift
			FLOD* lodV = (FLOD*)P.pVisual;
			Fvector Ldir, shift;
			Ldir.sub(lodV->vis.sphere.P, Device.vCameraPosition).normalize();
			shift.mul(Ldir, -.5f * lodV->vis.sphere.R);

			// gen geometry
			FLOD::_face* facets = lodV->facets;
			float dot_best = -2.f;
			float dot_next = -2.f;
			float dot_next_2 = -2.f;
			u32 id_best = 0;
			u32 id_next = 0;
			for (u32 s = 0; s < 8; ++s)
			{
				const float dot = Ldir.dotproduct(facets[s].N);
				if (dot > dot_best)
				{
					dot_next_2 = dot_next;
					dot_next = dot_best;
					id_next = id_best;
					dot_best = dot;
					id_best = s;
				}
				else if (dot > dot_next)
				{
					dot_next_2 = dot_next;
					dot_next = dot;
					id_next = s;
				}
				else if (dot > dot_next_2)
				{
					dot_next_2 = dot;
				}
			}

			// Now we have two "best" planes, calculate factor, and approx normal
			float fA = dot_best, fB = dot_next, fC = dot_next_2;
			const float dot_range = _max(fA - fC, EPS_S);
			float alpha = 0.5f + 0.5f * (1 - (fB - fC) / dot_range);
			int iF = iFloor(alpha * 255.5f);
			u32 uF = u32(clampr(iF, 0, 255));

			// Fill VB
			FLOD::_face& FA = facets[id_best];
			FLOD::_face& FB = facets[id_next];
			static int vid[4] = {3, 0, 2, 1};
			for (u32 vit = 0; vit < 4; vit++)
			{
				int id = vid[vit];
				V->p0.add(FB.v[id].v, shift);
				V->p1.add(FA.v[id].v, shift);
				V->n0 = FB.N;
				V->n1 = FA.N;
				V->sun_af = color_rgba(FB.v[id].c_sun, FA.v[id].c_sun, uA, uF);
				V->t0 = FB.v[id].t;
				V->t1 = FA.v[id].t;
				V->rgbh0 = FB.v[id].c_rgb_hemi;
				V->rgbh1 = FA.v[id].c_rgb_hemi;
				V++;
			}
		}
		lstLODgroups.push_back(cur_count);
		RCache.Vertex.Unlock(iBatchSize * uiVertexPerImposter, firstV->geom->vb_stride);

		// *** Render
		RCache.set_xform_world(Fidentity);
		for (u32 uiPass = 0; uiPass < SHADER_PASSES_MAX; ++uiPass)
		{
			u32 current = batch_start;
			u32 vCurOffset = vOffset;

			for (int& p_count : lstLODgroups)
			{
				u32 uiNumPasses = RGraph.mapLOD[current].pVisual->shader->E[shid]->passes.size();
				if (uiPass < uiNumPasses)
				{
					RCache.set_Element(RGraph.mapLOD[current].pVisual->shader->E[shid], uiPass);
					RCache.set_Geometry(firstV->geom);
					RCache.Render(D3DPT_TRIANGLELIST, vCurOffset, 0, 4 * p_count, 0, 2 * p_count);
				}
				RCache.stat.r.s_flora_lods.add(4 * p_count);
				current += p_count;
				vCurOffset += 4 * p_count;
			}
		}

		lstLODgroups.clear();
	}

	if (_clear)
		RGraph.mapLOD.clear();
}
