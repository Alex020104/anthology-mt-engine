////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_object_registry.cpp
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife object registry
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "alife_object_registry.h"
#include "ai_debug.h"
#include "alife_graph_registry.h"
#include "alife_simulator.h"
#include "xrServer_Objects_ALife_Monsters.h"

static bool can_serialize_alife_object(CSE_ALifeDynamicObject* object)
{
	if (_valid(object->Position()))
		return true;

	CSE_ALifeCreatureAbstract* creature = smart_cast<CSE_ALifeCreatureAbstract*>(object);
	const bool alive = creature && creature->g_Alive();
	const bool story_object = object->m_story_id != INVALID_STORY_ID || object->m_spawn_story_id != INVALID_SPAWN_STORY_ID;

	if (!alive && !story_object)
	{
		Msg("! [SAVE_SANITIZE_SKIP] Skipping dead non-story ALife object with invalid position: section[%s] name[%s] id[%u] pos[%.5f, %.5f, %.5f]",
		    object->name(),
		    object->name_replace(),
		    object->ID,
		    object->Position().x,
		    object->Position().y,
		    object->Position().z);

		return false;
	}

	const Fvector bad_position = object->Position();
	Fvector rescue_position;
	CSE_ALifeCreatureActor* actor = object->alife().graph().actor();
	if (actor && _valid(actor->Position()))
		rescue_position.set(actor->Position());
	else
		rescue_position.set(0.f, 0.f, 0.f);

	Msg("! [SAVE_SANITIZE_REPAIR] Repairing protected ALife object with invalid position: section[%s] name[%s] id[%u] alive[%s] story[%u] spawn_story[%u] pos[%.5f, %.5f, %.5f] -> rescue[%.5f, %.5f, %.5f]",
	    object->name(),
	    object->name_replace(),
	    object->ID,
	    alive ? "true" : "false",
	    object->m_story_id,
	    object->m_spawn_story_id,
	    bad_position.x,
	    bad_position.y,
	    bad_position.z,
	    rescue_position.x,
	    rescue_position.y,
	    rescue_position.z);

	object->o_Position.set(rescue_position);
	return true;
}

CALifeObjectRegistry::CALifeObjectRegistry(LPCSTR section)
{
}

CALifeObjectRegistry::~CALifeObjectRegistry()
{
	OBJECT_REGISTRY::iterator const B = m_objects.begin();
	OBJECT_REGISTRY::iterator I = B;
	OBJECT_REGISTRY::iterator const E = m_objects.end();
	for (; I != E; ++I)
		(*I).second->on_unregister();

	for (I = B; I != E; ++I)
		xr_delete((*I).second);
}

void CALifeObjectRegistry::save(IWriter& memory_stream, CSE_ALifeDynamicObject* object, u32& object_count)
{
	if (!can_serialize_alife_object(object))
		return;

	++object_count;

	NET_Packet tNetPacket;
	// Spawn
	object->Spawn_Write(tNetPacket,TRUE);
	memory_stream.w_u16(u16(tNetPacket.B.count));
	memory_stream.w(tNetPacket.B.data, tNetPacket.B.count);

	// Update
	tNetPacket.w_begin(M_UPDATE);
	object->UPDATE_Write(tNetPacket);

	memory_stream.w_u16(u16(tNetPacket.B.count));
	memory_stream.w(tNetPacket.B.data, tNetPacket.B.count);

	ALife::OBJECT_VECTOR::const_iterator I = object->children.begin();
	ALife::OBJECT_VECTOR::const_iterator E = object->children.end();
	for (; I != E; ++I)
	{
		CSE_ALifeDynamicObject* child = this->object(*I, true);
		if (!child)
			continue;

		if (!child->can_save())
			continue;

		save(memory_stream, child, object_count);
	}
}

void CALifeObjectRegistry::save(IWriter& memory_stream)
{
	Msg("* Saving objects...");
	memory_stream.open_chunk(OBJECT_CHUNK_DATA);

	u32 position = memory_stream.tell();
	memory_stream.w_u32(u32(-1));

	u32 object_count = 0;
	OBJECT_REGISTRY::iterator I = m_objects.begin();
	OBJECT_REGISTRY::iterator E = m_objects.end();
	for (; I != E; ++I)
	{
		if (!(*I).second->can_save())
			continue;

		if ((*I).second->redundant())
			continue;

		if ((*I).second->ID_Parent != 0xffff)
			continue;

		save(memory_stream, (*I).second, object_count);
	}

	u32 last_position = memory_stream.tell();
	memory_stream.seek(position);
	memory_stream.w_u32(object_count);
	memory_stream.seek(last_position);

	memory_stream.close_chunk();

	Msg("* %d objects are successfully saved", object_count);
}

CSE_ALifeDynamicObject* CALifeObjectRegistry::get_object(IReader& file_stream)
{
	NET_Packet tNetPacket;
	u16 u_id;
	// Spawn
	tNetPacket.B.count = file_stream.r_u16();
	file_stream.r(tNetPacket.B.data, tNetPacket.B.count);
	tNetPacket.r_begin(u_id);
	R_ASSERT2(M_SPAWN==u_id, "Invalid packet ID (!= M_SPAWN)");

	string64 s_name;
	tNetPacket.r_stringZ(s_name);
#ifdef DEBUG
	if (psAI_Flags.test(aiALife)) {
		Msg					("Loading object %s [%d]b", s_name, tNetPacket.B.count);
	}
#endif
	// create entity
	CSE_Abstract* tpSE_Abstract = F_entity_Create(s_name);
	if (!tpSE_Abstract)
	{
		Msg("! Can't create entity '%s'", s_name);
		tNetPacket.B.count = file_stream.r_u16();
		file_stream.advance(tNetPacket.B.count);
		return nullptr;
	}
	CSE_ALifeDynamicObject* tpALifeDynamicObject = smart_cast<CSE_ALifeDynamicObject*>(tpSE_Abstract);
	R_ASSERT2(tpALifeDynamicObject, "Non-ALife object in the saved game!");
	tpALifeDynamicObject->Spawn_Read(tNetPacket);

	// Update
	tNetPacket.B.count = file_stream.r_u16();
	file_stream.r(tNetPacket.B.data, tNetPacket.B.count);
	tNetPacket.r_begin(u_id);
	R_ASSERT2(M_UPDATE==u_id, "Invalid packet ID (!= M_UPDATE)");
	tpALifeDynamicObject->UPDATE_Read(tNetPacket);

	return (tpALifeDynamicObject);
}

void CALifeObjectRegistry::load(IReader& file_stream)
{
	Msg("* Loading objects...");
	R_ASSERT2(file_stream.find_chunk(OBJECT_CHUNK_DATA), "Can't find chunk OBJECT_CHUNK_DATA!");

	m_objects.clear();

	u32 count = file_stream.r_u32();
	for (u32 I = 0; I < count; ++I)
	{
		CSE_ALifeDynamicObject* tpSE_Abstract = get_object(file_stream);
		if (!tpSE_Abstract)
			continue;

		add(tpSE_Abstract);
	}

	Msg("* %d objects are successfully loaded", m_objects.size());
}
