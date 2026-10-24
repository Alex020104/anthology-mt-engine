#include "pch_script.h"
#include "UIScriptWnd.h"
#include "object_broker.h"
#include "../callback_info.h"
#include "../AddonRuntime.h"


CUIDialogWndEx::CUIDialogWndEx()
{
}

CUIDialogWndEx::~CUIDialogWndEx()
{
	ClearNativeMarker();
	delete_data(m_callbacks);
}

void CUIDialogWndEx::Register(CUIWindow* pChild)
{
	pChild->SetMessageTarget(this);
}

void CUIDialogWndEx::Register(CUIWindow* pChild, LPCSTR name)
{
	pChild->SetWindowName(name);
	pChild->SetMessageTarget(this);
}

void CUIDialogWndEx::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
	event_comparer ec(pWnd, msg);

	CALLBACK_IT it = std::find_if(m_callbacks.begin(), m_callbacks.end(), ec);
	if (it == m_callbacks.end())
		return inherited::SendMessage(pWnd, msg, pData);

	((*it)->m_callback)();

	//	if ( (*it)->m_cpp_callback )	
	//		(*it)->m_cpp_callback(pData);
}

bool CUIDialogWndEx::Load(LPCSTR xml_name)
{
	return true;
}

SCallbackInfo* CUIDialogWndEx::NewCallback()
{
	m_callbacks.push_back(xr_new<SCallbackInfo>());
	return m_callbacks.back();
}

void CUIDialogWndEx::AddCallback(LPCSTR control_id, s16 evt, const ::luabind::functor<void>& functor,
                                 const ::luabind::object& object)
{
	// Find existing callback for this control+event and replace it
	for (auto& cb : m_callbacks)
	{
		if (cb->m_control_name == control_id && cb->m_event == evt)
		{
			cb->m_callback.set(functor, object);
			return;
		}
	}
	// No existing callback, create new one
	SCallbackInfo* c = NewCallback();
	c->m_callback.set(functor, object);
	c->m_control_name = control_id;
	c->m_event = evt;
}


bool CUIDialogWndEx::OnKeyboardAction(int dik, EUIMessages keyboard_action)
{
	return inherited::OnKeyboardAction(dik, keyboard_action);
}

bool CUIDialogWndEx::OnMouseAction(float x, float y, EUIMessages mouse_action)
{
	return inherited::OnMouseAction(x, y, mouse_action);
}

void CUIDialogWndEx::Update()
{
	if (m_addonMarker) m_addonMarker->Update();
	inherited::Update();
}

bool CUIDialogWndEx::ConfigureNativeMarker(const ::luabind::object& data)
{
	if (!m_addonMarker) m_addonMarker = xr_new<CAddonMarkerLayout>();
	if (!m_addonMarker->Configure(this, data)) { ClearNativeMarker(); return false; }
	m_addonUpdateInterval = 50;
	m_addonMarker->Update();
	return true;
}

bool CUIDialogWndEx::SampleNativeMarker(const ::luabind::object& data)
{
	return addon_marker_sample(data);
}

void CUIDialogWndEx::SetNativeMarkerVisible(bool visible)
{
	if (m_addonMarker) m_addonMarker->SetVisible(visible);
}

void CUIDialogWndEx::ClearNativeMarker()
{
	xr_delete(m_addonMarker);
	m_addonUpdateInterval = 0;
	m_addonNextUpdate = 0;
}

bool CUIDialogWndEx::ScriptUpdateDue()
{
	if (!m_addonUpdateInterval) return true;
	if (s32(Device.dwTimeGlobal - m_addonNextUpdate) < 0) return false;
	m_addonNextUpdate = Device.dwTimeGlobal + m_addonUpdateInterval;
	return true;
}

void CUIDialogWndEx::SetNativeMarkerPolicyInterval(u32 interval)
{
	m_addonUpdateInterval = std::min(interval, 1000u);
	m_addonNextUpdate = 0;
}
