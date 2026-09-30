//////////////////////////////////////////////////////////////////////
// ExplosiveItem.cpp:	класс для вещи которая взрывается под 
//						действием различных хитов (канистры,
//						балоны с газом и т.д.)
////////////////////////////////////////////////////////////////////////////
//	Modified by Axel DominatoR
//	Last updated: 13/08/2015
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "../../script_game_object.h"
#include "ExplosiveItem.h"
#include "Actor.h"
#include "Level.h"
#include "game_cl_base.h"


CExplosiveItem::CExplosiveItem(void) :
	m_mpMineMode(empMineDisabled),
	m_mpMineTriggerTime(0),
	m_mpMineTriggerRadius(2.5f),
	m_mpMineTriggered(false)
{
}

CExplosiveItem::~CExplosiveItem(void)
{
}

void CExplosiveItem::Load(LPCSTR section)
{
	inherited::Load(section);
	CExplosive::Load(section);

	// Added by Axel, to enable optional condition use on any item
	m_flags.set(FUsingCondition, READ_IF_EXISTS(pSettings, r_bool, section, "use_condition", TRUE));

	CDelayedActionFuse::Initialize(pSettings->r_float(section, "time_to_explode"),
	                               pSettings->r_float(section, "condition_to_explode"));
	VERIFY(pSettings->line_exist (section,"set_timer_particles"));

	m_mpMineMode = empMineDisabled;
	if (pSettings->line_exist(section, "mp_mine_mode"))
	{
		LPCSTR mode = pSettings->r_string(section, "mp_mine_mode");
		if (0 == xr_strcmp(mode, "proximity"))
			m_mpMineMode = empMineProximity;
		else if (0 == xr_strcmp(mode, "timer"))
			m_mpMineMode = empMineTimer;
	}
	m_mpMineTriggerRadius = READ_IF_EXISTS(pSettings, r_float, section, "mp_mine_trigger_radius", 2.5f);
}


BOOL CExplosiveItem::net_Spawn(CSE_Abstract* DC)
{
	const BOOL result = inherited::net_Spawn(DC);
	if (!result || m_mpMineMode == empMineDisabled)
		return result;

	m_mpMineTriggered = false;
	const float delay_seconds = READ_IF_EXISTS(pSettings, r_float, *cNameSect(), "mp_mine_delay", 5.f);
	m_mpMineTriggerTime = Device.dwTimeGlobal + iFloor(_max(delay_seconds, 0.f) * 1000.f);

	if (Local())
	{
		// The server sends GE_MP_MINE_SET_INITIATOR after spawning. Using
		// CurrentControlEntity here attributes every authoritative mine to the
		// listen-server host, including mines planted by remote clients.
		Msg("* [MP_MINE] armed local object: id=%u section=%s mode=%u delay=%.1f waiting_initiator=%u",
			ID(), *cNameSect(), u32(m_mpMineMode), delay_seconds, CurrentParentID());
	}

	return result;
}

void CExplosiveItem::TriggerMPMine()
{
	if (m_mpMineTriggered || !Local())
		return;

	if (CurrentParentID() == u16(-1))
	{
		Msg("! [MP_MINE] trigger deferred: id=%u section=%s has no planting actor",
			ID(), *cNameSect());
		return;
	}

	Fvector normal;
	FindNormal(normal);
	m_mpMineTriggered = true;
	Msg("* [MP_MINE] trigger: id=%u section=%s initiator=%u", ID(), *cNameSect(), Initiator());
	GenExplodeEvent(Position(), normal);
}

void CExplosiveItem::UpdateMPMine()
{
	if (m_mpMineMode == empMineDisabled || m_mpMineTriggered || !Local())
		return;
	if (Device.dwTimeGlobal < m_mpMineTriggerTime)
		return;

	if (m_mpMineMode == empMineTimer)
	{
		TriggerMPMine();
		return;
	}

	const float trigger_radius_sqr = m_mpMineTriggerRadius * m_mpMineTriggerRadius;
	// The transport caps sessions at 32 players; do not rescan every world
	// object for every armed mine.
	if (!Level().game)
		return;
	u32 checked = 0;
	for (const auto& player : Level().game->players)
	{
		if (++checked > 32)
			break;
		if (!player.second)
			continue;
		CActor* actor = smart_cast<CActor*>(Level().Objects.net_Find(player.second->GameID));
		if (!actor || !actor->g_Alive())
			continue;

		Fvector distance;
		distance.sub(actor->Position(), Position());
		if (distance.square_magnitude() <= trigger_radius_sqr)
		{
			TriggerMPMine();
			return;
		}
	}
}

void CExplosiveItem::net_Destroy()
{
	inherited::net_Destroy();
	CExplosive::net_Destroy();
}

//void CExplosiveItem::Hit(float P, Fvector &dir,	CObject* who, s16 element,
//						Fvector position_in_object_space, float impulse, 
//						ALife::EHitType hit_type)
void CExplosiveItem::Hit(SHit* pHDS)
{
	//	inherited::Hit(P,dir,who,element,position_in_object_space,impulse,hit_type);
	if (CDelayedActionFuse::isActive())pHDS->power = 0.f;
	inherited::Hit(pHDS);
	VERIFY(pHDS->who);
	if (!CDelayedActionFuse::isActive() &&
		CDelayedActionFuse::CheckCondition(GetCondition()) &&
			pHDS->who/*&&CExplosive::Initiator()==u16(-1)*/)
	{
		//запомнить того, кто взорвал вещь
		SetInitiator(pHDS->who->ID());
	}
}

void CExplosiveItem::StartTimerEffects()
{
	CParticlesPlayer::StartParticles(pSettings->r_string(*cNameSect(), "set_timer_particles"), Fvector().set(0, 1, 0),
	                                 ID());
}

void CExplosiveItem::OnEvent(NET_Packet& P, u16 type)
{
	if (type == GE_MP_MINE_SET_INITIATOR)
	{
		const u16 initiator = P.r_u16();
		SetInitiator(initiator);
		Msg("* [MP_MINE] initiator assigned: id=%u section=%s initiator=%u",
			ID(), *cNameSect(), initiator);
	}

	CExplosive::OnEvent(P, type);
	inherited::OnEvent(P, type);
}

void CExplosiveItem::UpdateCL()
{
	CExplosive::UpdateCL();
	inherited::UpdateCL();
}

void CExplosiveItem::shedule_Update(u32 dt)
{
	inherited::shedule_Update(dt);
	UpdateMPMine();
	if (CDelayedActionFuse::isActive() && CDelayedActionFuse::Update(GetCondition()))
	{
		Fvector normal;
		FindNormal(normal);
		CExplosive::GenExplodeEvent(Position(), normal);
		CParticlesPlayer::StopParticles(ID(), BI_NONE, true);
		::luabind::functor<void> funct;
		if (ai().script_engine().functor("_G.CExplosiveItem__OnExplode", funct))
		{
			funct(this->lua_game_object());
		}
	}
}

bool CExplosiveItem::shedule_Needed()
{
	//.	return true;

	return (inherited::shedule_Needed() || CDelayedActionFuse::isActive() ||
		(m_mpMineMode != empMineDisabled && !m_mpMineTriggered && Local()));
}

void CExplosiveItem::renderable_Render(IDSGraphManager* DM)
{
	inherited::renderable_Render(DM);
}

void CExplosiveItem::net_Relcase(CObject* O)
{
	CExplosive::net_Relcase(O);
	inherited::net_Relcase(O);
}

void CExplosiveItem::ActivateExplosionBox(const Fvector& size, Fvector& in_out_pos)
{
	//PKinematics(Visual())->CalculateBones();
}

void CExplosiveItem::GetRayExplosionSourcePos(Fvector& pos)
{
	random_point_in_object_box(pos, this);
}
