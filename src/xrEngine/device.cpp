#include "stdafx.h"
#include "../xrCDB/frustum.h"
#include "xr_ioconsole.h"
#include "xr_input.h"
#include "../xrCore/profiler.h"

#pragma warning(disable:4995)
// mmsystem.h
#define MMNOSOUND
#define MMNOMIDI
#define MMNOAUX
#define MMNOMIXER
#define MMNOJOY
#include <mmsystem.h>
// d3dx9.h
#include <d3dx9.h>
#pragma warning(default:4995)

#include "x_ray.h"
#include "discord\discord.h"
#include "render.h"
#include <chrono>

// must be defined before include of FS_impl.h
#define INCLUDE_FROM_ENGINE
#include "../xrCore/FS_impl.h"

#ifdef INGAME_EDITOR
# include "../include/editor/ide.hpp"
# include "engine_impl.hpp"
#endif // #ifdef INGAME_EDITOR

#include "xrSash.h"
#include "igame_persistent.h"

#include "CustomHUD.h"
#include "EngineThreading.h"
#include "IGame_Level.h"

#include "Rain.h"

#pragma comment( lib, "d3dx9.lib" )

ENGINE_API CRenderDevice Device;
ENGINE_API CLoadScreenRenderer load_screen_renderer;
ENGINE_API CRenderDevice* DevicePtr = nullptr;

namespace
{
struct SPrecacheFrameCallbackProfile
{
	const void* object = nullptr;
	xr_string type_name;
	int priority = REG_PRIORITY_INVALID;
	u32 calls = 0;
	u64 total_ticks = 0;
	u64 max_ticks = 0;
};

struct SRuntimeFrameCallbackProfile
{
	const void* object = nullptr;
	xr_string type_name;
	int priority = REG_PRIORITY_INVALID;
	u32 calls = 0;
	u64 total_ticks = 0;
	u64 max_ticks = 0;
};

xr_vector<SRuntimeFrameCallbackProfile>& RuntimeFrameCallbackProfiles()
{
	static xr_vector<SRuntimeFrameCallbackProfile> profiles;
	return profiles;
}

u32& RuntimeFrameCallbackFrames()
{
	static u32 frames = 0;
	return frames;
}

void RecordRuntimeFrameCallback(const _REG_INFO& info, LPCSTR type_name, u64 elapsed_ticks)
{
	auto& profiles = RuntimeFrameCallbackProfiles();
	auto profile = std::find_if(profiles.begin(), profiles.end(), [&info](const auto& candidate)
	{
		return candidate.object == info.Object;
	});

	if (profile == profiles.end())
	{
		profiles.emplace_back();
		profile = profiles.end() - 1;
		profile->object = info.Object;
		profile->type_name = type_name;
		profile->priority = info.Prio;
	}

	++profile->calls;
	profile->total_ticks += elapsed_ticks;
	profile->max_ticks = std::max(profile->max_ticks, elapsed_ticks);
}

void PrintAndResetRuntimeFrameCallbackProfiles()
{
	auto& profiles = RuntimeFrameCallbackProfiles();
	const u32 frames = RuntimeFrameCallbackFrames();
	std::sort(profiles.begin(), profiles.end(), [](const auto& left, const auto& right)
	{
		return left.total_ticks > right.total_ticks;
	});

	const double ticks_to_average_ms = frames && CPU::qpc_freq ?
		1000.0 / (double(CPU::qpc_freq) * double(frames)) : 0.0;
	const double ticks_to_ms = CPU::qpc_freq ? 1000.0 / double(CPU::qpc_freq) : 0.0;
	for (u32 index = 0; index < profiles.size(); ++index)
	{
		const auto& profile = profiles[index];
		Msg("* [mt-frame/seqframe] #%02u avg=%.3f ms max=%.2f ms calls/frame=%.2f prio=%d type=%s",
			index + 1, profile.total_ticks * ticks_to_average_ms,
			profile.max_ticks * ticks_to_ms, double(profile.calls) / double(frames),
			profile.priority, profile.type_name.c_str());
	}

	profiles.clear();
	RuntimeFrameCallbackFrames() = 0;
}

xr_vector<SPrecacheFrameCallbackProfile>& PrecacheFrameCallbackProfiles()
{
	static xr_vector<SPrecacheFrameCallbackProfile> profiles;
	return profiles;
}

void RecordPrecacheFrameCallback(const _REG_INFO& info, LPCSTR type_name, u64 elapsed_ticks)
{
	auto& profiles = PrecacheFrameCallbackProfiles();
	auto profile = std::find_if(profiles.begin(), profiles.end(), [&info](const auto& candidate)
	{
		return candidate.object == info.Object;
	});

	if (profile == profiles.end())
	{
		profiles.emplace_back();
		profile = profiles.end() - 1;
		profile->object = info.Object;
		profile->type_name = type_name;
		profile->priority = info.Prio;
	}

	++profile->calls;
	profile->total_ticks += elapsed_ticks;
	profile->max_ticks = std::max(profile->max_ticks, elapsed_ticks);
}

void PrintPrecacheFrameCallbackProfiles()
{
	auto profiles = PrecacheFrameCallbackProfiles();
	std::sort(profiles.begin(), profiles.end(), [](const auto& left, const auto& right)
	{
		return left.total_ticks > right.total_ticks;
	});

	u64 total_ticks = 0;
	for (const auto& profile : profiles)
		total_ticks += profile.total_ticks;

	const double ticks_to_ms = 1000.0 / double(CPU::qpc_freq);
	Msg("* [load-session/frame-callbacks] callbacks=%u total=%.2f ms",
		static_cast<u32>(profiles.size()), total_ticks * ticks_to_ms);
	for (u32 index = 0; index < profiles.size(); ++index)
	{
		const auto& profile = profiles[index];
		Msg("* [load-session/frame-callbacks] #%02u total=%.2f ms max=%.2f ms calls=%u prio=%d type=%s",
			index + 1, profile.total_ticks * ticks_to_ms, profile.max_ticks * ticks_to_ms,
			profile.calls, profile.priority, profile.type_name.c_str());
	}
}

void ProcessPrecacheFrameCallbacks()
{
	auto& registrator = Device.seqFrame;
	registrator.in_process = true;

	if (registrator.R.empty())
	{
		registrator.in_process = false;
		return;
	}

	auto process = [](const _REG_INFO& info)
	{
		pureFrame* callback = static_cast<pureFrame*>(info.Object);
		const xr_string type_name = typeid(*callback).name();
		const u64 started_at = CPU::QPC();
		rp_Frame(info.Object);
		RecordPrecacheFrameCallback(info, type_name.c_str(), CPU::QPC() - started_at);
	};

	if (registrator.R[0].Prio == REG_PRIORITY_CAPTURE)
	{
		const _REG_INFO info = registrator.R[0];
		process(info);
	}
	else
	{
		for (u32 index = 0; index < registrator.R.size(); ++index)
		{
			const _REG_INFO info = registrator.R[index];
			if (info.Prio != REG_PRIORITY_INVALID)
				process(info);
		}
	}

	if (registrator.changed)
		registrator.Resort();
	registrator.in_process = false;
}

void ProcessRuntimeFrameCallbacks()
{
	auto& registrator = Device.seqFrame;
	registrator.in_process = true;

	if (registrator.R.empty())
	{
		registrator.in_process = false;
		return;
	}

	auto process = [](const _REG_INFO& info)
	{
		pureFrame* callback = static_cast<pureFrame*>(info.Object);
		// A frame callback may remove and destroy itself (CUISequencer does this
		// when the loading prompt is dismissed).  Keep all RTTI access before the
		// callback so runtime profiling never dereferences a dead object.
		const xr_string type_name = typeid(*callback).name();
		const u64 started_at = CPU::QPC();
		rp_Frame(info.Object);
		RecordRuntimeFrameCallback(info, type_name.c_str(), CPU::QPC() - started_at);
	};

	if (registrator.R[0].Prio == REG_PRIORITY_CAPTURE)
	{
		const _REG_INFO info = registrator.R[0];
		process(info);
	}
	else
	{
		for (u32 index = 0; index < registrator.R.size(); ++index)
		{
			const _REG_INFO info = registrator.R[index];
			if (info.Prio != REG_PRIORITY_INVALID)
				process(info);
		}
	}

	if (registrator.changed)
		registrator.Resort();
	registrator.in_process = false;

	if (++RuntimeFrameCallbackFrames() >= 300)
		PrintAndResetRuntimeFrameCallbackProfiles();
}
} // namespace

ENGINE_API xr_atomic_bool g_bRendering = false;
extern ENGINE_API float psHUD_FOV;

BOOL g_bLoaded = FALSE;
ref_light precache_light = 0;

BOOL mt_calc_bones = TRUE;
BOOL psLua_ParallelGC = TRUE;
BOOL psLua_ParallelGC_debug = FALSE;

extern discord::Core* discord_core;
extern bool use_discord;

extern Fvector4 ps_ssfx_grass_interactive;

#ifdef ECO_RENDER
ENGINE_API float refresh_rate = 0;
#endif // ECO_RENDER


BOOL CRenderDevice::Begin()
{
	PROF_EVENT("Render: Begin");

#ifndef DEDICATED_SERVER
	switch (m_pRender->GetDeviceState())
	{
	case IRenderDeviceRender::dsOK:
		break;

	case IRenderDeviceRender::dsLost:
		// If the device was lost, do not render until we get it back
		Sleep(33);
		return FALSE;
		break;

	case IRenderDeviceRender::dsNeedReset:
		// Check if the device is ready to be reset
		Reset();
		break;

	default:
		R_ASSERT(0);
	}

	m_pRender->Begin();

	FPU::m24r();
	g_bRendering = true;
#endif
	return TRUE;
}

void CRenderDevice::Clear()
{
	m_pRender->Clear();
}

extern void CheckPrivilegySlowdown();


void CRenderDevice::End(void)
{
	PROF_EVENT("Render: End");

#ifndef DEDICATED_SERVER
	const bool measure_precache = pApp && pApp->LoadSessionMeasurePrecache();

#ifdef INGAME_EDITOR
    bool load_finished = false;
#endif // #ifdef INGAME_EDITOR
	if (dwPrecacheFrame)
	{
		::Sound->set_master_volume(0.f);
		dwPrecacheFrame--;

		if (!dwPrecacheFrame)
		{
#ifdef INGAME_EDITOR
            load_finished = true;
#endif // #ifdef INGAME_EDITOR

			m_pRender->updateGamma();

			if (precache_light)
			{
				precache_light->set_active(false);
				precache_light.destroy();
			}
			::Sound->set_master_volume(1.f);

			if (!pApp || !pApp->LoadSessionActive())
			{
				m_pRender->ResourcesDestroyNecessaryTextures();
				Msg("* [x-ray]: Handled Necessary Textures Destruction");
			}
			Memory.mem_compact();
			//Msg("* MEMORY USAGE: %lld K", Memory.mem_usage() / 1024);
			//Msg("* End of synchronization A[%d] R[%d]", b_is_Active, b_is_Ready);

#ifdef FIND_CHUNK_BENCHMARK_ENABLE
            g_find_chunk_counter.flush();
#endif // FIND_CHUNK_BENCHMARK_ENABLE

			CheckPrivilegySlowdown();

			if (g_pGamePersistent->GameType() == 1) //haCk
			{
				WINDOWINFO wi;
				GetWindowInfo(m_hWnd, &wi);
				if (wi.dwWindowStatus != WS_ACTIVECAPTION)
					Pause(TRUE, TRUE, TRUE, "application start");
			}
		}
	}

	g_bRendering = false;
	// end scene
	// Present goes here, so call OA Frame end.
	if (g_SASH.IsBenchmarkRunning())
		g_SASH.DisplayFrame(Device.fTimeGlobal);
	const u64 present_started_at = measure_precache ? CPU::QPC() : 0;
	m_pRender->End();
	if (measure_precache)
		pApp->LoadSessionRecordPrecachePresent(CPU::QPC() - present_started_at);

# ifdef INGAME_EDITOR
    if (load_finished && m_editor)
        m_editor->on_load_finished();
# endif // #ifdef INGAME_EDITOR
#endif
}

void CRenderDevice::PreCache(u32 amount, bool b_draw_loadscreen, bool b_wait_user_input)
{
#ifdef DEDICATED_SERVER
    amount = 0;
#else
	if (m_pRender->GetForceGPU_REF())
		amount = 0;
#endif
	if (pApp)
		pApp->LoadSessionPrecacheBegin();
	if (amount == 60)
		PrecacheFrameCallbackProfiles().clear();

	dwPrecacheFrame = dwPrecacheTotal = amount;
	if (amount && !precache_light && g_pGameLevel && g_loading_events.empty())
	{
		precache_light = ::Render->light_create();
		precache_light->set_shadow(false);
		precache_light->set_position(vCameraPosition);
		precache_light->set_color(255, 255, 255);
		precache_light->set_range(5.0f);
		precache_light->set_active(true);
	}

	if (amount && b_draw_loadscreen && !load_screen_renderer.b_registered)
	{
		load_screen_renderer.start(b_wait_user_input);
	}
}

int g_svDedicateServerUpdateReate = 100;

ENGINE_API xr_list<LOADING_EVENT> g_loading_events;

extern bool IsMainMenuActive(); //ECO_RENDER add

static HMONITOR g_StartupMonitor = NULL;

#include "MonitorList.h"

static void InitMonitor()
{
	if (g_StartupMonitor)
		return;

	HMONITOR chosen = ResolveSelectedMonitor();
	if (chosen)
	{
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		if (GetMonitorInfoA(chosen, &mi))
		{
			g_StartupMonitor = chosen;
			return;
		}
		Msg("! vid_monitor: resolved handle is invalid, using Auto");
	}

	POINT cursorPos;
	GetCursorPos(&cursorPos);
	g_StartupMonitor = MonitorFromPoint(cursorPos, MONITOR_DEFAULTTOPRIMARY);
}

ENGINE_API void ResetStartupMonitor()
{
	g_StartupMonitor = NULL;
}

ENGINE_API void SetStartupMonitor(HMONITOR h)
{
	g_StartupMonitor = h;
}

ENGINE_API HMONITOR GetStartupMonitor()
{
	InitMonitor();
	return g_StartupMonitor;
}

void GetMonitorResolution(u32& horizontal, u32& vertical)
{
	InitMonitor();

	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoA(g_StartupMonitor, &mi))
	{
		horizontal = mi.rcMonitor.right - mi.rcMonitor.left;
		vertical = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}
	else
	{
		RECT desktop;
		const HWND hDesktop = GetDesktopWindow();
		GetWindowRect(hDesktop, &desktop);
		horizontal = desktop.right - desktop.left;
		vertical = desktop.bottom - desktop.top;
	}
}

void GetMonitorPosition(int& x, int& y)
{
	InitMonitor();

	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	if (GetMonitorInfoA(g_StartupMonitor, &mi))
	{
		x = mi.rcMonitor.left;
		y = mi.rcMonitor.top;
	}
	else
	{
		x = 0;
		y = 0;
	}
}

float GetMonitorRefresh()
{
	DEVMODE lpDevMode;
	memset(&lpDevMode, 0, sizeof(DEVMODE));
	lpDevMode.dmSize = sizeof(DEVMODE);
	lpDevMode.dmDriverExtra = 0;

	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode) == 0)
	{
		return 1.f / 60.f;
	}
	else
		return 1.f / lpDevMode.dmDisplayFrequency;
}

extern int ps_framelimiter;
extern int ps_menu_framelimiter;
extern u32 g_screenmode;

CTimer FreezeTimer;
void mt_FreezeThread(void *ptr) {
	float freezetime = 0.f;
	float repeatcheck = 500.f;

	while (true)
	{
		PROF_EVENT();

		if (g_loading_events.size())
			freezetime = 25000.0f;
		else
			freezetime = 5000.0f;

		repeatcheck = 500.f;

		START_PROFILE("Check timer");
		if (FreezeTimer.GetElapsed_sec()*1000.f > freezetime)
		{
			xrLogger::FlushLog();
			repeatcheck = 5000.f;
		}
		STOP_PROFILE;

		Sleep(repeatcheck);
	}
}

void CRenderDevice::on_idle()
{

	FreezeTimer.Start();

	if (!b_is_Ready)
	{
		Sleep(100);
		return;
	}

	PROF_FRAME("Main Thread");

#ifdef DEDICATED_SERVER
    u32 FrameStartTime = TimerGlobal.GetElapsed_ms();
#endif

	START_PROFILE("Set stat gathering");
	if (psDeviceFlags.test(rsStatistic))
		g_bEnableStatGather = TRUE;
	else g_bEnableStatGather = FALSE;
	STOP_PROFILE;

	if (g_loading_events.size())
	{
		{
			PROF_EVENT("Loading...");
			if (g_loading_events.front()())
				g_loading_events.pop_front();
		}
		PROF_EVENT("LoadDraw");
		pApp->LoadDraw();
		return;
	}

	if (!Device.dwPrecacheFrame && !g_SASH.IsBenchmarkRunning() && g_bLoaded)
	{
		PROF_EVENT("Start xrSASH Benchmark");
		g_SASH.StartBenchmark();
	}

	const bool measure_mt_frame = mt_FrameProfile && !Device.dwPrecacheFrame && g_bLoaded;
	const u64 mt_frame_started_at = measure_mt_frame ? CPU::QPC() : 0;

	if (Device.ModelDefferClear)
	{
		Device.ModelDefferClear();
	}

	{
		PROF_EVENT("seqParallelBeforRender");
		for (auto& it : Device.seqParallelBeforRender)
			it();

		Device.seqParallelBeforRender.clear();
	}

	const bool precache_before_frame = pApp && pApp->LoadSessionMeasurePrecache();
	const u64 frame_started_at = precache_before_frame ? CPU::QPC() : 0;
	const u64 mt_frame_move_started_at = measure_mt_frame ? CPU::QPC() : 0;
	FrameMove();
	const u64 mt_frame_move_ticks = measure_mt_frame ? CPU::QPC() - mt_frame_move_started_at : 0;
	const bool measure_precache_frame = pApp && pApp->LoadSessionMeasurePrecache();
	const u64 frame_move_finished_at = measure_precache_frame ? CPU::QPC() : 0;
	const u64 measured_frame_started_at = precache_before_frame ? frame_started_at : frame_move_finished_at;
	const u64 frame_move_ticks = precache_before_frame ? frame_move_finished_at - frame_started_at : 0;
	u64 seq_render_ticks = 0;
	u64 end_ticks = 0;
	u64 mt_seq_render_ticks = 0;

    if (g_pGamePersistent != nullptr)
    {
        PROF_EVENT("Update Particles");
        g_pGamePersistent->UpdateParticles();
    }
    secondary_tasks.run(&XRay::Engine::PreRenderThread);

	// Precache
	if (dwPrecacheFrame)
	{
		PROF_EVENT("Precache frame");
		float factor = float(dwPrecacheFrame) / float(dwPrecacheTotal);
		float angle = PI_MUL_2 * factor;
		vCameraDirection.set(_sin(angle), 0, _cos(angle));
		vCameraDirection.normalize();
		vCameraTop.set(0, 1, 0);
		vCameraRight.crossproduct(vCameraTop, vCameraDirection);

		mView.build_camera_dir(vCameraPosition, vCameraDirection, vCameraTop);
	}

	// Matrices
	START_PROFILE("Matrices");
	mFullTransform.mul(mProject, mView);
	mFullTransformHud.mul(mProjectHud, mView);
	mFullTransformCam.mul(mProjectCam, mView);
	m_pRender->SetCacheXform(mView, mProject);

	mViewHud_prev = mViewHud;
	mProjectHud_prev = mProjectHud;
	mFullTransformHud_prev = mFullTransformHud;
	mViewCam_prev = mViewCam;
	mProjectCam_prev = mProjectCam;
	mFullTransformCam_prev = mFullTransformCam;

	// Keep previous camera transforms per viewport. SecondVP alternates with the
	// main view, so sharing one previous transform makes its TAA reproject across
	// unrelated FOVs.
	const bool svp_frame = m_SecondViewport.IsSVPFrame();
	if (svp_frame)
	{
		const u32 frame_delay = std::max<u8>(m_SecondViewport.GetSVPFrameDelay(), 2);
		const bool svp_camera_valid = mSVPCameraSaved && dwFrame <= mSVPCameraFrame + frame_delay;
		mView_prev = svp_camera_valid ? mView_saved_svp : mView;
		mProject_prev = svp_camera_valid ? mProject_saved_svp : mProject;
	}
	else
	{
		mView_prev = mView_saved;
		mProject_prev = mProject_saved;
		if (!m_SecondViewport.IsSVPActive())
			mSVPCameraSaved = false;
	}
	mFullTransform_prev = mFullTransform_saved; // Unused?

	m_pRender->SetCacheXform_prev(mView_prev, mProject_prev);

	mProjectHud.build_projection(deg2rad(psHUD_FOV * 83.f), fASPECT, R_VIEWPORT_NEAR, g_pGamePersistent->Environment().CurrentEnv->far_plane);
	mProjectCam.build_projection(deg2rad(83.f), fASPECT, R_VIEWPORT_NEAR, g_pGamePersistent->Environment().CurrentEnv->far_plane);
	
	mViewHud.set(mView);
	mViewCam.set(mView);
	mFullTransformHud.mul(mProjectHud, mViewHud);
	mFullTransformCam.mul(mProjectCam, mViewCam);

	// Save previous frame grass benders data
	IGame_Persistent::grass_data& GData = g_pGamePersistent->grass_shader_data;

	GData.prev_pos[0].set(Device.vCameraPosition.x, Device.vCameraPosition.y, Device.vCameraPosition.z, -1);
	GData.prev_dir[0].set(0.0f, -99.0f, 0.0f, 1.0f);

	for (int pBend = 1; pBend < _min(16, ps_ssfx_grass_interactive.y + 1); pBend++)
	{
		GData.prev_pos[pBend].set(GData.pos[pBend].x, GData.pos[pBend].y, GData.pos[pBend].z, GData.radius_curr[pBend]);
		GData.prev_dir[pBend].set(GData.dir[pBend].x, GData.dir[pBend].y, GData.dir[pBend].z, GData.str[pBend]);
	}

	// Save wind animation position
	wind_anim_prev = wind_anim_saved;
	wind_anim_saved = g_pGamePersistent->Environment().wind_anim;

	//RCache.set_xform_view ( mView );
	//RCache.set_xform_project ( mProject );
	D3DXMatrixInverse((D3DXMATRIX*)&mInvFullTransform, 0, (D3DXMATRIX*)&mFullTransform);

	if (svp_frame)
	{
		mView_saved_svp = mView;
		mProject_saved_svp = mProject;
		mSVPCameraSaved = true;
		mSVPCameraFrame = dwFrame;
	}
	else
	{
		vCameraPosition_saved = vCameraPosition;
		mFullTransform_saved = mFullTransform;
		mView_saved = mView;
		mProject_saved = mProject;
	}
	STOP_PROFILE;

	// HOM/detail preparation and skeleton matrices are consumed only by a world
	// render. Sparse loading precache deliberately skips most world renders, so
	// repeating those render-only jobs on the skipped frames just makes the main
	// thread wait for work whose result is overwritten before it is displayed.
	// FrameMove, Lua-visible callbacks, object updates, particles and the full
	// render frames are left untouched.
	const bool prepare_world_render = !measure_precache_frame ||
		pApp->LoadSessionShouldRenderPrecacheWorld(dwPrecacheFrame, dwPrecacheTotal);
	if (prepare_world_render)
	{
		secondary_tasks.run(&XRay::Engine::PreRenderPostTransformsThread);
		if (mt_calc_bones)
			secondary_tasks.run(&XRay::Engine::CalculateBonesThread);
		else
			XRay::Engine::CalculateBonesThread();
	}

	Device.isRendering = true;
	secondary_tasks.run(&XRay::Engine::GameThread);
	
#ifdef ECO_RENDER // ECO_RENDER START
	{
		PROF_EVENT("Eco Render");
		using limiter_clock = std::chrono::steady_clock;
		static limiter_clock::time_point previous_frame = limiter_clock::now();
		static bool limiter_armed = false;

		const bool loading = Device.dwPrecacheFrame || !g_loading_events.empty() ||
			load_screen_renderer.IsActive() || (pApp && pApp->LoadSessionActive());
		const bool menu_limit_active = !loading && IsMainMenuActive() && ps_menu_framelimiter > 0;
		const int target_fps = ps_framelimiter > 0 ? ps_framelimiter :
			(menu_limit_active ? ps_menu_framelimiter : 0);
		float target_seconds = target_fps > 0 ? 1.f / float(target_fps) : 0.f;

		// Preserve the old paused-game eco behaviour without throttling an active
		// load. Menu limiting is explicit and defaults to 60 FPS.
		if (!loading && target_seconds <= 0.f && Device.Paused())
		{
			if (refresh_rate == 0)
				refresh_rate = GetMonitorRefresh();
			target_seconds = refresh_rate;
		}

		if (target_seconds > 0.f)
		{
			const auto now = limiter_clock::now();
			if (!limiter_armed)
			{
				previous_frame = now;
				limiter_armed = true;
			}
			else
			{
				const auto target = previous_frame +
					std::chrono::duration_cast<limiter_clock::duration>(std::chrono::duration<float>(target_seconds));
				auto current = now;
				auto remaining = target - current;
				const auto one_ms = std::chrono::milliseconds(1);

				// Give the CPU to the OS for the coarse part, then yield only for
				// the short remainder. This avoids the old full-frame busy spin.
				if (remaining > std::chrono::milliseconds(2))
				{
					const auto sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(remaining) - one_ms;
					Sleep(static_cast<DWORD>(sleep_ms.count()));
				}
				while ((current = limiter_clock::now()) < target)
					SwitchToThread();

				// Do not accumulate a catch-up burst after a long external stall.
				const auto frame_period = target - previous_frame;
				previous_frame = current - target > frame_period ? current : target;
			}
		}
		else
		{
			limiter_armed = false;
		}
	}
#endif // ECO_RENDER END

#ifndef DEDICATED_SERVER
	Statistic->RenderTOTAL_Real.FrameStart();
	Statistic->RenderTOTAL_Real.Begin();

	if (b_is_Active && Begin())
	{
		START_PROFILE("Process seqRender");
		const bool measure_seq_render = measure_precache_frame || measure_mt_frame;
		const u64 seq_render_started_at = measure_seq_render ? CPU::QPC() : 0;
		seqRender.Process(rp_Render);
		if (measure_seq_render)
		{
			const u64 elapsed = CPU::QPC() - seq_render_started_at;
			if (measure_precache_frame)
				seq_render_ticks = elapsed;
			if (measure_mt_frame)
				mt_seq_render_ticks = elapsed;
		}
		STOP_PROFILE;

		if (psDeviceFlags.test(rsCameraPos) || psDeviceFlags.test(rsStatistic) || Statistic->errors.size())
		{
			PROF_EVENT("Draw statistics");
			Statistic->Show();
		}

		const u64 end_started_at = measure_precache_frame ? CPU::QPC() : 0;
		End();
		if (measure_precache_frame)
			end_ticks = CPU::QPC() - end_started_at;
	}
	Statistic->RenderTOTAL_Real.End();
	Statistic->RenderTOTAL_Real.FrameEnd();
	Statistic->RenderTOTAL.accum = Statistic->RenderTOTAL_Real.accum;
#endif 
	Device.isRendering = false;

	const bool measure_secondary_wait = measure_precache_frame || measure_mt_frame;
	const u64 secondary_wait_started_at = measure_secondary_wait ? CPU::QPC() : 0;
	secondary_tasks.wait();
	const u64 frame_finished_at = measure_secondary_wait ? CPU::QPC() : 0;
	if (measure_precache_frame)
	{
		pApp->LoadSessionRecordPrecacheFrame(frame_finished_at - measured_frame_started_at,
			frame_move_ticks, seq_render_ticks, end_ticks, frame_finished_at - secondary_wait_started_at);
	}

	const SFrameTaskProfile task_profile = mt_FrameProfile ?
		XRay::Engine::ConsumeFrameTaskProfile() : SFrameTaskProfile{};
	if (measure_mt_frame)
	{
		struct SFrameProfileAccumulator
		{
			u32 frames = 0;
			u64 total = 0;
			u64 frame_move = 0;
			u64 seq_render = 0;
			u64 secondary_wait = 0;
			u64 max_total = 0;
			u64 max_frame_move = 0;
			u64 max_seq_render = 0;
			u64 max_secondary_wait = 0;
			SFrameTaskProfile tasks;
		};
		static SFrameProfileAccumulator profile;
		const u64 total_ticks = frame_finished_at - mt_frame_started_at;
		const u64 wait_ticks = frame_finished_at - secondary_wait_started_at;
		++profile.frames;
		profile.total += total_ticks;
		profile.frame_move += mt_frame_move_ticks;
		profile.seq_render += mt_seq_render_ticks;
		profile.secondary_wait += wait_ticks;
		profile.max_total = std::max(profile.max_total, total_ticks);
		profile.max_frame_move = std::max(profile.max_frame_move, mt_frame_move_ticks);
		profile.max_seq_render = std::max(profile.max_seq_render, mt_seq_render_ticks);
		profile.max_secondary_wait = std::max(profile.max_secondary_wait, wait_ticks);
		profile.tasks.pre_render += task_profile.pre_render;
		profile.tasks.post_transforms += task_profile.post_transforms;
		profile.tasks.calculate_bones += task_profile.calculate_bones;
		profile.tasks.game += task_profile.game;
		profile.tasks.game_scheduler += task_profile.game_scheduler;
		profile.tasks.game_parallel += task_profile.game_parallel;
		profile.tasks.game_frame_mt += task_profile.game_frame_mt;
		profile.tasks.lua_gc += task_profile.lua_gc;
		profile.tasks.vision += task_profile.vision;
		profile.tasks.lua_gc_calls += task_profile.lua_gc_calls;
		profile.tasks.lua_gc_skipped_busy += task_profile.lua_gc_skipped_busy;
		profile.tasks.lua_gc_skipped_postload += task_profile.lua_gc_skipped_postload;
		profile.tasks.game_parallel_items += task_profile.game_parallel_items;
		profile.tasks.max_pre_render = std::max(profile.tasks.max_pre_render, task_profile.max_pre_render);
		profile.tasks.max_post_transforms = std::max(profile.tasks.max_post_transforms, task_profile.max_post_transforms);
		profile.tasks.max_calculate_bones = std::max(profile.tasks.max_calculate_bones, task_profile.max_calculate_bones);
		profile.tasks.max_game = std::max(profile.tasks.max_game, task_profile.max_game);
		profile.tasks.max_game_scheduler = std::max(profile.tasks.max_game_scheduler, task_profile.max_game_scheduler);
		profile.tasks.max_game_parallel = std::max(profile.tasks.max_game_parallel, task_profile.max_game_parallel);
		if (task_profile.max_game_parallel_item > profile.tasks.max_game_parallel_item)
		{
			profile.tasks.max_game_parallel_item = task_profile.max_game_parallel_item;
			profile.tasks.max_game_parallel_item_name = task_profile.max_game_parallel_item_name;
		}
		profile.tasks.max_game_frame_mt = std::max(profile.tasks.max_game_frame_mt, task_profile.max_game_frame_mt);
		profile.tasks.max_lua_gc = std::max(profile.tasks.max_lua_gc, task_profile.max_lua_gc);
		profile.tasks.max_vision = std::max(profile.tasks.max_vision, task_profile.max_vision);

		if (profile.frames >= 300)
		{
			const double ticks_to_average_ms = 1000.0 /
				(double(CPU::qpc_freq) * double(profile.frames));
			const double ticks_to_ms = 1000.0 / double(CPU::qpc_freq);
			Msg("* [mt-frame/profile] frames=%u avg(total/frame/render/wait)=%.2f/%.2f/%.2f/%.2f ms "
				"workers(pre/post/bones/game/lua-gc/vision)=%.2f/%.2f/%.2f/%.2f/%.2f/%.2f ms "
				"max(total/frame/render/wait)=%.2f/%.2f/%.2f/%.2f ms",
				profile.frames, profile.total * ticks_to_average_ms,
				profile.frame_move * ticks_to_average_ms, profile.seq_render * ticks_to_average_ms,
				profile.secondary_wait * ticks_to_average_ms,
				profile.tasks.pre_render * ticks_to_average_ms,
				profile.tasks.post_transforms * ticks_to_average_ms,
				profile.tasks.calculate_bones * ticks_to_average_ms,
				profile.tasks.game * ticks_to_average_ms,
				profile.tasks.lua_gc * ticks_to_average_ms,
				profile.tasks.vision * ticks_to_average_ms,
				profile.max_total * ticks_to_ms,
				profile.max_frame_move * ticks_to_ms,
				profile.max_seq_render * ticks_to_ms,
				profile.max_secondary_wait * ticks_to_ms);
			Msg("* [mt-frame/profile] max-workers(pre/post/bones/game/lua-gc/vision)=%.2f/%.2f/%.2f/%.2f/%.2f/%.2f ms",
				profile.tasks.max_pre_render * ticks_to_ms,
				profile.tasks.max_post_transforms * ticks_to_ms,
				profile.tasks.max_calculate_bones * ticks_to_ms,
				profile.tasks.max_game * ticks_to_ms,
				profile.tasks.max_lua_gc * ticks_to_ms,
				profile.tasks.max_vision * ticks_to_ms);
			Msg("* [mt-frame/profile] game-breakdown avg(scheduler/parallel/frame-mt)=%.2f/%.2f/%.2f ms "
				"max=%.2f/%.2f/%.2f ms gc(calls/busy/postload)=%llu/%llu/%llu",
				profile.tasks.game_scheduler * ticks_to_average_ms,
				profile.tasks.game_parallel * ticks_to_average_ms,
				profile.tasks.game_frame_mt * ticks_to_average_ms,
				profile.tasks.max_game_scheduler * ticks_to_ms,
				profile.tasks.max_game_parallel * ticks_to_ms,
				profile.tasks.max_game_frame_mt * ticks_to_ms,
				static_cast<unsigned long long>(profile.tasks.lua_gc_calls),
				static_cast<unsigned long long>(profile.tasks.lua_gc_skipped_busy),
				static_cast<unsigned long long>(profile.tasks.lua_gc_skipped_postload));
			if (mt_FrameProfileDetailed)
			{
				Msg("* [mt-frame/profile] seqParallel slowest=%s %.2f ms, items/frame=%.2f",
					profile.tasks.max_game_parallel_item_name ? profile.tasks.max_game_parallel_item_name : "none",
					profile.tasks.max_game_parallel_item * ticks_to_ms,
					double(profile.tasks.game_parallel_items) / double(profile.frames));
			}
			profile = {};
		}
	}

	if (psLua_ParallelGC_debug && psLua_ParallelGC && Device.LuaGCDebug)
	{
		Device.LuaGCDebug();
	}

#ifdef DEDICATED_SERVER
    u32 FrameEndTime = TimerGlobal.GetElapsed_ms();
    u32 FrameTime = (FrameEndTime - FrameStartTime);
    u32 DSUpdateDelta = 1000 / g_svDedicateServerUpdateReate;
    if (FrameTime < DSUpdateDelta)
        Sleep(DSUpdateDelta - FrameTime);
#endif
	if (!b_is_Active)
		Sleep(1);
}

#ifdef INGAME_EDITOR
void CRenderDevice::message_loop_editor()
{
    m_editor->run();
    m_editor_finalize(m_editor);
    xr_delete(m_engine);
}
#endif // #ifdef INGAME_EDITOR

void CRenderDevice::Screenshot()
{
	PROF_EVENT();
	Render->Screenshot();
}

void CRenderDevice::message_loop()
{
#ifdef INGAME_EDITOR
    if (editor())
    {
        message_loop_editor();
        return;
    }
#endif
	MSG msg;
	PeekMessage(&msg, NULL, 0U, 0U, PM_NOREMOVE);
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
		on_idle();
	}
}

void mt_DiscordThread(void*)
{
	while (true)
	{
		if (!pApp)
		{
			Msg("[Discord] pApp destroyed, killing thread");
			return;
		}

		//Discord
		if (use_discord && psDeviceFlags2.test(rsDiscord))
		{
			START_PROFILE("Discord");
			discord_core->RunCallbacks();
			updateDiscordPresence();
			STOP_PROFILE;
			Sleep(int(discord_update_rate * 1000));
		}
		else
		{
			Sleep(1000); // Sleep for 1 second if Discord is not used or disabled
		}
	}
}

void CRenderDevice::Run()
{
	// DUMP_PHASE;
	g_bLoaded = FALSE;
	Log("Starting engine...");
	thread_name("X-RAY Primary thread");
	// Startup timers and calculate timer delta
	dwTimeGlobal = 0;
	Timer_MM_Delta = 0;
	{
		u32 time_mm = timeGetTime();
		while (timeGetTime() == time_mm); // wait for next tick
		u32 time_system = timeGetTime();
		u32 time_local = TimerAsync();
		Timer_MM_Delta = time_system - time_local;
	}

	// Start extra threads
	thread_spawn(mt_FreezeThread, "Freeze detecting thread", 0, 0);
	thread_spawn(mt_DiscordThread, "X-RAY Discord thread", 0, 0);

	// Message cycle
	CTimer app_start_timer;
	app_start_timer.Start();
	seqAppStart.Process(rp_AppStart);
	Msg("* [STARTUP] app start callbacks: %d ms", app_start_timer.GetElapsed_ms());

	//m_pRender->ClearTarget();
	SetForegroundWindow(m_hWnd);
	message_loop();

	seqAppEnd.Process(rp_AppEnd);

	secondary_tasks.wait();
	ParticleWorkerCallback.clear();
}

u32 app_inactive_time = 0;
u32 app_inactive_time_start = 0;

void CRenderDevice::FrameMove()
{
	PROF_EVENT("Render: Frame Move");

	if (InterlockedExchange(&g_monitor_list_dirty, 0))
		refresh_vid_monitor_list();

	dwFrame++;
	Core.dwFrame = dwFrame;
	dwTimeContinual = TimerMM.GetElapsed_ms() - app_inactive_time;
	if (psDeviceFlags.test(rsConstantFPS))
	{
		PROF_EVENT("Constant FPS");

		// 20ms = 50fps
		//fTimeDelta = 0.020f;
		//fTimeGlobal += 0.020f;
		//dwTimeDelta = 20;
		//dwTimeGlobal += 20;
		// 33ms = 30fps
		fTimeDelta = 0.033f;
		fTimeGlobal += 0.033f;
		dwTimeDelta = 33;
		dwTimeGlobal += 33;
	}
	else
	{
		PROF_EVENT("Timer FPS");

		// Timer
		float fPreviousFrameTime = Timer.GetElapsed_sec();
		Timer.Start(); // previous frame
		fTimeDelta = 0.1f * fTimeDelta + 0.9f * fPreviousFrameTime;
		// smooth random system activity - worst case ~7% error
		//fTimeDelta = 0.7f * fTimeDelta + 0.3f*fPreviousFrameTime; // smooth random system activity
		if (fTimeDelta > .1f)
			fTimeDelta = .1f; // limit to 15fps minimum
		if (fTimeDelta <= 0.f)
			fTimeDelta = EPS_S + EPS_S; // limit to 15fps minimum
		if (Paused())
			fTimeDelta = 0.0f;
		// u64 qTime = TimerGlobal.GetElapsed_clk();
		fTimeGlobal = TimerGlobal.GetElapsed_sec(); //float(qTime)*CPU::cycles2seconds;
		u32 _old_global = dwTimeGlobal;
		dwTimeGlobal = TimerGlobal.GetElapsed_ms();
		dwTimeDelta = dwTimeGlobal - _old_global;
	}

	// Frame move
	Statistic->EngineTOTAL.Begin();

	START_PROFILE("Process seqFrame");
	const bool measure_precache_callbacks = pApp && pApp->LoadSessionMeasurePrecache();
	if (measure_precache_callbacks)
	{
		ProcessPrecacheFrameCallbacks();
		if (dwPrecacheFrame == 1)
			PrintPrecacheFrameCallbackProfiles();
	}
	else if (mt_FrameProfile && mt_FrameProfileDetailed && !dwPrecacheFrame)
		ProcessRuntimeFrameCallbacks();
	else
		Device.seqFrame.Process(rp_Frame);
	STOP_PROFILE;
	
	g_bLoaded = TRUE;
	
	Statistic->EngineTOTAL.End();
}

ENGINE_API BOOL bShowPauseString = TRUE;

void CRenderDevice::Pause(BOOL bOn, BOOL bTimer, BOOL bSound, LPCSTR reason)
{
	PROF_EVENT();

	static int snd_emitters_ = -1;

	if (g_bBenchmark)
		return;
#ifndef DEDICATED_SERVER
	if (bOn)
	{
		if (!Paused())
			bShowPauseString =
#ifdef INGAME_EDITOR
                editor() ? FALSE :
#endif // #ifdef INGAME_EDITOR
#ifdef DEBUG
                !xr_strcmp(reason, "li_pause_key_no_clip") ? FALSE :
#endif // DEBUG
				TRUE;

		if (bTimer && (!g_pGamePersistent || g_pGamePersistent->CanBePaused()))
		{
			g_pauseMngr().Pause(true);
#ifdef DEBUG
            if (!xr_strcmp(reason, "li_pause_key_no_clip"))
                TimerGlobal.Pause(FALSE);
#endif // DEBUG
		}

		if (bSound && ::Sound)
		{
			snd_emitters_ = ::Sound->pause_emitters(true);
#ifdef DEBUG
			// Log("snd_emitters_[true]",snd_emitters_);
#endif // DEBUG
		}
	}
	else
	{
		if (bTimer && g_pauseMngr().Paused())
		{
			fTimeDelta = EPS_S + EPS_S;
			g_pauseMngr().Pause(false);
		}

		if (bSound)
		{
			if (snd_emitters_ > 0) //avoid crash
			{
				snd_emitters_ = ::Sound->pause_emitters(false);
#ifdef DEBUG
				// Log("snd_emitters_[false]",snd_emitters_);
#endif
			}
			else
			{
#ifdef DEBUG
                Log("Sound->pause_emitters underflow");
#endif
			}
		}
	}

#endif
}

bool CRenderDevice::Paused()
{
	return g_pauseMngr().Paused();
}

void CRenderDevice::OnWM_Activate(WPARAM wParam, LPARAM lParam)
{
	u16 fActive = LOWORD(wParam);
	BOOL fMinimized = (BOOL)HIWORD(wParam);
	BOOL bActive = ((fActive != WA_INACTIVE) && (!fMinimized)) ? TRUE : FALSE;

	if (psDeviceFlags2.test(rsAlwaysActive) && g_screenmode != 2)
	{
		Device.b_is_Active = TRUE;

		if (Device.b_hide_cursor != bActive)
		{
			Device.b_hide_cursor = bActive;

			if (Device.b_hide_cursor)
			{
				ShowCursor(FALSE);
				if (m_hWnd)
				{
					RECT winRect;
					GetClientRect(m_hWnd, &winRect);
					MapWindowPoints(m_hWnd, nullptr, reinterpret_cast<LPPOINT>(&winRect), 2);
					ClipCursor(&winRect);
				}
				pInput->OnAppActivate();
			}
			else
			{
				ShowCursor(TRUE);
				ClipCursor(NULL);
				pInput->OnAppDeactivate();
			}
		}

		return;
	}

	if (bActive != Device.b_is_Active)
	{
		Device.b_is_Active = bActive;

		if (Device.b_is_Active)
		{
			Device.seqAppActivate.Process(rp_AppActivate);
			app_inactive_time += TimerMM.GetElapsed_ms() - app_inactive_time_start;

#ifndef DEDICATED_SERVER
# ifdef INGAME_EDITOR
            if (!editor())
# endif // #ifdef INGAME_EDITOR
			ShowCursor(FALSE);
			if (m_hWnd)
			{
				RECT winRect;
				GetClientRect(m_hWnd, &winRect);
				MapWindowPoints(m_hWnd, nullptr, reinterpret_cast<LPPOINT>(&winRect), 2);
				ClipCursor(&winRect);
			}
#endif // #ifndef DEDICATED_SERVER
		}
		else
		{
			app_inactive_time_start = TimerMM.GetElapsed_ms();
			Device.seqAppDeactivate.Process(rp_AppDeactivate);
			ShowCursor(TRUE);
			ClipCursor(NULL);
		}
	}
}

void CRenderDevice::AddSeqFrame(pureFrame* f, bool mt)
{
	PROF_EVENT();

	if (mt)
		seqFrameMT.Add(f, REG_PRIORITY_HIGH);
	else
		seqFrame.Add(f, REG_PRIORITY_LOW);
}

void CRenderDevice::RemoveSeqFrame(pureFrame* f)
{
	PROF_EVENT();

	seqFrameMT.Remove(f);
	seqFrame.Remove(f);
}

CLoadScreenRenderer::CLoadScreenRenderer()
	: b_registered(false)
{
}

void CLoadScreenRenderer::start(bool b_user_input)
{
	PROF_EVENT();

	Device.seqRender.Add(this, 0);
	b_registered = true;
	b_need_user_input = b_user_input;
}

void CLoadScreenRenderer::stop()
{
	PROF_EVENT();

	if (!b_registered)
		return;
	Device.seqRender.Remove(this);
	pApp->destroy_loading_shaders();
	b_registered = false;
	b_need_user_input = false;
}

void CLoadScreenRenderer::OnRender()
{
	PROF_EVENT();

	const bool measure_precache = pApp && pApp->LoadSessionMeasurePrecache();
	const u64 started_at = measure_precache ? CPU::QPC() : 0;
	pApp->load_draw_internal();
	if (measure_precache)
		pApp->LoadSessionRecordPrecacheLoadscreen(CPU::QPC() - started_at);
}

void CRenderDevice::CSecondVPParams::SetSVPActive(bool bState) //--#SM+#-- +SecondVP+
{
	isActive = bState;
	if (g_pGamePersistent != NULL)
		g_pGamePersistent->m_pGShaderConstants->m_blender_mode.z = (isActive ? 1.0f : 0.0f);
}

bool CRenderDevice::CSecondVPParams::IsSVPFrame() //--#SM+#-- +SecondVP+
{
	return IsSVPActive() && Device.dwFrame % frameDelay == 0;
}
