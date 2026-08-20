#pragma once

struct SFrameTaskProfile
{
	u64 pre_render = 0;
	u64 post_transforms = 0;
	u64 calculate_bones = 0;
	u64 game = 0;
	u64 lua_gc = 0;
	u64 vision = 0;
	u64 max_pre_render = 0;
	u64 max_post_transforms = 0;
	u64 max_calculate_bones = 0;
	u64 max_game = 0;
	u64 max_lua_gc = 0;
	u64 max_vision = 0;
};

extern ENGINE_API BOOL mt_FrameProfile;

namespace XRay::Engine
{
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
