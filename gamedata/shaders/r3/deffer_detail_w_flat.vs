#include "common.h"

uniform float4 consts; // {1/quant,1/quant,diffusescale,ambient}
uniform float4 wave;   // cx,cy,cz,tm
uniform float4 dir2D;

// Per-phase scale fade supplied by the Anthology v87+ DX11 detail renderer:
// x=1 camera-distance fade vs y, x=2 light xz fade vs zw, x=0 no fade.
uniform float4 dt_fade_params;

// Original Anomaly waving detail shader adapted to the v87+ DX11 hardware
// instance stream. Wind, normals and lighting remain vanilla.
v2p_flat main(v_detail v,
              float4 m0 : TEXCOORD1, float4 m1 : TEXCOORD2, float4 m2 : TEXCOORD3,
              float4 inst_na : TEXCOORD4, float4 inst_sh : TEXCOORD5)
{
	v2p_flat O;

	// Transform position to world coordinates.
	float4 pos;
	pos.x = dot(m0, v.pos);
	pos.y = dot(m1, v.pos);
	pos.z = dot(m2, v.pos);
	pos.w = 1;

	// Reproduce the old CPU-side blade fade around the instance pivot. Keeping
	// it in the shader lets one instance buffer serve the main and shadow passes.
	float3 Tr = float3(m0.w, m1.w, m2.w);
	float fade = 1.0f;
	if (dt_fade_params.x > 1.5f)
	{
		float2 dxz = Tr.xz - dt_fade_params.zw;
		fade = 1.0f - dot(dxz, dxz) * 0.005f;
	}
	else if (dt_fade_params.x > 0.5f)
	{
		float3 dc = Tr - eye_position;
		fade = 1.0f - max(dot(dc, dc) - dt_fade_params.y, 0.0f) * 0.005f;
	}
	pos.xyz = Tr + (pos.xyz - Tr) * saturate(fade);

	// Original Anomaly wind.
	float base = m1.w;
	float dp = calc_cyclic(dot(pos, wave));
	float H = pos.y - base;
	float frac = v.misc.z * consts.x;
	float inten = H * dp;
	float2 result = calc_xz_wave(dir2D.xz * inten, frac);
	pos = float4(pos.x + result.x, pos.y, pos.z + result.y, 1);

	// Original Anomaly puffball normal.
	float3 norm;
	norm.x = pos.x - m0.w;
	norm.y = pos.y - m1.w + 0.75f;
	norm.z = pos.z - m2.w;

	float4 Pp = mul(m_WVP, pos);
	O.hpos = Pp;
	float3 orig = mul(m_WV, normalize(norm));
	O.N = lerp(orig, mul((float3x3)m_WV, v.pos), 0.25f);

	float3 Pe = mul(m_WV, pos);
	O.tcdh = float4((v.misc * consts).xyyy);

#if defined(USE_R2_STATIC_SUN)
	O.tcdh.w = inst_sh.x;
#endif

	O.position = float4(Pe, inst_sh.y);
	return O;
}
FXVS;
