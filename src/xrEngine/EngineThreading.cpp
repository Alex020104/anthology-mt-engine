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
#include "../../Include/xrRender/Kinematics.h"

BOOL mt_Scheduler = TRUE;
BOOL mt_FrameProfile = TRUE;

namespace
{
enum EFrameTaskProfile
{
	FrameTaskPreRender,
	FrameTaskPostTransforms,
	FrameTaskCalculateBones,
	FrameTaskGame,
	FrameTaskLuaGC,
	FrameTaskCount
};

std::atomic<u64> frame_task_ticks[FrameTaskCount]{};

class CFrameTaskTimer
{
	EFrameTaskProfile task;
	u64 started_at;

public:
	explicit CFrameTaskTimer(EFrameTaskProfile value) : task(value), started_at(mt_FrameProfile ? CPU::QPC() : 0) {}
	~CFrameTaskTimer()
	{
		if (started_at)
			frame_task_ticks[task].fetch_add(CPU::QPC() - started_at, std::memory_order_relaxed);
	}
};
}

SFrameTaskProfile XRay::Engine::ConsumeFrameTaskProfile()
{
	SFrameTaskProfile result;
	result.pre_render = frame_task_ticks[FrameTaskPreRender].exchange(0, std::memory_order_relaxed);
	result.post_transforms = frame_task_ticks[FrameTaskPostTransforms].exchange(0, std::memory_order_relaxed);
	result.calculate_bones = frame_task_ticks[FrameTaskCalculateBones].exchange(0, std::memory_order_relaxed);
	result.game = frame_task_ticks[FrameTaskGame].exchange(0, std::memory_order_relaxed);
	result.lua_gc = frame_task_ticks[FrameTaskLuaGC].exchange(0, std::memory_order_relaxed);
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
int psLua_ParallelGC_CallAmount = 25;
int psLua_ParallelGC_BudgetUs = 250;
void XRay::Engine::GameThread()
{
	CFrameTaskTimer frame_task_timer(FrameTaskGame);
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
			PROF_EVENT("Sheduler Deferred");
			::Engine.Sheduler.UpdateDeferred();
			::Engine.Sheduler.UpdateFinalize();
		}
	}

	{
		PROF_EVENT("seqParallel");
		for (u32 pit = 0; pit < Device.seqParallel.size(); pit++)
			Device.seqParallel[pit]();
		Device.seqParallel.clear();
	}

    // demonized: While Renderer prepares frame and GPU renders it, use time opportunity to repeatedly call Lua GC with small step value
    // Reduces stutters since less work will be done in main GC step or no work at all
    static auto LuaGC = []()
    {
		CFrameTaskTimer frame_task_timer(FrameTaskLuaGC);
        PROF_EVENT("seqLuaGC");
		const u64 started_at = CPU::QPC();
		const u64 budget_ticks = CPU::qpc_freq * static_cast<u64>(psLua_ParallelGC_BudgetUs) / 1000000ULL;
        // Do at least once
        do
        {
            Device.LuaGCCount++;
            if (Device.LuaGC() == 1) // 1 informs that GC cycle is complete
            {
                Device.LuaGCDone = true;
                break;
            }

			// A large Lua heap can otherwise occupy a PPL worker for the whole
			// render and turn the final task_group wait into a CPU-side frame stall.
		} while (Device.isRendering.load(std::memory_order_relaxed) &&
			Device.LuaGCCount < psLua_ParallelGC_CallAmount &&
			CPU::QPC() - started_at < budget_ticks);
    };
    if (psLua_ParallelGC && Device.LuaGC)
        Device.secondary_tasks.run(LuaGC);

	{
		PROF_EVENT("seqFrameMT");
		Device.seqFrameMT.Process(rp_Frame);
	}
}
