#pragma once

#include <algorithm>

namespace ndlr::pattern
{
[[nodiscard]] inline float normalizeValue (int value, int maximum) noexcept
{
    maximum = std::max (2, maximum);
    value = std::max (1, std::min (maximum, value));
    return static_cast<float> (value - 1) / static_cast<float> (maximum - 1);
}
}
