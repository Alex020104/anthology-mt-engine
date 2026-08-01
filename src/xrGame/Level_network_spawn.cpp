#include "pch_script.h"
#include "xrServer_Objects_ALife_All.h"
#include "level.h"
#include "game_cl_base.h"
#include "net_queue.h"
#include "ai_space.h"
#include "game_level_cross_table.h"
#include "level_graph.h"
#include "client_spawn_manager.h"
#include "../xrEngine/x_ray.h"
#include "../xrEngine/xr_object.h"
#include "../xrEngine/IGame_Persistent.h"

void CLevel::cl_Process_Spawn(NET_Packet& P)
{
	const bool measure_spawn = pApp && pApp->LoadSessionActive();
	const u64 total_started_at = measure_spawn ? CPU::QPC() : 0;
	#ifdef SPAWN_ANTIFREEZE
	PublishPreparedClientSpawnResource(P);
	#endif
	// Begin analysis
	shared_str s_name;
	P.r_stringZ(s_name);
	PROF_EVENT("CLevel::cl_Process_Spawn");

	//Msg("cl_Process_Spawn spawning %s", s_name.c_str());

	// Create DC (xrSE)
	CSE_Abstract* E = F_entity_Create(*s_name);
	R_ASSERT2(E, *s_name);


	E->Spawn_Read(P);
	if (E->s_flags.is(M_SPAWN_UPDATE))
		E->UPDATE_Read(P);

	if (!E->match_configuration())
	{
		F_entity_Destroy(E);
		return;
	}
	//-------------------------------------------------
	//.	Msg ("M_SPAWN - %s[%d][%x] - %d %d", *s_name,  E->ID, E,E->ID_Parent, Device.dwFrame);
	//-------------------------------------------------
	//force object to be local for server client
	if (OnServer())
	{
		E->s_flags.set(M_SPAWN_OBJECT_LOCAL, TRUE);
	};

	/*
	game_spawn_queue.push_back(E);
	if (g_bDebugEvents)		ProcessGameSpawns();
	/*/
	client_spawn_profile_sample profile;
	if (measure_spawn)
		profile.entity_decode_ticks = CPU::QPC() - total_started_at;
	g_sv_Spawn(E, measure_spawn ? &profile : nullptr);

	F_entity_Destroy(E);
	if (measure_spawn)
	{
		profile.total_ticks = CPU::QPC() - total_started_at;
		RecordClientSpawnProfile(s_name, profile);
	}
	//*/
};

void CLevel::g_cl_Spawn(LPCSTR name, u8 rp, u16 flags, Fvector pos)
{
	// Create
	CSE_Abstract* E = F_entity_Create(name);
	VERIFY(E);

	// Fill
	E->s_name = name;
	E->set_name_replace("");
	//.	E->s_gameid			=	u8(GameID());
	E->s_RP = rp;
	E->ID = 0xffff;
	E->ID_Parent = 0xffff;
	E->ID_Phantom = 0xffff;
	E->s_flags.assign(flags);
	E->RespawnTime = 0;
	E->o_Position = pos;

	// Send
	NET_Packet P;
	E->Spawn_Write(P,TRUE);
	Send(P, net_flags(TRUE));

	// Destroy
	F_entity_Destroy(E);
}

#ifdef DEBUG
	extern Flags32				psAI_Flags;
	extern float				debug_on_frame_gather_stats_frequency;
#	include "ai_debug.h"
#endif // DEBUG

void CLevel::g_sv_Spawn(CSE_Abstract* E, client_spawn_profile_sample* profile)
{
#ifdef DEBUG_MEMORY_MANAGER
	size_t							E_mem = 0;
	if (g_bMEMO)	{
		lua_gc					(ai().script_engine().lua(),LUA_GCCOLLECT,0);
		lua_gc					(ai().script_engine().lua(),LUA_GCCOLLECT,0);
		E_mem					= Memory.mem_usage();	
		Memory.stat_calls		= 0;
	}
#endif // DEBUG_MEMORY_MANAGER
	//-----------------------------------------------------------------
	//	CTimer		T(false);

#ifdef DEBUG
	//	Msg					("* CLIENT: Spawn: %s, ID=%d", *E->s_name, E->ID);
#endif

	// Optimization for single-player only	- minimize traffic between client and server
	if (GameID() == eGameIDSingle) psNET_Flags.set(NETFLAG_MINIMIZEUPDATES,TRUE);
	else psNET_Flags.set(NETFLAG_MINIMIZEUPDATES,FALSE);

	// Client spawn
	//	T.Start		();
	const u64 object_create_started_at = profile ? CPU::QPC() : 0;
	// The server entity already resolved the section class. Reuse it instead of
	// repeating the same system.ltx lookup for every client object.
	CObject* O = Objects.Create(*E->s_name, E->m_tClassID);
	if (profile)
		profile->object_create_ticks = CPU::QPC() - object_create_started_at;
	// Msg				("--spawn--CREATE: %f ms",1000.f*T.GetAsync());

	//	T.Start		();
#ifdef DEBUG_MEMORY_MANAGER
	mem_alloc_gather_stats		(false);
#endif // DEBUG_MEMORY_MANAGER

    if (0 == O)
    {
        Msg("! Failed to spawn entity '%s', O is nullptr", *E->s_name);
        return;
    }

	const u64 net_spawn_started_at = profile ? CPU::QPC() : 0;
	const bool net_spawned = O->net_Spawn(E);
	if (profile)
		profile->net_spawn_ticks = CPU::QPC() - net_spawn_started_at;
	if (!net_spawned)
	{
        Msg("! Failed to spawn entity '%s', net_Spawn failed", *E->s_name);
		O->net_Destroy();
		if (!g_dedicated_server)
			client_spawn_manager().clear(O->ID());
		Objects.Destroy(O);
		Msg("! Failed to spawn entity '%s'", *E->s_name);

#ifdef DEBUG_MEMORY_MANAGER
		mem_alloc_gather_stats	(!!psAI_Flags.test(aiDebugOnFrameAllocs));
#endif // DEBUG_MEMORY_MANAGER

	}
	else
	{
		const u64 callbacks_started_at = profile ? CPU::QPC() : 0;

#ifdef DEBUG_MEMORY_MANAGER
		mem_alloc_gather_stats	(!!psAI_Flags.test(aiDebugOnFrameAllocs));
#endif // DEBUG_MEMORY_MANAGER

		if (!g_dedicated_server)
			client_spawn_manager().callback(O);
		//Msg			("--spawn--SPAWN: %f ms",1000.f*T.GetAsync());

		if ((E->s_flags.is(M_SPAWN_OBJECT_LOCAL)) &&
			(E->s_flags.is(M_SPAWN_OBJECT_ASPLAYER)))
		{
			if (IsDemoPlayStarted())
			{
				if (E->s_flags.is(M_SPAWN_OBJECT_PHANTOM))
				{
					SetControlEntity(O);
					SetEntity(O); //do not switch !!!
					SetDemoSpectator(O);
				}
			}
			else
			{
				if (CurrentEntity() != NULL)
				{
					CGameObject* pGO = smart_cast<CGameObject*>(CurrentEntity());
					if (pGO) pGO->On_B_NotCurrentEntity();
				}
				SetControlEntity(O);
				SetEntity(O); //do not switch !!!
			}
		}

		if (0xffff != E->ID_Parent)
		{
			/*
			// Generate ownership-event
			NET_Packet			GEN;
			GEN.w_begin			(M_EVENT);
			GEN.w_u32			(E->m_dwSpawnTime);//-NET_Latency);
			GEN.w_u16			(GE_OWNERSHIP_TAKE);
			GEN.w_u16			(E->ID_Parent);
			GEN.w_u16			(u16(O->ID()));
			game_events->insert	(GEN);
			/*/
			NET_Packet GEN;
			GEN.write_start();
			GEN.read_start();
			GEN.w_u16(u16(O->ID()));
			cl_Process_Event(E->ID_Parent, GE_OWNERSHIP_TAKE, GEN);
			//*/
		}

#ifdef NET_SPAWN_AFTER_CALLBACKS
        if (smart_cast<CGameObject*>(O))
        {
            smart_cast<CGameObject*>(O)->callback(GameObject::eNetSpawnAfter)();
        }
#endif
		if (profile)
			profile->post_spawn_callback_ticks = CPU::QPC() - callbacks_started_at;
	}

	/*if (E->s_flags.is(M_SPAWN_UPDATE)) {
		NET_Packet				temp;
		temp.B.count			= 0;
		E->UPDATE_Write			(temp);
		if (temp.B.count > 0)
		{
			temp.r_seek				(0);
			O->net_Import			(temp);
		}
		}*/ //:(

	//---------------------------------------------------------
	const u64 game_spawn_started_at = profile ? CPU::QPC() : 0;
	Game().OnSpawn(O);
	if (profile)
		profile->game_on_spawn_ticks = CPU::QPC() - game_spawn_started_at;
	//---------------------------------------------------------

#ifdef DEBUG_MEMORY_MANAGER
	if (g_bMEMO) {
		lua_gc					(ai().script_engine().lua(),LUA_GCCOLLECT,0);
		lua_gc					(ai().script_engine().lua(),LUA_GCCOLLECT,0);
		Msg						("* %20s : %lld bytes, %d ops", *E->s_name,Memory.mem_usage()-E_mem, Memory.stat_calls );
	}
#endif // DEBUG_MEMORY_MANAGER

}

void CLevel::RecordClientSpawnProfile(const shared_str& section, const client_spawn_profile_sample& sample)
{
	auto& entry = m_client_spawn_profile[section];
	++entry.count;
	entry.total_ticks += sample.total_ticks;
	entry.entity_decode_ticks += sample.entity_decode_ticks;
	entry.object_create_ticks += sample.object_create_ticks;
	entry.net_spawn_ticks += sample.net_spawn_ticks;
	entry.post_spawn_callback_ticks += sample.post_spawn_callback_ticks;
	entry.game_on_spawn_ticks += sample.game_on_spawn_ticks;
}

void CLevel::DumpClientSpawnProfile()
{
	if (m_client_spawn_profile_dumped || m_client_spawn_profile.empty())
		return;
	m_client_spawn_profile_dumped = true;

	struct ranked_spawn
	{
		shared_str section;
		client_spawn_profile_entry entry;
	};
	xr_vector<ranked_spawn> ranked;
	ranked.reserve(m_client_spawn_profile.size());
	client_spawn_profile_entry aggregate;
	for (const auto& [section, entry] : m_client_spawn_profile)
	{
		ranked.push_back({section, entry});
		aggregate.count += entry.count;
		aggregate.total_ticks += entry.total_ticks;
		aggregate.entity_decode_ticks += entry.entity_decode_ticks;
		aggregate.object_create_ticks += entry.object_create_ticks;
		aggregate.net_spawn_ticks += entry.net_spawn_ticks;
		aggregate.post_spawn_callback_ticks += entry.post_spawn_callback_ticks;
		aggregate.game_on_spawn_ticks += entry.game_on_spawn_ticks;
	}
	std::sort(ranked.begin(), ranked.end(), [](const ranked_spawn& left, const ranked_spawn& right)
	{
		return left.entry.total_ticks > right.entry.total_ticks;
	});
	const auto to_ms = [](u64 ticks) { return double(ticks) * 1000.0 / double(CPU::qpc_freq); };
	const u64 accounted = aggregate.entity_decode_ticks + aggregate.object_create_ticks +
		aggregate.net_spawn_ticks + aggregate.post_spawn_callback_ticks + aggregate.game_on_spawn_ticks;
	const u64 other = aggregate.total_ticks > accounted ? aggregate.total_ticks - accounted : 0;
	Msg("* [client-spawn/profile] count=%u sections=%u total=%.2f ms decode=%.2f create/load=%.2f "
		"net_spawn=%.2f callbacks=%.2f game_on_spawn=%.2f other=%.2f",
		aggregate.count, static_cast<u32>(ranked.size()), to_ms(aggregate.total_ticks),
		to_ms(aggregate.entity_decode_ticks), to_ms(aggregate.object_create_ticks),
		to_ms(aggregate.net_spawn_ticks), to_ms(aggregate.post_spawn_callback_ticks),
		to_ms(aggregate.game_on_spawn_ticks), to_ms(other));
	const u32 top_count = std::min<u32>(15, static_cast<u32>(ranked.size()));
	for (u32 i = 0; i < top_count; ++i)
	{
		const ranked_spawn& item = ranked[i];
		Msg("* [client-spawn/profile] #%02u total=%.2f ms count=%u avg=%.3f ms section=%s",
			i + 1, to_ms(item.entry.total_ticks), item.entry.count,
			to_ms(item.entry.total_ticks) / double(item.entry.count), item.section.c_str());
	}
}

CSE_Abstract* CLevel::spawn_item(LPCSTR section, const Fvector& position, u32 level_vertex_id, u16 parent_id,
                                 bool return_item)
{
	CSE_Abstract* abstract = F_entity_Create(section);
	R_ASSERT3(abstract, "Cannot find item with section", section);
	CSE_ALifeDynamicObject* dynamic_object = smart_cast<CSE_ALifeDynamicObject*>(abstract);
	if (dynamic_object && ai().get_level_graph())
	{
		dynamic_object->m_tNodeID = level_vertex_id;
		if (ai().level_graph().valid_vertex_id(level_vertex_id) && ai().get_game_graph() && ai().get_cross_table())
			dynamic_object->m_tGraphID = ai().cross_table().vertex(level_vertex_id).game_vertex_id();
	}

	//оружие спавним с полным магазинои
	CSE_ALifeItemWeapon* weapon = smart_cast<CSE_ALifeItemWeapon*>(abstract);
	if (weapon)
		weapon->a_elapsed = weapon->get_ammo_magsize();

	// Fill
	abstract->s_name = section;
	abstract->set_name_replace(section);
	//.	abstract->s_gameid		= u8(GameID());
	abstract->o_Position = position;
	abstract->s_RP = 0xff;
	abstract->ID = 0xffff;
	abstract->ID_Parent = parent_id;
	abstract->ID_Phantom = 0xffff;
	abstract->s_flags.assign(M_SPAWN_OBJECT_LOCAL);
	abstract->RespawnTime = 0;

	if (!return_item)
	{
		NET_Packet P;
		abstract->Spawn_Write(P,TRUE);
		Send(P, net_flags(TRUE));
		F_entity_Destroy(abstract);
		return (0);
	}
	else
		return (abstract);
}

void CLevel::ProcessGameSpawns()
{
	while (!game_spawn_queue.empty())
	{
		CSE_Abstract* E = game_spawn_queue.front();

		g_sv_Spawn(E);

		F_entity_Destroy(E);

		game_spawn_queue.pop_front();
	}
}
