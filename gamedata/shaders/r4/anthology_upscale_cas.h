// CAS adapted from OGSR's postprocess_cas.ps, based on AMD's CAS presentation:
// https://gpuopen.com/wp-content/uploads/2019/07/FidelityFX-CAS.pptx
// Kept separate from NGX: operates on the reconstructed display-resolution image.
#define dlss_sharpening anthology_upscale_sharpness
float4 anthology_upscale_sharpness;

float3 DlssCasSample(float2 uv, int2 offset)
{
    float3 c = max(display_color(uv + float2(offset) / screen_res.xy), 0.0);
    return c / (1.0 + c);
}

float4 DlssCas(float2 uv)
{
    float4 original = float4(display_color(uv), 1);
    float strength = saturate(dlss_sharpening.x);
    if (strength <= 0.0) return original;

    float3 a = DlssCasSample(uv, int2(-1, -1));
    float3 b = DlssCasSample(uv, int2( 0, -1));
    float3 c = DlssCasSample(uv, int2( 1, -1));
    float3 d = DlssCasSample(uv, int2(-1,  0));
    float3 e = max(original.rgb, 0.0);
    e = e / (1.0 + e);
    float3 f = DlssCasSample(uv, int2( 1,  0));
    float3 g = DlssCasSample(uv, int2(-1,  1));
    float3 h = DlssCasSample(uv, int2( 0,  1));
    float3 i = DlssCasSample(uv, int2( 1,  1));

    float3 mn = min(min(min(d, e), min(f, b)), h);
    mn += min(mn, min(min(a, c), min(g, i)));
    float3 mx = max(max(max(d, e), max(f, b)), h);
    mx += max(mx, max(max(a, c), max(g, i)));
    // Guard black neighborhoods against 0/0; sqrt avoids reciprocal infinity.
    float3 amplitude = sqrt(saturate(min(mn, 2.0 - mx) / max(mx, 1e-6)));
    float3 weight = -amplitude / 5.0;
    float3 sharpened = saturate(((b + d + f + h) * weight + e) / (1.0 + 4.0 * weight));
    float3 result = lerp(e, sharpened, strength);
    return float4(min(result / max(1.0 - result, 1e-5), 65504.0), original.a);
}
