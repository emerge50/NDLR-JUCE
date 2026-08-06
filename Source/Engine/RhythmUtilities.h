#pragma once

#include <array>
#include <algorithm>

namespace ndlr::rhythm
{
using Pattern = std::array<int, 32>;

inline void buildLevel (int level, const std::array<int, 32>& counts,
                        const std::array<int, 33>& remainders, Pattern& output, int& size)
{
    if (level == -1) output[static_cast<size_t> (size++)] = 0;
    else if (level == -2) output[static_cast<size_t> (size++)] = 1;
    else
    {
        for (auto i = 0; i < counts[static_cast<size_t> (level)]; ++i)
            buildLevel (level - 1, counts, remainders, output, size);
        if (remainders[static_cast<size_t> (level)] != 0)
            buildLevel (level - 2, counts, remainders, output, size);
    }
}

inline Pattern euclidean (int steps, int pulses, int rotation)
{
    Pattern result {};
    steps = std::clamp (steps, 1, 32);
    pulses = std::clamp (pulses, 0, steps);
    if (pulses == 0) return result;
    if (pulses == steps)
    {
        std::fill_n (result.begin(), steps, 1);
        return result;
    }

    std::array<int, 32> counts {};
    std::array<int, 33> remainders {};
    remainders[0] = pulses;
    auto divisor = steps - pulses;
    auto level = 0;
    for (;;)
    {
        counts[static_cast<size_t> (level)] = divisor / remainders[static_cast<size_t> (level)];
        remainders[static_cast<size_t> (level + 1)] = divisor % remainders[static_cast<size_t> (level)];
        divisor = remainders[static_cast<size_t> (level++)];
        if (remainders[static_cast<size_t> (level)] <= 1) break;
    }
    counts[static_cast<size_t> (level)] = divisor;

    Pattern raw {};
    auto rawSize = 0;
    buildLevel (level, counts, remainders, raw, rawSize);
    auto firstPulse = 0;
    while (firstPulse < steps && raw[static_cast<size_t> (firstPulse)] == 0) ++firstPulse;
    rotation = ((rotation % steps) + steps) % steps;
    for (auto i = 0; i < steps; ++i)
        result[static_cast<size_t> (i)] = raw[static_cast<size_t> ((firstPulse + i + rotation) % steps)];
    return result;
}
}
