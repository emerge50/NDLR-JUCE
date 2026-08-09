#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include "Engine/NdlrEngine.h"
#include "Engine/PerlinNoise.h"

namespace ndlr::modulation
{
enum Destination
{
    none = 0,
    harmonyKey, harmonyMode, harmonyDegree, harmonyChordType,
    padOn, padRegister, padInversion, padVoicing, padStrum, padVelocity,
    droneOn, dronePosition, droneType, droneTrigger, droneVelocity,
    motif1On, motif1Position, motif1Pattern, motif1ActivePattern, motif1Length, motif1Variation,
    motif1Division, motif1Velocity, motif1Gate, motif1Accent, motif1Rhythm,
    motif2On, motif2Position, motif2Pattern, motif2ActivePattern, motif2Length, motif2Variation,
    motif2Division, motif2Velocity, motif2Gate, motif2Accent, motif2Rhythm,
    perlinZoom, perlinSpacing, perlinRoughness, perlinPersistence,
    perlinSeed, perlinResolution, perlinBrightness,
    midiCc,
    count
};
}

class NdlrAudioProcessor final : public juce::AudioProcessor,
                                 private juce::Timer
{
public:
    NdlrAudioProcessor();
    ~NdlrAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    [[nodiscard]] NdlrEngine& getEngine() noexcept { return engine; }
    [[nodiscard]] juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    [[nodiscard]] float getModulationValue (int slot) const noexcept
    { return modulationVisualization[static_cast<size_t> (juce::jlimit (0, 7, slot))].load(); }
    [[nodiscard]] bool isModulationTransportRunning() const noexcept
    { return modulationTransportRunning.load(); }
    [[nodiscard]] uint64_t getLfoRestartCount (int lfo) const noexcept
    { return modulationRestartCount[static_cast<size_t> (juce::jlimit (0, 2, lfo))].load(); }
    [[nodiscard]] bool getModulationPatternValues (
        int slot, std::array<float, 16>& destination, int& length) const noexcept;
    [[nodiscard]] bool isDestinationModulated (int destination) const noexcept
    { return modulationDestinationActive[static_cast<size_t> (
        juce::jlimit (0, ndlr::modulation::midiCc - 1, destination))].load(); }
    [[nodiscard]] int getModulatedDestinationValue (int destination) const noexcept
    { return modulationDestinationValues[static_cast<size_t> (
        juce::jlimit (0, ndlr::modulation::midiCc - 1, destination))].load(); }
    void requestPanic() noexcept { panicRequested.store (true); }
    struct RhythmMemory
    {
        bool used = false;
        int length = 8, mode = 0, pulses = 4, rotation = 0, division = 7, ratchetVelocityMode = 0;
        std::array<int, 32> state {}, velocity {}, ratchet {};
    };
    void refreshRhythmMemoryCache();
    [[nodiscard]] bool getRhythmMemory (int slot, RhythmMemory& destination);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void timerCallback() override;
    void cacheParameterPointers();
    static void migrateStateTree (juce::ValueTree&);

    struct MotifParameterRefs
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* division = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* variation = nullptr;
        std::atomic<float>* velocity = nullptr;
        std::atomic<float>* channel = nullptr;
        std::atomic<float>* pattern = nullptr;
        std::atomic<float>* patternType = nullptr;
        std::atomic<float>* position = nullptr;
        std::atomic<float>* rhythmLength = nullptr;
        std::atomic<float>* rhythmSlot = nullptr;
        std::atomic<float>* ratchetVelocityMode = nullptr;
        std::atomic<float>* rhythmMode = nullptr;
        std::atomic<float>* euclideanPulses = nullptr;
        std::atomic<float>* rotation = nullptr;
        std::atomic<float>* humanize = nullptr;
        std::atomic<float>* accent = nullptr;
        std::atomic<float>* gate = nullptr;
        std::atomic<float>* customPattern = nullptr;
        std::atomic<float>* customPatternSource = nullptr;
        std::array<std::atomic<float>*, 16> patternSteps {};
        std::array<std::atomic<float>*, 32> rhythmStates {};
        std::array<std::atomic<float>*, 32> rhythmVelocities {};
        std::array<std::atomic<float>*, 32> rhythmRatchets {};
    };

    struct LfoParameterRefs
    {
        std::atomic<float>* shape = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* sync = nullptr;
        std::atomic<float>* retrigger = nullptr;
        std::atomic<float>* division = nullptr;
        std::atomic<float>* probability = nullptr;
        std::atomic<float>* pulseWidth = nullptr;
        std::atomic<float>* length = nullptr;
        std::atomic<float>* phase = nullptr;
        std::atomic<float>* amplitudeSource = nullptr;
        std::atomic<float>* amplitude = nullptr;
        std::atomic<float>* amplitudeCcNumber = nullptr;
        std::atomic<float>* amplitudeCcChannel = nullptr;
    };

    struct ModSlotParameterRefs
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* source = nullptr;
        std::atomic<float>* destination = nullptr;
        std::atomic<float>* ccNumber = nullptr;
        std::atomic<float>* ccChannel = nullptr;
        std::atomic<float>* amount = nullptr;
        std::atomic<float>* low = nullptr;
        std::atomic<float>* high = nullptr;
    };

    NdlrEngine engine;
    ndlr::PerlinNoise perlinNoise;
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* transportPlay = nullptr;
    std::atomic<float>* transportTempo = nullptr;
    std::atomic<float>* harmonyRoot = nullptr;
    std::atomic<float>* harmonyScale = nullptr;
    std::atomic<float>* harmonyDegree = nullptr;
    std::atomic<float>* harmonyType = nullptr;
    std::atomic<float>* harmonyKeyMidiChannel = nullptr;
    std::atomic<float>* harmonyDegreeMidiChannel = nullptr;
    std::array<MotifParameterRefs, 2> motifParameters {};
    std::array<std::array<std::atomic<float>*, 16>, 20> sharedPatternSteps {};
    std::array<LfoParameterRefs, 3> lfoParameters {};
    std::array<ModSlotParameterRefs, 8> modSlotParameters {};
    std::atomic<float>* droneOn = nullptr;
    std::atomic<float>* dronePosition = nullptr;
    std::atomic<float>* droneType = nullptr;
    std::atomic<float>* droneTrigger = nullptr;
    std::atomic<float>* droneVelocity = nullptr;
    std::atomic<float>* droneChannel = nullptr;
    std::atomic<float>* padOn = nullptr;
    std::atomic<float>* padVoicing = nullptr;
    std::atomic<float>* padRegister = nullptr;
    std::atomic<float>* padVelocity = nullptr;
    std::atomic<float>* padChannel = nullptr;
    std::atomic<float>* padStrum = nullptr;
    std::atomic<float>* padStrumDivision = nullptr;
    std::atomic<float>* padGroup = nullptr;
    std::atomic<float>* padInversionMode = nullptr;
    std::atomic<float>* padQuantize = nullptr;
    std::atomic<float>* perlinOn = nullptr;
    std::atomic<float>* perlinM1 = nullptr;
    std::atomic<float>* perlinM2 = nullptr;
    std::atomic<float>* perlinSeed = nullptr;
    std::atomic<float>* perlinX = nullptr;
    std::atomic<float>* perlinY = nullptr;
    std::atomic<float>* perlinZoom = nullptr;
    std::atomic<float>* perlinSpacing = nullptr;
    std::atomic<float>* perlinBrightness = nullptr;
    std::atomic<float>* perlinRoughness = nullptr;
    std::atomic<float>* perlinPersistence = nullptr;
    std::atomic<float>* perlinResolution = nullptr;
    std::atomic<bool> panicRequested { false };
    std::atomic<int> pendingMidiKey { -1 }, pendingMidiDegree { -1 };
    std::atomic<int> harmonyTriggerCounter { 0 };
    std::array<std::array<int, 128>, 16> incomingCcValues {};
    std::array<int, 16> incomingPitchBendValues {};
    std::array<int, 16> incomingAftertouchValues {};
    int incomingMidiVelocity = 0;
    std::array<double, 3> modulationPhase {};
    std::array<float, 3> modulationRandom { 0.5f, 0.5f, 0.5f };
    std::array<uint64_t, 3> modulationCycle {};
    bool modulationHostTransportWasPlaying = false;
    bool modulationInternalTransportWasPlaying = false;
    double modulationLastProcessTimeMs = 0.0;
    std::atomic<bool> modulationTransportRunning { false };
    std::array<std::atomic<uint64_t>, 3> modulationRestartCount {};
    std::array<std::atomic<float>, 8> modulationVisualization {};
    std::array<std::atomic<int>, ndlr::modulation::midiCc> modulationDestinationValues {};
    std::array<std::atomic<bool>, ndlr::modulation::midiCc> modulationDestinationActive {};
    std::array<int, 8> lastMidiCcValue { -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<int, 8> lastMidiCcNumber { -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<int, 8> lastMidiCcChannel { -1, -1, -1, -1, -1, -1, -1, -1 };
    std::mutex rhythmMemoryMutex;
    std::array<RhythmMemory, 40> rhythmMemoryCache {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NdlrAudioProcessor)
};
