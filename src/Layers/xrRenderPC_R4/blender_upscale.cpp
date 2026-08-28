#include "stdafx.h"
#include "blender_upscale.h"

CBlender_upscale::CBlender_upscale()
{
    description.CLS = 0;
}

void CBlender_upscale::Compile(CBlender_Compile& C)
{
    IBlender::Compile(C);

	if (C.iElement == 2)
	{
		// Native-resolution menu composition. The normal world generic targets
		// are core-sized while an upscaler is active, so the menu uses the
		// display-sized upscale output and PDA target instead.
		C.r_Pass("stub_notransform_t", "distort", FALSE, FALSE, FALSE);
		C.r_dx10Texture("s_base", r4_RT_upscale_output);
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

    C.r_Pass("stub_notransform_t", "anthology_upscale_copy", FALSE, FALSE, FALSE);
    C.r_dx10Texture("s_image", C.iElement == 0 ? r4_RT_upscale_input : r4_RT_upscale_output);
    C.r_dx10Sampler("smp_rtlinear");
    C.r_End();
}
