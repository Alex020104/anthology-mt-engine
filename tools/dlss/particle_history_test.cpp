#include "../../src/xrParticles/ParticleRenderHistory.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using History = ParticleRenderHistory;
History::Quad quad(float x)
{
    return {{{x,0,0}, {x,1,0}, {x+1,0,0}, {x+1,1,0}}};
}
int main()
{
    History a;
    std::memset(&a, 0xA5, sizeof(a)); // xr_alloc storage is not value-initialized.
    a.Reset();
    a.Advance(10, quad(1));
    assert(a.previous == quad(1));
    a.Advance(11, quad(2));
    assert(a.previous == quad(1));
    a.Advance(11, quad(3)); // A second render pass cannot advance history.
    assert(a.previous == quad(1) && a.current == quad(2));
    a.Advance(13, quad(5)); // Visibility gap: reject stale geometry.
    assert(a.previous == quad(5));
    std::vector<History> pool(2);
    pool[0] = a;
    pool[1].Reset(); pool[1].Advance(13, quad(20));
    pool[0] = pool.back(); pool.pop_back(); // Actual particle-pool removal policy.
    pool.reserve(200); // Actual resize preserves the moved particle's identity.
    pool[0].Advance(14, quad(21));
    assert(pool[0].previous == quad(20));
    pool[0].Reset(); pool[0].Advance(15, quad(40)); // Reused slot: no old trails.
    assert(pool[0].previous == quad(40));
    // Skipping Advance for shadow/PiP draws preserves the main-view epoch.
    pool[0].Advance(16, quad(41));
    assert(pool[0].previous == quad(40));
    std::cout << "Particle history lifecycle PASS\n";
}
