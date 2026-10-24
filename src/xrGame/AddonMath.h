#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

namespace addon_math
{
struct Random
{
    std::uint32_t state;
    explicit Random(std::uint32_t seed) : state(seed ? seed : 0x9e3779b9u) {}
    unsigned next(unsigned bound)
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return bound ? state % bound : 0;
    }
};
struct Sudoku
{
    std::array<unsigned,81> solution{};
    std::array<bool,81> given{};
};
inline Sudoku sudoku(unsigned difficulty, std::uint32_t seed)
{
    Sudoku board;
    Random random(seed);
    std::array<unsigned,9> digits{{1,2,3,4,5,6,7,8,9}};
    for (unsigned i=8;i>0;--i) std::swap(digits[i],digits[random.next(i+1)]);
    for(unsigned x=0;x<9;++x)
        for(unsigned y=0;y<9;++y)
        { board.solution[x*9+y]=digits[(x*3+x/3+y)%9]; board.given[x*9+y]=true; }
    const unsigned hidden = difficulty==1 ? 3 : (difficulty==3 ? 5 : 4);
    for(unsigned bx=0;bx<3;++bx)
        for(unsigned by=0;by<3;++by)
        {
            std::array<unsigned,9> cells{{0,1,2,3,4,5,6,7,8}};
            for(unsigned i=0;i<hidden;++i)
            {
                std::swap(cells[i],cells[i+random.next(9-i)]);
                const unsigned x=bx*3+cells[i]/3,y=by*3+cells[i]%3;
                board.given[x*9+y]=false;
            }
        }
    return board;
}
inline std::array<unsigned,16> puzzle(std::uint32_t seed)
{
    Random random(seed);
    std::array<unsigned,16> cells{};
    for(unsigned i=0;i<16;++i) cells[i]=i;
    unsigned blank=15, previous=16;
    for(unsigned moves=0;moves<256;++moves)
    {
        unsigned options[4], count=0;
        if(blank>=4 && blank-4!=previous) options[count++]=blank-4;
        if(blank<12 && blank+4!=previous) options[count++]=blank+4;
        if(blank%4 && blank-1!=previous) options[count++]=blank-1;
        if(blank%4<3 && blank+1!=previous) options[count++]=blank+1;
        const unsigned chosen=options[random.next(count)];
        std::swap(cells[blank],cells[chosen]); previous=blank;blank=chosen;
        if(moves>=29 && blank%4!=3 && blank/4!=3) break;
    }
    // A legal move sequence guarantees solvability, including this bounded fallback.
    if(blank/4==3) { std::swap(cells[blank],cells[blank-4]);blank-=4; }
    if(blank%4==3) std::swap(cells[blank],cells[blank-1]);
    return cells;
}
inline float clamp(float x, float lo, float hi) { return std::max(lo, std::min(x, hi)); }
inline float compass_x(float dx, float dz, float cx, float cz, float previous, float dt)
{
    const float dl = std::sqrt(dx * dx + dz * dz);
    const float cl = std::sqrt(cx * cx + cz * cz);
    if (dl < 0.0001f || cl < 0.0001f) return 512.f;
    dx /= dl; dz /= dl; cx /= cl; cz /= cl;
    const float dot = clamp(dx * cx + dz * cz, -1.f, 1.f);
    const float side = dz * cx - dx * cz < 0.f ? 1.f : -1.f;
    const float target = 512.f + side * (1.f - dot) * 220.f;
    const float response = std::fabs(target - previous) > 100.f ? 0.5f : 0.9f;
    // Preserve the old response at 30 Hz without making it frame-rate dependent.
    const float blend = 1.f - std::pow(response, clamp(dt, 0.f, 0.25f) / 0.033f);
    return clamp(previous + (target - previous) * blend, 212.f, 812.f);
}
inline unsigned fade(float distance, float limit, float range, bool always)
{
    if (always || distance <= limit) return 255;
    if (range <= 0.f) return 0;
    return static_cast<unsigned>(clamp(255.f * (1.f - (distance - limit) / range), 0.f, 255.f));
}
}
