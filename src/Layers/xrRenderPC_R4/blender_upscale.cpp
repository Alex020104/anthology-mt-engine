#include "stdafx.h"
#include "blender_upscale.h"

CBlender_upscale::CBlender_upscale()
{
    description.CLS = 0;
}

void CBlender_upscale::Compile(CBlender_Compile& C)
{
    IBlender::Compile(C);

	if (C.iElement == 4 || C.iElement == 5)
	{
		const bool colorMap = C.iElement == 5;
		C.r_Pass("stub_notransform_postpr",
			colorMap ? "anthology_upscale_postprocess_cm" : "anthology_upscale_postprocess",
			FALSE, FALSE, FALSE, FALSE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
		C.r_dx10Texture("s_base0", r4_RT_upscale_post);
		C.r_dx10Texture("s_base1", r4_RT_upscale_post);
		C.r_dx10Texture("s_noise", "fx\\fx_noise2");
		if (colorMap)
		{
			C.r_dx10Texture("s_grad0", "$user$cmap0");
			C.r_dx10Texture("s_grad1", "$user$cmap1");
		}
		C.r_dx10Sampler("smp_rtlinear");
		C.r_dx10Sampler("smp_linear");
		C.r_End();
		return;
	}

	if (C.iElement == 2)
	{
		// Native-resolution menu composition. The normal world generic targets
		// are core-sized while an upscaler is active, so the menu uses the
		// display-sized upscale output and PDA target instead.
		C.r_Pass("stub_notransform_t", "distort", FALSE, FALSE, FALSE);
		C.r_dx10Texture("s_base", r4_RT_upscale_post);
		C.r_dx10Texture("s_distort", r2_RT_ui);
		C.r_dx10Sampler("smp_rtlinear");
		C.r_End();
		return;
	}

	if (C.iElement == 3)
	{
		// Convert the typeless D24S8 main depth SRV to a dedicated R32_FLOAT
		// texture. FSR must not receive the packed depth/stencil resource.
		C.r_Pass("stub_notransform_t", "anthology_upscale_depth", FALSE, FALSE, FALSE);
		C.r_dx10Texture("s_image", r2_RT_depth);
		C.r_End();
		return;
	}

	if (C.iElement == 1)
	{
		// Recover a linear scene signal from the SSS split-tonemap target before
		// handing it to the temporal vendor. This pass also performs the required
		// UNORM-to-FP16 conversion without an invalid CopyResource.
		C.r_Pass("stub_notransform_t", "anthology_upscale_prepare", FALSE, FALSE, FALSE);
		C.r_dx10Texture("s_image", r2_RT_generic0);
		C.r_dx10Sampler("smp_rtlinear");
		C.r_End();
		return;
	}

    C.r_Pass("stub_notransform_t", "anthology_upscale_copy", FALSE, FALSE, FALSE);
    C.r_dx10Texture("s_image", C.iElement == 0 ? r4_RT_upscale_input : r4_RT_upscale_output);
    C.r_dx10Sampler("smp_rtlinear");
    C.r_End();
}
