#include "../../src/Layers/xrRender/GeometryVisibility.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

using anthology::geometry::visible;

int main()
{
    // Small static props: every optimization level has a distinct cutoff.
    assert(visible(true, 0, 5, 1000*1000, 67));
    assert(visible(true, 1, 5, 100*100, 67));
    assert(!visible(true, 2, 5, 100*100, 67));
    assert(visible(true, 2, 35, 100*100, 67));
    assert(!visible(true, 3, 35, 100*100, 67));
    assert(visible(true, 3, 5, 40*40, 67));
    assert(!visible(true, 4, 5, 40*40, 67));
    // A boundary is inclusive; narrowing the view keeps the magnified prop.
    assert(visible(true, 4, 5, 35*35, 67));
    assert(!visible(true, 4, 5, 36*36, 67));
    assert(visible(true, 4, 5, 100*100, 20));
    // Root-object bounds keep a visible NPC's head and body together.
    const float npcVolume = 5.f, headVolume = .05f, distanceSq = 45*45;
    assert(visible(false, 4, npcVolume, distanceSq, 67));
    assert(!visible(false, 4, headVolume, distanceSq, 67));
    assert(!visible(false, 4, npcVolume, 120*120, 67));
    assert(visible(false, 1, .5f, 60*60, 67));
    assert(!visible(false, 2, .5f, 60*60, 67));
    // Invalid controls fail open. Higher levels cannot reveal culled props.
    assert(visible(true, -1, 1, 1e8f, 67));
    assert(visible(false, 5, 1, 1e8f, 67));
    int checks = 0;
    for (bool isStatic : {false, true})
        for (float volume : {.01f, .5f, 1.2f, 3.5f, 20.f, 150.f, 800.f, 4500.f, 24000.f, 60000.f})
            for (float fov : {15.f, 40.f, 67.f, 90.f, 110.f})
                for (int distance = 1; distance < 500; distance += 7)
                {
                    bool previous = true;
                    for (int level = 0; level <= 4; ++level)
                    {
                        const bool current = visible(isStatic, level, volume, float(distance*distance), fov);
                        assert(previous || !current);
                        previous = current;
                        ++checks;
                    }
                }
    std::printf("PASS geometry controls: boundaries, FOV, root bounds, %d monotonic checks\n", checks);
}
