#include "stdafx.h"
#include "blender_upscale.h"

CBlender_upscale::CBlender_upscale()
{
    description.CLS = 0;
}

void CBlender_upscale::Compile(CBlender_Compile& C)
{
    IBlender::Compile(C);
    C.r_Pass("stub_notransform_t", "anthology_upscale_copy", FALSE, FALSE, FALSE);
    C.r_dx10Texture("s_image", C.iElement == 0 ? r4_RT_upscale_input : r4_RT_upscale_output);
    C.r_dx10Sampler("smp_rtlinear");
    C.r_End();
}
