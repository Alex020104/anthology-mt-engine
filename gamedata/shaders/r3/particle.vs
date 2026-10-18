#include "common.h"
#include "dlss_contract.h"

struct vv
{
	float4 P	: POSITION;
	float2 tc	: TEXCOORD0;
	float4 c	: COLOR0;
};

struct v2p
{
	float2 tc	: TEXCOORD0;
	float4 c	: COLOR0;

//	Igor: for additional depth dest
#ifdef	USE_SOFT_PARTICLES
	float4 tctexgen	: TEXCOORD1;
#endif	//	USE_SOFT_PARTICLES
	float  fog	: FOG;	// Fog
	float4 hpos	: SV_Position;
};

uniform float4x4 	mVPTexgen;

v2p main (vv v)
{
	v2p 		o;

	o.hpos 		= mul	(m_WVP, v.P);		// xform, input in world coords
	o.tc		= v.tc;				// copy tc
	o.c			= unpack_D3DCOLOR(v.c);				// copy color

//	Igor: for additional depth dest
#ifdef	USE_SOFT_PARTICLES
	o.tctexgen 	= mul( mVPTexgen, v.P);
	o.tctexgen.z	= o.hpos.z;
#endif	//	USE_SOFT_PARTICLES
	o.fog = saturate(calc_fogging(v.P));	// // ForserX (Port SkyLoader fog fix): fog, input in world coords

    // Scene particles and their soft-depth lookup must use the same raster grid.
    // dlss_params is disabled in shadow and native HUD passes by the engine.
    o.hpos.xy = dlss_raster_clip(o.hpos);
#ifdef USE_SOFT_PARTICLES
    o.tctexgen.xy += dlss_raster_uv(float2(0, 0)) * o.tctexgen.w;
#endif
	return o;
}