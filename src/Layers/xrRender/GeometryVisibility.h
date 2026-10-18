#pragma once

namespace anthology { namespace geometry {

// Legacy Monolith volume/distance limits, indexed by optimization level 1..4.
// Level 0 disables this distance cut; it does not disable frustum/SSA culling.
struct Limit { float volume, distance; };
static constexpr Limit staticLimits[4][5] = {
    {{10,150}, {100,200}, {500,250}, {2500,350}, {7000,400}},
    {{25,50}, {200,150}, {1000,200}, {2500,300}, {7000,400}},
    {{50,50}, {400,150}, {1500,200}, {5000,300}, {20000,350}},
    {{50,35}, {500,125}, {1750,175}, {5250,250}, {25000,300}}
};
static constexpr Limit dynamicLimits[4][3] = {
    {{1,80}, {3,150}, {4000,250}},
    {{1,40}, {4,100}, {4000,200}},
    {{1.4f,30}, {4,80}, {4000,150}},
    {{2,30}, {8,50}, {4000,110}}
};

inline bool visible(bool isStatic, int level, float volume, float distanceSquared, float fov)
{
    if (level < 1 || level > 4 || !(fov > 0.f))
        return true;
    const float fovScale = fov / 67.f;
    const float adjustedSquared = distanceSquared * fovScale * fovScale;
    const Limit* limits = isStatic ? staticLimits[level - 1] : dynamicLimits[level - 1];
    const int count = isStatic ? 5 : 3;
    for (int i = 0; i < count; ++i)
        if (volume < limits[i].volume && adjustedSquared > limits[i].distance * limits[i].distance)
            return false;
    return true;
}

}} // namespace anthology::geometry
