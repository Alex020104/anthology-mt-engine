#pragma once

struct SFrameTaskProfile
{
	u64 pre_render = 0;
	u64 post_transforms = 0;
	u64 calculate_bones = 0;
	u64 game = 0;
	u64 lua_gc = 0;
};

extern ENGINE_API BOOL mt_FrameProfile;

namespace XRay::Engine
{
	void PreRenderThread();
    void PreRenderPostTransformsThread();
	void CalculateBonesThread();
	void GameThread();
	SFrameTaskProfile ConsumeFrameTaskProfile();
}
