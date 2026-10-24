#pragma once

struct lua_State;
namespace luabind { class object; }
class CUIWindow;

class CAddonMarkerLayout
{
    struct State;
    State* m_state;
public:
    CAddonMarkerLayout();
    ~CAddonMarkerLayout();
    bool Configure(CUIWindow* owner, const luabind::object& data);
    void SetVisible(bool value);
    void Update();
};

bool addon_marker_sample(const luabind::object& data);
void register_addon_runtime(lua_State* L);
