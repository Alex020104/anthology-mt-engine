#include "stdafx.h"
#include "EngineThreading.h"
#include "CustomHUD.h"
#include "IGame_Persistent.h"
#include "IGame_Level.h"
#include "Rain.h"
#include "../../xrCDB/ISpatial.h"
#include "../../xrCDB/Frustum.h"
#include "Render.h"
#include "Irenderable.h"
#include "x_ray.h"
#include "../../Include/xrRender/Kinematics.h"

BOOL mt_Scheduler = TRUE;
BOOL mt_FrameProfile = FALSE;
BOOL mt_FrameProfileDetailed = FALSE;

namespace
{
enum EFrameTaskProfile
{
	FrameTaskPreRender,
	FrameTaskPostTransforms,
	FrameTaskCalculateBones,
	FrameTaskGame,
	FrameTaskGameScheduler,
	FrameTaskGameParallel,
	FrameTaskGameFrameMT,
	FrameTaskLuaGC,
	FrameTaskVision,
	FrameTaskCount
};

std::atomic<u64> frame_task_ticks[FrameTaskCount]{};
std::atomic<u64> frame_task_max_ticks[FrameTaskCount]{};
std::atomic<u64> frame_lua_gc_calls{};
std::atomic<u64> frame_lua_gc_skipped_busy{};
std::atomic<u64> frame_lua_gc_skipped_postload{};
std::atomic<u64> frame_parallel_items{};
std::atomic<u64> frame_parallel_item_max_ticks{};
std::atomic<LPCSTR> frame_parallel_item_max_name{};

void RecordFrameTask(EFrameTaskProfile task, u64 elapsed)
{
	frame_task_ticks[task].fetch_add(elapsed, std::memory_order_relaxed);
	u64 previous = frame_task_max_ticks[task].load(std::memory_order_relaxed);
	while (previous < elapsed &&
		!frame_task_max_ticks[task].compare_exchange_weak(previous, elapsed, std::memory_order_relaxed))
	{
	}
}

void RecordParallelItem(LPCSTR name, u64 elapsed)
{
	frame_parallel_items.fetch_add(1, std::memory_order_relaxed);
	u64 previous = frame_parallel_item_max_ticks.load(std::memory_order_relaxed);
	while (previous < elapsed)
	{
		if (frame_parallel_item_max_ticks.compare_exchange_weak(
			previous, elapsed, std::memory_order_relaxed))
		{
			frame_parallel_item_max_name.store(name, std::memory_order_relaxed);
			break;
		}
	}
}

class CFrameTaskTimer
{
	EFrameTaskProfile task;
	u64 started_at;

public:
	explicit CFrameTaskTimer(EFrameTaskProfile value) : task(value), started_at(mt_FrameProfile ? CPU::QPC() : 0) {}
	~CFrameTaskTimer()
	{
		if (started_at)
			RecordFrameTask(task, CPU::QPC() - started_at);
	}
};
}

u64 XRay::Engine::BeginVisionTaskProfile()
{
	return mt_FrameProfile ? CPU::QPC() : 0;
}

u64 XRay::Engine::BeginLuaGCTaskProfile()
{
	return mt_FrameProfile ? CPU::QPC() : 0;
}

void XRay::Engine::EndLuaGCTaskProfile(u64 started_at)
{
	if (started_at)
		RecordFrameTask(FrameTaskLuaGC, CPU::QPC() - started_at);
}

void XRay::Engine::EndVisionTaskProfile(u64 started_at)
{
	if (started_at)
		RecordFrameTask(FrameTaskVision, CPU::QPC() - started_at);
}

SFrameTaskProfile XRay::Engine::ConsumeFrameTaskProfile()
{
	SFrameTaskProfile result;
	result.pre_render = frame_task_ticks[FrameTaskPreRender].exchange(0, std::memory_order_relaxed);
	result.post_transforms = frame_task_ticks[FrameTaskPostTransforms].exchange(0, std::memory_order_relaxed);
	result.calculate_bones = frame_task_ticks[FrameTaskCalculateBones].exchange(0, std::memory_order_relaxed);
	result.game = frame_task_ticks[FrameTaskGame].exchange(0, std::memory_order_relaxed);
	result.game_scheduler = frame_task_ticks[FrameTaskGameScheduler].exchange(0, std::memory_order_relaxed);
	result.game_parallel = frame_task_ticks[FrameTaskGameParallel].exchange(0, std::memory_order_relaxed);
	result.game_frame_mt = frame_task_ticks[FrameTaskGameFrameMT].exchange(0, std::memory_order_relaxed);
	result.lua_gc = frame_task_ticks[FrameTaskLuaGC].exchange(0, std::memory_order_relaxed);
	result.vision = frame_task_ticks[FrameTaskVision].exchange(0, std::memory_order_relaxed);
	result.lua_gc_calls = frame_lua_gc_calls.exchange(0, std::memory_order_relaxed);
	result.lua_gc_skipped_busy = frame_lua_gc_skipped_busy.exchange(0, std::memory_order_relaxed);
	result.lua_gc_skipped_postload = frame_lua_gc_skipped_postload.exchange(0, std::memory_order_relaxed);
	result.game_parallel_items = frame_parallel_items.exchange(0, std::memory_order_relaxed);
	result.max_pre_render = frame_task_max_ticks[FrameTaskPreRender].exchange(0, std::memory_order_relaxed);
	result.max_post_transforms = frame_task_max_ticks[FrameTaskPostTransforms].exchange(0, std::memory_order_relaxed);
	result.max_calculate_bones = frame_task_max_ticks[FrameTaskCalculateBones].exchange(0, std::memory_order_relaxed);
	result.max_game = frame_task_max_ticks[FrameTaskGame].exchange(0, std::memory_order_relaxed);
	result.max_game_scheduler = frame_task_max_ticks[FrameTaskGameScheduler].exchange(0, std::memory_order_relaxed);
	result.max_game_parallel = frame_task_max_ticks[FrameTaskGameParallel].exchange(0, std::memory_order_relaxed);
	result.max_game_parallel_item = frame_parallel_item_max_ticks.exchange(0, std::memory_order_relaxed);
	result.max_game_parallel_item_name = frame_parallel_item_max_name.exchange(nullptr, std::memory_order_relaxed);
	result.max_game_frame_mt = frame_task_max_ticks[FrameTaskGameFrameMT].exchange(0, std::memory_order_relaxed);
	result.max_lua_gc = frame_task_max_ticks[FrameTaskLuaGC].exchange(0, std::memory_order_relaxed);
	result.max_vision = frame_task_max_ticks[FrameTaskVision].exchange(0, std::memory_order_relaxed);
	return result;
}

void XRay::Engine::PreRenderThread()
{
	CFrameTaskTimer frame_task_timer(FrameTaskPreRender);
	PROF_THREAD("Secondary Task 1");

	if (g_pGamePersistent && g_pGamePersistent->pEnvironment && g_pGamePersistent->pEnvironment->eff_Rain)
	{
		PROF_EVENT("CEffect_Rain::UpdateItems");
		g_pGamePersistent->pEnvironment->eff_Rain->UpdateItems();
	}

	if (g_pGamePersistent && Device.ParticleWorkerCallback)
	{
		PROF_EVENT("Process Particles");
		Device.ParticleWorkerCallback();
	}
}

void XRay::Engine::PreRenderPostTransformsThread()
{
	CFrameTaskTimer frame_task_timer(FrameTaskPostTransforms);
    PROF_THREAD("Secondary Task 1.1");
    {
        PROF_EVENT("seqParallelRender");
        for (auto& it : Device.seqParallelRender)
            it();
    }
}

struct SpatialSnapshot
{
	ISpatialShared ptr;
    IKinematics* pKin;
    float distSq;

    SpatialSnapshot(ISpatialShared _ptr, IKinematics* _pKin, float _distSq) : ptr(_ptr), pKin(_pKin), distSq(_distSq) {};
};
void XRay::Engine::CalculateBonesThread()
{
	CFrameTaskTimer frame_task_timer(FrameTaskCalculateBones);
	PROF_THREAD("Secondary Task 3");

	PROF_EVENT("CalculateBones");

	if (!g_SpatialSpace) return;
	if (Device.Paused()) return;
	if (!psDeviceFlags.test(rsDrawDynamic)) return;
	if (!g_pGameLevel || !g_pGameLevel->bReady) return;
	if (!g_pGameLevel->CurrentEntity()) return;

	static CFrustum ViewBase;
	ViewBase.CreateFromMatrix(Device.mFullTransform_saved, FRUSTUM_P_LRTB | FRUSTUM_P_FAR);

	static xr_vector<ISpatialShared> spatials = {};
    spatials.clear();
	g_SpatialSpace->q_sphere(
		spatials,
		ISpatial_DB::O_ORDERED,
		STYPE_RENDERABLE + STYPE_RENDERABLESHADOW + STYPE_PARTICLE + STYPE_LIGHTSOURCE,
		Device.vCameraPosition_saved,
		g_pGamePersistent->Environment().CurrentEnv->fog_distance
	);

	static xr_vector<SpatialSnapshot> spatialsSnapshot;
	spatialsSnapshot.clear();
	{
		for (ISpatialShared spatial : spatials)
		{
			if (!spatial)
				continue;

            float distSq = Device.vCameraPosition_saved.distance_to_sqr(spatial->spatial.sphere.P);
			if (!ViewBase.testSphere_dirty(spatial->spatial.sphere.P, spatial->spatial.sphere.R))
			{
				if (distSq > 62500.f)//250 m
					continue;
			}
				
			spatial->spatial_updatesector();

            if
            (
                (spatial->spatial.type & (STYPE_PARTICLE | STYPE_LIGHTSOURCE)) ||
                !(spatial->spatial.type & (STYPE_RENDERABLE | STYPE_RENDERABLESHADOW))
            )
                continue;

            auto renderable = spatial->dcast_Renderable();
            if (!(renderable && renderable->renderable.visual))
                continue;

            IKinematics* pKin = renderable->renderable.visual->dcast_PKinematics();
            if (!pKin)
                continue;

			spatialsSnapshot.emplace_back(spatial, pKin, distSq);
		}
	}

	static auto sortFunc = [](const SpatialSnapshot& _1, const SpatialSnapshot& _2) noexcept
	{
		return _1.distSq < _2.distSq;
	};
	std::sort(spatialsSnapshot.begin(), spatialsSnapshot.end(), sortFunc);

	// This function already overlaps the renderer as one secondary task. Nested
	// PPL jobs here cost more scheduling/synchronization than they save on the
	// relatively small visible-skeleton list (matching current Monolith).
	for (const auto& snapshot : spatialsSnapshot)
		snapshot.pKin->CalculateBones(TRUE);
}

extern BOOL psLua_ParallelGC;
int psLua_ParallelGC_CallAmount = 6;
int psLua_ParallelGC_BudgetUs = 1200;
BOOL psLua_ParallelGC_Adaptive = TRUE;
int psLua_ParallelGC_FrameBudgetUs = 12000;
int psLua_ParallelGC_PostLoadDelayMs = 8000;
void XRay::Engine::GameThread()
{
	CFrameTaskTimer frame_task_timer(FrameTaskGame);
	const u64 game_work_started_at = CPU::QPC();
	PROF_THREAD("Secondary Task 2")
		
	// we has granted permission to execute
	if (g_hud)
	{
		PROF_EVENT("g_hud OnFrameMT");
		g_hud->OnFrameMT();
	}

	if (g_pGameLevel && g_pGameLevel->bReady)
	{
		PROF_EVENT("SoundEvent_Dispatch");
		g_pGameLevel->SoundEvent_Dispatch();
	}

	if (!Device.Paused())
	{
		if (mt_Scheduler)
		{
			CFrameTaskTimer scheduler_profile(FrameTaskGameScheduler);
			PROF_EVENT("Sheduler Deferred");
			::Engine.Sheduler.UpdateDeferred();
			::Engine.Sheduler.UpdateFinalize();
		}
	}

	{
		CFrameTaskTimer parallel_profile(FrameTaskGameParallel);
		PROF_EVENT("seqParallel");
		for (u32 pit = 0; pit < Device.seqParallel.size(); pit++)
		{
			const LPCSTR task_name = pit < Device.seqParallelNames.size() ?
				Device.seqParallelNames[pit] : "legacy";
			const u64 task_started_at = mt_FrameProfile && mt_FrameProfileDetailed ? CPU::QPC() : 0;
			Device.seqParallel[pit]();
			if (task_started_at)
				RecordParallelItem(task_name, CPU::QPC() - task_started_at);
		}
		Device.seqParallel.clear();
		Device.seqParallelNames.clear();
	}
	{
		CFrameTaskTimer frame_mt_profile(FrameTaskGameFrameMT);
		PROF_EVENT("seqFrameMT");
		Device.seqFrameMT.Process(rp_Frame);
	}

	// Match current Monolith's useful idle-time placement without its former
	// nested-task race: all script MT callbacks above are complete before the
	// single secondary worker touches the LuaJIT VM. The main thread can keep
	// preparing/rendering the frame, and the frame-time/budget guards prevent
	// ordinary propagation work from extending past that overlap window.
	if (psLua_ParallelGC && Device.LuaGC && !EngineShouldDeferFullLuaGC())
	{
		if (EnginePostLoadGCCooldownActive(static_cast<u32>(psLua_ParallelGC_PostLoadDelayMs)))
		{
			frame_lua_gc_skipped_postload.fetch_add(1, std::memory_order_relaxed);
			return;
		}

		const u64 now = CPU::QPC();
		const u64 game_work_ticks = now - game_work_started_at;
		u64 budget_ticks = CPU::qpc_freq * static_cast<u64>(psLua_ParallelGC_BudgetUs) / 1000000ULL;
		if (psLua_ParallelGC_Adaptive)
		{
			const u64 frame_budget_ticks = CPU::qpc_freq *
				static_cast<u64>(psLua_ParallelGC_FrameBudgetUs) / 1000000ULL;
			if (game_work_ticks >= frame_budget_ticks ||
				!Device.isRendering.load(std::memory_order_relaxed))
			{
				frame_lua_gc_skipped_busy.fetch_add(1, std::memory_order_relaxed);
				return;
			}
			budget_ticks = std::min(budget_ticks, frame_budget_ticks - game_work_ticks);
		}

		CFrameTaskTimer lua_gc_profile(FrameTaskLuaGC);
		PROF_EVENT("seqLuaGC");
		const u64 started_at = CPU::QPC();
		Device.LuaGCCount = 0;
		Device.LuaGCDone = false;
		while (Device.isRendering.load(std::memory_order_relaxed) &&
			Device.LuaGCCount < psLua_ParallelGC_CallAmount &&
			CPU::QPC() - started_at < budget_ticks)
		{
			++Device.LuaGCCount;
			frame_lua_gc_calls.fetch_add(1, std::memory_order_relaxed);
			if (Device.LuaGC() == 1)
			{
				Device.LuaGCDone = true;
				break;
			}
		}
	}
}
