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
#include "../xrEngine/device.h"

namespace
{
	bool CanContinueALife()
	{
		return Device.CanContinueGameLogicWork();
	}
}

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

void CALifeScheduleRegistry::update_object(CSE_ALifeSchedulable* object, bool frame_limited)
{
	const bool profile = mt_FrameProfile && mt_FrameProfileDetailed && CPU::qpc_freq;
#if XRCORE_PROFILER == PROFILER_OPTICK
	const bool capture = Optick::IsActive();
#else
	const bool capture = false;
#endif
	const CSE_Abstract* base = profile || capture ? object->base() : nullptr;
	const ALife::_OBJECT_ID profile_id = profile && base ? base->ID : ALife::_OBJECT_ID(-1);
	const shared_str profile_section = profile && base ? base->s_name : shared_str();
	const shared_str profile_name = profile && base ? shared_str(base->name_replace()) : shared_str();
	const u64 started_at = profile || frame_limited ? CPU::QPC() : 0;
	START_PROFILE("ALife/scheduled/update")
		if (capture && base)
		{
			PROF_TAG("ALife object id", u32(base->ID));
			PROF_TAG("ALife section", base->s_name.c_str());
			PROF_TAG("ALife name", base->name_replace());
		}
		object->update();
	STOP_PROFILE
	const u64 elapsed = profile || frame_limited ? CPU::QPC() - started_at : 0;
	if (profile)
	{
		if (!m_profile_update_count)
			m_profile_first_frame = Device.dwFrame;
		++m_profile_update_count;
		m_profile_total_ticks += elapsed;
		if (elapsed > m_profile_slowest_ticks)
		{
			m_profile_slowest_ticks = elapsed;
			m_profile_slowest_id = profile_id;
			m_profile_slowest_section = profile_section;
			m_profile_slowest_name = profile_name;
		}
	}
	// The callback has completely returned. Reuse the existing allocation-
	// pressure policy here so a long ALife batch cannot starve collection.
	if (frame_limited)
		Device.LuaGCAllocationSafePoint(elapsed);
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
	Msg("* [anthology/v100/alife-object] avg=%.3f ms max=%.2f ms "
		"object=[%u][%s][%s] updates=%u registry=%u cap=%u",
		double(m_profile_total_ticks) * ticks_to_ms / double(m_profile_update_count),
		double(m_profile_slowest_ticks) * ticks_to_ms,
		static_cast<u32>(m_profile_slowest_id),
		*m_profile_slowest_section, *m_profile_slowest_name,
		m_profile_update_count, static_cast<u32>(objects().size()), m_objects_per_update);
	reset_profile();
}

bool CALifeScheduleRegistry::update(bool frame_limited)
{
	if (!frame_limited || objects().empty() || !m_objects_per_update)
	{
		m_objects_remaining = 0;
		m_update_in_progress = false;
		if (!objects().empty())
			inherited::update(CUpdatePredicate(this, m_objects_per_update), false);
		flush_profile();
		return true;
	}

	if (!m_objects_remaining)
		m_objects_remaining = m_objects_per_update;
	bool completed = false;
	const u32 processed = inherited::update(CUpdatePredicate(this, m_objects_remaining, true),
		false, &completed, &CanContinueALife);
	if (completed || processed >= m_objects_remaining)
	{
		m_objects_remaining = 0;
		m_update_in_progress = false;
	}
	else
		m_objects_remaining -= processed;
	PROF_TAG("ALife objects processed", processed);
	PROF_TAG("ALife objects remaining", m_objects_remaining);
	flush_profile();
	return m_objects_remaining == 0;
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
