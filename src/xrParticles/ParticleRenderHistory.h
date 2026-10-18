#pragma once

#include <array>
#include <cstdint>

// Runtime-only history follows the particle when the pool swaps or resizes it.
// Only presented main-view draws may advance it; shadow/PiP draws cannot.
struct ParticleRenderHistory
{
    using Point = std::array<float, 3>;
    using Quad = std::array<Point, 4>;
    Quad current;
    Quad previous;
    std::uint64_t serial;

    void Reset() { serial = 0; }

    void Advance(std::uint64_t nextSerial, const Quad& quad)
    {
        if (serial == nextSerial && serial != 0)
            return;
        previous = serial != 0 && serial + 1 == nextSerial ? current : quad;
        current = quad;
        serial = nextSerial;
    }
};
