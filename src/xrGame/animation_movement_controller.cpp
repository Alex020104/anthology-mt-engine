#include "StdAfx.h"
#include "animation_movement_controller.h"

#include "../Include/xrRender/Kinematics.h"
#include "game_object_space.h"
#include "../xrphysics/matrix_utils.h"
#ifdef	 DEBUG
#include "phdebug.h"
#endif

void DBG_DrawBones(const Fmatrix& xform, IKinematics* K);
#ifdef	 DEBUG
BOOL	dbg_draw_animation_movement_controller  = FALSE;
u16		dbg_frame_count = 0;
#endif

namespace
{
bool starts_with(LPCSTR value, LPCSTR prefix)
{
	return value && prefix && std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

bool trace_cop_motion_owner(LPCSTR owner)
{
	return starts_with(owner, "pri_a15_") || starts_with(owner, "jup_b219_") || starts_with(owner, "pas_b400_");
}

float matrix_basis_determinant(const Fmatrix& matrix)
{
	Fvector cross;
	cross.crossproduct(matrix.j, matrix.k);
	return matrix.i.dotproduct(cross);
}

shared_str trace_motion_name(IKinematicsAnimated* animated, MotionID motion_id)
{
	if (!animated || !motion_id.valid() || motion_id.slot >= animated->LL_MotionsSlotCount())
		return shared_str("");

	shared_motions& motions = const_cast<shared_motions&>(animated->LL_MotionsSlot(motion_id.slot));
	accel_map* motion_map = motions.motion_map();
	for (accel_map::const_iterator it = motion_map->begin(); it != motion_map->end(); ++it)
	{
		if (it->second == motion_id.idx)
			return it->first;
	}

	return shared_str("");
}

void trace_frame(LPCSTR stage, LPCSTR owner, LPCSTR section, LPCSTR motion, bool local_animation, const CBlend* blend,
	const Fmatrix& before, const Fmatrix& start, const Fmatrix& root, const Fmatrix& target, const Fmatrix& after)
{
	Msg("* [cop-root-trace] %s f=%u ms=%u dt=%.6f owner=%s section=%s motion=%s id=%u:%u local=%u "
		"time=%.6f/%.6f before=(%.5f,%.5f,%.5f) start=(%.5f,%.5f,%.5f) "
		"root=(%.5f,%.5f,%.5f) target=(%.5f,%.5f,%.5f) after=(%.5f,%.5f,%.5f) "
		"basis_start=(%.6f,%.6f,%.6f,det=%.6f) basis_root=(%.6f,%.6f,%.6f,det=%.6f) "
		"basis_target=(%.6f,%.6f,%.6f,det=%.6f) basis_after=(%.6f,%.6f,%.6f,det=%.6f) "
		"k_start=(%.6f,%.6f,%.6f) k_root=(%.6f,%.6f,%.6f) "
		"k_target=(%.6f,%.6f,%.6f) k_after=(%.6f,%.6f,%.6f)",
		stage, Device.dwFrame, Device.dwTimeGlobal, Device.fTimeDelta, owner ? owner : "", section ? section : "",
		motion ? motion : "", blend ? u32(blend->motionID.slot) : 0u, blend ? u32(blend->motionID.idx) : 0u,
		local_animation ? 1u : 0u, blend ? blend->timeCurrent : 0.f, blend ? blend->timeTotal : 0.f,
		VPUSH(before.c), VPUSH(start.c), VPUSH(root.c), VPUSH(target.c), VPUSH(after.c),
		start.i.magnitude(), start.j.magnitude(), start.k.magnitude(), matrix_basis_determinant(start),
		root.i.magnitude(), root.j.magnitude(), root.k.magnitude(), matrix_basis_determinant(root),
		target.i.magnitude(), target.j.magnitude(), target.k.magnitude(), matrix_basis_determinant(target),
		after.i.magnitude(), after.j.magnitude(), after.k.magnitude(), matrix_basis_determinant(after),
		VPUSH(start.k), VPUSH(root.k), VPUSH(target.k), VPUSH(after.k));
}

bool trace_frame_sample(u32 sample, const CBlend* blend)
{
	if (sample <= 3 || (sample % 8) == 0)
		return true;

	return blend && blend->timeTotal - blend->timeCurrent <= 3.f * Device.fTimeDelta;
}

void trace_blend(LPCSTR owner, LPCSTR section, LPCSTR old_motion, LPCSTR new_motion, bool local_animation,
	const CBlend* old_blend, const CBlend* new_blend, const Fmatrix& before, const Fmatrix& start_before,
	const Fmatrix& previous_root, const Fmatrix& start_after, const Fmatrix& requested)
{
	Msg("* [cop-root-trace] blend f=%u ms=%u owner=%s section=%s old=%s id_old=%u:%u "
		"time_old=%.6f/%.6f new=%s id_new=%u:%u time_new=%.6f/%.6f local=%u "
		"before=(%.5f,%.5f,%.5f) start0=(%.5f,%.5f,%.5f) old_root=(%.5f,%.5f,%.5f) "
		"start1=(%.5f,%.5f,%.5f) request=(%.5f,%.5f,%.5f) "
		"basis_start0=(%.6f,%.6f,%.6f,det=%.6f) basis_root=(%.6f,%.6f,%.6f,det=%.6f) "
		"basis_start1=(%.6f,%.6f,%.6f,det=%.6f) basis_request=(%.6f,%.6f,%.6f,det=%.6f)",
		Device.dwFrame, Device.dwTimeGlobal, owner ? owner : "", section ? section : "", old_motion ? old_motion : "",
		old_blend ? u32(old_blend->motionID.slot) : 0u, old_blend ? u32(old_blend->motionID.idx) : 0u,
		old_blend ? old_blend->timeCurrent : 0.f, old_blend ? old_blend->timeTotal : 0.f,
		new_motion ? new_motion : "", new_blend ? u32(new_blend->motionID.slot) : 0u,
		new_blend ? u32(new_blend->motionID.idx) : 0u, new_blend ? new_blend->timeCurrent : 0.f,
		new_blend ? new_blend->timeTotal : 0.f, local_animation ? 1u : 0u,
		VPUSH(before.c), VPUSH(start_before.c), VPUSH(previous_root.c), VPUSH(start_after.c), VPUSH(requested.c),
		start_before.i.magnitude(), start_before.j.magnitude(), start_before.k.magnitude(),
		matrix_basis_determinant(start_before), previous_root.i.magnitude(), previous_root.j.magnitude(),
		previous_root.k.magnitude(), matrix_basis_determinant(previous_root), start_after.i.magnitude(),
		start_after.j.magnitude(), start_after.k.magnitude(), matrix_basis_determinant(start_after),
		requested.i.magnitude(), requested.j.magnitude(), requested.k.magnitude(), matrix_basis_determinant(requested));
}
} // namespace

animation_movement_controller::animation_movement_controller(Fmatrix* _pObjXForm, const Fmatrix& inital_pose,
                                                             IKinematics* _pKinematicsC, CBlend* b,
                                                             LPCSTR owner_name, LPCSTR owner_section,
                                                             bool local_animation):
	m_startObjXForm(inital_pose),
	m_pObjXForm(*_pObjXForm),
	m_pKinematicsC(_pKinematicsC),
	m_pKinematicsA(smart_cast<IKinematicsAnimated*>(_pKinematicsC)),
	inital_position_blending(true),
	stopped(false),
	blend_linear_speed(0),
	blend_angular_speed(0),
	m_trace_owner_name(owner_name ? owner_name : ""),
	m_trace_owner_section(owner_section ? owner_section : ""),
	m_trace_motion_name(""),
	m_trace_cop_motion(trace_cop_motion_owner(owner_name)),
	m_trace_local_animation(local_animation),
	m_trace_samples(0),
	m_control_blend(b),
	m_poses_blending(Fidentity, Fidentity, -1.f)
#ifdef	DEBUG
, DBG_previous_position( *_pObjXForm )
#endif
{
	VERIFY(_pKinematicsC);
	VERIFY(m_pKinematicsA);
	VERIFY(_pObjXForm);
	VERIFY(b);

#ifdef	DEBUG
	if( dbg_draw_animation_movement_controller )
	{
		m_pKinematicsC->CalculateBones_Invalidate();
		m_pKinematicsC->CalculateBones(TRUE);
		DBG_OpenCashedDraw();
		DBG_DrawBones( *_pObjXForm,  _pKinematicsC );
		DBG_ClosedCashedDraw( 50000 );
	}
#endif
	CBoneInstance& B = m_pKinematicsC->LL_GetBoneInstance(m_pKinematicsC->LL_GetBoneRoot());
	VERIFY(!B.callback() && !B.callback_param());
	B.set_callback(bctCustom, RootBoneCallback, this, TRUE);
	B.mTransform = Fidentity;
	GetInitalPositionBlenSpeed();
	//m_pKinematicsC->LL_VisBoxInvalidate();
	m_pKinematicsA->SetBlendDestroyCallback(this);
	m_pKinematicsC->CalculateBones_Invalidate();
	m_pKinematicsC->CalculateBones(TRUE);
	SetPosesBlending();

	if (m_trace_cop_motion)
		m_trace_motion_name = trace_motion_name(m_pKinematicsA, b->motionID);
	if (m_trace_cop_motion)
	{
		Fmatrix root;
		animation_root_position(root);
		Fmatrix target = Fmatrix().mul_43(m_startObjXForm, root);
		trace_frame("create", m_trace_owner_name.c_str(), m_trace_owner_section.c_str(), m_trace_motion_name.c_str(),
			m_trace_local_animation, m_control_blend, m_pObjXForm, m_startObjXForm, root, target, m_pObjXForm);
	}
#ifdef	DEBUG
	if( dbg_draw_animation_movement_controller )
	{
		DBG_OpenCashedDraw();
		DBG_DrawMatrix( *_pObjXForm, 3, 100 );
		DBG_DrawBones( *_pObjXForm,  _pKinematicsC );
		DBG_ClosedCashedDraw( 50000 );
	}
#endif
}

animation_movement_controller::~animation_movement_controller()
{
	if (IsActive())
		deinitialize();
}

IC bool is_blending_in(CBlend& b)
{
	return b.blend_state() == CBlend::eAccrue && b.blendPower - EPS > b.blendAmount;
}

void animation_movement_controller::deinitialize()
{
#ifdef	DEBUG
	if( dbg_draw_animation_movement_controller )
	{
		DBG_OpenCashedDraw();
		DBG_DrawMatrix( m_pObjXForm, 3, 100 );
		DBG_DrawBones( m_pObjXForm,  m_pKinematicsC );
		DBG_ClosedCashedDraw( 50000 );
	}
#endif


	CBoneInstance& B = m_pKinematicsC->LL_GetBoneInstance(m_pKinematicsC->LL_GetBoneRoot());
	VERIFY(B.callback() == RootBoneCallback);
	VERIFY(B.callback_param() == (void*)this);
	B.reset_callback();
	m_pKinematicsA->SetBlendDestroyCallback(0);
	m_control_blend = 0;

#ifdef	DEBUG
	if( dbg_draw_animation_movement_controller )
	{
		DBG_OpenCashedDraw();
		DBG_DrawMatrix( m_pObjXForm, 3, 100 );
		DBG_DrawBones( m_pObjXForm,  m_pKinematicsC );
		DBG_ClosedCashedDraw( 50000 );
	}
#endif
}

void animation_movement_controller::GetInitalPositionBlenSpeed()
{
	float sv_blend_time = m_control_blend->timeCurrent;

	//u16 root = m_pKinematicsC->LL_GetBoneRoot();
	Fmatrix m1;
	//m_pKinematicsC->Bone_GetAnimPos( m1, root, u8(-1), true );
	animation_root_position(m1);
	m_control_blend->timeCurrent += Device.fTimeDelta;
	clamp(m_control_blend->timeCurrent, 0.f, m_control_blend->timeTotal);
	Fmatrix m0;
	//m_pKinematicsC->Bone_GetAnimPos( m0, root, u8(-1), true );
	animation_root_position(m0);
	float l, a;
	get_diff_value(m0, m1, l, a);
	blend_linear_speed = l / Device.fTimeDelta;
	blend_angular_speed = a / Device.fTimeDelta;
	m_control_blend->timeCurrent = sv_blend_time;
}

bool animation_movement_controller::IsBlending() const
{
	return is_blending_in(*m_control_blend); //inital_position_blending ||
}

float blend_linear_accel = 1.f;
float blend_angular_accel = 1.f;

void animation_movement_controller::InitalPositionBlending(const Fmatrix& to)
{
#ifdef	DEBUG
	if( dbg_draw_animation_movement_controller )
	{
		DBG_DrawMatrix( m_pObjXForm, 1 );
		//DBG_DrawMatrix( m_startObjXForm, 3 );
	}
#endif
	//if( !inital_position_blending )
	//{
	//	if( m_control_blend->stop_at_end_callback && !IsBlending() )
	//	m_pObjXForm.set( to );
	//	return ;
	//}

#ifdef	DEBUG
	if( dbg_draw_animation_movement_controller )
		DBG_DrawMatrix( to, 2 );
#endif
	/*
		Fmatrix res = to;
		blend_linear_speed  += blend_linear_accel *Device.fTimeDelta ;
		blend_angular_speed += blend_angular_accel *Device.fTimeDelta ;
		
		inital_position_blending = !clamp_change( res, m_pObjXForm, blend_linear_speed*Device.fTimeDelta, blend_angular_speed*Device.fTimeDelta, 0.00001, 0.000001 ); 
		m_pObjXForm.set( res );
	*/
	if (!m_poses_blending.target_reached(m_control_blend->timeCurrent))
		m_poses_blending.pose(m_pObjXForm, m_control_blend->timeCurrent);
	else
		m_pObjXForm.set(to);

#ifdef	DEBUG
	DBG_previous_position = m_pObjXForm;
#endif
}

static void get_animation_root_position(Fmatrix& pos, IKinematics* K, IKinematicsAnimated* KA, CBlend* control_blend)
{
	VERIFY(KA);
	VERIFY(K);
	VERIFY(smart_cast<IKinematics*>(KA) == K);

	SKeyTable keys;
	KA->LL_BuldBoneMatrixDequatize(&K->LL_GetData(0), u8(1 << 0), keys);

	//find
	CKey* key = 0;
	for (int i = 0; i < keys.chanel_blend_conts[0]; ++i)
	{
		if (keys.blends[0][i] == control_blend)
			key = &keys.keys[0][i];
	}
	VERIFY(key);

	float sv_amount = control_blend->blendAmount;
	control_blend->blendAmount = 1.f;
	keys.blends[0][0] = control_blend;
	keys.chanel_blend_conts[0] = 1;
	keys.keys[0][0] = *key;


	for (int j = 1; j < MAX_CHANNELS; ++j)
		keys.chanel_blend_conts[j] = 0;

	CBoneInstance BI = K->LL_GetBoneInstance(0);

	KA->LL_BoneMatrixBuild(0, BI, &Fidentity, keys);
	pos.set(BI.mTransform);
	control_blend->blendAmount = sv_amount;
}

void animation_movement_controller::animation_root_position(Fmatrix& pos)
{
	get_animation_root_position(pos, m_pKinematicsC, m_pKinematicsA, m_control_blend);
}

void animation_movement_controller::OnFrame()
{
	//if( !isActive() )
	//	return;
	VERIFY(IsActive());
	DBG_verify_position_not_chaged();
	//		ka->CalculateBones_Invalidate( );
	//	ka->CalculateBones( TRUE );

#ifdef	DEBUG
	if( dbg_draw_animation_movement_controller && dbg_frame_count < 3 )
	{
		DBG_OpenCashedDraw();
		DBG_DrawBones( m_pObjXForm,  m_pKinematicsC );
		DBG_ClosedCashedDraw( 50000 );
	}
#endif


	//m_pKinematicsC->Bone_GetAnimPos( root_pos, m_pKinematicsC->LL_GetBoneRoot( ), u8(-1), true ); 
	Fmatrix root_pos;
	animation_root_position(root_pos);

	const Fmatrix object_before = m_pObjXForm;
	Fmatrix obj_pos = Fmatrix().mul_43(m_startObjXForm, root_pos);
	//Fvector prv_pos = m_pObjXForm.c;
	InitalPositionBlending(obj_pos);
	if (m_trace_cop_motion && m_trace_samples < 4096)
	{
		++m_trace_samples;
		if (trace_frame_sample(m_trace_samples, m_control_blend))
		{
			trace_frame("frame", m_trace_owner_name.c_str(), m_trace_owner_section.c_str(),
				m_trace_motion_name.c_str(), m_trace_local_animation, m_control_blend, object_before,
				m_startObjXForm, root_pos, obj_pos, m_pObjXForm);
		}
	}

#ifdef DEBUG
	DBG_previous_position = m_pObjXForm;
#endif


	//	UpdateVisBox( Fvector().sub(m_pObjXForm.c,prv_pos).square_magnitude()  );

	/*
		if( IsActive() && IsBlending() )
		{
			m_control_blend->timeCurrent = 0;
	
			struct scb : public IterateBlendsCallback, private xray::noncopyable
			{
				const CBlend &m_control_blend;
				scb( const CBlend &B ): m_control_blend( B ){}
				virtual	void	operator () ( CBlend &B )
				{
					if(B.motionID == m_control_blend.motionID )
						B.timeCurrent  = m_control_blend.timeCurrent;
				}
			} cb( *m_control_blend );
	
			m_pKinematicsA->LL_IterateBlends(cb);
		}
	*/
	//m_pKinematicsC->CalculateBones( );
#ifdef	DEBUG
	++dbg_frame_count;
#endif
}

void animation_movement_controller::NewBlend(CBlend* B, const Fmatrix& new_matrix, bool local_animation)
{
	/*
#ifdef	DEBUG
	LPCSTR old_anim_name	= m_pKinematicsC->dcast_PKinematicsAnimated( )->LL_MotionDefName_dbg( ControlBlend( )->motionID ).first;
	LPCSTR old_anim_set		= m_pKinematicsC->dcast_PKinematicsAnimated( )->LL_MotionDefName_dbg( ControlBlend( )->motionID ).second;
	LPCSTR new_anim_name	= m_pKinematicsC->dcast_PKinematicsAnimated( )->LL_MotionDefName_dbg( B->motionID ).first;
	LPCSTR new_anim_set		= m_pKinematicsC->dcast_PKinematicsAnimated( )->LL_MotionDefName_dbg( B->motionID ).second;
	
	if( ControlBlend( )->playing )
		Msg( " ! obj movement anim not yet ended anim: %s anim set: %s \n and already another started anim: %s anim set: %s", 	
			new_anim_name,new_anim_set,old_anim_name,old_anim_set
			);
	if( !ControlBlend( )->stop_at_end )
		Msg( " ! obj movement anim  : %s anim set: %s  is not stop-at-end but fallowed in chain by another obj movement anim: %s anim set: %s", 	
			old_anim_name,old_anim_set,new_anim_name,new_anim_set
			);
	if( !B->stop_at_end )
		Msg( " ! obj movement anim  : %s anim set: %s  is not stop-at-end but fallowing after another obj movement anim: %s anim set: %s", 	
			new_anim_name,new_anim_set,old_anim_name,old_anim_set
			);
#endif
	//	VERIFY(  );
		ControlBlend( )->blendAmount = 0;// B->blendPower;
		B->blendAmount = B->blendPower;
		m_control_blend = B;
	*/
	//CMotion* m_curr = smart_cast<IKinematicsAnimated*>(m_pKinematicsC)->LL_GetRootMotion(m_control_blend->motionID);
	//CMotion* m_new = smart_cast<IKinematicsAnimated*>(m_pKinematicsC)->LL_GetRootMotion(B->motionID);
	VERIFY(IsActive( ));
	const shared_str old_motion_name = m_trace_motion_name;
	const shared_str new_motion_name = m_trace_cop_motion ? trace_motion_name(m_pKinematicsA, B->motionID) : shared_str("");
	const CBlend* old_blend = m_control_blend;
	const Fmatrix object_before = m_pObjXForm;
	const Fmatrix start_before = m_startObjXForm;
	Fmatrix previous_root = Fidentity;

	//m_control_blend->timeCurrent = m_control_blend->timeTotal - SAMPLE_SPF;
	//m_pKinematicsC->Bone_GetAnimPos( m_pObjXForm, 0, u8(-1), true );
	//m_pObjXForm.mulA_43( m_startObjXForm );

#ifdef DEBUG
	DBG_previous_position = m_pObjXForm;
#endif

	bool set_blending = !m_poses_blending.target_reached(m_control_blend->timeCurrent);

	if (stopped)
	{
		m_control_blend = B;
		m_startObjXForm.set(new_matrix);
		GetInitalPositionBlenSpeed();
		inital_position_blending = true;
		set_blending = true;
		stopped = false;
	}
	else if (local_animation)
	{
		float blend_time = m_control_blend->timeCurrent;
		m_control_blend->timeCurrent = m_control_blend->timeTotal - SAMPLE_SPF; //(SAMPLE_SPF+EPS);
		animation_root_position(previous_root);
		m_startObjXForm.mulB_43(previous_root);
#ifdef	DEBUG
		if( dbg_draw_animation_movement_controller )
		{
		DBG_OpenCashedDraw();
		DBG_DrawMatrix( m_startObjXForm, 1 );
		DBG_ClosedCashedDraw(5000);
		}
#endif
		m_control_blend->timeCurrent = blend_time;
	}

	if (m_trace_cop_motion)
	{
		trace_blend(m_trace_owner_name.c_str(), m_trace_owner_section.c_str(), old_motion_name.c_str(),
			new_motion_name.c_str(), local_animation, old_blend, B, object_before, start_before, previous_root,
			m_startObjXForm, new_matrix);
	}

	m_control_blend = B;
	m_trace_motion_name = new_motion_name;
	m_trace_local_animation = local_animation;
	m_trace_samples = 0;
	if (set_blending)
		SetPosesBlending();
	else
		m_poses_blending = poses_blending(Fidentity, Fidentity, -1.f);
}

void animation_movement_controller::DBG_verify_position_not_chaged() const
{
#ifdef	DEBUG
	VERIFY( !IsActive()||inital_position_blending || cmp_matrix( DBG_previous_position, m_pObjXForm, EPS, EPS ) );
#endif
}

void animation_movement_controller::RootBoneCallback(CBoneInstance* B)
{
	VERIFY(B);
	VERIFY(B->callback_param());

	animation_movement_controller* O = (animation_movement_controller*)(B->callback_param());

	O->DBG_verify_position_not_chaged();

	//if( O->m_control_blend->stop_at_end_callback && !O->IsBlending() )
	//{
	//	O->m_pObjXForm.mul_43( O->m_startObjXForm, B->mTransform );
	//}

	//else
	//	Msg("blending");
	B->mTransform.set(Fidentity);


#if 0
	VERIFY( cmp_matrix( O->DBG_previous_position, O->m_pObjXForm, 1.f, 1.f ) );
#endif
	R_ASSERT2(_valid( B->mTransform ), "animation_movement_controller::RootBoneCallback");
}

bool animation_movement_controller::IsActive() const
{
	return !!m_control_blend;
}

void animation_movement_controller::BlendDestroy(CBlend& blend)
{
	VERIFY(m_control_blend);
	//Msg("deinit");
	if (m_control_blend == &blend)
		deinitialize();
}

void animation_movement_controller::stop()
{
	stopped = true;
}

const float percent_blending = 0.2f;

void animation_movement_controller::SetPosesBlending()
{
	VERIFY(IsActive());
	float blending_time = percent_blending * m_control_blend->timeTotal;

	float sv_time = m_control_blend->timeCurrent;
	m_control_blend->timeCurrent = blending_time;

	Fmatrix root;
	animation_root_position(root);

	poses_blending blending(m_pObjXForm, Fmatrix().mul_43(m_startObjXForm, root), blending_time);
	m_poses_blending = blending;
	m_control_blend->timeCurrent = sv_time;
}

float change_pos_delta = 0.02f;

//void	animation_movement_controller::UpdateVisBox	( float pos_sq_delta )
//{
//	VERIFY( m_pKinematicsC );
//	const Fbox &b = m_pKinematicsC->GetBox();
//	Fsphere		sphere; b.getsphere( sphere.P, sphere.R );
//	float sq_diff = Fvector().sub( m_pObjXForm.c,m_update_vis_pos).magnitude();
//
//	float change_pos_sq_delta = change_pos_delta * change_pos_delta * (( Device.fTimeDelta/0.01f )*( Device.fTimeDelta/0.01f ));
//
//	if(  pos_sq_delta > change_pos_sq_delta || sphere.P.square_magnitude() + change_pos_sq_delta + pos_sq_delta > sphere.R*sphere.R )
//	{
//		m_update_vis_pos = m_pObjXForm.c;
//		m_pKinematicsC->LL_VisBoxInvalidate();
//		m_pKinematicsC->CalculateBones_Invalidate( );
//		m_pKinematicsC->CalculateBones(TRUE);// TRUE 
//	}
//}
