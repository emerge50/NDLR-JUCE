#include <JuceHeader.h>

#include "Engine/NdlrEngine.h"
#include "Engine/PatternUtilities.h"
#include "Engine/PerlinNoise.h"
#include "Engine/TimingUtilities.h"

#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void expect (bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

juce::AudioPlayHead::PositionInfo hostPosition (double ppq, double bpm = 120.0)
{
    juce::AudioPlayHead::PositionInfo position;
    position.setPpqPosition (ppq);
    position.setBpm (bpm);
    position.setIsPlaying (true);
    return position;
}

NdlrEngine::MotifSettings enabledMotif()
{
    NdlrEngine::MotifSettings settings;
    settings.enabled = true;
    settings.divisionIndex = 7;
    settings.rhythmLength = 8;
    settings.rhythmState.fill (1);
    settings.rhythmVelocity.fill (100);
    settings.rhythmRatchet.fill (1);
    return settings;
}

void testTimingUtilities()
{
    expect (ndlr::timing::positiveModulo (-1, 8) == 7,
            "negative indices must wrap into the rhythm array");
    expect (ndlr::timing::positiveModulo (9, 8) == 1,
            "positive indices must wrap normally");

    const auto positive = ndlr::timing::phaseFromPpq (35.0, 32.0);
    expect (positive.cycle == 1 && std::abs (positive.phase - 0.09375) < 1.0e-12,
            "host-synced phase must follow PPQ");
    const auto negative = ndlr::timing::phaseFromPpq (-0.25, 1.0);
    expect (negative.cycle == -1 && std::abs (negative.phase - 0.75) < 1.0e-12,
            "host-synced phase must remain normalized during preroll");

    const auto offset = ndlr::timing::phaseWithOffset (0.80, 0.25);
    expect (offset.cycleOffset == 1 && std::abs (offset.phase - 0.05) < 1.0e-12,
            "LFO phase offset must wrap and advance the effective cycle");
    const auto fullCycleOffset = ndlr::timing::phaseWithOffset (0.37, 1.0);
    expect (fullCycleOffset.cycleOffset == 0
                && std::abs (fullCycleOffset.phase - 0.37) < 1.0e-12,
            "a 100-percent LFO phase offset must equal zero percent");

    expect (ndlr::timing::patternStepFromPhase (0.0, 7) == 0
                && ndlr::timing::patternStepFromPhase (0.999, 7) == 6,
            "an LFO pattern must traverse only its active steps");
    expect (ndlr::timing::patternStepFromPhase (0.999, 16) == 15,
            "the default LFO pattern length must keep all sixteen steps active");
    expect (std::abs (ndlr::pattern::normalizeValue (9, 16) - 8.0f / 15.0f) < 1.0e-6f
                && ndlr::pattern::normalizeValue (16, 16) == 1.0f,
            "LFO patterns above value eight must remain inside their adaptive range");

    for (uint64_t cycle = 0; cycle < 256; ++cycle)
    {
        expect (! ndlr::timing::probabilityGate (cycle, 0),
                "zero-percent probability must always be closed");
        expect (ndlr::timing::probabilityGate (cycle, 100),
                "hundred-percent probability must always be open");
    }
}

void testNegativeHostPreroll()
{
    NdlrEngine engine;
    engine.prepare (48000.0);
    juce::MidiBuffer midi;
    auto motif1 = enabledMotif();
    NdlrEngine::MotifSettings motif2;
    motif2.enabled = false;
    engine.process (midi, 512, hostPosition (-1.01), true,
                    {}, {}, motif1, motif2, {}, {});

    const auto rhythmStep = engine.getMotif1RhythmStep();
    expect (rhythmStep >= 0 && rhythmStep < motif1.rhythmLength,
            "preroll must never publish or index a negative rhythm step");
    for (const auto metadata : midi)
        expect (metadata.samplePosition >= 0 && metadata.samplePosition < 512,
                "preroll MIDI events must stay inside the current block");
}

void testPadQuantize()
{
    NdlrEngine engine;
    engine.prepare (48000.0);
    NdlrEngine::MotifSettings motif1, motif2;
    motif1.enabled = motif2.enabled = false;
    NdlrEngine::PadSettings pad;
    pad.enabled = true;
    pad.quantize = 1; // Quarter note.

    juce::MidiBuffer beforeBoundary;
    engine.process (beforeBoundary, 512, hostPosition (0.25), true,
                    {}, {}, motif1, motif2, {}, pad);
    expect (beforeBoundary.isEmpty(),
            "quarter-note quantize must defer a pad triggered between grid lines");

    juce::MidiBuffer crossingBoundary;
    engine.process (crossingBoundary, 512, hostPosition (0.99), true,
                    {}, {}, motif1, motif2, {}, pad);
    auto noteOnCount = 0;
    auto firstNoteOnSample = 512;
    for (const auto metadata : crossingBoundary)
        if (metadata.getMessage().isNoteOn())
        {
            ++noteOnCount;
            firstNoteOnSample = juce::jmin (firstNoteOnSample, metadata.samplePosition);
        }
    expect (noteOnCount > 0, "quantized pad must fire when the grid boundary enters the block");
    expect (firstNoteOnSample >= 200 && firstNoteOnSample <= 280,
            "quantized pad onset must be sample-accurate inside the block");
}

void testPadRangeExpansion()
{
    NdlrEngine::HarmonySettings harmony;
    NdlrEngine::PadSettings pad;
    pad.position = 50;
    pad.range = 12;
    const auto range12 = NdlrEngine::calculatePadVoicing (harmony, pad);

    pad.range = 22;
    const auto range22 = NdlrEngine::calculatePadVoicing (harmony, pad);

    expect (range12.candidateCount == 12,
            "pad Range 12 must expose twelve candidate notes");
    expect (range22.candidateCount == 22,
            "pad Range 22 must expose twenty-two candidate notes");
    expect (range22.candidates[21] - range22.candidates[0]
                > range12.candidates[11] - range12.candidates[0],
            "pad Range 22 must cover a wider keyboard span than Range 12");
}

void testSevenStepScaleMotifDoesNotWrapEarly()
{
    NdlrEngine::HarmonySettings harmony;
    harmony.root = 4;  // E in the circle-of-fifths key ordering.
    harmony.scale = 0; // Major.

    NdlrEngine::MotifSettings motif;
    motif.patternType = 1; // Scale.
    motif.position = 2;    // Register 0.

    static constexpr std::array<int, 7> expected {
        64, 66, 68, 69, 71, 73, 75
    };
    for (auto degree = 1; degree <= 7; ++degree)
        expect (NdlrEngine::motifMidiNote (harmony, motif, degree)
                    == expected[static_cast<size_t> (degree - 1)],
                "a 1..7 Scale motif must play all seven degrees without wrapping");

    expect (NdlrEngine::motifMidiNote (harmony, motif, 8) == 76,
            "the eighth Scale value must continue at the tonic one octave above");
}

void testChordMotifContinuesAcrossOctaves()
{
    NdlrEngine::HarmonySettings harmony;
    harmony.root = 0;      // C.
    harmony.scale = 0;     // Major.
    harmony.degree = 0;    // I.
    harmony.chordType = 0; // Triad.

    NdlrEngine::MotifSettings motif;
    motif.patternType = 0; // Chord.
    motif.position = 2;    // Register 0.

    static constexpr std::array<int, 7> expected {
        48, 52, 55, 60, 64, 67, 72
    };
    for (auto chordTone = 1; chordTone <= 7; ++chordTone)
        expect (NdlrEngine::motifMidiNote (harmony, motif, chordTone)
                    == expected[static_cast<size_t> (chordTone - 1)],
                "Chord values must continue through successive octaves without wrapping");
}

void testPadChordInversionMinimisesMovement()
{
    std::array<int, 22> chord {};
    chord[0] = 60;
    chord[1] = 64;
    chord[2] = 67;
    std::array<int, 22> previous {};
    previous[0] = 64;
    previous[1] = 67;
    previous[2] = 72;

    const auto inverted = NdlrEngine::minimisePadVoiceMovement (
        chord, 3, previous, 3);
    expect (inverted[0] == 64 && inverted[1] == 67 && inverted[2] == 72,
            "Pad Invert must rotate the chord into the nearest previous voicing");
    expect (inverted[0] < inverted[1] && inverted[1] < inverted[2],
            "Pad Invert must preserve a strictly ascending, duplicate-free voicing");
}

void testPadAutoInversionRespectsPositionRegister()
{
    std::array<int, 22> previous {};
    previous[0] = 60;
    previous[1] = 64;
    previous[2] = 67;
    std::array<int, 22> requested {};
    requested[0] = 72;
    requested[1] = 76;
    requested[2] = 79;

    const auto moved = NdlrEngine::minimisePadVoiceMovement (
        requested, 3, previous, 3);
    const auto previousRegister = previous[0] + previous[1] + previous[2];
    const auto movedRegister = moved[0] + moved[1] + moved[2];

    expect (movedRegister >= previousRegister + 12,
            "Pad AUTO must follow POS instead of cancelling a register change");
    expect (moved[0] < moved[1] && moved[1] < moved[2],
            "Pad AUTO must keep the position-adjusted voicing strictly ascending");
}

void testPadFixedChordInversions()
{
    std::array<int, 22> chord {};
    chord[0] = 60;
    chord[1] = 64;
    chord[2] = 67;

    const auto first = NdlrEngine::rotatePadVoicing (chord, 3, 1);
    expect (first[0] == 64 && first[1] == 67 && first[2] == 72,
            "Pad 1ST inversion must move the root above the other chord tones");

    const auto second = NdlrEngine::rotatePadVoicing (chord, 3, 2);
    expect (second[0] == 55 && second[1] == 60 && second[2] == 64,
            "Pad 2ND inversion must keep the fifth in the bass near the original register");
}

void testPerlinPatternLengthTrimsFromTheRight()
{
    ndlr::PerlinNoise noise;
    noise.setSeed (73);
    std::array<int, 16> fullPattern {};
    for (auto step = 0; step < 16; ++step)
        fullPattern[static_cast<size_t> (step)] = noise.patternValue (
            step, 16, 0, 60, 241.0f, 617.0f, 42.0f, 19.0f,
            2.1f, 0.47f, 0.86f);

    for (auto reducedLength = 1; reducedLength < 16; ++reducedLength)
        for (auto step = 0; step < reducedLength; ++step)
            expect (noise.patternValue (step, reducedLength, 0, 60,
                                        241.0f, 617.0f, 42.0f, 19.0f,
                                        2.1f, 0.47f, 0.86f)
                        == fullPattern[static_cast<size_t> (step)],
                    "reducing Perlin pattlen must preserve the left-hand values");
}

void testActivePatternAmplitude()
{
    expect (NdlrEngine::applyPatternAmplitude (9, 50, 20) == 9,
            "50-percent active-pattern amplitude must preserve every step");
    expect (NdlrEngine::applyPatternAmplitude (1, 100, 20) == 20
            && NdlrEngine::applyPatternAmplitude (9, 100, 20) == 20,
            "100-percent active-pattern amplitude must push every step to the maximum");
    expect (NdlrEngine::applyPatternAmplitude (1, 0, 20) == 1
            && NdlrEngine::applyPatternAmplitude (20, 0, 20) == 1,
            "zero active-pattern amplitude must push every step to the minimum");
}
}

int main()
{
    testTimingUtilities();
    testNegativeHostPreroll();
    testPadQuantize();
    testPadRangeExpansion();
    testSevenStepScaleMotifDoesNotWrapEarly();
    testChordMotifContinuesAcrossOctaves();
    testPadChordInversionMinimisesMovement();
    testPadAutoInversionRespectsPositionRegister();
    testPadFixedChordInversions();
    testPerlinPatternLengthTrimsFromTheRight();
    testActivePatternAmplitude();
    if (failures == 0)
        std::cout << "All NDLR regression tests passed\n";
    return failures == 0 ? 0 : 1;
}
