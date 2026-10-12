// Anthology: read the actual scene depth in both native and SR rendering.
#include "dlss_contract.h"
#ifdef USE_MSAA
Texture2DMS<float4> s_wdpos;
#else
Texture2D s_wdpos;
#endif
#define WD_OCCLUDE_BIAS 0.005
void wd_occlude(float4 cpos)
{
    float2 tc = cpos.xy / cpos.w * float2(0.5, -0.5) + 0.5;
    // The widget is unjittered; scene depth is still on the raster grid.
    if (dlss_params.x > 0.5) tc += dlss_temporal_jitter.xy;
#ifdef USE_MSAA
    uint width, height, samples;
    s_wdpos.GetDimensions(width, height, samples);
    int2 pixel = clamp(int2(tc * float2(width, height)), int2(0, 0), int2(width, height) - 1);
    float scene_z = s_wdpos.Load(pixel, 0).z;
#else
    float scene_z = s_wdpos.SampleLevel(smp_nofilter, tc, 0).z;
#endif
    if (scene_z > SKY_EPS && scene_z < cpos.w - WD_OCCLUDE_BIAS) discard;
}
