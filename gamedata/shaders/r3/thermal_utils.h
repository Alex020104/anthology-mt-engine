/*
	=====================================================================
	Original Author  : vegeta1k95
	Link             : https://www.moddb.com/mods/stalker-anomaly/addons/heatvision-v02-extension-for-beefs-nvg-dx11engine-mod
	=====================================================================
*/

#ifdef USE_MSAA
	#ifndef SM_5
        Texture2DMS<float4,MSAA_SAMPLES> s_heat;
	#else
        Texture2DMS<float4> s_heat;
	#endif
#else
    Texture2D   s_heat;
#endif

#include "thermal_coordinates.h"

#define FADE_DISTANCE_NEAR 0.0
#define FADE_DISTANCE_FAR 200.0

#define COLOR_FAR_MIN 0.037
#define COLOR_FAR_MAX 0.07
#define COLOR_NEAR_MIN 0.2425
#define COLOR_NEAR_MAX 0.325
#define COLOR_HOT 0.7
	
float3 normal_blur(float2 pos2d, int samples) 
{
    float3 accum = (0.0, 0.0, 0.0);

    for (int i = -samples; i < samples; i++) {        
        for (int j = -samples; j < samples; j++) {
    		accum += gbuf_unpack_normal(s_position.Load(int3(pos2d+float2(i, j), 0), 0).xy);
        }
	}
    
    accum /= samples * samples * 4;
    return accum;
}

float3 infrared(gbuffer_data gbd, float2 HPos, float2 Tex0) 
{	
    uint heat_width, heat_height;
#ifdef USE_MSAA
    uint heat_samples;
    s_heat.GetDimensions(heat_width, heat_height, heat_samples);
#else
    s_heat.GetDimensions(heat_width, heat_height);
#endif
    int2 heat_pixel = clamp(int2(Tex0 * float2(heat_width, heat_height)), int2(0, 0), int2(heat_width, heat_height) - 1);
    float hotness = s_heat.Load(int3(heat_pixel, 0), 0).r;
    
    float depth = gbd.P.z;
    float hot_color = COLOR_NEAR_MAX;
    if (hotness > 0.0) 
    {
        int samples = lerp(15, 4, smoothstep(0.0, 60, depth));
        float3 normal = normal_blur(HPos, samples);
        normal = normalize(normal);
        float projection = dot(normal, float3(0.0, 0.0, -1.0));
        
        if (projection <= 0.f) {
            hot_color = COLOR_NEAR_MAX;
        } else {        
            hot_color = lerp(COLOR_NEAR_MAX, COLOR_HOT, smoothstep(0, 1.0, projection));
        }
	}
	
	float projection = dot(normalize(gbd.N), float3(0.0, 0.0, -1.0)); 

	if (depth <= 0) 
	{
		depth = FADE_DISTANCE_FAR;
	}

    float mixed;
	if (depth < FADE_DISTANCE_NEAR)
	{
		mixed = lerp(COLOR_NEAR_MIN, COLOR_NEAR_MAX, projection);
	}
	else
	{
		float arg = smoothstep(FADE_DISTANCE_NEAR, FADE_DISTANCE_FAR, clamp(depth, FADE_DISTANCE_NEAR, FADE_DISTANCE_FAR));
		float min_color = lerp(COLOR_NEAR_MIN, COLOR_FAR_MIN, arg);
		float max_color = lerp(COLOR_NEAR_MAX, COLOR_FAR_MAX, arg);
		mixed = lerp(min_color, max_color, projection);
	}
    mixed = lerp(mixed, hot_color, hotness);

    return mixed;
}