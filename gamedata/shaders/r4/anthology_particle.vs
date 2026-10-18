#include "common.h"
#include "dlss_contract.h"
#include "screenspace_mvectors.h"

struct vv
{
    float4 P : POSITION;
    float2 tc : TEXCOORD0;
    float4 c : COLOR0;
    float4 previous : TEXCOORD1;
};
struct v2p
{
    float2 tc : TEXCOORD0;
    float4 c : COLOR0;
#ifdef USE_SOFT_PARTICLES
    float4 tctexgen : TEXCOORD1;
#endif
    float4 current_clip : TEXCOORD2;
    float4 previous_clip : TEXCOORD3;
    float motion_enabled : TEXCOORD4;
    float fog : FOG;
    float4 hpos : SV_Position;
};
uniform float4x4 mVPTexgen;

v2p main(vv v)
{
    v2p o;
    o.hpos = mul(m_WVP, v.P);
    o.current_clip = o.hpos;
    o.previous_clip = mul(m_vp_prev, float4(v.previous.xyz, 1));
    o.motion_enabled = v.previous.w * dlss_params.x;
    o.tc = v.tc;
    o.c = unpack_D3DCOLOR(v.c);
#ifdef USE_SOFT_PARTICLES
    o.tctexgen = mul(mVPTexgen, v.P);
    o.tctexgen.z = o.hpos.z;
    o.tctexgen.xy += dlss_raster_uv(float2(0, 0)) * o.tctexgen.w;
#endif
    o.fog = saturate(calc_fogging(v.P));
    o.hpos.xy = dlss_raster_clip(o.hpos);
    return o;
}
