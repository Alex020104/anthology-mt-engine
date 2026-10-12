/* Full-screen thermal palette using the same $user$heat buffer as 3DSS. */

#ifndef THERMAL_GOGGLES_H
#define THERMAL_GOGGLES_H

#include "common.h"

#ifdef USE_MSAA
	#ifndef SM_5
		Texture2DMS<float4,MSAA_SAMPLES> s_heat;
	#else
		Texture2DMS<float4> s_heat;
	#endif
#else
	Texture2D s_heat;
#endif

// The MCM packs the cutoff distance as metres / 10 in shader_param_8.y.
// Values below the supported MCM minimum fall back to the tested 140 m preset.
float thermal_cold_fade_end()
{
	// The third fractional digit is reserved for the battery-effects toggle.
	float range_index = floor(frac(shader_param_8.y) * 100.0f + 0.05f);
	return range_index >= 5.0f ? clamp(range_index * 10.0f, 50.0f, 300.0f) : 140.0f;
}

float thermal_cold_fade_start()
{
	return max(10.0f, thermal_cold_fade_end() - 40.0f);
}

// $user$heat follows the internal render resolution.  With DLSS/FSR this is
// smaller than screen_res, while older SSS builds and native/DLAA rendering
// normally keep both sizes equal.  Always deriving pixel coordinates from the
// resource itself therefore supports both paths without a version-specific
// switch or a user-tuned offset.
int2 thermal_heat_size()
{
	uint width = 1;
	uint height = 1;
#ifdef USE_MSAA
	uint samples = 1;
	s_heat.GetDimensions(width, height, samples);
#else
	s_heat.GetDimensions(width, height);
#endif
	return max(int2(width, height), int2(1, 1));
}

int2 thermal_heat_pixel(float2 tc, int2 offset, int2 heat_size)
{
	int2 p = int2(saturate(tc) * float2(heat_size)) + offset;
	return clamp(p, int2(0, 0), heat_size - int2(1, 1));
}

float thermal_heat_sample(float2 tc)
{
	int2 heat_size = thermal_heat_size();
	int2 p = thermal_heat_pixel(tc, int2(0, 0), heat_size);
	float3 h = s_heat.Load(int3(p, 0), 0).rgb * 4.0f;
	h += s_heat.Load(int3(thermal_heat_pixel(tc, int2( 1,  0), heat_size), 0), 0).rgb;
	h += s_heat.Load(int3(thermal_heat_pixel(tc, int2(-1,  0), heat_size), 0), 0).rgb;
	h += s_heat.Load(int3(thermal_heat_pixel(tc, int2( 0,  1), heat_size), 0), 0).rgb;
	h += s_heat.Load(int3(thermal_heat_pixel(tc, int2( 0, -1), heat_size), 0), 0).rgb;
	h *= 0.125f;
	return saturate(max(h.r, max(h.g, h.b)));
}

float thermal_distance_gate(gbuffer_data gbd, float2 tc)
{
	// Cold geometry is clipped by range, while a sufficiently strong semantic
	// heat signal can still punch through as a distant target.
	float geometry = step(SKY_EPS, gbd.P.z);
	float cold_visibility = geometry * (1.0f - smoothstep(thermal_cold_fade_start(), thermal_cold_fade_end(), gbd.P.z));
	float hot_visibility = smoothstep(0.030f, 0.320f, thermal_heat_sample(tc));
	return max(cold_visibility, hot_visibility);
}

float thermal_heat_raw(float2 tc, int2 offset)
{
	int2 heat_size = thermal_heat_size();
	int2 p = thermal_heat_pixel(tc, offset, heat_size);
	float3 h = s_heat.Load(int3(p, 0), 0).rgb;
	// The depth buffer is addressed in output/screen pixels. Convert the actual
	// heat texel back to normalized coordinates before performing the depth gate.
	float2 sample_tc = (float2(p) + 0.5f) / float2(heat_size);
	uint depth_width, depth_height;
#ifdef USE_MSAA
    uint depth_samples;
    s_position.GetDimensions(depth_width, depth_height, depth_samples);
#else
    s_position.GetDimensions(depth_width, depth_height);
#endif
    float2 depth_size = float2(depth_width, depth_height);
	int2 depth_p = int2(saturate(sample_tc) * depth_size);
	depth_p = clamp(depth_p, int2(0, 0), int2(depth_size) - int2(1, 1));
	float depth = s_position.Load(int3(depth_p, 0), 0).z;
	float world_target = smoothstep(0.40f, 1.10f, depth);
	return saturate(max(h.r, max(h.g, h.b))) * world_target;
}

float thermal_luminance(gbuffer_data gbd, float2 tc)
{
	if (gbd.P.z <= SKY_EPS)
		return 0.08f;

	float hotness = thermal_heat_sample(tc);
	// Atmospheric loss is deliberately mild: distant living targets remain
	// visible, but no longer keep exactly the same contrast at every range.
	hotness *= lerp(1.0f, 0.72f, smoothstep(70.0f, 160.0f, gbd.P.z));
	float facing = saturate(dot(normalize(gbd.N), float3(0.0f, 0.0f, -1.0f)) * 0.5f + 0.5f);
	// Albedo comes directly from the G-buffer and therefore does not become dark
	// at night or react to flashlights. It supplies stable material detail while
	// the normal term keeps the shapes of otherwise flat surfaces readable.
	float albedo = pow(saturate(dot(gbd.C, float3(0.299f, 0.587f, 0.114f))), 0.65f);
	float cold = saturate(0.15f + facing * 0.18f + albedo * 0.16f);
	float cold_visibility = 1.0f - smoothstep(thermal_cold_fade_start(), thermal_cold_fade_end(), gbd.P.z);
	cold = lerp(0.11f, cold, cold_visibility);
	float hot = lerp(0.68f, 1.0f, pow(facing, 0.55f));
	return saturate(lerp(cold, hot, smoothstep(0.02f, 0.82f, hotness)));
}

float3 thermal_ironbow(float x)
{
	x = saturate(x);
	float3 c0 = float3(0.01f, 0.00f, 0.03f);
	float3 c1 = float3(0.24f, 0.00f, 0.42f);
	float3 c2 = float3(0.90f, 0.07f, 0.03f);
	float3 c3 = float3(1.00f, 0.76f, 0.04f);
	float3 c4 = float3(1.00f, 1.00f, 0.96f);
	if (x < 0.25f) return lerp(c0, c1, x * 4.0f);
	if (x < 0.50f) return lerp(c1, c2, (x - 0.25f) * 4.0f);
	if (x < 0.75f) return lerp(c2, c3, (x - 0.50f) * 4.0f);
	return lerp(c3, c4, (x - 0.75f) * 4.0f);
}

float3 thermal_outline_image(gbuffer_data gbd, float2 tc)
{
	// A one-pixel Sobel filter over the semantic heat buffer produces a stable
	// silhouette without outlining every branch and blade of grass.
	float tl = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2(-1, -1)));
	float t  = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2( 0, -1)));
	float tr = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2( 1, -1)));
	float l  = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2(-1,  0)));
	float c  = smoothstep(0.015f, 0.22f, thermal_heat_raw(tc, int2( 0,  0)));
	float r  = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2( 1,  0)));
	float bl = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2(-1,  1)));
	float b  = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2( 0,  1)));
	float br = smoothstep(0.020f, 0.26f, thermal_heat_raw(tc, int2( 1,  1)));

	float gx = -tl - 2.0f * l - bl + tr + 2.0f * r + br;
	float gy = -tl - 2.0f * t - tr + bl + 2.0f * b + br;
	float silhouette = smoothstep(0.22f, 1.80f, length(float2(gx, gy)));

	float normal_detail = saturate((length(ddx(gbd.N)) + length(ddy(gbd.N))) * 0.90f);
	float depth_detail = saturate((abs(ddx(gbd.P.z)) + abs(ddy(gbd.P.z))) * 0.12f);
	float scene_detail = saturate(normal_detail * 0.22f + depth_detail * 0.12f);
	float hot_detail = c * saturate(normal_detail * 0.55f + depth_detail * 0.18f);

	// Start above absolute black even for sky/no-G-buffer pixels.  Geometry below
	// replaces this floor with a cyan rendering of the same stable cold-value
	// range used by the monochrome modes.
	float3 image = float3(0.050f, 0.145f, 0.165f);
	if (gbd.P.z > SKY_EPS)
	{
		float facing = saturate(dot(normalize(gbd.N), float3(0.0f, 0.0f, -1.0f)) * 0.5f + 0.5f);
		float albedo = pow(saturate(dot(gbd.C, float3(0.299f, 0.587f, 0.114f))), 0.65f);
		float cold_visibility = 1.0f - smoothstep(thermal_cold_fade_start(), thermal_cold_fade_end(), gbd.P.z);
		float cold_value = saturate(0.18f + facing * 0.20f + albedo * 0.22f);
		float3 cold_image = cold_value * float3(0.34f, 0.82f, 0.92f);
		image = lerp(image, cold_image, cold_visibility);
		image += scene_detail * float3(0.075f, 0.220f, 0.250f) * cold_visibility;
	}

	// Hot objects retain a faint translucent body; their silhouette and surface
	// breaks receive the bright cyan-white emission seen in outline thermals.
	image = lerp(image, float3(0.120f, 0.380f, 0.420f), c * 0.78f);
	image += hot_detail * float3(0.16f, 0.52f, 0.58f);
	image += silhouette * float3(0.42f, 0.92f, 0.95f);
	return saturate(image);
}

float3 thermal_palette(float heat, int mode)
{
	if (mode == 1) return pow(saturate(1.0f - heat), 0.90f).xxx;
	// Keep cold scenery in the black/purple part of the ironbow ramp while
	// preserving yellow/white for models marked in the heat buffer.
	if (mode == 2) return thermal_ironbow(pow(saturate((heat - 0.10f) / 0.90f), 0.90f));
	return pow(saturate(heat), 0.90f).xxx;
}

#endif
