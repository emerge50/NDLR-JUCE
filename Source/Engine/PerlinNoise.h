#pragma once

#include <JuceHeader.h>
#include <numeric>

namespace ndlr
{
class PerlinNoise
{
public:
    void setSeed (int newSeed)
    {
        if (newSeed == seed && ready) return;
        seed = newSeed;
        std::array<int, 256> values {};
        std::iota (values.begin(), values.end(), 0);
        uint32_t state = static_cast<uint32_t> (seed * 7919 + 13);
        for (auto i = 255; i > 0; --i)
        {
            state = state * 1664525u + 1013904223u;
            const auto j = static_cast<int> (state % static_cast<uint32_t> (i + 1));
            std::swap (values[static_cast<size_t> (i)], values[static_cast<size_t> (j)]);
        }
        for (auto i = 0; i < 512; ++i) permutation[static_cast<size_t> (i)] = values[static_cast<size_t> (i & 255)];
        ready = true;
    }

    float sample (float x, float y, float persistence, float roughness) const
    {
        auto total = 0.0f, amplitude = 1.0f, frequency = 1.0f, amplitudeSum = 0.0f;
        for (auto octave = 0; octave < 4; ++octave)
        {
            total += noise (x * frequency, y * frequency) * amplitude;
            amplitudeSum += amplitude;
            amplitude *= persistence;
            frequency *= 2.0f;
        }
        return juce::jlimit (0.0f, 1.0f,
            ((juce::jlimit (-1.0f, 1.0f, total / amplitudeSum * roughness)) + 1.0f) * 0.5f);
    }

    int patternValue (int step, int length, int motif, int maximum,
                      float x, float y, float zoom, float spacing,
                      float roughness, float persistence, float brightness) const
    {
        const auto scale = 0.03f + zoom * 0.0045f;
        // La grille Perlin reste ancrée sur les 16 pas du pattern. Pattlen ne
        // doit pas étirer/rééchantillonner la courbe : une réduction conserve
        // le préfixe gauche et retire uniquement les valeurs situées à droite.
        constexpr auto patternCapacity = 16.0f;
        const auto stepSpacing = scale * (32.0f / patternCapacity);
        const auto rowOffset = motif == 0 ? 0.0f : spacing * 0.025f;
        const auto value = sample (x * 0.005f + static_cast<float> (step) * stepSpacing,
                                   y * 0.005f + rowOffset, persistence, roughness) * brightness;
        juce::ignoreUnused (length);
        return juce::jlimit (1, maximum, juce::roundToInt (value * static_cast<float> (maximum)));
    }

private:
    static float fade (float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float grad (int hash, float x, float y)
    {
        const auto h = hash & 7;
        const auto u = h < 4 ? x : y;
        const auto v = h < 4 ? y : x;
        return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -2.0f * v : 2.0f * v);
    }
    float noise (float x, float y) const
    {
        const auto floorX = std::floor (x), floorY = std::floor (y);
        const auto X = static_cast<int> (floorX) & 255, Y = static_cast<int> (floorY) & 255;
        const auto xf = x - floorX, yf = y - floorY, u = fade (xf), v = fade (yf);
        const auto aa = permutation[static_cast<size_t> (X + permutation[static_cast<size_t> (Y)])];
        const auto ab = permutation[static_cast<size_t> (X + permutation[static_cast<size_t> (Y + 1)])];
        const auto ba = permutation[static_cast<size_t> (X + 1 + permutation[static_cast<size_t> (Y)])];
        const auto bb = permutation[static_cast<size_t> (X + 1 + permutation[static_cast<size_t> (Y + 1)])];
        const auto a = juce::jmap (u, grad (aa, xf, yf), grad (ba, xf - 1.0f, yf));
        const auto b = juce::jmap (u, grad (ab, xf, yf - 1.0f), grad (bb, xf - 1.0f, yf - 1.0f));
        return juce::jmap (v, a, b);
    }

    std::array<int, 512> permutation {};
    int seed = -1;
    bool ready = false;
};
}
