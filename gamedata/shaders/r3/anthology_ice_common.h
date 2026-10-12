// Ice helpers derived from Ascii1457 Screen Space Shaders Update 14.
// https://www.moddb.com/mods/stalker-anomaly/addons/screen-space-shaders
#ifdef USE_MSAA
	Texture2DMS<float4,MSAA_SAMPLES> s_rimage;
#else
	Texture2D s_rimage;
#endif

uniform float4 sky_color;

TextureCube sky_s0;
TextureCube sky_s1;

static const float2 ssfx_pixel_size = 1.0f / screen_res.xy;
static const float ssfx_PI = 3.14159265f;

struct RayTrace
{
	float2 r_pos;
	float2 r_step;
	float2 r_start;
	float r_length;
	float z_start;
	float z_end;
};

bool SSFX_is_saturated(float2 value) { return (value.x == saturate(value.x) && value.y == saturate(value.y)); }

bool SSFX_is_valid_uv(float2 value) { return (value.x >= 0.0f && value.x <= 1.0f) && (value.y >= 0.0f && value.y <= 1.0f); }

float2 SSFX_view_to_uv(float3 Pos)
{
	float4 tc = mul(m_P, float4(Pos, 1));
	return (tc.xy / tc.w) * float2(0.5f, -0.5f) + 0.5f;
}

float SSFX_calc_SSR_fade(float2 tc, float start, float end)
{
	// Vectical fade
	float ray_fade = saturate(tc.y * 5.0f);
	
	// Horizontal fade
	float2 calc_edges = smoothstep(start, end, float2(tc.x, 1.0f - tc.x));
	ray_fade *= calc_edges.x * calc_edges.y;

	return ray_fade;
}

float3 SSFX_yaw_vector(float3 Vec, float Rot)
{
	float s, c;
	sincos(Rot, s, c);

	// y-axis rotation matrix
	float3x3 rot_mat = 
	{
		c, 0, s,
		0, 1, 0,
		-s, 0, c
	};
	return mul(rot_mat, Vec);
}

float SSFX_get_depth(float2 tc, uint iSample : SV_SAMPLEINDEX)
{
	#ifndef USE_MSAA
		return s_position.SampleLevel(smp_nofilter, tc, 0).z;
	#else
		return s_position.Load(int3(tc * screen_res.xy, 0), iSample).z;
	#endif
}

float4 SSFX_get_position(float2 tc, uint iSample : SV_SAMPLEINDEX)
{
	#ifndef USE_MSAA
		return s_position.SampleLevel(smp_nofilter, tc, 0);
	#else
		return s_position.Load(int3(tc * screen_res.xy, 0), iSample);
	#endif
}

float3 SSFX_get_image(float2 tc, uint iSample : SV_SAMPLEINDEX)
{
	#ifndef USE_MSAA
		return s_rimage.SampleLevel(smp_nofilter, tc, 0).rgb;
	#else
		return s_rimage.Load(int3(tc * screen_res.xy, 0), 0).rgb;
	#endif
}

RayTrace SSFX_ray_init(float3 ray_start_vs, float3 ray_dir_vs, float ray_max_dist, int ray_steps, float noise)
{
	RayTrace rt;
	
	float3 ray_end_vs = ray_start_vs + ray_dir_vs * ray_max_dist;
	
	// Ray depth ( from start and end point )
	rt.z_start		= ray_start_vs.z;
	rt.z_end		= ray_end_vs.z;

	// Compute ray start and end (in UV space)
	rt.r_start		= SSFX_view_to_uv(ray_start_vs);
	float2 ray_end	= SSFX_view_to_uv(ray_end_vs);

	// Compute ray step
	float2 ray_diff	= ray_end - rt.r_start;
	rt.r_step		= ray_diff / (float)ray_steps; 
	rt.r_length		= length(ray_diff);
	
	rt.r_pos		= rt.r_start + rt.r_step * noise;
	
	return rt;
}

float3 SSFX_ray_intersect(RayTrace Ray, uint iSample)
{
	float len = length(Ray.r_pos - Ray.r_start);
	float alpha = len / Ray.r_length;
	float depth_ray = (Ray.z_start * Ray.z_end) / lerp(Ray.z_end, Ray.z_start, alpha);
	float depth_scene = SSFX_get_depth(Ray.r_pos, iSample);
	
	return float3(depth_ray.x - depth_scene, depth_scene, len);
}

float3 SSFX_calc_sky(float3 dir)
{
	dir = SSFX_yaw_vector(dir, -sky_color.w); // Sky rotation
	
	dir.y = (dir.y - max(cos(dir.x) * 0.65f, cos(dir.z) * 0.65f)) * 2.1f; // Fix perspective
	dir.y -= -0.35; // Altitude
	
	float3 sky0 = sky_s0.SampleLevel(smp_base, dir, 0).xyz;
	float3 sky1 = sky_s1.SampleLevel(smp_base, dir, 0).xyz;
	
	// Use hemi color or real sky color if the modded executable is installed.
#ifndef SSFX_MODEXE
	return saturate(L_hemi_color.rgb) * lerp(sky0, sky1, L_ambient.w);
#else
	return saturate(sky_color.bgr) * lerp(sky0, sky1, L_ambient.w);
#endif
}

