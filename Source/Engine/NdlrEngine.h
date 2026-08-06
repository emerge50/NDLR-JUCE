#pragma once

#include <JuceHeader.h>
#include <atomic>

class NdlrEngine
{
public:
    static constexpr int ppqn = 96;

    struct MotifSettings
    {
        bool enabled = true;
        int divisionIndex = 7;
        int length = 16;
        int variation = 0;
        int velocity = 100;
        int midiChannel = 1;
        int pattern = 0;
        int patternAmplitude = 50;
        int patternType = 0;
        int position = 1;
        int rhythmLength = 8;
        std::array<int, 32> rhythmState {};
        std::array<int, 32> rhythmVelocity {};
        std::array<int, 32> rhythmRatchet {};
        int ratchetVelocityMode = 0;
        int rhythmMode = 0;
        int euclideanPulses = 4;
        int rotation = 0;
        bool customPattern = false;
        int customPatternSource = 0;
        std::array<int, 16> patternSteps {};
        int humanize = 0;
        int accent = 1;
        int gate = 102;
    };

    struct HarmonySettings
    {
        int root = 0;
        int scale = 0;
        int degree = 0;
        int chordType = 0;
        int triggerCounter = 0;
    };

    struct TransportSettings
    {
        bool internalPlaying = false;
        double internalTempo = 120.0;
    };
    struct DroneSettings { bool enabled = false; int position = 1, type = 1, trigger = 8, velocity = 90, midiChannel = 2; };
    struct PadSettings { bool enabled=false; int position=50, range=13, spread=4, velocity=100, midiChannel=1; bool strum=false; int strumDivision=16; bool group=false; int polyChain=1; int inversionMode=0; int quantize=0; };
    struct PadVoicing
    {
        int targetNote = 64;
        int centreNote = 64;
        std::array<int, 22> candidates {};
        int candidateCount = 0;
        std::array<int, 22> notes {};
        int noteCount = 0;
    };

    [[nodiscard]] static PadVoicing calculatePadVoicing (
        const HarmonySettings&, const PadSettings&) noexcept;
    [[nodiscard]] static std::array<int, 22> minimisePadVoiceMovement (
        const std::array<int, 22>& notes, int noteCount,
        const std::array<int, 22>& previousNotes, int previousNoteCount) noexcept;
    [[nodiscard]] static std::array<int, 22> rotatePadVoicing (
        const std::array<int, 22>& notes, int noteCount, int rotation) noexcept;

    void prepare (double newSampleRate);
    void reset();
    void panic (juce::MidiBuffer& midi);
    void process (juce::MidiBuffer& midi, int numSamples,
                  const juce::AudioPlayHead::PositionInfo& position,
                  bool hasHostPosition,
                  const TransportSettings& transportSettings,
                  const HarmonySettings& harmonySettings,
                  const MotifSettings& motif1Settings,
                  const MotifSettings& motif2Settings,
                  const DroneSettings& droneSettings,
                  const PadSettings& padSettings);

    [[nodiscard]] double getTempo() const noexcept { return tempo.load(); }
    [[nodiscard]] bool isTransportRunning() const noexcept { return transportRunning.load(); }
    [[nodiscard]] int getMotif1Step() const noexcept { return motif1Runtime.patternStep.load(); }
    [[nodiscard]] int getMotif1Length() const noexcept { return motif1Runtime.patternLength.load(); }
    [[nodiscard]] int getMotif1RhythmStep() const noexcept { return motif1Runtime.rhythmStep.load(); }
    [[nodiscard]] int getMotif2Step() const noexcept { return motif2Runtime.patternStep.load(); }
    [[nodiscard]] int getMotif2Length() const noexcept { return motif2Runtime.patternLength.load(); }
    [[nodiscard]] int getMotif2RhythmStep() const noexcept { return motif2Runtime.rhythmStep.load(); }
    [[nodiscard]] static int motifPatternValue (int pattern, int step) noexcept
    { return getPatternValue (pattern, step); }
    [[nodiscard]] static int motifMidiNote (const HarmonySettings& harmony,
                                            const MotifSettings& motif,
                                            int patternValue) noexcept
    { return getMidiNote (harmony, motif, patternValue); }
    [[nodiscard]] static int applyPatternAmplitude (int value, int amplitude,
                                                     int maximum) noexcept
    {
        maximum = juce::jmax (1, maximum);
        const auto offset = juce::roundToInt (
            static_cast<float> (juce::jlimit (0, 100, amplitude) - 50)
                / 50.0f * static_cast<float> (maximum - 1));
        return juce::jlimit (1, maximum, value + offset);
    }

private:
    struct MotifRuntimeState
    {
        bool noteIsActive = false;
        int activeNote = 60;
        int activeChannel = 1;
        std::atomic<int> patternStep { -1 };
        std::atomic<int> patternLength { 8 };
        std::atomic<int> rhythmStep { -1 };
        int64_t noteCounter = 0;
        int64_t lastRhythmBoundary = -1;
        int pendingRatchets = 0;
        int ratchetNote = 60;
        int ratchetChannel = 1;
        int ratchetVelocity = 100;
        int ratchetTotal = 1;
        int ratchetHit = 1;
        double nextRatchetPpq = 0.0;
        double ratchetIntervalPpq = 0.0;
        bool ratchetTieAfter = false;
        bool scheduledNoteOff = false;
        double scheduledNoteOffPpq = 0.0;
        int scheduledNote = 60;
        int scheduledChannel = 1;

        void reset() noexcept
        {
            noteIsActive = false; activeNote = 60; activeChannel = 1;
            patternStep.store (-1); patternLength.store (8); rhythmStep.store (-1);
            noteCounter = 0; lastRhythmBoundary = -1; pendingRatchets = 0;
            ratchetNote = 60; ratchetChannel = 1; ratchetVelocity = 100;
            ratchetTotal = 1; ratchetHit = 1; nextRatchetPpq = 0.0;
            ratchetIntervalPpq = 0.0; ratchetTieAfter = false;
            scheduledNoteOff = false; scheduledNoteOffPpq = 0.0;
            scheduledNote = 60; scheduledChannel = 1;
        }
    };

    static int getDivisionTicks (int divisionIndex) noexcept;
    static int applyVariation (int64_t stepNumber, int length, int variation) noexcept;
    static int getPatternValue (int pattern, int motifStep) noexcept;
    static int getMidiNote (const HarmonySettings&, const MotifSettings&,
                            int patternValue) noexcept;
    void stopActiveNote (MotifRuntimeState&, juce::MidiBuffer&, int sampleOffset);
    void emitNoteOn (juce::MidiBuffer&, int channel, int note, int velocity, int sampleOffset);
    void emitNoteOff (juce::MidiBuffer&, int channel, int note, int sampleOffset);
    void processMotif (MotifRuntimeState&, juce::MidiBuffer&, int numSamples,
                       double currentPpq, bool isPlaying,
                       const HarmonySettings&, const MotifSettings&);

    double sampleRate = 44100.0;
    std::atomic<double> tempo { 120.0 };
    std::atomic<bool> transportRunning { false };
    double internalPpq = 0.0;
    bool wasInternalPlaying = false;
    std::array<std::array<int, 128>, 16> noteOwners {};
    MotifRuntimeState motif1Runtime;
    MotifRuntimeState motif2Runtime;
    std::array<int, 3> droneNotes {};
    int droneNoteCount = 0;
    int droneActiveChannel = 2;
    int64_t droneLastBoundary = -1;
    std::array<int, 22> padNotes {};
    std::array<int, 22> padChannels {};
    std::array<bool, 22> padNoteSounded {};
    int padNoteCount = 0;
    int padNextStrumNote = 0;
    double padNextStrumPpq = 0.0, padStrumIntervalPpq = 0.0;
    bool padPendingTrigger = false;
    double padPendingTriggerPpq = 0.0;
    int padLastVelocity = -1, padLastChannel = -1, padLastPolyChain = -1;
    int padLastStrumDivision = -1, padLastQuantize = -1;
    int lastHarmonyTriggerCounter = 0;
    bool padLastStrum = false, padLastGroup = false;
    int padLastInversionMode = -1;
};
