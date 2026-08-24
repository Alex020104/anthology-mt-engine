#include "pch_script.h"
#include "ai_space.h"
#include "script_engine.h"
#include "ActorEffector.h"
#include "../xrEngine/ObjectAnimator.h"

void CAnimatorCamEffectorScriptCB::Start(LPCSTR fn)
{
	inherited::Start(fn);
	if (m_bAbsolutePositioning)
	{
		// Start() leaves the animator XFORM at identity. Scripted absolute
		// camera chains are added to the camera manager at the end of the frame,
		// so their first ProcessCam used to render that identity transform for
		// one frame before evaluating frame zero. Prime the same pipeline once:
		// frame zero is ready for the first render and animation time is not
		// extended by an extra duplicate frame.
		m_objectAnimator->Update(Device.fTimeDelta);
	}
}

void CAnimatorCamEffectorScriptCB::ProcessIfInvalid(SCamEffectorInfo& info)
{
	if (m_bAbsolutePositioning)
	{
		const Fmatrix& m = m_objectAnimator->XFORM();
		info.d = m.k;
		info.n = m.j;
		info.p = m.c;
		if (m_fov > 0.0f)
			info.fFov = m_fov;
	}
}

BOOL CAnimatorCamEffectorScriptCB::Valid()
{
	BOOL res = inherited::Valid();
	if (!res && cb_name.size())
	{
		::luabind::functor<LPCSTR> fl;
		R_ASSERT(ai().script_engine().functor<LPCSTR>(*cb_name,fl));
		fl();
		cb_name = "";
	}
	return res;
}
