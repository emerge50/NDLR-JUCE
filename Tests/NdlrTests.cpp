#include <JuceHeader.h>

#include "Engine/HarmonyData.h"
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

void testMotifLengthsResynchroniseDuringPlayback()
{
    NdlrEngine engine;
    engine.prepare (48000.0);
    auto motif1 = enabledMotif();
    NdlrEngine::MotifSettings motif2;
    motif2.enabled = false;

    motif1.length = 5;
    motif1.rhythmLength = 4;
    for (auto ppq = 0; ppq < 3; ++ppq)
    {
        juce::MidiBuffer block;
        engine.process (block, 512, hostPosition (static_cast<double> (ppq)), true,
                        {}, {}, motif1, motif2, {}, {});
    }
    expect (engine.getMotif1Step() == 2,
            "the pattern must advance independently through sounding rhythm steps");

    motif1.rhythmLength = 7;
    juce::MidiBuffer rhythmLengthChange;
    engine.process (rhythmLengthChange, 512, hostPosition (3.0), true,
                    {}, {}, motif1, motif2, {}, {});
    expect (engine.getMotif1Step() == 0,
            "changing rhythm length during Play must resynchronise the pattern phase");
    expect (engine.getMotif1RhythmStep() == 3 && engine.getMotif1Length() == 5,
            "rhythm and pattern lengths must remain independent after resynchronising");

    motif1.length = 3;
    juce::MidiBuffer patternLengthChange;
    engine.process (patternLengthChange, 512, hostPosition (4.0), true,
                    {}, {}, motif1, motif2, {}, {});
    expect (engine.getMotif1Step() == 0,
            "changing pattern length during Play must resynchronise with the rhythm");
    expect (engine.getMotif1RhythmStep() == 4 && engine.getMotif1Length() == 3,
            "resynchronising must not copy one motif length into the other");
}

void testDronePositionMatchesDisplayedRegister()
{
    for (auto position = 0; position < 4; ++position)
    {
        NdlrEngine engine;
        engine.prepare (48000.0);
        NdlrEngine::DroneSettings drone;
        drone.enabled = true;
        drone.position = position;
        drone.type = 1; // Root + octave spans the complete displayed register.
        drone.trigger = 1;

        juce::MidiBuffer midi;
        engine.process (midi, 512, hostPosition (0.0), true,
                        {}, {}, {}, {}, drone, {});

        std::array<int, 2> noteOns { -1, -1 };
        auto noteOnCount = 0;
        for (const auto metadata : midi)
            if (metadata.getMessage().isNoteOn() && noteOnCount < 2)
                noteOns[static_cast<size_t> (noteOnCount++)] =
                    metadata.getMessage().getNoteNumber();

        const auto expectedRoot = 24 + position * 12;
        expect (noteOnCount == 2
                    && noteOns[0] == expectedRoot
                    && noteOns[1] == expectedRoot + 12,
                "drone positions must match the displayed C1-B2 through C4-B5 registers");
    }
}

void testMotifAccentsMatchNdlrMax()
{
    static constexpr std::array<std::array<int, 8>, 7> accentPatterns {{
        {{1,0,0,0,0,0,0,0}}, {{1,0,0,0,0,0,0,0}}, {{1,0,1,0,0,0,0,0}},
        {{1,0,0,0,0,0,0,0}}, {{1,1,0,0,0,0,0,0}}, {{1,0,0,1,0,0,0,0}},
        {{1,0,1,0,1,0,0,0}}
    }};
    static constexpr std::array<int, 7> accentLengths { 2, 4, 6, 3, 4, 6, 8 };

    const auto velocitiesForChannels = [] (const juce::MidiBuffer& midi)
    {
        std::array<int, 2> velocities { -1, -1 };
        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn() && message.getChannel() >= 1 && message.getChannel() <= 2)
                velocities[static_cast<size_t> (message.getChannel() - 1)] =
                    message.getVelocity();
        }
        return velocities;
    };

    for (auto accent = 1; accent <= 10; ++accent)
    {
        NdlrEngine engine;
        engine.prepare (48000.0);
        auto motif1 = enabledMotif();
        auto motif2 = enabledMotif();
        motif1.midiChannel = 1;
        motif2.midiChannel = 2;
        motif1.accent = motif2.accent = accent;
        motif1.velocity = motif2.velocity = 100;
        motif1.humanize = motif2.humanize = 0;
        motif1.rhythmVelocity.fill (64);
        motif2.rhythmVelocity.fill (64);

        const auto stepsToCheck = accent >= 4
            ? accentLengths[static_cast<size_t> (accent - 4)] : 1;
        for (auto step = 0; step < stepsToCheck; ++step)
        {
            juce::MidiBuffer midi;
            engine.process (midi, 512, hostPosition (static_cast<double> (step)), true,
                            {}, {}, motif1, motif2, {}, {});
            const auto velocities = velocitiesForChannels (midi);
            const auto expected = accent <= 2 ? 50
                                : accent == 3 ? 100
                                : accentPatterns[static_cast<size_t> (accent - 4)]
                                                [static_cast<size_t> (step)] != 0
                                    ? 100 : 55;
            expect (velocities[0] == expected && velocities[1] == expected,
                    "Motif 1 and 2 accent modes must match NDLR-Max");
        }
    }

    NdlrEngine engine;
    engine.prepare (48000.0);
    auto motif1 = enabledMotif();
    auto motif2 = enabledMotif();
    motif1.midiChannel = 1;
    motif2.midiChannel = 2;
    motif1.accent = motif2.accent = 2;
    motif1.humanize = motif2.humanize = 10;
    motif1.velocity = motif2.velocity = 100;
    motif1.rhythmVelocity.fill (10);
    motif2.rhythmVelocity.fill (10);
    auto motifsProducedDifferentHumanize = false;
    for (auto step = 0; step < 16; ++step)
    {
        juce::MidiBuffer midi;
        engine.process (midi, 512, hostPosition (static_cast<double> (step)), true,
                        {}, {}, motif1, motif2, {}, {});
        const auto velocities = velocitiesForChannels (midi);
        motifsProducedDifferentHumanize = motifsProducedDifferentHumanize
            || velocities[0] != velocities[1];
        expect (velocities[0] >= 1 && velocities[0] <= 16
                    && velocities[1] >= 1 && velocities[1] <= 16,
                "humanized accents must scale jitter from the effective rhythm velocity");
    }
    expect (motifsProducedDifferentHumanize,
            "Motif 1 and 2 humanize sequences must be independent like NDLR-Max");
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

void testPadVoicingInversionAndRegister()
{
    expect (ndlr::harmony::padVoicingNames.size() == 12
                && juce::String (ndlr::harmony::padVoicingNames.front()) == "None"
                && juce::String (ndlr::harmony::padVoicingNames.back()) == "Voicing Lock",
            "Pad must expose the 12 NDLR-Max voicing profiles in order");
    expect (ndlr::harmony::padInversionNames.size() == 7
                && juce::String (ndlr::harmony::padInversionNames.front()) == "No Inversion"
                && juce::String (ndlr::harmony::padInversionNames.back())
                    == "Octaves & Inverted 3rds",
            "Pad must expose the 7 NDLR-Max inversion profiles in order");

    NdlrEngine::HarmonySettings harmony;
    NdlrEngine::PadSettings pad;
    const auto root = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (root.noteCount == 3 && root.notes[0] == 48
                && root.notes[1] == 52 && root.notes[2] == 55,
            "None voicing must match the compact NDLR-Max root voicing");

    harmony.chordType = 1; // Cmaj7.
    pad.voicing = 1;
    const auto dynamicMaj7 = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (dynamicMaj7.noteCount == 5
                && dynamicMaj7.notes[0] == 36
                && dynamicMaj7.notes[1] == 55
                && dynamicMaj7.notes[2] == 60
                && dynamicMaj7.notes[3] == 64
                && dynamicMaj7.notes[4] == 71,
            "Cmaj7 Dynamic must voice C1 G2 C3 E3 B3 like NDLR-Max");

    harmony.chordType = 2; // Cmaj9.
    const auto dynamicMaj9 = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (dynamicMaj9.noteCount == 6
                && dynamicMaj9.notes[0] == 36
                && dynamicMaj9.notes[1] == 55
                && dynamicMaj9.notes[2] == 60
                && dynamicMaj9.notes[3] == 64
                && dynamicMaj9.notes[4] == 71
                && dynamicMaj9.notes[5] == 74,
            "Cmaj9 Dynamic must voice C1 G2 C3 E3 B3 D4 like NDLR-Max");

    harmony.chordType = 4; // Cmaj13.
    const auto dynamicMaj13 = NdlrEngine::calculatePadVoicing (harmony, pad);
    static constexpr std::array<int, 8> expectedMaj13 {
        36, 55, 60, 64, 69, 71, 74, 77
    };
    expect (dynamicMaj13.noteCount == static_cast<int> (expectedMaj13.size()),
            "Cmaj13 Dynamic must retain all chord tones");
    for (auto i = 0; i < dynamicMaj13.noteCount; ++i)
        expect (dynamicMaj13.notes[static_cast<size_t> (i)]
                    == expectedMaj13[static_cast<size_t> (i)],
                "Cmaj13 Dynamic must place the 13th on A3");

    harmony.chordType = 0;
    pad.voicing = 3;
    const auto open1 = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (open1.noteCount == 3 && open1.notes[0] == 48
                && open1.notes[1] == 55 && open1.notes[2] == 64,
            "Open 1 Voicing must raise alternating inner voices");

    pad.voicing = 0;
    pad.inversion = 1;
    const auto firstInversion = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (firstInversion.noteCount == 3 && firstInversion.notes[0] == 52
                && firstInversion.notes[1] == 55 && firstInversion.notes[2] == 60,
            "1st Inversion must be applied before voice grouping");

    pad.inversion = 2;
    const auto secondInversion = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (secondInversion.noteCount == 3 && secondInversion.notes[0] == 55
                && secondInversion.notes[1] == 60 && secondInversion.notes[2] == 64,
            "2nd Inversion must voice G2 C3 E3");

    pad.inversion = 3;
    const auto drop2 = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (drop2.noteCount == 3 && drop2.notes[0] == 52
                && drop2.notes[1] == 60 && drop2.notes[2] == 67,
            "Drop 2 inversion must voice E2 C3 G3");

    pad.inversion = 4;
    const auto drop2And3 = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (drop2And3.noteCount == 3 && drop2And3.notes[0] == 48
                && drop2And3.notes[1] == 52 && drop2And3.notes[2] == 67,
            "Drop 2 & 3 inversion must voice C2 E2 G3");

    harmony.chordType = 1;
    pad.inversion = 5;
    const auto seventhsInverted = NdlrEngine::calculatePadVoicing (harmony, pad);
    static constexpr std::array<int, 5> expectedSeventhsInverted { 36, 48, 55, 59, 64 };
    expect (seventhsInverted.noteCount
                == static_cast<int> (expectedSeventhsInverted.size()),
            "7ths Inverted must retain its five defining voices");
    for (auto i = 0; i < seventhsInverted.noteCount; ++i)
        expect (seventhsInverted.notes[static_cast<size_t> (i)]
                    == expectedSeventhsInverted[static_cast<size_t> (i)],
                "7ths Inverted must voice C1 C2 G2 B2 E3");

    harmony.chordType = 0;
    pad.inversion = 6;
    const auto octavesAndThirds = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (octavesAndThirds.noteCount == 4
                && octavesAndThirds.notes[0] == 36
                && octavesAndThirds.notes[1] == 48
                && octavesAndThirds.notes[2] == 64
                && octavesAndThirds.notes[3] == 72,
            "Octaves & Inverted 3rds must voice C1 C2 E3 C4");

    pad.inversion = 1;
    pad.registerOctaves = 1;
    const auto raised = NdlrEngine::calculatePadVoicing (harmony, pad);
    expect (raised.noteCount == firstInversion.noteCount
                && raised.notes[0] == firstInversion.notes[0] + 12
                && raised.notes[2] == firstInversion.notes[2] + 12,
            "Register must transpose the complete final voicing by octaves");

    for (auto voicing = 0; voicing < 12; ++voicing)
        for (auto inversion = 0; inversion < 7; ++inversion)
            for (auto registerOctaves = -2; registerOctaves <= 2; ++registerOctaves)
            {
                pad.voicing = voicing;
                pad.inversion = inversion;
                pad.registerOctaves = registerOctaves;
                const auto result = NdlrEngine::calculatePadVoicing (harmony, pad);
                expect (result.noteCount > 0,
                        "every Pad voicing/inversion/register combination must produce notes");
                for (auto i = 0; i < result.noteCount; ++i)
                {
                    expect (result.notes[static_cast<size_t> (i)] >= 0
                                && result.notes[static_cast<size_t> (i)] <= 127,
                            "Pad voicing notes must remain in the MIDI range");
                    if (i > 0)
                        expect (result.notes[static_cast<size_t> (i - 1)]
                                    < result.notes[static_cast<size_t> (i)],
                                "Pad voicing notes must remain ordered and unique");
                }
            }
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

void testOriginalNdlrScalesAndNdlrMaxColours()
{
    expect (ndlr::harmony::scales.size() == 15,
            "the harmony catalogue must expose the 15 original NDLR scales");
    expect (ndlr::harmony::colourNames.size() == 9,
            "the harmony catalogue must expose all 9 NDLR-Max colours");

    static constexpr std::array<const char*, 15> expectedNames {{
        "Major", "Dorian", "Phrygian", "Lydian", "Mixolydian",
        "Minor (Aeolian)", "Locrian", "Gypsy Min", "Harmonic Minor",
        "Minor Pentatonic", "Whole Tone", "Tonic 2nds", "Tonic 3rds",
        "Tonic 4ths", "Tonic 6ths"
    }};
    static constexpr std::array<std::array<int, 8>, 15> expectedIntervals {{
        {{ 0, 2, 4, 5, 7, 9, 11, -1 }},
        {{ 0, 2, 3, 5, 7, 9, 10, -1 }},
        {{ 0, 1, 3, 5, 7, 8, 10, -1 }},
        {{ 0, 2, 4, 6, 7, 9, 11, -1 }},
        {{ 0, 2, 4, 5, 7, 9, 10, -1 }},
        {{ 0, 2, 3, 5, 7, 8, 10, -1 }},
        {{ 0, 1, 3, 5, 6, 8, 10, -1 }},
        {{ 0, 2, 3, 6, 7, 8, 10, -1 }},
        {{ 0, 2, 3, 5, 7, 8, 11, -1 }},
        {{ 0, 3, 5, 7, 10, -1, -1, -1 }},
        {{ 0, 2, 4, 6, 8, 10, -1, -1 }},
        {{ 0, 2, -1, -1, -1, -1, -1, -1 }},
        {{ 0, 4, -1, -1, -1, -1, -1, -1 }},
        {{ 0, 5, -1, -1, -1, -1, -1, -1 }},
        {{ 0, 9, -1, -1, -1, -1, -1, -1 }}
    }};
    static constexpr std::array<int, 15> expectedLengths {
        7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 6, 2, 2, 2, 2
    };
    for (auto i = 0; i < static_cast<int> (expectedNames.size()); ++i)
    {
        expect (juce::String (ndlr::harmony::scaleNames[static_cast<size_t> (i)])
                    == expectedNames[static_cast<size_t> (i)],
                "the original NDLR scale names and order must be preserved");
        expect (ndlr::harmony::scales[static_cast<size_t> (i)].intervals
                    == expectedIntervals[static_cast<size_t> (i)]
                    && ndlr::harmony::scales[static_cast<size_t> (i)].length
                        == expectedLengths[static_cast<size_t> (i)],
                "the original NDLR scale intervals must be preserved");
    }
    expect (juce::String (ndlr::harmony::colourNames.back())
                == "Suspended 4ths, 7ths & 9ths",
            "the NDLR-Max colour labels must be preserved");

    struct Formula
    {
        std::array<int, 7> degrees {};
        int length = 0;
        bool chromatic = false;
    };

    const auto expectedFormula = [] (int scaleLength, int colour)
    {
        if (colour == 7)
            return Formula { {{ 0, 3, 7, 10, 14, 0, 0 }}, 5, true };
        if (colour == 8)
            return Formula { {{ 0, 5, 7, 10, 14, 0, 0 }}, 5, true };

        if (scaleLength == 7)
        {
            static constexpr std::array<Formula, 7> formulas {{
                Formula { {{ 0, 2, 4, 0, 0, 0, 0 }}, 3 },
                Formula { {{ 0, 2, 4, 6, 0, 0, 0 }}, 4 },
                Formula { {{ 0, 2, 4, 6, 8, 0, 0 }}, 5 },
                Formula { {{ 0, 2, 4, 6, 8, 10, 0 }}, 6 },
                Formula { {{ 0, 2, 4, 6, 8, 10, 12 }}, 7 },
                Formula { {{ 0, 3, 4, 0, 0, 0, 0 }}, 3 },
                Formula { {{ 0, 2, 4, 5, 6, 0, 0 }}, 5 }
            }};
            return formulas[static_cast<size_t> (colour)];
        }

        if (scaleLength == 6)
        {
            static constexpr std::array<Formula, 7> formulas {{
                Formula { {{ 0, 2, 4, 0, 0, 0, 0 }}, 3 },
                Formula { {{ 0, 2, 4, 5, 0, 0, 0 }}, 4 },
                Formula { {{ 0, 2, 4, 5, 7, 0, 0 }}, 5 },
                Formula { {{ 0, 1, 2, 3, 4, 5, 0 }}, 6 },
                Formula { {{ 0, 1, 2, 3, 4, 5, 7 }}, 7 },
                Formula { {{ 0, 3, 4, 0, 0, 0, 0 }}, 3 },
                Formula { {{ 0, 2, 4, 5, 0, 0, 0 }}, 4 }
            }};
            return formulas[static_cast<size_t> (colour)];
        }

        if (scaleLength == 2)
            return Formula { {{ 0, 1, 0, 0, 0, 0, 0 }}, 2 };

        static constexpr std::array<Formula, 7> formulas {{
            Formula { {{ 0, 1, 3, 0, 0, 0, 0 }}, 3 },
            Formula { {{ 0, 1, 3, 4, 0, 0, 0 }}, 4 },
            Formula { {{ 0, 1, 3, 4, 5, 0, 0 }}, 5 },
            Formula { {{ 0, 1, 2, 3, 4, 0, 0 }}, 5 },
            Formula { {{ 0, 1, 2, 3, 4, 5, 0 }}, 6 },
            Formula { {{ 0, 2, 3, 0, 0, 0, 0 }}, 3 },
            Formula { {{ 0, 1, 3, 4, 0, 0, 0 }}, 4 }
        }};
        return formulas[static_cast<size_t> (colour)];
    };

    NdlrEngine::MotifSettings motif;
    motif.patternType = 0;
    motif.position = 2;

    for (auto scaleIndex = 0; scaleIndex < static_cast<int> (ndlr::harmony::scales.size());
         ++scaleIndex)
    {
        const auto& scale = ndlr::harmony::scales[static_cast<size_t> (scaleIndex)];
        for (auto degree = 0; degree < 7; ++degree)
        {
            for (auto colour = 0; colour < 9; ++colour)
            {
                NdlrEngine::HarmonySettings harmony;
                harmony.scale = scaleIndex;
                harmony.degree = degree;
                harmony.chordType = colour;

                std::array<bool, 12> actual {};
                for (auto voice = 1; voice <= 7; ++voice)
                    actual[static_cast<size_t> (
                        NdlrEngine::motifMidiNote (harmony, motif, voice) % 12)] = true;

                std::array<bool, 12> expected {};
                const auto formula = expectedFormula (scale.length, colour);
                for (auto i = 0; i < formula.length; ++i)
                {
                    auto pitchClass = 0;
                    if (formula.chromatic)
                    {
                        pitchClass = scale.intervals[static_cast<size_t> (
                            degree % scale.length)]
                            + formula.degrees[static_cast<size_t> (i)];
                    }
                    else
                    {
                        const auto rawIndex = degree
                            + formula.degrees[static_cast<size_t> (i)];
                        pitchClass = scale.intervals[static_cast<size_t> (
                            rawIndex % scale.length)];
                    }
                    expected[static_cast<size_t> (pitchClass % 12)] = true;
                }

                expect (actual == expected,
                        "every scale, degree and colour must match NDLR-Max chord_builder.js");
            }
        }
    }
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
    testMotifLengthsResynchroniseDuringPlayback();
    testDronePositionMatchesDisplayedRegister();
    testMotifAccentsMatchNdlrMax();
    testPadQuantize();
    testPadVoicingInversionAndRegister();
    testSevenStepScaleMotifDoesNotWrapEarly();
    testChordMotifContinuesAcrossOctaves();
    testOriginalNdlrScalesAndNdlrMaxColours();
    testPadChordInversionMinimisesMovement();
    testPadAutoInversionRespectsPositionRegister();
    testPadFixedChordInversions();
    testPerlinPatternLengthTrimsFromTheRight();
    testActivePatternAmplitude();
    if (failures == 0)
        std::cout << "All NDLR regression tests passed\n";
    return failures == 0 ? 0 : 1;
}
