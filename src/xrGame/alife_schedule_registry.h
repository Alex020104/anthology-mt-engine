////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_schedule_registry.h
//	Created 	: 15.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife schedule registry
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "safe_map_iterator.h"
#include "xrServer_Objects_ALife.h"
#include "ai_debug.h"
#include "profiler.h"

class CALifeScheduleRegistry : public CSafeMapIterator<
		ALife::_OBJECT_ID, CSE_ALifeSchedulable, std::less<ALife::_OBJECT_ID>, true, u64, false>
{
private:
	struct CUpdatePredicate
	{
		CALifeScheduleRegistry* m_registry;
		u32 m_count;
		mutable u32 m_current;

		IC CUpdatePredicate(CALifeScheduleRegistry* registry, const u32& count)
		{
			m_registry = registry;
			m_count = count;
			m_current = 0;
		}

		IC bool operator()(_iterator& i, u64 cycle_count, bool) const
		{
			if ((*i).second->m_schedule_counter == cycle_count)
				return (false);

			if (m_current >= m_count)
				return (false);

			++m_current;
			(*i).second->m_schedule_counter = cycle_count;

			return (true);
		}

		IC void operator()(_iterator& i, u64 cycle_count) const
		{
			m_registry->update_object((*i).second);
		}
	};

protected:
	typedef CSafeMapIterator<
		ALife::_OBJECT_ID, CSE_ALifeSchedulable, std::less<ALife::_OBJECT_ID>, true, u64, false> inherited;

protected:
	u32 m_objects_per_update;
	u32 m_profile_first_frame;
	u32 m_profile_update_count;
	u64 m_profile_total_ticks;
	u64 m_profile_slowest_ticks;
	ALife::_OBJECT_ID m_profile_slowest_id;
	shared_str m_profile_slowest_section;
	shared_str m_profile_slowest_name;

private:
	void update_object(CSE_ALifeSchedulable* object);
	void reset_profile();
	void flush_profile();

public:
	IC CALifeScheduleRegistry();
	virtual ~CALifeScheduleRegistry();
	void add(CSE_ALifeDynamicObject* object);
	void remove(CSE_ALifeDynamicObject* object, bool no_assert = false);
	void update();
	IC CSE_ALifeSchedulable* object(const ALife::_OBJECT_ID& id, bool no_assert = false) const;
	IC const u32& objects_per_update() const;
	IC void objects_per_update(const u32& objects_per_update);
};

#include "alife_schedule_registry_inline.h"
