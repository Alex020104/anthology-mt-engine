////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_schedule_registry.cpp
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife schedule registry
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "alife_schedule_registry.h"
#include "../xrEngine/EngineThreading.h"

void CALifeScheduleRegistry::reset_profile()
{
	m_profile_first_frame = 0;
	m_profile_update_count = 0;
	m_profile_total_ticks = 0;
	m_profile_slowest_ticks = 0;
	m_profile_slowest_id = ALife::_OBJECT_ID(-1);
	m_profile_slowest_section = "none";
	m_profile_slowest_name = "none";
}

void CALifeScheduleRegistry::update_object(CSE_ALifeSchedulable* object)
{
	const bool profile = mt_FrameProfileDetailed && CPU::qpc_freq;
	const CSE_Abstract* base = profile ? object->base() : nullptr;
	const ALife::_OBJECT_ID id = base ? base->ID : ALife::_OBJECT_ID(-1);
	const shared_str section = base ? base->s_name : shared_str("unknown");
	const shared_str name = base ? base->name_replace() : shared_str("unknown");
	const u64 started_at = profile ? CPU::QPC() : 0;

	START_PROFILE("ALife/scheduled/update")
		object->update();
	STOP_PROFILE

	if (!profile)
		return;

	const u64 elapsed = CPU::QPC() - started_at;
	if (!m_profile_update_count)
		m_profile_first_frame = Device.dwFrame;
	++m_profile_update_count;
	m_profile_total_ticks += elapsed;
	if (elapsed > m_profile_slowest_ticks)
	{
		m_profile_slowest_ticks = elapsed;
		m_profile_slowest_id = id;
		m_profile_slowest_section = section;
		m_profile_slowest_name = name;
	}
}

void CALifeScheduleRegistry::flush_profile()
{
	if (!mt_FrameProfileDetailed || !CPU::qpc_freq)
	{
		reset_profile();
		return;
	}

	if (!m_profile_update_count || Device.dwFrame - m_profile_first_frame < 300)
		return;

	const double ticks_to_ms = 1000.0 / double(CPU::qpc_freq);
	Msg("* [A-Life/v88/profile] scheduled avg=%.3f ms max=%.3f ms "
		"object=[%u][%s][%s] updates=%u registry=%u cap=%u budget=%.3f ms",
		double(m_profile_total_ticks) * ticks_to_ms / double(m_profile_update_count),
		double(m_profile_slowest_ticks) * ticks_to_ms,
		static_cast<u32>(m_profile_slowest_id),
		*m_profile_slowest_section, *m_profile_slowest_name,
		m_profile_update_count, static_cast<u32>(objects().size()),
		m_objects_per_update, m_max_process_time * 1000.f);
	reset_profile();
}

void CALifeScheduleRegistry::update()
{
	if (!objects().empty())
		inherited::update(CUpdatePredicate(this, m_objects_per_update), false);
	flush_profile();
}

CALifeScheduleRegistry::~CALifeScheduleRegistry()
{
}

void CALifeScheduleRegistry::add(CSE_ALifeDynamicObject* object)
{
	CSE_ALifeSchedulable* schedulable = smart_cast<CSE_ALifeSchedulable*>(object);
	if (!schedulable)
		return;

	if (!schedulable->need_update(object))
		return;

	inherited::add(object->ID, schedulable);
}

void CALifeScheduleRegistry::remove(CSE_ALifeDynamicObject* object, bool no_assert)
{
	CSE_ALifeSchedulable* schedulable = smart_cast<CSE_ALifeSchedulable*>(object);
	if (!schedulable)
		return;

	inherited::remove(object->ID, no_assert || !schedulable->need_update(object));
}
