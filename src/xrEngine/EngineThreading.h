#pragma once

struct SFrameTaskProfile
{
	u64 pre_render = 0;
	u64 post_transforms = 0;
	u64 calculate_bones = 0;
	u64 game = 0;
	u64 game_scheduler = 0;
	u64 game_parallel = 0;
	u64 game_frame_mt = 0;
	u64 lua_gc = 0;
	u64 vision = 0;
	u64 lua_gc_calls = 0;
	u64 lua_gc_skipped_busy = 0;
	u64 lua_gc_skipped_postload = 0;
	u64 game_parallel_items = 0;
	u64 max_pre_render = 0;
	u64 max_post_transforms = 0;
	u64 max_calculate_bones = 0;
	u64 max_game = 0;
	u64 max_game_scheduler = 0;
	u64 max_game_parallel = 0;
	u64 max_game_parallel_item = 0;
	LPCSTR max_game_parallel_item_name = nullptr;
	u64 max_game_frame_mt = 0;
	u64 max_lua_gc = 0;
	u64 max_vision = 0;
};

extern ENGINE_API BOOL mt_FrameProfile;
extern ENGINE_API BOOL mt_FrameProfileDetailed;

namespace XRay::Engine
{
	// Original CoP scripted root-motion scenes must update their animation
	// controller before any worker calculates the corresponding skeleton. The
	// counter is non-zero only while one of those temporary controllers exists.
	ENGINE_API bool IsOriginalCoPCinematicObjectName(LPCSTR name);
	ENGINE_API u32 RegisterOriginalCoPCinematicController();
	ENGINE_API u32 UnregisterOriginalCoPCinematicController();
	ENGINE_API bool OriginalCoPCinematicControllerActive();

	void PreRenderThread();
    void PreRenderPostTransformsThread();
	void CalculateBonesThread();
	void GameThread();
	u64 BeginLuaGCTaskProfile();
	void EndLuaGCTaskProfile(u64 started_at);
	u64 BeginVisionTaskProfile();
	void EndVisionTaskProfile(u64 started_at);
	SFrameTaskProfile ConsumeFrameTaskProfile();
}
