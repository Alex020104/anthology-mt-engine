#include "pch_script.h"
#include "AddonRuntime.h"
#include "AddonMath.h"
#include "level.h"
#include "actor.h"
#include "ai_space.h"
#include "alife_simulator.h"
#include "alife_object_registry.h"
#include "game_graph.h"
#include "game_graph_inline.h"
#include "ai_object_location.h"
#include "xrServer_Objects_ALife.h"
#include "xrServer_Objects_ALife_Monsters.h"
#include "map_manager.h"
#include "ScriptXMLInit.h"
#include "ui/UIScriptWnd.h"
#include "ui/UIStatic.h"
#include "ui/UI3tButton.h"
#include "ui/UILines.h"
#include "script_game_object.h"
#include "../Include/xrRender/Kinematics.h"
#include "script_engine.h"
#include "entity_alive.h"
#include "../xrcdb/xr_collide_defs.h"
#include <tuple>

namespace
{
template<class T> std::optional<T> optional_field(const luabind::object& obj, const char* key)
{
    if (!obj.is_valid() || (obj.type() != LUA_TTABLE && obj.type() != LUA_TUSERDATA)) return {};
    const luabind::object value = obj[key];
    // This luabind accepts nil as false/zero for primitive types. Missing
    // settings must remain absent, including optional screen-axis locks.
    if (!value.is_valid() || value.type() == LUA_TNIL) return {};
    return luabind::object_cast_nothrow<T>(value);
}

template<class T> T field(const luabind::object& obj, const char* key, T fallback)
{
    return optional_field<T>(obj, key).value_or(fallback);
}

bool ready() { return g_pGameLevel && g_pGameLevel->bReady && g_actor && ai().get_alife(); }

luabind::object addon_sudoku_reset(luabind::object grid, unsigned difficulty, u32 seed)
{
    auto opened = luabind::newtable(ai().script_engine().lua());
    const auto board = addon_math::sudoku(difficulty,seed);
    if (!grid.is_valid() || grid.type()!=LUA_TTABLE) return opened;
    for(unsigned x=0;x<9;++x)
    {
        luabind::object row=grid[x+1];
        if (!row.is_valid() || row.type()!=LUA_TTABLE) continue;
        luabind::object visible=luabind::newtable(grid.lua_state());
        opened[x+1]=visible;
        for(unsigned y=0;y<9;++y)
        {
            luabind::object cell=row[y+1];
            CUIStatic* element=field<CUIStatic*>(cell,"elem",nullptr);
            if (!element) continue;
            cell["val"]=board.solution[x*9+y];
            string16 text="";
            if (board.given[x*9+y])
            { xr_sprintf(text,"%u",board.solution[x*9+y]);visible[y+1]=true; }
            element->TextItemControl()->SetText(text);
            element->TextItemControl()->SetTextColor(board.given[x*9+y] ?
                color_argb(255,90,200,200):color_argb(255,190,190,190));
        }
    }
    return opened;
}

bool addon_puzzle_shuffle(luabind::object grid, u32 seed)
{
    if (!grid.is_valid() || grid.type()!=LUA_TTABLE) return false;
    CUIWindow* buttons[16];
    for(unsigned i=0;i<16;++i)
    {
        luabind::object cell=grid[i+1];
        buttons[i]=field<CUIWindow*>(cell,"btn",nullptr);
        if(!buttons[i]) return false;
    }
    const auto cells=addon_math::puzzle(seed);
    for(unsigned i=0;i<16;++i)
        buttons[cells[i]]->SetWndPos(Fvector2().set(float(i%4)*55.f,float(i/4)*55.f));
    return true;
}

luabind::object addon_object_ids(luabind::object rules)
{
    auto result = luabind::newtable(ai().script_engine().lua());
    if (!ready() || !rules.is_valid() || rules.type()!=LUA_TTABLE) return result;
    xr_set<int> classes;
    luabind::object types = rules["classes"];
    const bool filter_classes = types.is_valid() && types.type() == LUA_TTABLE;
    if (filter_classes)
        for (auto i = types.begin(); i != types.end(); ++i)
        {
            auto key = luabind::object_cast_nothrow<int>(i.key());
            auto enabled = luabind::object_cast_nothrow<bool>(*i);
            if (key && enabled && *enabled) classes.insert(*key);
        }
    const LPCSTR contains = field<LPCSTR>(rules,"contains", "");
    const LPCSTR excludes = field<LPCSTR>(rules,"excludes", "");
    const bool online = field<bool>(rules,"online",false);
    int index = 1;
    for (const auto& entry : ai().alife().objects().objects())
    {
        const auto* object = entry.second;
        if (!object || entry.first == 0) continue;
        if (online && (!object->m_bOnline || !Level().Objects.net_Find(entry.first))) continue;
        if (filter_classes && !classes.count(object->script_clsid())) continue;
        const LPCSTR section = object->s_name.c_str();
        if (*contains && (!section || !strstr(section, contains))) continue;
        if (*excludes && section && strstr(section, excludes)) continue;
        result[index++] = entry.first;
    }
    return result;
}

luabind::object addon_anomaly_groups(luabind::object rules)
{
    if(!rules.is_valid() || rules.type()!=LUA_TTABLE)
        return luabind::newtable(ai().script_engine().lua());
    luabind::object result=rules["catalogue"];
    const bool supplied=result.type()==LUA_TTABLE;
    if(!supplied) result=luabind::newtable(ai().script_engine().lua());
    if (!ready()) return result;
    struct Zone { u16 id, vertex; u32 level; Fvector pos; bool retained=true; };
    xr_vector<Zone> zones;
    xr_map<u16,unsigned> indices;
    using Cell = std::tuple<u32,int,int>;
    xr_map<Cell, xr_vector<unsigned>> grid;
    xr_set<xr_string> excluded_levels;
    xr_set<u16> excluded_vertices;
    luabind::object levels = rules["excluded_levels"], vertices = rules["excluded_vertices"];
    luabind::object offsets = rules["level_offsets"];
    if (levels.is_valid() && levels.type() == LUA_TTABLE)
        for (auto i=levels.begin(); i!=levels.end(); ++i)
        { auto name=luabind::object_cast_nothrow<LPCSTR>(*i); if(name) excluded_levels.insert(*name); }
    if (vertices.is_valid() && vertices.type() == LUA_TTABLE)
        for (auto i=vertices.begin(); i!=vertices.end(); ++i)
        { auto id=luabind::object_cast_nothrow<u16>(*i); if(id) excluded_vertices.insert(*id); }
    const float radius = addon_math::clamp(field<float>(rules,"radius",20.f),1.f,1000.f);
    const unsigned required = std::min(field<unsigned>(rules,"minimum_neighbors",4), 256u);
    const LPCSTR contains = field<LPCSTR>(rules,"contains","zone_mine");
    const LPCSTR excludes = field<LPCSTR>(rules,"excludes","_soc");
    const auto& graph = ai().game_graph();
    for (const auto& entry : ai().alife().objects().objects())
    {
        const auto* object = entry.second;
        if (!object || !graph.valid_vertex_id(object->m_tGraphID)) continue;
        const LPCSTR section = object->s_name.c_str();
        if (!section || (*contains && !strstr(section,contains)) || (*excludes && strstr(section,excludes))) continue;
        const auto level = graph.vertex(object->m_tGraphID)->level_id();
        if (level >= 50 || excluded_levels.count(graph.header().level(level).name().c_str())) continue;
        if(supplied)
        {
            luabind::object group=result[level];
            if(group.type()!=LUA_TTABLE || luabind::object(group[entry.first]).type()==LUA_TNIL) continue;
        }
        Fvector pos = object->o_Position;
        if(offsets.type()==LUA_TTABLE)
        {
            luabind::object offset=offsets[level];
            if(offset.type()==LUA_TTABLE)
            {
                pos.x=float(double(pos.x)+luabind::object_cast_nothrow<double>(offset[1]).value_or(0.));
                pos.z=float(double(pos.z)+luabind::object_cast_nothrow<double>(offset[3]).value_or(0.));
            }
        }
        if (!_valid(pos)) continue;
        grid[Cell(level,int(floorf(pos.x/radius)),int(floorf(pos.z/radius)))].push_back(unsigned(zones.size()));
        indices[entry.first]=unsigned(zones.size());
        zones.push_back({entry.first, object->m_tGraphID,level,pos});
        luabind::object group=result[level];
        if(group.type()!=LUA_TTABLE) { group=luabind::newtable(result.lua_state());result[level]=group; }
        if(!supplied) group[entry.first]=object->m_tGraphID;
    }
    // Preserve Lua's existing pruning order, including removal affecting later neighbors.
    xr_vector<unsigned> order;
    for(auto level=result.begin();level!=result.end();++level)
    {
        luabind::object group=*level;
        for(auto object=group.begin();object!=group.end();++object)
        {
            auto key=luabind::object_cast_nothrow<u16>(object.key());
            if(key && indices.count(*key)) order.push_back(indices[*key]);
        }
    }
    for (unsigned index : order)
    {
        auto& zone = zones[index];
        unsigned count = 0;
        const int x=int(floorf(zone.pos.x/radius)), z=int(floorf(zone.pos.z/radius));
        const bool mapped=offsets.type()!=LUA_TTABLE || luabind::object(offsets[zone.level]).type()!=LUA_TNIL;
        for (int dx=-1; dx<=1 && count<required; ++dx)
            for (int dz=-1; dz<=1 && count<required; ++dz)
            {
                auto found=grid.find(Cell(zone.level,x+dx,z+dz));
                if(found==grid.end()) continue;
                for(unsigned other:found->second)
                {
                    if(other==index || !mapped || !zones[other].retained) continue;
                    if(zone.pos.distance_to_xz(zones[other].pos)<radius && ++count>=required) break;
                }
            }
        if (count < required) zone.retained=false;
    }
    xr_set<u16> retained_vertices;
    for(unsigned index:order)
    {
        const auto& zone=zones[index];
        if(!zone.retained || excluded_vertices.count(zone.vertex) || !retained_vertices.insert(zone.vertex).second)
        {
            luabind::object group=result[zone.level];
            group[zone.id]=static_cast<CGameObject*>(nullptr);
        }
    }
    return result;
}

luabind::object addon_unseen_nearby(const Fvector& position, float radius, luabind::object seen, bool skip_reuse)
{
    auto result = luabind::newtable(ai().script_engine().lua());
    if (!ready() || !_valid(position) || !_valid(radius) || radius <= 0.f ||
        !seen.is_valid() || seen.type() != LUA_TTABLE) return result;
    xr_vector<CObject*> objects;
    Level().ObjectSpace.GetNearest(objects,position,radius,nullptr);
    int index = 1;
    for (CObject* object : objects)
    {
        CGameObject* game_object = smart_cast<CGameObject*>(object);
        if (!game_object || game_object->ID() == 0 || game_object->ID() == u16(-1)) continue;
        luabind::object old = seen[game_object->ID()];
        const int type = old.type();
        if (type != LUA_TNIL && !(type == LUA_TBOOLEAN && !luabind::object_cast<bool>(old)))
        {
            if (skip_reuse) continue;
            auto name = luabind::object_cast_nothrow<LPCSTR>(old);
            if (name && !xr_strcmp(*name,game_object->cName().c_str())) continue;
        }
        result[index++] = game_object->ID();
    }
    return result;
}

luabind::object addon_enemy_counts(float radius, const luabind::functor<bool>& hostile, luabind::object fighting)
{
    auto result = luabind::newtable(ai().script_engine().lua());
    unsigned total=0, combat=0, alert=0;
    if (ready() && _valid(radius) && radius>0.f)
    {
        xr_vector<CObject*> objects;
        xr_map<xr_string,bool> communities;
        Level().ObjectSpace.GetNearest(objects,g_actor->Position(),radius,nullptr);
        for(CObject* candidate:objects)
        {
            auto* object=smart_cast<CGameObject*>(candidate);
            auto* living=object ? object->cast_entity_alive() : nullptr;
            if(!living || !living->g_Alive() || object->ID()==0 || object->ID()==u16(-1)) continue;
            const bool stalker=object->cast_stalker()!=nullptr, monster=object->cast_base_monster()!=nullptr;
            if(!stalker && !monster) continue;
            ++total;
            bool enemy=monster;
            if(!enemy && stalker)
            {
                auto* server=smart_cast<CSE_ALifeTraderAbstract*>(ai().alife().objects().object(object->ID(),true));
                LPCSTR community=server ? server->CommunityName() : object->lua_game_object()->CharacterCommunity();
                if(community && *community)
                {
                    auto it=communities.find(community);
                    if(it==communities.end()) it=communities.emplace(community,hostile(community)).first;
                    enemy=it->second;
                }
            }
            if(!enemy) continue;
            bool active=false;
            if(fighting.is_valid() && fighting.type()==LUA_TTABLE)
            {
                luabind::object value=fighting[object->ID()];
                active=value.type()!=LUA_TNIL && (value.type()!=LUA_TBOOLEAN || luabind::object_cast<bool>(value));
            }
            if(active) ++combat;
            else if(object->Position().distance_to_sqr(g_actor->Position())<100.f) ++alert;
        }
    }
    result["total"]=total; result["combat"]=combat; result["alert"]=alert;
    return result;
}

bool marker_position(u16 id, LPCSTR bone, const Fvector& adjustment, Fvector& position)
{
    if (!ready()) return false;
    CObject* object = Level().Objects.net_Find(id);
    if (!object) return false;
    object->Center(position);
    IKinematics* skeleton = object->Visual() ? object->Visual()->dcast_PKinematics() : nullptr;
    if (skeleton && bone && *bone)
    {
        const u16 bone_id = skeleton->LL_BoneID(bone);
        if (bone_id != BI_NONE && bone_id < skeleton->LL_BoneCount())
        {
            Fmatrix transform;
            transform.mul_43(object->XFORM(), skeleton->LL_GetTransform(bone_id));
            position = transform.c;
        }
    }
    position.add(adjustment);
    return _valid(position);
}

bool addon_line_of_sight(u16 id, float range, LPCSTR bone, int flags, float tolerance, const Fvector& offset)
{
    if (!ready() || !_valid(range) || range <= 0.f || !_valid(tolerance) || !_valid(offset)) return false;
    CGameObject* object = smart_cast<CGameObject*>(Level().Objects.net_Find(id));
    if (!object) return false;
    if ((!bone || !*bone) && object->cast_stalker())
        bone = object->cast_entity_alive() && object->cast_entity_alive()->g_Alive() ? "bip01_head" : "bip01_spine1";
    Fvector position, direction;
    if (!marker_position(id, bone, offset, position)) return false;
    direction.sub(Device.vCameraPosition, position);
    if (direction.square_magnitude() < EPS_S) return true;
    direction.normalize_safe();
    collide::rq_result hit;
    const bool blocked = Level().ObjectSpace.RayPick(position, direction, range,
        collide::rq_target(flags & 3), hit, object);
    // Keep the addon's actor-distance and tolerance convention during migration.
    const float difference = position.distance_to(g_actor->Position()) - floorf(blocked ? hit.range : range);
    return difference < tolerance;
}

bool project_marker(const Fvector& position, bool offscreen, Fvector2& screen)
{
    const Fmatrix& m = Device.mFullTransform;
    const float w = position.x * m._14 + position.y * m._24 + position.z * m._34 + m._44;
    if (!_valid(w) || _abs(w) < EPS_S) return false;
    const float x = (position.x * m._11 + position.y * m._21 + position.z * m._31 + m._41) / w;
    const float y = (position.x * m._12 + position.y * m._22 + position.z * m._32 + m._42) / w;
    const float z = (position.x * m._13 + position.y * m._23 + position.z * m._33 + m._43) / w;
    if (!offscreen && (w < 0.f || z < 0.f || _abs(x) > 1.f || _abs(y) > 1.f)) return false;
    screen.set((1.f + x) * 512.f, (1.f - y) * 384.f);
    return _valid(screen.x) && _valid(screen.y);
}

bool object_position(u16 id, Fvector& position)
{
    if (!ready()) return false;
    if (CObject* object = Level().Objects.net_Find(id))
    {
        CGameObject* game_object=smart_cast<CGameObject*>(object);
        if(game_object && (game_object->cast_stalker() || game_object->cast_base_monster()))
        {
            LPCSTR bone="bip01_head";
            if(game_object->cast_base_monster() && pSettings->line_exist(game_object->cNameSect(),"bone_head"))
                bone=pSettings->r_string(game_object->cNameSect(),"bone_head");
            IKinematics* skeleton=object->Visual() ? object->Visual()->dcast_PKinematics() : nullptr;
            if(skeleton)
            {
                if(skeleton->LL_BoneID(bone)==BI_NONE) bone="bip01_head";
                if(skeleton->LL_BoneID(bone)!=BI_NONE)
                    return marker_position(id,bone,Fvector().set(0,0,0),position);
            }
        }
        position = object->Position();
        position.y += 1.f;
        return _valid(position);
    }
    const CSE_ALifeDynamicObject* object = ai().alife().objects().object(id, true);
    if (!object) return false;
    const auto target = object->m_tGraphID;
    const auto actor = g_actor->ai_location().game_vertex_id();
    const auto& graph = ai().game_graph();
    if (!graph.valid_vertex_id(target) || !graph.valid_vertex_id(actor) ||
        graph.vertex(target)->level_id() != graph.vertex(actor)->level_id()) return false;
    position = object->o_Position;
    position.y += 1.f;
    return _valid(position);
}

// Owns its XML controls. No saved raw game-object or map-location pointers.
class CUIAddonCompass final : public CUIDialogWndEx
{
    struct Category
    {
        bool enabled = false, distance_text = false, always = false;
        float distance = 0.f;
    };
    struct Marker
    {
        CUIStatic* icon = nullptr;
        CUIStatic* arrow = nullptr;
        xr_string category;
        float x = 512.f;
        int last_distance = -2, arrow_direction = 0;
        bool seen = false;
    };
    using Key = std::pair<u16, xr_string>;
    CScriptXmlInit m_xml;
    CUIStatic* m_container = nullptr;
    CUIStatic* m_cardinal = nullptr;
    xr_map<xr_string, Category> m_categories;
    xr_map<xr_string, xr_string> m_spots;
    xr_map<Key, Marker> m_markers;
    xr_set<u16> m_looted;
    bool m_ribbon = false, m_enabled = true, m_visible = true;
    float m_offset_x = 0.f, m_offset_y = 0.f, m_width = 360.f, m_fade = 10.f;
    bool m_top = false;
    u32 m_next_collect = 0;
    u64 m_revision = u64(-1);

    void collect()
    {
        const u32 now = Device.dwTimeGlobal;
        const u64 revision = Level().MapManager().AddonRevision();
        if (revision == m_revision && now < m_next_collect) return;
        m_next_collect = now + 1000;
        m_revision = revision;
        for (auto& entry : m_markers) entry.second.seen = false;
        unsigned created = 0;
        bool pending = false;
        for (const auto& spot : Level().MapManager().Locations())
        {
            if (!spot.actual || !spot.location || !spot.spot_type.size()) continue;
            auto type = m_spots.find(spot.spot_type.c_str());
            if (type == m_spots.end()) continue;
            auto category = m_categories.find(type->second);
            if (category == m_categories.end() || !category->second.enabled) continue;
            if (type->second == "deadbody" && m_looted.count(spot.object_id)) continue;
            Fvector position;
            if(!object_position(spot.object_id,position)) continue;
            Key key(spot.object_id, type->first);
            auto it = m_markers.find(key);
            if (it != m_markers.end() && it->second.category != type->second)
            {
                m_container->DetachChild(it->second.icon);
                m_markers.erase(it);
                it = m_markers.end();
            }
            if (it == m_markers.end())
            {
                // XML creation has a per-frame budget; render existing markers meanwhile.
                if (created >= 16 || m_markers.size() >= 2048) { pending = true; continue; }
                xr_string path = "markers:marker_" + type->second;
                if (!m_xml.NodeExist(path.c_str(), 0)) continue;
                Marker marker;
                marker.category = type->second;
                marker.icon = m_xml.InitStatic(path.c_str(), m_container);
                const xr_string arrow = path + ":arrow";
                if (m_xml.NodeExist(arrow.c_str(), 0))
                    marker.arrow = m_xml.InitStatic(arrow.c_str(), marker.icon);
                it = m_markers.emplace(key, marker).first;
                ++created;
            }
            it->second.seen = true;
        }
        for (auto it = m_markers.begin(); it != m_markers.end();)
            if (!it->second.seen)
            {
                m_container->DetachChild(it->second.icon);
                it = m_markers.erase(it);
            }
            else ++it;
        if (pending && m_markers.size() < 2048) m_revision = u64(-1);
    }

public:
    void Init(bool ribbon)
    {
        if (m_container) return;
        m_ribbon = ribbon;
        SetAutoDelete(true);
        m_xml.ParseFile(ribbon ? "ui_compass.xml" : "ui_markers.xml");
        m_container = m_xml.InitStatic(ribbon ? "compass_container" : "markers", this);
        if (ribbon)
        {
            m_cardinal = m_xml.InitStatic("compass_container:cardinal_dir", m_container);
            m_xml.InitStatic("compass_container:static_frame", m_container);
        }
    }
    void Configure(luabind::object config)
    {
        m_offset_x = field<float>(config, "offset_x", 0.f);
        m_offset_y = field<float>(config, "offset_y", 0.f);
        m_width = field<float>(config, "width", 360.f);
        m_fade = field<float>(config, "fade", 10.f);
        m_top = field<bool>(config, "top", false);
        m_categories.clear(); m_spots.clear();
        luabind::object categories = config["categories"];
        if (categories.is_valid() && categories.type() == LUA_TTABLE)
            for (auto i = categories.begin(); i != categories.end(); ++i)
            {
                auto key = luabind::object_cast_nothrow<LPCSTR>(i.key());
                if (!key) continue;
                const luabind::object value = *i;
                Category category;
                category.enabled = field<bool>(value, "is_enabled", false);
                category.distance_text = field<bool>(value, "is_distance", false);
                category.always = field<bool>(value, "is_always_visible", false);
                category.distance = field<float>(value, "distance", 0.f);
                m_categories[*key] = category;
            }
        luabind::object spots = config["spots"];
        if (spots.is_valid() && spots.type() == LUA_TTABLE)
            for (auto i = spots.begin(); i != spots.end(); ++i)
            {
                auto key = luabind::object_cast_nothrow<LPCSTR>(i.key());
                auto value = luabind::object_cast_nothrow<LPCSTR>(*i);
                if (key && value && m_categories.count(*value)) m_spots[*key] = *value;
            }
        m_revision = u64(-1);
    }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    void SetHudVisible(bool visible) { m_visible = visible; }
    void Looted(u16 id, bool value)
    {
        if (value) m_looted.insert(id); else m_looted.erase(id);
        m_revision = u64(-1);
    }
    void Invalidate() { m_revision = u64(-1); }
    void Update() override
    {
        if (!m_container) return;
        const bool visible = ready() && m_enabled && m_visible;
        m_container->Show(visible);
        if (!visible) { CUIDialogWndEx::Update(); return; }
        if (m_ribbon)
        {
            float angle = atan2f(Device.vCameraDirection.x, Device.vCameraDirection.z) * 180.f / PI;
            if (angle < 0.f) angle += 360.f;
            m_container->SetWndPos(Fvector2().set(512.f - m_width / 2.f + m_offset_x,
                (m_top ? 721.f : 19.f) + m_offset_y));
            m_cardinal->SetTextureRect(Frect().set(angle * 10.f, 0.f, angle * 10.f + 2400.f, 100.f));
        }
        else
        {
            collect();
            for (auto& entry : m_markers)
            {
                auto& marker = entry.second;
                auto category = m_categories.find(marker.category);
                Fvector position;
                if (category == m_categories.end() || !object_position(entry.first.first, position))
                { marker.icon->Show(false); continue; }
                const auto& c = category->second;
                const float distance = floorf(position.distance_to(Device.vCameraPosition));
                marker.x = addon_math::compass_x(position.x - Device.vCameraPosition.x,
                    position.z - Device.vCameraPosition.z, Device.vCameraDirection.x,
                    Device.vCameraDirection.z, marker.x, Device.fTimeDelta);
                const unsigned alpha = addon_math::fade(distance, c.distance, m_fade, c.always);
                const float width = m_container->GetWidth() * 0.75f;
                const float edge = (1024.f - width) / 2.f + marker.icon->GetWidth() * 0.375f;
                const bool show = alpha && marker.x > edge && marker.x < 1024.f - edge;
                marker.icon->Show(show);
                if (!show) continue;
                const bool corpse = marker.category == "deadbody";
                const float y = corpse ? (m_top ? 749.f : 47.f) : (m_top ? 727.f : 25.f);
                marker.icon->SetWndPos(Fvector2().set(marker.x + m_offset_x, y + m_offset_y - 5.f));
                marker.icon->SetTextureColor(color_argb(alpha, 255, 255, 255));
                const int text_distance = ((c.distance_text || c.always) && _abs(marker.x - 512.f) <= 10.f)
                    ? int(distance) : -1;
                if (text_distance != marker.last_distance)
                {
                    string32 text = "";
                    if (text_distance >= 0) xr_sprintf(text, "%d", text_distance);
                    marker.icon->TextItemControl()->SetText(text);
                    marker.last_distance = text_distance;
                }
                if (marker.arrow)
                {
                    const float diff = floorf(Device.vCameraPosition.y) - floorf(position.y);
                    const int direction = diff < -2.f ? 1 : (diff > 2.f ? -1 : 0);
                    marker.arrow->Show(direction != 0);
                    if (direction && direction != marker.arrow_direction)
                    {
                        marker.arrow->InitTexture(direction > 0 ? "tc_arrow_up" : "tc_arrow_down");
                        marker.arrow->SetWndPos(Fvector2().set(marker.icon->GetWidth() / 2.f, -2.f));
                    }
                    marker.arrow_direction = direction;
                    marker.arrow->SetTextureColor(color_argb(alpha, 255, 255, 255));
                }
            }
        }
        CUIDialogWndEx::Update();
    }
};
}

struct CAddonMarkerLayout::State
{
    CUIStatic *marker = nullptr, *label = nullptr, *label_shadow = nullptr,
        *distance = nullptr, *distance_shadow = nullptr, *label_ref = nullptr, *distance_ref = nullptr;
    u16 id = u16(-1);
    unsigned style = 0;
    bool is_place = false, waypoint_zone = false;
    xr_string bone, name;
    Fvector offset = {0.f, 0.f, 0.f}, fixed_world = {0.f, 0.f, 0.f};
    Fvector2 fixed_screen = {0.f, 0.f};
    bool world_locked = false, screen_locked = false, offscreen = false, visible = false;
    bool hint = false, dist = false, scale = true, aspect = true, clamp_position = false, hide_oob = false;
    bool x_locked = false, y_locked = false, unfade = false;
    float lock_x = 0.f, lock_y = 0.f, adjust_x = 0.f, adjust_y = 0.f;
    float min_x = 0.f, min_y = 0.f, max_x = 1024.f, max_y = 768.f;
    float width = 24.f, height = 24.f, multiplier = 1.f, near_scale = 10.f, mid_scale = 20.f, far_scale = 50.f;
    float scale_up = .5f, scale_down = .4f, hint_y = 0.f, distance_y = 0.f, fixed_text_height = 0.f;
    float near_hide = -1.f, near_fade = -1.f, far_fade = 99999.f, far_hide = 99999.f;
    float text_near_hide = -1.f, text_near_fade = -1.f, text_far_fade = 99999.f, text_far_hide = 99999.f;
    u32 color = 0xffffffff;
    int distance_key = -1;

    unsigned alpha(float distance, float floor, float fade_near, float mid, float fade_far, unsigned base) const
    {
        if (unfade) return base;
        float value = float(base);
        if (distance < floor) value = 0.f;
        else if (distance < fade_near && fade_near > floor) value *= distance / (fade_near - floor);
        else if (mid > 0.f && distance > mid)
            value *= fade_far > 0.f ? 1.f - (distance - mid) / fade_far : 0.f;
        return unsigned(addon_math::clamp(value, 0.f, 255.f));
    }
    void show(bool show)
    {
        if (marker) marker->Show(show);
        if (label) label->Show(show && hint);
        if (label_shadow) label_shadow->Show(show && hint);
        if (distance) distance->Show(show && dist);
        if (distance_shadow) distance_shadow->Show(show && dist);
    }
};

CAddonMarkerLayout::CAddonMarkerLayout() : m_state(xr_new<State>()) {}
CAddonMarkerLayout::~CAddonMarkerLayout() { xr_delete(m_state); }
void CAddonMarkerLayout::SetVisible(bool value)
{
    m_state->visible = value;
    if (!value) m_state->show(false);
}

bool CAddonMarkerLayout::Configure(CUIWindow* owner, const luabind::object& data)
{
    auto& s = *m_state;
    if (!ready() || field<bool>(data, "actor_taking", false)) return false;
    s.style = field<unsigned>(data, "_native_paw_kind", 0);
    s.is_place = field<bool>(data, "_native_is_place", false);
    s.waypoint_zone = field<bool>(data, "_native_waypoint_zone", false);
    auto control = [&](const char* key)
    {
        CUIStatic* value = field<CUIStatic*>(data, key, nullptr);
        CUIWindow* ancestor = value;
        while (ancestor && ancestor != owner) ancestor = ancestor->GetParent();
        return ancestor == owner ? value : nullptr;
    };
    s.marker = control("marker"); s.label = control("label"); s.label_shadow = control("label_sh");
    s.distance = control("dist"); s.distance_shadow = control("dist_sh");
    s.label_ref = control("lref"); s.distance_ref = control("wref");
    if (!s.marker || !s.distance || !s.distance_shadow || !s.distance_ref) return false;
    if (s.style != 2 && (!s.label || !s.label_shadow || !s.label_ref)) return false;
    s.id = field<u16>(data, "id", u16(-1));
    const luabind::object args = data["args"];
    s.bone = field<LPCSTR>(args, "bone", "");
    s.offset = field<Fvector>(data, "pos_adjust", Fvector().set(0,0,0));
    auto world = optional_field<Fvector>(data, "locked_world_pos");
    s.world_locked = world.has_value(); if (world) s.fixed_world = *world;
    auto screen = optional_field<Fvector2>(data, "locked_screen_pos");
    s.screen_locked = screen.has_value(); if (screen) s.fixed_screen = *screen;
    s.offscreen = field<bool>(data, "show_offscreen", false);
    s.width = field<float>(data, "base_w", 24.f); s.height = field<float>(data, "base_h", s.width);
    s.multiplier = field<float>(data, "marker_scale", 1.f);
    s.scale = !field<bool>(data, "skip_scale_by_dist", false);
    s.fixed_text_height = field<float>(data, "base_y", s.marker->GetHeight());
    s.aspect = field<bool>(data, "correct_aspect", true);
    s.near_scale = field<float>(data, "scale_dist_min", 10.f);
    s.mid_scale = field<float>(data, "scale_dist_mid", 20.f);
    s.far_scale = field<float>(data, "scale_dist_max", 50.f);
    s.scale_up = field<float>(data, "scale_up_amt", .5f);
    s.scale_down = field<float>(data, "scale_dn_amt", .4f);
    s.hint = field<bool>(data, "_native_show_hint", false);
    s.dist = field<bool>(data, "_native_show_dist", false);
    s.unfade = field<bool>(data, "_native_unfade", false);
    s.hint_y = field<float>(data, "hint_y_adj", 0.f);
    s.distance_y = field<float>(data, "dist_y_adj", 0.f);
    s.near_hide = field<float>(data, "near_hide_dist", -1.f);
    s.near_fade = field<float>(data, "near_fade_dist", -1.f);
    s.far_fade = field<float>(data, "far_fade_dist", 99999.f);
    s.far_hide = field<float>(data, "far_hide_dist", 99999.f);
    s.text_near_hide = field<float>(data, "h_near_hide_dist", s.near_hide);
    s.text_near_fade = field<float>(data, "h_near_fade_dist", s.near_fade);
    s.text_far_fade = field<float>(data, "h_far_fade_dist", s.far_fade);
    s.text_far_hide = field<float>(data, "h_far_hide_dist", s.far_hide);
    const luabind::object color = data["custom_argb"];
    if (color.is_valid() && color.type() == LUA_TTABLE)
        s.color = color_argb(field<u32>(color,"a",255), field<u32>(color,"r",255),
            field<u32>(color,"g",255), field<u32>(color,"b",255));
    else s.color = 0xffffffff;
    s.clamp_position = field<bool>(data, "final_pos_adjustments", false);
    s.hide_oob = field<bool>(data, "hide_when_oob", false);
    auto lock_x = optional_field<float>(data, "lock_x");
    auto lock_y = optional_field<float>(data, "lock_y");
    s.x_locked = lock_x.has_value(); s.lock_x = lock_x.value_or(0.f);
    s.y_locked = lock_y.has_value(); s.lock_y = lock_y.value_or(0.f);
    s.adjust_x = field<float>(data, "adj_final_x", 0.f); s.adjust_y = field<float>(data, "adj_final_y", 0.f);
    s.min_x = field<float>(data,"min_x",0.f); s.max_x = field<float>(data,"max_x",1024.f);
    s.min_y = field<float>(data,"min_y",0.f); s.max_y = field<float>(data,"max_y",768.f);
    const LPCSTR name = field<LPCSTR>(data, "name", "");
    if (s.name != name && s.label && s.label_shadow && s.label_ref)
    {
        s.name = name;
        s.label->TextItemControl()->SetText(name);
        s.label_shadow->TextItemControl()->SetText(name);
        s.label_ref->TextItemControl()->SetText(name);
        s.label_ref->AdjustWidthToText();
    }
    s.visible = true;
    return true;
}

void CAddonMarkerLayout::Update()
{
    auto& s = *m_state;
    if (!s.visible || !ready()) { s.show(false); return; }
    Fvector world = s.fixed_world;
    Fvector2 screen = s.fixed_screen;
    float source_distance = 1.f;
    if (!s.screen_locked)
    {
        if (s.style)
        {
            auto* server = ai().alife().objects().object(s.id, true);
            if (!server || !server->m_bOnline) { s.show(false); return; }
            const auto& graph=ai().game_graph();
            const auto actor_vertex=g_actor->ai_location().game_vertex_id();
            if(!graph.valid_vertex_id(server->m_tGraphID) || !graph.valid_vertex_id(actor_vertex) ||
                graph.vertex(server->m_tGraphID)->level_id()!=graph.vertex(actor_vertex)->level_id())
            { s.show(false); return; }
            CGameObject* object = smart_cast<CGameObject*>(Level().Objects.net_Find(s.id));
            if(!object)
                if(auto* squad=smart_cast<CSE_ALifeOnlineOfflineGroup*>(server))
                {
                    object=smart_cast<CGameObject*>(Level().Objects.net_Find(squad->commander_id()));
                    if(!object) { s.show(false); return; }
                }
            const bool npc = object && (object->cast_stalker() || object->cast_base_monster());
            world = object ? object->Position() : server->o_Position;
            if (npc) marker_position(object->ID(),"bip01_head",Fvector().set(0,0,0),world);
            source_distance = server->o_Position.distance_to(g_actor->Position());
            float base_y = .575f, minimum = 0.f, close_adjust = 0.f, far_adjust = 1.75f;
            if (s.waypoint_zone) { base_y = 0.f; far_adjust = 0.f; }
            else if (s.is_place) close_adjust = 1.2f;
            else if (npc) { minimum = 2.f; close_adjust = .45f; }
            const float d = addon_math::clamp(world.distance_to(g_actor->Position()),minimum,60.f);
            float vertical = close_adjust;
            if (d <= 15.f) vertical -= close_adjust * _abs((15.f - d) * .1f);
            else vertical += ((d - 15.f) / 45.f) * far_adjust;
            world.y += base_y + vertical * (Device.fFOV / 85.f);
        }
        else if (!s.world_locked && !marker_position(s.id,s.bone.c_str(),s.offset,world))
        { s.show(false); return; }
        if (!project_marker(world,s.offscreen,screen)) { s.show(false); return; }
    }
    if (s.hide_oob && (screen.x < s.min_x || screen.x > s.max_x || screen.y < s.min_y || screen.y > s.max_y))
    { s.show(false); return; }
    if (s.clamp_position)
        screen.set(addon_math::clamp((s.x_locked ? s.lock_x : screen.x) + s.adjust_x, s.min_x, s.max_x),
            addon_math::clamp((s.y_locked ? s.lock_y : screen.y) + s.adjust_y, s.min_y, s.max_y));
    s.marker->SetWndPos(screen);
    const float distance = s.screen_locked ? 1.f : (s.style ? source_distance : world.distance_to(g_actor->Position()));
    float factor = 1.f;
    const float d = addon_math::clamp(distance, 0.f, s.far_scale);
    if (s.scale && d <= s.near_scale && s.near_scale > 0.f) factor += (1.f - d / s.near_scale) * s.scale_up;
    else if (s.scale && d > s.mid_scale && s.far_scale > s.mid_scale)
        factor -= ((d - s.mid_scale) / (s.far_scale - s.mid_scale)) * s.scale_down;
    const float aspect = s.aspect && Device.dwWidth ? float(Device.dwHeight) / Device.dwWidth * (1024.f / 768.f) : 1.f;
    const float width = s.width * factor * s.multiplier * aspect;
    const float height = s.height * factor * s.multiplier;
    // Custom-size controls retain their dimensions. Legacy text placement uses
    // the distance-scaled height before the optional icon-size multiplier.
    if (s.scale) s.marker->SetWndSize(Fvector2().set(width, height));
    const float text_height = s.scale ? s.height * factor : s.fixed_text_height;
    const unsigned alpha = s.alpha(distance, s.near_hide, s.near_fade, s.far_fade, s.far_hide, color_get_A(s.color));
    s.marker->SetTextureColor((s.color & 0xffffff) | (alpha << 24));
    const unsigned text_alpha = s.alpha(distance, s.text_near_hide, s.text_near_fade, s.text_far_fade, s.text_far_hide, 255);
    if (s.label) s.label->TextItemControl()->SetTextColor(color_argb(text_alpha,230,230,230));
    s.distance->TextItemControl()->SetTextColor(color_argb(text_alpha,230,230,230));
    if (s.label_shadow) s.label_shadow->TextItemControl()->SetTextColor(color_argb(text_alpha,0,0,0));
    s.distance_shadow->TextItemControl()->SetTextColor(color_argb(text_alpha,0,0,0));
    const float y = std::min(distance,100.f) * .06f;
    if (s.hint && s.label && s.label_ref) s.label->SetWndPos(Fvector2().set(screen.x - s.label_ref->GetWidth() * .5f, screen.y - 22.f + y + s.hint_y));
    if (s.dist)
    {
        const int key = int(floorf(distance * 10.f + .5f));
        if (key != s.distance_key)
        {
            string32 text; xr_sprintf(text, "%.1fm", key * .1f);
            s.distance->TextItemControl()->SetText(text);
            s.distance_shadow->TextItemControl()->SetText(text);
            s.distance_ref->TextItemControl()->SetText(text);
            s.distance_ref->AdjustWidthToText();
            s.distance_key = key;
        }
        s.distance->SetWndPos(Fvector2().set(screen.x - s.distance_ref->GetWidth() * .5f - (s.style==2 ? 1.f : 0.f),
            screen.y + text_height + (s.style==2 ? -8.f : -11.f+y) + s.distance_y));
    }
    s.show(true);
}

bool addon_marker_sample(const luabind::object& data)
{
    if (!ready()) return false;
    Fvector* output = field<Fvector*>(data, "_anthology_world_pos", nullptr);
    Fvector2* screen = field<Fvector2*>(data, "_anthology_final_pos", nullptr);
    if (!output || !screen) return false;
    const luabind::object args = data["args"];
    const auto fixed = optional_field<Fvector>(data, "locked_world_pos");
    bool found = fixed.has_value();
    if (found) *output = *fixed;
    else found = marker_position(field<u16>(data,"id",u16(-1)), field<LPCSTR>(args,"bone",""),
        field<Fvector>(data,"pos_adjust",Fvector().set(0,0,0)), *output);
    const bool onscreen = found && project_marker(*output, field<bool>(data,"show_offscreen",false), *screen);
    luabind::object self = data;
    if (found) self["last_pos"] = data["_anthology_world_pos"];
    else self["last_pos"] = static_cast<Fvector*>(nullptr);
    if (onscreen)
    {
        self["world2ui_vec"] = data["_anthology_final_pos"];
        self["last_screen_pos"] = data["_anthology_final_pos"];
    }
    else
    {
        self["world2ui_vec"] = static_cast<Fvector2*>(nullptr);
        self["last_screen_pos"] = static_cast<Fvector2*>(nullptr);
    }
    self["is_offscreen"] = !onscreen;
    self["killswitched"] = !onscreen;
    return onscreen;
}

void register_addon_runtime(lua_State* L)
{
    using namespace luabind;
    module(L)
    [
        class_<CUIAddonCompass, CUIDialogWndEx>("CUIAddonCompass")
            .def(constructor<>())
            .def("Init", &CUIAddonCompass::Init)
            .def("Configure", &CUIAddonCompass::Configure)
            .def("SetEnabled", &CUIAddonCompass::SetEnabled)
            .def("SetHudVisible", &CUIAddonCompass::SetHudVisible)
            .def("Looted", &CUIAddonCompass::Looted)
            .def("Invalidate", &CUIAddonCompass::Invalidate)
    ];
    module(L, "level")
    [
        def("addon_object_ids", &addon_object_ids),
        def("addon_anomaly_groups", &addon_anomaly_groups),
        def("addon_unseen_nearby", &addon_unseen_nearby),
        def("addon_enemy_counts", &addon_enemy_counts),
        def("addon_sudoku_reset", &addon_sudoku_reset),
        def("addon_puzzle_shuffle", &addon_puzzle_shuffle),
        def("addon_line_of_sight", &addon_line_of_sight)
    ];
}
