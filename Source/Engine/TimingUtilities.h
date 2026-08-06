#pragma once

#include <cmath>
#include <cstdint>

namespace ndlr::timing
{
template <typename Integer>
[[nodiscard]] constexpr Integer positiveModulo (Integer value, Integer modulus) noexcept
{
    const auto remainder = value % modulus;
    return remainder < 0 ? remainder + modulus : remainder;
}

struct SyncedPhase
{
    double phase = 0.0;
    int64_t cycle = 0;
};

struct OffsetPhase
{
    double phase = 0.0;
    int64_t cycleOffset = 0;
};

[[nodiscard]] inline SyncedPhase phaseFromPpq (double ppq, double periodInQuarters) noexcept
{
    const auto cycles = ppq / periodInQuarters;
    const auto cycle = static_cast<int64_t> (std::floor (cycles));
    return { cycles - static_cast<double> (cycle), cycle };
}

[[nodiscard]] inline OffsetPhase phaseWithOffset (double phase,
                                                  double offsetInCycles) noexcept
{
    // A full-cycle offset is identical to zero (100% == 360 degrees).
    const auto normalizedOffset = offsetInCycles - std::floor (offsetInCycles);
    const auto shifted = phase + normalizedOffset;
    const auto cycleOffset = static_cast<int64_t> (std::floor (shifted));
    return { shifted - static_cast<double> (cycleOffset), cycleOffset };
}

[[nodiscard]] constexpr bool probabilityGate (uint64_t cycle, int probabilityPercent) noexcept
{
    if (probabilityPercent <= 0) return false;
    if (probabilityPercent >= 100) return true;
    return (cycle * 2654435761u) % 100u < static_cast<uint64_t> (probabilityPercent);
}

[[nodiscard]] inline int patternStepFromPhase (double phase, int length) noexcept
{
    length = length < 1 ? 1 : length > 16 ? 16 : length;
    const auto normalized = phase - std::floor (phase);
    const auto step = static_cast<int> (normalized * static_cast<double> (length));
    return step < 0 ? 0 : step >= length ? length - 1 : step;
}
}
