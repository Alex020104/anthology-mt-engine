////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_schedule_registry_inline.h
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife schedule registry inline functions
////////////////////////////////////////////////////////////////////////////

#pragma once

IC CALifeScheduleRegistry::CALifeScheduleRegistry()
{
	m_objects_per_update = 1;
	m_profile_first_frame = 0;
	m_profile_update_count = 0;
	m_profile_total_ticks = 0;
	m_profile_slowest_ticks = 0;
	m_profile_slowest_id = ALife::_OBJECT_ID(-1);
	m_profile_slowest_section = "none";
	m_profile_slowest_name = "none";
}

IC const u32& CALifeScheduleRegistry::objects_per_update() const
{
	return (m_objects_per_update);
}

IC void CALifeScheduleRegistry::objects_per_update(const u32& objects_per_update)
{
	m_objects_per_update = objects_per_update;
}

IC CSE_ALifeSchedulable* CALifeScheduleRegistry::object(const ALife::_OBJECT_ID& id, bool no_assert) const
{
	_const_iterator I = objects().find(id);
	if (I == objects().end())
	{
		THROW2(no_assert, "The spesified object hasn't been found in the schedule registry!");
		return (0);
	}
	return ((*I).second);
}
