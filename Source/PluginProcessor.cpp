#include "PluginProcessor.h"
#include "Engine/PatternUtilities.h"
#include "PluginEditor.h"
#include "Engine/TimingUtilities.h"

namespace
{
constexpr auto parameterNodeType = "PARAM";
constexpr auto parameterIdProperty = "id";
constexpr auto parameterValueProperty = "value";
constexpr auto modDestinationOrderProperty = "modDestinationOrderVersion";
constexpr auto currentModDestinationOrderVersion = 2;
constexpr auto lfoRateUnitProperty = "lfoRateUnitVersion";
constexpr auto currentLfoRateUnitVersion = 1;
constexpr auto padInversionModeProperty = "padInversionModeVersion";
constexpr auto currentPadInversionModeVersion = 1;
constexpr auto minimumLfoRateHz = 0.01f;
constexpr auto maximumLfoRateHz = 20.0f;
constexpr auto defaultLfoRateHz = 1.0f;

juce::ValueTree findParameterNode (const juce::ValueTree& state, const juce::String& id)
{
    return state.getChildWithProperty (parameterIdProperty, id);
}

float readStateParameter (const juce::ValueTree& state, const juce::String& id, float fallback)
{
    const auto node = findParameterNode (state, id);
    return node.isValid() ? static_cast<float> (node.getProperty (parameterValueProperty, fallback))
                          : fallback;
}

void writeStateParameter (juce::ValueTree& state, const juce::String& id, float value)
{
    auto node = findParameterNode (state, id);
    if (! node.isValid())
    {
        node = juce::ValueTree { parameterNodeType };
        node.setProperty (parameterIdProperty, id, nullptr);
        state.appendChild (node, nullptr);
    }
    node.setProperty (parameterValueProperty, value, nullptr);
}
}

NdlrAudioProcessor::NdlrAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "NDLRState", createParameterLayout())
{
    incomingPitchBendValues.fill (8192);
    parameters.state.setProperty (modDestinationOrderProperty,
                                  currentModDestinationOrderVersion, nullptr);
    parameters.state.setProperty (lfoRateUnitProperty,
                                  currentLfoRateUnitVersion, nullptr);
    parameters.state.setProperty (padInversionModeProperty,
                                  currentPadInversionModeVersion, nullptr);
    cacheParameterPointers();
    startTimerHz (30);
}

NdlrAudioProcessor::~NdlrAudioProcessor()
{
    stopTimer();
}

bool NdlrAudioProcessor::getModulationPatternValues (
    int slot, std::array<float, 16>& destination, int& length) const noexcept
{
    const auto slotIndex = static_cast<size_t> (juce::jlimit (0, 7, slot));
    const auto& slotRefs = modSlotParameters[slotIndex];
    if (slotRefs.on->load() < 0.5f
        || juce::roundToInt (slotRefs.destination->load()) < 1)
        return false;

    const auto source = juce::roundToInt (slotRefs.source->load());
    if (source < 1 || source > 3)
        return false;

    const auto lfoIndex = static_cast<size_t> (source - 1);
    const auto& lfoRefs = lfoParameters[lfoIndex];
    const auto shape = juce::roundToInt (lfoRefs.shape->load());
    if (shape < 7)
        return false;

    // A modulated pattern is no longer a static sequence: its amplitude
    // changes over time. Let the scope use the live post-modulation value
    // instead of drawing the unmodulated stepped template.
    const auto amplitudeSource = juce::roundToInt (lfoRefs.amplitudeSource->load());
    const auto amplitudeModulationActive = lfoRefs.amplitude->load() > 0.0f
        && amplitudeSource >= 1 && amplitudeSource <= 7
        && amplitudeSource != source;
    if (amplitudeModulationActive)
        return false;

    const auto patternIndex = juce::jlimit (0, 39, shape - 7);
    length = juce::jlimit (1, 16, juce::roundToInt (lfoRefs.length->load()));
    const auto amount = slotRefs.amount->load() / 100.0f;
    std::array<int, 16> values {};
    auto maximum = 2;
    for (auto step = 0; step < length; ++step)
    {
        const auto value = patternIndex < 20
            ? NdlrEngine::motifPatternValue (patternIndex, step)
            : juce::roundToInt (
                sharedPatternSteps[static_cast<size_t> (patternIndex - 20)]
                                  [static_cast<size_t> (step)]->load());
        values[static_cast<size_t> (step)] = value;
        maximum = juce::jmax (maximum, value);
    }
    for (auto step = 0; step < length; ++step)
    {
        const auto raw = ndlr::pattern::normalizeValue (
            values[static_cast<size_t> (step)], maximum);
        destination[static_cast<size_t> (step)] = (raw * 2.0f - 1.0f) * amount;
    }
    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout NdlrAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "transport.play", 1 }, "Internal Transport Play", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "transport.tempo", 1 }, "Internal Tempo",
        juce::NormalisableRange<float> { 20.0f, 300.0f, 0.1f }, 120.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "harmony.root", 1 }, "Harmony Root",
        juce::StringArray { "C", "G", "D", "A", "E", "B", "F#", "Db", "Ab", "Eb", "Bb", "F" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "harmony.scale", 1 }, "Harmony Scales",
        juce::StringArray { "Major", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian",
                            "Gypsy Min", "Harm. Minor", "Minor Penta", "Whole Tone", "Tonic 2nds",
                            "Tonic 3rds", "Tonic 4ths", "Tonic 6ths", "Major Penta", "Blues",
                            "Melodic Min", "Dorian b2", "Lydian Aug", "Lydian Dom", "Mixo b6",
                            "Locrian #2", "Altered", "Phryg. Dom", "Byzantine", "Dim H-W", "Augmented" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "harmony.degree", 1 }, "Harmony Degree", 1, 7, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "harmony.type", 1 }, "Harmony Colors",
        juce::StringArray { "Triad", "7th", "sus2", "sus4", "6th", "mu", "add9", "9th", "quartal",
                            "power", "shell", "quintal", "11th", "13th", "cluster", "shell9", "open9", "So What" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "harmony.keyMidiChannel", 1 }, "Key MIDI Input Channel", 1, 16, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "harmony.degreeMidiChannel", 1 }, "Degree MIDI Input Channel", 1, 16, 15));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "motif1.on", 1 }, "Motif 1 On", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif1.division", 1 }, "Motif 1 Division",
        juce::StringArray { "1nd", "1n", "1nt", "2nd", "2n", "2nt", "4nd", "4n", "4nt",
                            "8nd", "8n", "8nt", "16nd", "16n", "16nt", "32nd", "32n",
                            "32nt", "64nd", "64n", "128n" }, 7));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.length", 1 }, "Motif 1 Pattern Length", 1, 16, 8));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif1.pattern", 1 }, "Motif 1 Pattern",
        juce::StringArray { "Up-Down", "Third Jumps", "Root Pivot", "Rising Zigzag",
                            "High Drone", "Cascade", "Odds-Evens", "Alberti", "Chord Staircase",
                            "Bass + Chord", "Double Echo", "Mirror", "Clave", "Rhythmic",
                            "Pentatonic Feel", "Alternating Pairs", "Spiral", "Linked Steps",
                            "Fourth Jumps", "Rotating", "User 21", "User 22", "User 23", "User 24",
                            "User 25", "User 26", "User 27", "User 28", "User 29", "User 30",
                            "User 31", "User 32", "User 33", "User 34", "User 35", "User 36",
                            "User 37", "User 38", "User 39", "User 40" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif1.patternType", 1 }, "Motif 1 Pattern Type",
        juce::StringArray { "CHORD", "SCALE", "CHROMATIC" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif1.position", 1 }, "Motif 1 Register",
        juce::StringArray { "-2", "-1", "0", "1", "2" }, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "motif1.customPattern", 1 }, "Motif 1 Custom Pattern", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.customPatternSource", 1 }, "Motif 1 Custom Pattern Source", 1, 20, 1));
    for (auto step = 0; step < 16; ++step)
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "motif1.patternStep" + juce::String (step + 1), 1 },
            "Motif 1 Pattern Step " + juce::String (step + 1), 1, 60,
            NdlrEngine::motifPatternValue (0, step)));
    for (auto slot = 0; slot < 20; ++slot)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "motif1.user" + juce::String (slot + 1) + ".used", 1 },
            "Motif 1 User " + juce::String (slot + 21) + " Used", false));
        for (auto step = 0; step < 16; ++step)
            layout.add (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { "motif1.user" + juce::String (slot + 1)
                                    + ".step" + juce::String (step + 1), 1 },
                "Motif 1 User " + juce::String (slot + 21) + " Step " + juce::String (step + 1),
                1, 60, NdlrEngine::motifPatternValue (0, step)));
    }
    // Banque User commune aux deux motifs. Les anciennes banques motif1/2
    // restent déclarées pour pouvoir migrer les projets existants.
    for (auto slot = 0; slot < 20; ++slot)
    {
        const auto prefix = "pattern.user" + juce::String (slot + 1) + ".";
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { prefix + "used", 1 },
            "Shared User " + juce::String (slot + 21) + " Used", false));
        for (auto step = 0; step < 16; ++step)
            layout.add (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { prefix + "step" + juce::String (step + 1), 1 },
                "Shared User " + juce::String (slot + 21) + " Step " + juce::String (step + 1),
                1, 60, NdlrEngine::motifPatternValue (0, step)));
    }
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.rhythmLength", 1 }, "Motif 1 Rhythm Length", 4, 32, 8));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.rhythmSlot", 1 }, "Motif 1 Rhythm Slot", 1, 40, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif1.ratchetVelocityMode", 1 }, "Motif 1 Ratchet Velocity",
        juce::StringArray { "Equal", "Rising", "Falling" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif1.rhythmMode", 1 }, "Motif 1 Rhythm Mode",
        juce::StringArray { "Normal", "Euclidean" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.euclideanPulses", 1 }, "Motif 1 Euclidean Beats", 1, 32, 4));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.rotation", 1 }, "Motif 1 Rhythm Rotation", -31, 31, 0));
    for (auto step = 0; step < 32; ++step)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "motif1.rhythmState" + juce::String (step + 1), 1 },
            "Motif 1 Rhythm Step " + juce::String (step + 1),
            juce::StringArray { "REST", "NOTE", "TIE" }, 1));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "motif1.rhythmVelocity" + juce::String (step + 1), 1 },
            "Motif 1 Rhythm Velocity " + juce::String (step + 1), 1, 127, 100));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "motif1.rhythmRatchet" + juce::String (step + 1), 1 },
            "Motif 1 Rhythm Ratchet " + juce::String (step + 1), 1, 4, 1));
    }
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif1.variation", 1 }, "Motif 1 Variation",
        juce::StringArray { "Forward", "Reverse", "Ping-pong", "Ping-pong hold",
                            "Odds / evens", "Random" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.velocity", 1 }, "Motif 1 Velocity", 1, 127, 100));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.humanize", 1 }, "Motif 1 Humanize", 0, 10, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.accent", 1 }, "Motif 1 Accent", 1, 10, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.gate", 1 }, "Motif 1 Gate", 5, 127, 102));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif1.channel", 1 }, "Motif 1 MIDI Channel", 1, 16, 1));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "motif2.on", 1 }, "Motif 2 On", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif2.pattern", 1 }, "Motif 2 Pattern",
        juce::StringArray { "Up-Down", "Third Jumps", "Root Pivot", "Rising Zigzag",
                            "High Drone", "Cascade", "Odds-Evens", "Alberti", "Chord Staircase",
                            "Bass + Chord", "Double Echo", "Mirror", "Clave", "Rhythmic",
                            "Pentatonic Feel", "Alternating Pairs", "Spiral", "Linked Steps",
                            "Fourth Jumps", "Rotating", "User 21", "User 22", "User 23", "User 24",
                            "User 25", "User 26", "User 27", "User 28", "User 29", "User 30",
                            "User 31", "User 32", "User 33", "User 34", "User 35", "User 36",
                            "User 37", "User 38", "User 39", "User 40" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.length", 1 }, "Motif 2 Pattern Length", 1, 16, 8));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif2.patternType", 1 }, "Motif 2 Pattern Type",
        juce::StringArray { "CHORD", "SCALE", "CHROMATIC" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif2.position", 1 }, "Motif 2 Register",
        juce::StringArray { "-2", "-1", "0", "1", "2" }, 3));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "motif2.customPattern", 1 }, "Motif 2 Custom Pattern", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.customPatternSource", 1 }, "Motif 2 Custom Pattern Source", 1, 20, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.rhythmLength", 1 }, "Motif 2 Rhythm Length", 4, 32, 8));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.rhythmSlot", 1 }, "Motif 2 Rhythm Slot", 1, 40, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif2.rhythmMode", 1 }, "Motif 2 Rhythm Mode",
        juce::StringArray { "Normal", "Euclidean" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.euclideanPulses", 1 }, "Motif 2 Euclidean Beats", 1, 32, 4));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.rotation", 1 }, "Motif 2 Rotation", -31, 31, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif2.division", 1 }, "Motif 2 Division",
        juce::StringArray { "1nd", "1n", "1nt", "2nd", "2n", "2nt", "4nd", "4n", "4nt",
                            "8nd", "8n", "8nt", "16nd", "16n", "16nt", "32nd", "32n", "32nt",
                            "64nd", "64n", "128n" }, 7));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif2.ratchetVelocityMode", 1 }, "Motif 2 Ratchet Velocity",
        juce::StringArray { "Equal", "Rising", "Falling" }, 0));
    for (auto step = 0; step < 16; ++step)
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "motif2.patternStep" + juce::String (step + 1), 1 },
            "Motif 2 Pattern Step " + juce::String (step + 1), 1, 60,
            NdlrEngine::motifPatternValue (0, step)));
    for (auto slot = 0; slot < 20; ++slot)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "motif2.user" + juce::String (slot + 1) + ".used", 1 },
            "Motif 2 User " + juce::String (slot + 21) + " Used", false));
        for (auto step = 0; step < 16; ++step)
            layout.add (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { "motif2.user" + juce::String (slot + 1)
                                    + ".step" + juce::String (step + 1), 1 },
                "Motif 2 User " + juce::String (slot + 21) + " Step " + juce::String (step + 1),
                1, 60, NdlrEngine::motifPatternValue (0, step)));
    }
    for (auto step = 0; step < 32; ++step)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "motif2.rhythmState" + juce::String (step + 1), 1 },
            "Motif 2 Rhythm Step " + juce::String (step + 1), juce::StringArray { "REST", "NOTE", "TIE" }, 1));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "motif2.rhythmVelocity" + juce::String (step + 1), 1 },
            "Motif 2 Rhythm Velocity " + juce::String (step + 1), 1, 127, 100));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { "motif2.rhythmRatchet" + juce::String (step + 1), 1 },
            "Motif 2 Rhythm Ratchet " + juce::String (step + 1), 1, 4, 1));
    }
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "motif2.variation", 1 }, "Motif 2 Variation",
        juce::StringArray { "Forward", "Reverse", "Ping-pong", "Ping-pong hold",
                            "Odds / evens", "Random" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.velocity", 1 }, "Motif 2 Velocity", 1, 127, 100));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.humanize", 1 }, "Motif 2 Humanize", 0, 10, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.accent", 1 }, "Motif 2 Accent", 1, 10, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.gate", 1 }, "Motif 2 Gate", 5, 127, 102));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "motif2.channel", 1 }, "Motif 2 MIDI Channel", 1, 16, 2));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "drone.on", 1 }, "Drone On", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "drone.position", 1 }, "Drone Position",
        juce::StringArray { "C1-B2", "C2-B3", "C3-B4", "C4-B5" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "drone.type", 1 }, "Drone Type",
        juce::StringArray { "Root", "Root+Oct", "Root+5", "Root+5+Oct" }, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "drone.trigger", 1 }, "Drone Trigger", 1, 19, 9));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "drone.velocity", 1 }, "Drone Velocity", 1, 127, 90));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "drone.channel", 1 }, "Drone MIDI Channel", 1, 16, 2));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "pad.on", 1 }, "Pad On", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pad.position", 1 }, "Pad Position", 0, 100, 50));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pad.range", 1 }, "Pad Range", 1, 22, 13));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pad.spread", 1 }, "Pad Spread", 1, 6, 4));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pad.velocity", 1 }, "Pad Velocity", 1, 127, 100));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pad.channel", 1 }, "Pad MIDI Channel", 1, 16, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "pad.strum", 1 }, "Pad Strum", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pad.strumDivision", 1 }, "Pad Strum Division",
        juce::StringArray { "1nd","1n","1nt","2nd","2n","2nt","4nd","4n","4nt","8nd","8n","8nt","16nd","16n","16nt","32nd","32n","32nt","64nd","64n","128n" }, 16));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "pad.group", 1 }, "Pad Strum Group", false));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "pad.polyChain", 1 }, "Pad Poly Chain", 1, 4, 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "pad.invert", 1 }, "Pad Chord Invert", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "pad.inversionMode", 1 }, "Pad Chord Inversion Mode",
        juce::StringArray { "ROOT", "1ST", "2ND", "AUTO" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pad.quantize", 1 }, "Pad Quantize", juce::StringArray { "Off", "1/4", "1/8" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "perlin.on", 1 }, "Perlin On", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "perlin.m1", 1 }, "Perlin Motif 1", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "perlin.m2", 1 }, "Perlin Motif 2", true));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.x", 1 }, "Perlin X", 1, 1000, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.y", 1 }, "Perlin Y", 1, 1000, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.zoom", 1 }, "Perlin Zoom", 1, 100, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.spacing", 1 }, "Perlin Spacing", 0, 100, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.roughness", 1 }, "Perlin Roughness", 1, 100, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.persistence", 1 }, "Perlin Persistence", 1, 100, 1));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.seed", 1 }, "Perlin Seed", 0, 999, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.resolution", 1 }, "Perlin Resolution", 10, 80, 52));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.brightness", 1 }, "Perlin Brightness", 0, 100, 100));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.previousM1Pattern", 1 }, "Perlin Previous M1 Pattern", 0, 39, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "perlin.previousM2Pattern", 1 }, "Perlin Previous M2 Pattern", 0, 39, 0));
    juce::StringArray modShapes { "Sine", "Triangle", "Ramp", "Saw", "Square", "Pulse", "Random",
        "Up-Down", "Third Jumps", "Root Pivot", "Rising Zigzag", "High Drone", "Cascade",
        "Odds-Evens", "Alberti", "Chord Staircase", "Bass + Chord", "Double Echo", "Mirror",
        "Clave", "Rhythmic", "Pentatonic Feel", "Alternating Pairs", "Spiral", "Linked Steps",
        "Fourth Jumps", "Rotating" };
    for (auto slot = 21; slot <= 40; ++slot) modShapes.add ("User " + juce::String (slot));
    const juce::StringArray modSyncDivisions { "16 bars", "8 bars", "4 bars", "2 bars", "1 bar", "1/2 bar", "1/4 bar", "1/8 bar", "1/16 bar" };
    const juce::StringArray modSources { "Off","LFO 1","LFO 2","LFO 3","MIDI Velocity","Pitch Bend","MIDI CC","Aftertouch" };
    juce::StringArray midiCcNumbers, midiCcChannels;
    for (auto cc = 0; cc <= 127; ++cc) midiCcNumbers.add (juce::String (cc));
    for (auto channel = 1; channel <= 16; ++channel) midiCcChannels.add (juce::String (channel));
    for (auto lfo = 1; lfo <= 3; ++lfo)
    {
        const auto p = "mod.lfo" + juce::String (lfo) + ".";
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { p + "shape", 1 }, "LFO Shape", modShapes, 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p + "rate", 1 }, "LFO Rate (Hz)",
            juce::NormalisableRange<float> { minimumLfoRateHz, maximumLfoRateHz,
                                             0.01f, 0.5f },
            defaultLfoRateHz));
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { p + "sync", 1 }, "LFO Sync", false));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { p + "retrigger", 1 }, "LFO Retrigger On Play", false));
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { p + "division", 1 }, "LFO Division", modSyncDivisions, 4));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { p + "probability", 1 }, "LFO Probability", 0, 100, 100));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { p + "pulseWidth", 1 }, "LFO Pulse Width", 1, 99, 50));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { p + "length", 1 }, "LFO Pattern Length", 1, 16, 16));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { p + "phase", 1 }, "LFO Modulation Phase", 0, 100, 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { p + "amplitudeSource", 1 }, "LFO Amplitude Source", modSources, 0));
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { p + "amplitude", 1 }, "LFO Amplitude Modulation", 0, 100, 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { p + "amplitudeCcNumber", 1 }, "LFO Amplitude MIDI CC", midiCcNumbers, 1));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { p + "amplitudeCcChannel", 1 }, "LFO Amplitude MIDI Channel", midiCcChannels, 0));
    }
    const juce::StringArray modDestinations {
        "None",
        "Key", "Scales", "Degree", "Colors",
        "Pad On", "Pad Position", "Pad Range", "Pad Spread", "Pad Strum", "Pad Velocity",
        "Drone On", "Drone Position", "Drone Type", "Drone Trigger", "Drone Velocity",
        "M1 On", "M1 Register", "M1 Pattern", "M1 Active Pattern", "M1 Length", "M1 Variation",
        "M1 Division", "M1 Velocity", "M1 Gate", "M1 Accent", "M1 Rhythm",
        "M2 On", "M2 Register", "M2 Pattern", "M2 Active Pattern", "M2 Length", "M2 Variation",
        "M2 Division", "M2 Velocity", "M2 Gate", "M2 Accent", "M2 Rhythm",
        "Perlin Zoom", "Perlin Spacing", "Perlin Roughness", "Perlin Persistence",
        "Perlin Seed", "Perlin Resolution", "Perlin Brightness",
        "MIDI CC"
    };
    for (auto slot = 1; slot <= 8; ++slot)
    {
        const auto p = "mod.slot" + juce::String(slot) + ".";
        layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{p+"on",1},"Mod Slot On",false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{p+"source",1},"Mod Source",modSources,0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{p+"destination",1},"Mod Destination",modDestinations,0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{p+"ccNumber",1},"Mod MIDI CC Number",midiCcNumbers,1));
        layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{p+"ccChannel",1},"Mod MIDI CC Channel",midiCcChannels,0));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{p+"amount",1},"Mod Amount",0,100,0));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{p+"low",1},"Mod Low",0,100,0));
        layout.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{p+"high",1},"Mod High",0,100,100));
    }
    return layout;
}

void NdlrAudioProcessor::cacheParameterPointers()
{
    const auto get = [this] (const juce::String& id)
    {
        auto* value = parameters.getRawParameterValue (id);
        jassert (value != nullptr);
        return value;
    };

    transportPlay = get ("transport.play");
    transportTempo = get ("transport.tempo");
    harmonyRoot = get ("harmony.root");
    harmonyScale = get ("harmony.scale");
    harmonyDegree = get ("harmony.degree");
    harmonyType = get ("harmony.type");
    harmonyKeyMidiChannel = get ("harmony.keyMidiChannel");
    harmonyDegreeMidiChannel = get ("harmony.degreeMidiChannel");

    for (auto motif = 0; motif < 2; ++motif)
    {
        const auto prefix = "motif" + juce::String (motif + 1) + ".";
        auto& refs = motifParameters[static_cast<size_t> (motif)];
        refs.on = get (prefix + "on");
        refs.division = get (prefix + "division");
        refs.length = get (prefix + "length");
        refs.variation = get (prefix + "variation");
        refs.velocity = get (prefix + "velocity");
        refs.channel = get (prefix + "channel");
        refs.pattern = get (prefix + "pattern");
        refs.patternType = get (prefix + "patternType");
        refs.position = get (prefix + "position");
        refs.rhythmLength = get (prefix + "rhythmLength");
        refs.rhythmSlot = get (prefix + "rhythmSlot");
        refs.ratchetVelocityMode = get (prefix + "ratchetVelocityMode");
        refs.rhythmMode = get (prefix + "rhythmMode");
        refs.euclideanPulses = get (prefix + "euclideanPulses");
        refs.rotation = get (prefix + "rotation");
        refs.humanize = get (prefix + "humanize");
        refs.accent = get (prefix + "accent");
        refs.gate = get (prefix + "gate");
        refs.customPattern = get (prefix + "customPattern");
        refs.customPatternSource = get (prefix + "customPatternSource");
        for (auto step = 0; step < 16; ++step)
            refs.patternSteps[static_cast<size_t> (step)] = get (
                prefix + "patternStep" + juce::String (step + 1));
        for (auto step = 0; step < 32; ++step)
        {
            const auto number = juce::String (step + 1);
            refs.rhythmStates[static_cast<size_t> (step)] = get (prefix + "rhythmState" + number);
            refs.rhythmVelocities[static_cast<size_t> (step)] = get (prefix + "rhythmVelocity" + number);
            refs.rhythmRatchets[static_cast<size_t> (step)] = get (prefix + "rhythmRatchet" + number);
        }
    }

    for (auto slot = 0; slot < 20; ++slot)
        for (auto step = 0; step < 16; ++step)
            sharedPatternSteps[static_cast<size_t> (slot)][static_cast<size_t> (step)] = get (
                "pattern.user" + juce::String (slot + 1) + ".step" + juce::String (step + 1));

    for (auto lfo = 0; lfo < 3; ++lfo)
    {
        const auto prefix = "mod.lfo" + juce::String (lfo + 1) + ".";
        auto& refs = lfoParameters[static_cast<size_t> (lfo)];
        refs.shape = get (prefix + "shape");
        refs.rate = get (prefix + "rate");
        refs.sync = get (prefix + "sync");
        refs.retrigger = get (prefix + "retrigger");
        refs.division = get (prefix + "division");
        refs.probability = get (prefix + "probability");
        refs.pulseWidth = get (prefix + "pulseWidth");
        refs.length = get (prefix + "length");
        refs.phase = get (prefix + "phase");
        refs.amplitudeSource = get (prefix + "amplitudeSource");
        refs.amplitude = get (prefix + "amplitude");
        refs.amplitudeCcNumber = get (prefix + "amplitudeCcNumber");
        refs.amplitudeCcChannel = get (prefix + "amplitudeCcChannel");
    }

    for (auto slot = 0; slot < 8; ++slot)
    {
        const auto prefix = "mod.slot" + juce::String (slot + 1) + ".";
        auto& refs = modSlotParameters[static_cast<size_t> (slot)];
        refs.on = get (prefix + "on");
        refs.source = get (prefix + "source");
        refs.destination = get (prefix + "destination");
        refs.ccNumber = get (prefix + "ccNumber");
        refs.ccChannel = get (prefix + "ccChannel");
        refs.amount = get (prefix + "amount");
        refs.low = get (prefix + "low");
        refs.high = get (prefix + "high");
    }

    droneOn = get ("drone.on"); dronePosition = get ("drone.position");
    droneType = get ("drone.type"); droneTrigger = get ("drone.trigger");
    droneVelocity = get ("drone.velocity"); droneChannel = get ("drone.channel");
    padOn = get ("pad.on"); padPosition = get ("pad.position"); padRange = get ("pad.range");
    padSpread = get ("pad.spread"); padVelocity = get ("pad.velocity"); padChannel = get ("pad.channel");
    padStrum = get ("pad.strum"); padStrumDivision = get ("pad.strumDivision");
    padGroup = get ("pad.group"); padPolyChain = get ("pad.polyChain");
    padInvert = get ("pad.invert"); padInversionMode = get ("pad.inversionMode");
    padQuantize = get ("pad.quantize");
    perlinOn = get ("perlin.on"); perlinM1 = get ("perlin.m1"); perlinM2 = get ("perlin.m2");
    perlinSeed = get ("perlin.seed"); perlinX = get ("perlin.x"); perlinY = get ("perlin.y");
    perlinZoom = get ("perlin.zoom"); perlinSpacing = get ("perlin.spacing");
    perlinBrightness = get ("perlin.brightness"); perlinRoughness = get ("perlin.roughness");
    perlinPersistence = get ("perlin.persistence"); perlinResolution = get ("perlin.resolution");
}

void NdlrAudioProcessor::timerCallback()
{
    const auto commit = [this] (const juce::String& id, std::atomic<int>& pending, int offset)
    {
        const auto value = pending.load();
        if (value < 0) return;
        if (auto* parameter = parameters.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (static_cast<float> (value + offset)));
            parameter->endChangeGesture();
        }
        auto expected = value;
        pending.compare_exchange_strong (expected, -1);
    };

    commit ("harmony.root", pendingMidiKey, 0);
    commit ("harmony.degree", pendingMidiDegree, 1);
}

void NdlrAudioProcessor::migrateStateTree (juce::ValueTree& state)
{
    const auto savedPadInversionModeVersion = static_cast<int> (
        state.getProperty (padInversionModeProperty, 0));
    if (savedPadInversionModeVersion < currentPadInversionModeVersion)
    {
        const auto legacyInvert = readStateParameter (state, "pad.invert", 0.0f) >= 0.5f;
        writeStateParameter (state, "pad.inversionMode", legacyInvert ? 3.0f : 0.0f);
        state.setProperty (padInversionModeProperty,
                           currentPadInversionModeVersion, nullptr);
    }

    // Jusqu'ici, le rate libre etait sauvegarde comme une periode en secondes.
    // Il est maintenant exprime en hertz. L'inversion conserve la vitesse des
    // anciens presets et sessions, dans les limites de la plage actuelle.
    const auto savedLfoRateUnitVersion = static_cast<int> (
        state.getProperty (lfoRateUnitProperty, 0));
    if (savedLfoRateUnitVersion < currentLfoRateUnitVersion)
    {
        for (auto lfo = 1; lfo <= 3; ++lfo)
        {
            const auto parameter = "mod.lfo" + juce::String (lfo) + ".rate";
            const auto periodInSeconds = readStateParameter (state, parameter, 4.0f);
            const auto rateInHz = 1.0f / juce::jmax (0.001f, periodInSeconds);
            writeStateParameter (state, parameter,
                                 juce::jlimit (minimumLfoRateHz,
                                               maximumLfoRateHz, rateInHz));
        }
        state.setProperty (lfoRateUnitProperty,
                           currentLfoRateUnitVersion, nullptr);
    }

    // La premiere liste de destinations etait ordonnee par date d'ajout. Les
    // indices sauvegardes sont convertis vers la liste desormais groupee par
    // module afin que les anciens routages conservent exactement leur cible.
    const auto savedDestinationOrderVersion = static_cast<int> (
        state.getProperty (modDestinationOrderProperty, 0));
    if (savedDestinationOrderVersion < currentModDestinationOrderVersion)
    {
        static constexpr std::array<int, 36> legacyToCurrent {
            0, 1, 2, 3, 4,
            ndlr::modulation::padPosition, ndlr::modulation::padRange,
            ndlr::modulation::padSpread, ndlr::modulation::padStrum,
            ndlr::modulation::dronePosition, ndlr::modulation::droneType,
            ndlr::modulation::droneTrigger,
            ndlr::modulation::motif1Position, ndlr::modulation::motif1Pattern,
            ndlr::modulation::motif1Length, ndlr::modulation::motif1Variation,
            ndlr::modulation::motif1Division, ndlr::modulation::motif1Velocity,
            ndlr::modulation::motif2Position, ndlr::modulation::motif2Pattern,
            ndlr::modulation::motif2Length, ndlr::modulation::motif2Variation,
            ndlr::modulation::motif2Division, ndlr::modulation::motif2Velocity,
            ndlr::modulation::padVelocity, ndlr::modulation::padOn,
            ndlr::modulation::droneOn,
            ndlr::modulation::motif1Gate, ndlr::modulation::motif1Accent,
            ndlr::modulation::motif1Rhythm, ndlr::modulation::motif1On,
            ndlr::modulation::motif2Gate, ndlr::modulation::motif2Accent,
            ndlr::modulation::motif2Rhythm, ndlr::modulation::motif2On,
            ndlr::modulation::midiCc
        };
        static constexpr std::array<int, 44> version1ToCurrent {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
            ndlr::modulation::motif1Pattern,
            ndlr::modulation::motif1Length, ndlr::modulation::motif1Variation,
            ndlr::modulation::motif1Division, ndlr::modulation::motif1Velocity,
            ndlr::modulation::motif1Gate, ndlr::modulation::motif1Accent,
            ndlr::modulation::motif1Rhythm,
            ndlr::modulation::motif2On, ndlr::modulation::motif2Position,
            ndlr::modulation::motif2Pattern,
            ndlr::modulation::motif2Length, ndlr::modulation::motif2Variation,
            ndlr::modulation::motif2Division, ndlr::modulation::motif2Velocity,
            ndlr::modulation::motif2Gate, ndlr::modulation::motif2Accent,
            ndlr::modulation::motif2Rhythm,
            ndlr::modulation::perlinZoom, ndlr::modulation::perlinSpacing,
            ndlr::modulation::perlinRoughness, ndlr::modulation::perlinPersistence,
            ndlr::modulation::perlinSeed, ndlr::modulation::perlinResolution,
            ndlr::modulation::perlinBrightness, ndlr::modulation::midiCc
        };
        for (auto slot = 1; slot <= 8; ++slot)
        {
            const auto parameter = "mod.slot" + juce::String (slot) + ".destination";
            const auto saved = juce::roundToInt (readStateParameter (state, parameter, 0.0f));
            const auto current = savedDestinationOrderVersion < 1
                ? legacyToCurrent[static_cast<size_t> (juce::jlimit (0, 35, saved))]
                : version1ToCurrent[static_cast<size_t> (juce::jlimit (0, 43, saved))];
            writeStateParameter (state, parameter, static_cast<float> (current));
        }
        state.setProperty (modDestinationOrderProperty,
                           currentModDestinationOrderVersion, nullptr);
    }

    // Les anciennes versions avaient une banque User distincte pour chaque motif.
    // La migration doit fonctionner même lorsque l'éditeur n'est jamais créé.
    for (auto slot = 0; slot < 20; ++slot)
    {
        const auto shared = "pattern.user" + juce::String (slot + 1) + ".";
        if (readStateParameter (state, shared + "used", 0.0f) >= 0.5f)
            continue;
        const auto motif1 = "motif1.user" + juce::String (slot + 1) + ".";
        const auto motif2 = "motif2.user" + juce::String (slot + 1) + ".";
        const auto source = readStateParameter (state, motif1 + "used", 0.0f) >= 0.5f ? motif1
                          : readStateParameter (state, motif2 + "used", 0.0f) >= 0.5f ? motif2
                          : juce::String {};
        if (source.isEmpty()) continue;
        for (auto step = 0; step < 16; ++step)
        {
            const auto number = juce::String (step + 1);
            writeStateParameter (state, shared + "step" + number,
                readStateParameter (state, source + "step" + number,
                                    static_cast<float> (NdlrEngine::motifPatternValue (0, step))));
        }
        writeStateParameter (state, shared + "used", 1.0f);
    }

    // États sauvegardés par l'ancienne UI Perlin : restaure le vrai pattern de
    // base sans réserver les slots User 39/40 dans les nouvelles versions.
    if (readStateParameter (state, "perlin.on", 0.0f) >= 0.5f)
    {
        if (readStateParameter (state, "perlin.m1", 0.0f) >= 0.5f
            && juce::roundToInt (readStateParameter (state, "motif1.pattern", 0.0f)) == 38)
            writeStateParameter (state, "motif1.pattern",
                readStateParameter (state, "perlin.previousM1Pattern", 0.0f));
        if (readStateParameter (state, "perlin.m2", 0.0f) >= 0.5f
            && juce::roundToInt (readStateParameter (state, "motif2.pattern", 0.0f)) == 39)
            writeStateParameter (state, "motif2.pattern",
                readStateParameter (state, "perlin.previousM2Pattern", 0.0f));
    }
}

void NdlrAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    modulationPhase.fill (0.0);
    modulationRandom.fill (0.5f);
    modulationCycle.fill (0);
    modulationHostTransportWasPlaying = false;
    modulationInternalTransportWasPlaying = false;
    modulationLastProcessTimeMs = 0.0;
    modulationTransportRunning.store (false);
    for (auto& count : modulationRestartCount)
        count.store (0);
}

void NdlrAudioProcessor::releaseResources()
{
    engine.reset();
}

void NdlrAudioProcessor::refreshRhythmMemoryCache()
{
    std::array<RhythmMemory, 40> refreshed {};
    const auto memories = parameters.state.getChildWithName ("RhythmMemories");
    for (auto slot = 0; slot < 40; ++slot)
    {
        const auto slotNumber = slot + 1;
        auto memory = memories.getChildWithName ("User" + juce::String (slotNumber));
        if ((! memory.isValid() || ! static_cast<bool> (memory.getProperty ("used", false)))
            && slotNumber >= 21)
        {
            memory = memories.getChildWithName ("M1User" + juce::String (slotNumber));
            if (! memory.isValid() || ! static_cast<bool> (memory.getProperty ("used", false)))
                memory = memories.getChildWithName ("M2User" + juce::String (slotNumber));
        }
        auto& destination = refreshed[static_cast<size_t> (slot)];
        destination.used = memory.isValid() && static_cast<bool> (memory.getProperty ("used", false));
        if (! destination.used) continue;
        destination.length = static_cast<int> (memory.getProperty ("rhythmLength", 8));
        destination.mode = static_cast<int> (memory.getProperty ("rhythmMode", 0));
        destination.pulses = static_cast<int> (memory.getProperty ("euclideanPulses", 4));
        destination.rotation = static_cast<int> (memory.getProperty ("rotation", 0));
        destination.division = static_cast<int> (memory.getProperty ("division", 7));
        destination.ratchetVelocityMode = static_cast<int> (memory.getProperty ("ratchetVelocityMode", 0));
        for (auto step = 0; step < 32; ++step)
        {
            const auto number = juce::String (step + 1);
            destination.state[static_cast<size_t> (step)] = static_cast<int> (memory.getProperty ("state" + number, 1));
            destination.velocity[static_cast<size_t> (step)] = static_cast<int> (memory.getProperty ("velocity" + number, 100));
            destination.ratchet[static_cast<size_t> (step)] = static_cast<int> (memory.getProperty ("ratchet" + number, 1));
        }
    }
    const std::lock_guard<std::mutex> lock (rhythmMemoryMutex);
    rhythmMemoryCache = std::move (refreshed);
}

bool NdlrAudioProcessor::getRhythmMemory (int slot, RhythmMemory& destination)
{
    const std::lock_guard<std::mutex> lock (rhythmMemoryMutex);
    destination = rhythmMemoryCache[static_cast<size_t> (juce::jlimit (1, 40, slot) - 1)];
    return destination.used;
}

bool NdlrAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet().isDisabled()
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

void NdlrAudioProcessor::processBlock (juce::AudioBuffer<float>& audio,
                                       juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();

    const auto keyMidiChannel = juce::roundToInt (harmonyKeyMidiChannel->load());
    const auto degreeMidiChannel = juce::roundToInt (harmonyDegreeMidiChannel->load());
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto channelIndex = static_cast<size_t> (juce::jlimit (1, 16, message.getChannel()) - 1);
        if (message.isController())
            incomingCcValues[channelIndex][static_cast<size_t> (message.getControllerNumber())] =
                message.getControllerValue();
        else if (message.isPitchWheel())
            incomingPitchBendValues[channelIndex] = message.getPitchWheelValue();
        else if (message.isAftertouch())
            incomingAftertouchValues[channelIndex] = message.getAfterTouchValue();
        else if (message.isChannelPressure())
            incomingAftertouchValues[channelIndex] = message.getChannelPressureValue();

        if (! message.isNoteOn()) continue;
        incomingMidiVelocity = message.getVelocity();
        const auto pitchClass = message.getNoteNumber() % 12;
        if (message.getChannel() == keyMidiChannel)
        {
            // harmony.root suit le cercle des quintes (C, G, D, A...), pas
            // l'ordre chromatique des numéros de notes MIDI.
            static constexpr std::array<int, 12> keyIndexByPitchClass {
                0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5
            };
            pendingMidiKey.store (keyIndexByPitchClass[static_cast<size_t> (pitchClass)]);
            harmonyTriggerCounter.fetch_add (1);
        }
        if (message.getChannel() == degreeMidiChannel)
        {
            static constexpr std::array<int, 12> degreeByPitchClass {
                0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6
            };
            const auto degree = degreeByPitchClass[static_cast<size_t> (pitchClass)];
            if (degree >= 0)
            {
                pendingMidiDegree.store (degree);
                harmonyTriggerCounter.fetch_add (1);
            }
        }
    }

    // NDLR est un générateur MIDI, pas un MIDI Thru. Conserver les messages
    // d'entrée dans le buffer de sortie crée une boucle immédiate lorsque le
    // même bus IAC est sélectionné en entrée et en sortie (les CC 123 et les
    // notes sont alors réémis à chaque bloc audio).
    midi.clear();

    if (panicRequested.exchange (false))
        engine.panic (midi);

    juce::AudioPlayHead::PositionInfo position;
    auto hasHostPosition = false;
    if (auto* playHead = getPlayHead())
        if (const auto currentPosition = playHead->getPosition())
        {
            position = *currentPosition;
            hasHostPosition = true;
        }

    const NdlrEngine::TransportSettings transportSettings {
        transportPlay->load() >= 0.5f,
        transportTempo->load()
    };

    auto readMotifSettings = [this] (const MotifParameterRefs& refs)
    {
        NdlrEngine::MotifSettings settings;
        settings.enabled = refs.on->load() >= 0.5f;
        settings.divisionIndex = juce::roundToInt (refs.division->load());
        settings.length = juce::roundToInt (refs.length->load());
        settings.variation = juce::roundToInt (refs.variation->load());
        settings.velocity = juce::roundToInt (refs.velocity->load());
        settings.midiChannel = juce::roundToInt (refs.channel->load());
        settings.pattern = juce::roundToInt (refs.pattern->load());
        settings.patternType = juce::roundToInt (refs.patternType->load());
        settings.position = juce::roundToInt (refs.position->load());
        settings.rhythmLength = juce::roundToInt (refs.rhythmLength->load());

        for (auto step = 0; step < 32; ++step)
        {
            const auto index = static_cast<size_t> (step);
            settings.rhythmState[index] = juce::roundToInt (refs.rhythmStates[index]->load());
            settings.rhythmVelocity[index] = juce::roundToInt (refs.rhythmVelocities[index]->load());
            settings.rhythmRatchet[index] = juce::roundToInt (refs.rhythmRatchets[index]->load());
        }

        settings.ratchetVelocityMode = juce::roundToInt (refs.ratchetVelocityMode->load());
        settings.rhythmMode = juce::roundToInt (refs.rhythmMode->load());
        settings.euclideanPulses = juce::roundToInt (refs.euclideanPulses->load());
        settings.rotation = juce::roundToInt (refs.rotation->load());
        settings.humanize = juce::roundToInt (refs.humanize->load());
        settings.accent = juce::roundToInt (refs.accent->load());
        settings.gate = juce::roundToInt (refs.gate->load());
        settings.customPattern = refs.customPattern->load() >= 0.5f;
        settings.customPatternSource = juce::roundToInt (refs.customPatternSource->load()) - 1;

        const auto useEditBuffer = settings.customPattern
                                && settings.pattern == settings.customPatternSource;
        const auto userSlot = juce::jlimit (0, 19, settings.pattern - 20);
        for (auto step = 0; step < 16; ++step)
        {
            const auto index = static_cast<size_t> (step);
            const auto* value = settings.pattern >= 20 && ! useEditBuffer
                ? sharedPatternSteps[static_cast<size_t> (userSlot)][index]
                : refs.patternSteps[index];
            settings.patternSteps[index] = juce::roundToInt (value->load());
        }
        return settings;
    };

    auto motif1Settings = readMotifSettings (motifParameters[0]);
    auto motif2Settings = readMotifSettings (motifParameters[1]);
    const auto perlinEnabled = perlinOn->load() >= 0.5f;
    auto perlinZoomValue = juce::roundToInt (perlinZoom->load());
    auto perlinSpacingValue = juce::roundToInt (perlinSpacing->load());
    auto perlinRoughnessValue = juce::roundToInt (perlinRoughness->load());
    auto perlinPersistenceValue = juce::roundToInt (perlinPersistence->load());
    auto perlinSeedValue = juce::roundToInt (perlinSeed->load());
    auto perlinResolutionValue = juce::roundToInt (perlinResolution->load());
    auto perlinBrightnessValue = juce::roundToInt (perlinBrightness->load());
    NdlrEngine::DroneSettings droneSettings {
        droneOn->load() >= 0.5f, juce::roundToInt (dronePosition->load()),
        juce::roundToInt (droneType->load()), juce::roundToInt (droneTrigger->load()),
        juce::roundToInt (droneVelocity->load()), juce::roundToInt (droneChannel->load()) };
    NdlrEngine::PadSettings padSettings {
        padOn->load() >= 0.5f, juce::roundToInt (padPosition->load()), juce::roundToInt (padRange->load()),
        juce::roundToInt (padSpread->load()), juce::roundToInt (padVelocity->load()),
        juce::roundToInt (padChannel->load()), padStrum->load() >= 0.5f,
        juce::roundToInt (padStrumDivision->load()), padGroup->load() >= 0.5f,
        juce::roundToInt (padPolyChain->load()),
        juce::jlimit (0, 3, juce::roundToInt (padInversionMode->load())),
        juce::roundToInt (padQuantize->load()) };

    NdlrEngine::HarmonySettings harmonySettings {
        juce::roundToInt (harmonyRoot->load()), juce::roundToInt (harmonyScale->load()),
        juce::roundToInt (harmonyDegree->load()) - 1, juce::roundToInt (harmonyType->load()),
        harmonyTriggerCounter.load()
    };
    if (const auto midiKey = pendingMidiKey.load(); midiKey >= 0) harmonySettings.root = midiKey;
    if (const auto midiDegree = pendingMidiDegree.load(); midiDegree >= 0) harmonySettings.degree = midiDegree;

    std::array<float,3> lfoValues {};
    std::array<bool,3> lfoIsBipolar {};
    static constexpr std::array<double,9> syncedQuarters {64.0,32.0,16.0,8.0,4.0,2.0,1.0,0.5,0.25};
    const auto hostPpq = hasHostPosition ? position.getPpqPosition() : juce::Optional<double> {};
    const auto hostBpm = hasHostPosition ? position.getBpm() : juce::Optional<double> {};
    // A musical host position remains authoritative while the DAW is stopped.
    // The internal transport is only the fallback used by the Standalone app
    // (or by a host which does not expose a PPQ position).
    const auto hasHostMusicalPosition = hasHostPosition && static_cast<bool> (hostPpq);
    const auto hostTransportPlaying = hasHostMusicalPosition && position.getIsPlaying();
    const auto internalButtonPlaying = transportSettings.internalPlaying;
    const auto syncTransportPlaying = hasHostMusicalPosition
        ? hostTransportPlaying : internalButtonPlaying;
    modulationTransportRunning.store (syncTransportPlaying);

    const auto patternValueAtPhase = [this] (const LfoParameterRefs& refs,
                                              int shape, float phase)
    {
        const auto pattern = juce::jlimit (0, 39, shape - 7);
        const auto length = juce::jlimit (1, 16, juce::roundToInt (refs.length->load()));
        const auto step = ndlr::timing::patternStepFromPhase (phase, length);
        const auto readValue = [this, pattern] (int patternStep)
        {
            return pattern < 20
                ? NdlrEngine::motifPatternValue (pattern, patternStep)
                : juce::roundToInt (
                    sharedPatternSteps[static_cast<size_t> (pattern - 20)]
                                      [static_cast<size_t> (patternStep)]->load());
        };
        auto maximum = 2;
        for (auto activeStep = 0; activeStep < length; ++activeStep)
            maximum = juce::jmax (maximum, readValue (activeStep));
        return ndlr::pattern::normalizeValue (readValue (step), maximum);
    };

    // Some hosts suspend plug-in callbacks while stopped. In that case no
    // explicit stopped block reaches us, so a sufficiently long callback gap
    // followed by a playing block is also treated as a fresh Play edge.
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    const auto blockDurationMs = 1000.0 * static_cast<double> (audio.getNumSamples())
                               / juce::jmax (1.0, getSampleRate());
    const auto resumedAfterCallbackGap = modulationLastProcessTimeMs > 0.0
        && nowMs - modulationLastProcessTimeMs > juce::jmax (50.0, blockDurationMs * 4.0);
    modulationLastProcessTimeMs = nowMs;
    const auto hostTransportStarted = hostTransportPlaying
        && (! modulationHostTransportWasPlaying || resumedAfterCallbackGap);
    const auto internalTransportStarted = internalButtonPlaying
        && ! modulationInternalTransportWasPlaying;
    const auto syncTransportStarted = hasHostMusicalPosition
        ? hostTransportStarted : internalTransportStarted;
    const auto retriggerTransportStarted = hostTransportStarted || internalTransportStarted;
    const auto bpm = juce::jlimit (20.0, 300.0,
        hostTransportPlaying && hostBpm
            ? *hostBpm : static_cast<double> (transportSettings.internalTempo));
    for (auto i=0; i<3; ++i)
    {
        const auto index = static_cast<size_t> (i);
        const auto& refs = lfoParameters[index];
        const auto sync = refs.sync->load() >= 0.5f;
        const auto retrigger = refs.retrigger->load() >= 0.5f;
        const auto rate = refs.rate->load();
        const auto div = juce::jlimit (0, 8, juce::roundToInt (refs.division->load()));
        const auto periodInQuarters = syncedQuarters[static_cast<size_t> (div)];
        if (sync && hostTransportPlaying)
        {
            const auto timing = ndlr::timing::phaseFromPpq (*hostPpq, periodInQuarters);
            modulationPhase[index] = timing.phase;
            modulationCycle[index] = static_cast<uint64_t> (timing.cycle);
            modulationRandom[index] = static_cast<float> (
                (modulationCycle[index] * 1103515245u + 12345u) % 10000u) / 9999.0f;
            if (hostTransportStarted)
                modulationRestartCount[index].fetch_add (1);
        }
        else
        {
            const auto cyclesPerSecond = sync
                ? bpm / (periodInQuarters * 60.0)
                : static_cast<double> (rate);
            const auto resetOnPlay = sync
                ? syncTransportStarted
                : retrigger && retriggerTransportStarted;
            if (resetOnPlay)
            {
                modulationPhase[index] = 0.0;
                modulationCycle[index] = 0;
                modulationRandom[index] = 0.5f;
                modulationRestartCount[index].fetch_add (1);
            }
            else if (! sync || syncTransportPlaying)
            {
                modulationPhase[index] += audio.getNumSamples() * cyclesPerSecond
                    / getSampleRate();
            }
            if (modulationPhase[index] >= 1.0)
            {
                modulationPhase[index] -= std::floor (modulationPhase[index]);
                ++modulationCycle[index];
                modulationRandom[index] = static_cast<float> (
                    (modulationCycle[index] * 1103515245u + 12345u) % 10000u) / 9999.0f;
            }
        }
        const auto probability = juce::roundToInt (refs.probability->load());
        const auto gate = ndlr::timing::probabilityGate (modulationCycle[index], probability);
        const auto phase = static_cast<float> (modulationPhase[index]);
        const auto shape = juce::roundToInt (refs.shape->load());
        lfoIsBipolar[index] = shape == 0;
        const auto pw = refs.pulseWidth->load() / 100.0f;
        float v=0.0f;
        if(shape==0) v=std::sin(phase*juce::MathConstants<float>::twoPi);
        else if(shape==1) v=phase<0.5f?phase*2.0f:2.0f-phase*2.0f;
        else if(shape==2) v=phase; else if(shape==3) v=1.0f-phase;
        else if(shape==4) v=phase<0.5f?1.0f:0.0f; else if(shape==5) v=phase<pw?1.0f:0.0f;
        else if(shape==6) v=modulationRandom[index];
        else
            v = patternValueAtPhase (refs, shape, phase);
        lfoValues[static_cast<size_t>(i)]=gate?v:0.0f;
    }

    // Apply amplitude modulation in a second pass. Each LFO reads the
    // unmodulated outputs of the other LFOs, so cross-modulation is stable and
    // deterministic even when two LFOs point at one another.
    const auto unmodulatedLfoValues = lfoValues;
    const auto shiftedModulationSourceValue = [&] (size_t sourceIndex,
                                                    float phasePercent)
    {
        const auto normalizedOffset = std::fmod (
            juce::jlimit (0.0f, 100.0f, phasePercent), 100.0f) / 100.0f;
        if (normalizedOffset <= 0.0f)
            return unmodulatedLfoValues[sourceIndex];

        const auto& sourceRefs = lfoParameters[sourceIndex];
        const auto shiftedTiming = ndlr::timing::phaseWithOffset (
            modulationPhase[sourceIndex], static_cast<double> (normalizedOffset));
        const auto shiftedCycle = modulationCycle[sourceIndex]
            + static_cast<uint64_t> (juce::jmax<int64_t> (0, shiftedTiming.cycleOffset));
        const auto probability = juce::roundToInt (sourceRefs.probability->load());
        if (! ndlr::timing::probabilityGate (shiftedCycle, probability))
            return 0.0f;

        const auto phase = static_cast<float> (shiftedTiming.phase);
        const auto shape = juce::roundToInt (sourceRefs.shape->load());
        const auto pulseWidth = sourceRefs.pulseWidth->load() / 100.0f;
        if (shape == 0) return std::sin (phase * juce::MathConstants<float>::twoPi);
        if (shape == 1) return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;
        if (shape == 2) return phase;
        if (shape == 3) return 1.0f - phase;
        if (shape == 4) return phase < 0.5f ? 1.0f : 0.0f;
        if (shape == 5) return phase < pulseWidth ? 1.0f : 0.0f;
        if (shape == 6)
            return shiftedTiming.cycleOffset == 0
                ? modulationRandom[sourceIndex]
                : static_cast<float> ((shiftedCycle * 1103515245u + 12345u) % 10000u)
                    / 9999.0f;

        return patternValueAtPhase (sourceRefs, shape, phase);
    };
    for (auto i = 0; i < 3; ++i)
    {
        const auto index = static_cast<size_t> (i);
        const auto& refs = lfoParameters[index];
        const auto source = juce::roundToInt (refs.amplitudeSource->load());
        const auto amount = juce::jlimit (0.0f, 1.0f, refs.amplitude->load() / 100.0f);
        if (source < 1 || source > 7 || amount <= 0.0f || source == i + 1)
            continue;

        auto sourceLevel = 1.0f;
        if (source >= 1 && source <= 3)
        {
            const auto sourceIndex = static_cast<size_t> (source - 1);
            const auto sourceIsStoppedSync = lfoParameters[sourceIndex].sync->load() >= 0.5f
                                          && ! syncTransportPlaying;
            if (sourceIsStoppedSync)
                continue;
            const auto sourceValue = shiftedModulationSourceValue (
                sourceIndex, refs.phase->load());
            sourceLevel = lfoIsBipolar[sourceIndex]
                ? (sourceValue + 1.0f) * 0.5f : sourceValue;
        }
        else if (source == 4)
        {
            sourceLevel = static_cast<float> (juce::jlimit (0, 127, incomingMidiVelocity))
                        / 127.0f;
        }
        else
        {
            const auto channel = static_cast<size_t> (juce::jlimit (0, 15,
                juce::roundToInt (refs.amplitudeCcChannel->load())));
            if (source == 5)
                sourceLevel = static_cast<float> (juce::jlimit (
                    0, 16383, incomingPitchBendValues[channel])) / 16383.0f;
            else if (source == 6)
            {
                const auto cc = static_cast<size_t> (juce::jlimit (0, 127,
                    juce::roundToInt (refs.amplitudeCcNumber->load())));
                sourceLevel = static_cast<float> (juce::jlimit (
                    0, 127, incomingCcValues[channel][cc])) / 127.0f;
            }
            else
                sourceLevel = static_cast<float> (juce::jlimit (
                    0, 127, incomingAftertouchValues[channel])) / 127.0f;
        }

        sourceLevel = juce::jlimit (0.0f, 1.0f, sourceLevel);
        const auto gain = 1.0f - amount + amount * sourceLevel;
        lfoValues[index] *= gain;
    }
    modulationHostTransportWasPlaying = hostTransportPlaying;
    modulationInternalTransportWasPlaying = internalButtonPlaying;
    auto calculate=[](int base,int lo,int hi,float raw,int amount,int low,int high,bool bipolar)
    { const auto n=juce::jlimit(0.0f,1.0f,bipolar?(raw+1.0f)*0.5f:raw); const auto lof=static_cast<float>(lo), hif=static_cast<float>(hi), lowf=static_cast<float>(low), highf=static_cast<float>(high); const auto target=lof+(hif-lof)*(lowf+(highf-lowf)*n)/100.0f; return juce::jlimit(lo,hi,juce::roundToInt(static_cast<float>(base)+static_cast<float>(amount)*(target-static_cast<float>(base))/100.0f)); };
    auto motif1RhythmSlot = juce::roundToInt (motifParameters[0].rhythmSlot->load());
    auto motif2RhythmSlot = juce::roundToInt (motifParameters[1].rhythmSlot->load());
    auto applyRhythmMemory = [this] (NdlrEngine::MotifSettings& settings, int slot)
    {
        RhythmMemory memory;
        {
            std::unique_lock<std::mutex> lock (rhythmMemoryMutex, std::try_to_lock);
            if (! lock.owns_lock()) return;
            memory = rhythmMemoryCache[static_cast<size_t> (juce::jlimit (1, 40, slot) - 1)];
        }
        // Un slot vide ne remplace jamais le rythme courant.
        if (! memory.used) return;
        settings.rhythmLength = juce::jlimit (4, 32, memory.length);
        settings.rhythmMode = juce::jlimit (0, 1, memory.mode);
        settings.euclideanPulses = juce::jlimit (1, settings.rhythmLength, memory.pulses);
        settings.rotation = juce::jlimit (-(settings.rhythmLength - 1), settings.rhythmLength - 1, memory.rotation);
        settings.divisionIndex = juce::jlimit (0, 20, memory.division);
        settings.ratchetVelocityMode = juce::jlimit (0, 2, memory.ratchetVelocityMode);
        settings.rhythmState = memory.state;
        settings.rhythmVelocity = memory.velocity;
        settings.rhythmRatchet = memory.ratchet;
    };
    for (auto& value : modulationVisualization) value.store (0.0f);
    for (auto& active : modulationDestinationActive) active.store (false);
    for(auto s=1;s<=8;++s)
    {
        const auto slotIndex = static_cast<size_t> (s - 1);
        const auto& refs = modSlotParameters[slotIndex];
        if(refs.on->load()<0.5f)
        {
            lastMidiCcValue[slotIndex] = -1;
            continue;
        }
        const auto src=juce::roundToInt(refs.source->load());
        const auto dst=juce::roundToInt(refs.destination->load());
        if(src<1||src>7||dst<1)
        {
            lastMidiCcValue[slotIndex] = -1;
            continue;
        }

        // A synced LFO is inactive while its reference transport is stopped.
        // Do not keep reapplying its frozen value to internal destinations and
        // do not emit MIDI CC data. Resetting the cache makes the first value
        // at the next Play edge explicit for external MIDI devices.
        const auto stoppedSyncedLfo = src >= 1 && src <= 3
            && lfoParameters[static_cast<size_t> (src - 1)].sync->load() >= 0.5f
            && ! syncTransportPlaying;
        if (stoppedSyncedLfo)
        {
            lastMidiCcValue[slotIndex] = -1;
            continue;
        }

        auto raw = 0.0f;
        auto bipolar = false;
        if (src >= 1 && src <= 3)
        {
            raw = lfoValues[static_cast<size_t>(src-1)];
            bipolar = juce::roundToInt(lfoParameters[static_cast<size_t> (src - 1)].shape->load())==0;
        }
        else if (src == 4)
            raw = static_cast<float> (juce::jlimit (0, 127, incomingMidiVelocity)) / 127.0f;
        else if (src == 5)
        {
            const auto ccChannel = juce::roundToInt (refs.ccChannel->load());
            raw = static_cast<float> (juce::jlimit (0, 16383, incomingPitchBendValues[static_cast<size_t> (ccChannel)])) / 16383.0f;
        }
        else if (src == 6)
        {
            const auto ccNumber = juce::roundToInt (refs.ccNumber->load());
            const auto ccChannel = juce::roundToInt (refs.ccChannel->load());
            raw = static_cast<float> (incomingCcValues[static_cast<size_t> (ccChannel)]
                                                    [static_cast<size_t> (ccNumber)]) / 127.0f;
        }
        else if (src == 7)
        {
            const auto ccChannel = juce::roundToInt (refs.ccChannel->load());
            raw = static_cast<float> (juce::jlimit (0, 127, incomingAftertouchValues[static_cast<size_t> (ccChannel)])) / 127.0f;
        }
        const auto amt=juce::roundToInt(refs.amount->load());
        const auto savedLow=juce::roundToInt(refs.low->load());
        const auto savedHigh=juce::roundToInt(refs.high->load());
        const auto low=juce::jmin(savedLow,savedHigh), high=juce::jmax(savedLow,savedHigh);
        modulationVisualization[slotIndex].store (
            juce::jlimit (-1.0f, 1.0f, (bipolar ? raw : raw * 2.0f - 1.0f)
                                      * static_cast<float> (amt) / 100.0f));

        // MIDI CC est une destination externe : la valeur modulée est envoyée
        // directement dans le buffer MIDI, uniquement lorsqu'elle change.
        if (dst == ndlr::modulation::midiCc)
        {
            const auto ccNumber = juce::roundToInt (refs.ccNumber->load());
            const auto ccChannel = juce::roundToInt (refs.ccChannel->load()) + 1;
            const auto ccValue = calculate (0, 0, 127, raw, amt, low, high, bipolar);
            if (ccValue != lastMidiCcValue[slotIndex]
                || ccNumber != lastMidiCcNumber[slotIndex]
                || ccChannel != lastMidiCcChannel[slotIndex])
            {
                midi.addEvent (juce::MidiMessage::controllerEvent (ccChannel, ccNumber, ccValue), 0);
                lastMidiCcValue[slotIndex] = ccValue;
                lastMidiCcNumber[slotIndex] = ccNumber;
                lastMidiCcChannel[slotIndex] = ccChannel;
            }
            continue;
        }
        lastMidiCcValue[slotIndex] = -1;
        auto m=[&](int base,int lo,int hi)
        {
            const auto result = calculate(base,lo,hi,raw,amt,low,high,bipolar);
            modulationDestinationValues[static_cast<size_t> (dst)].store (result);
            modulationDestinationActive[static_cast<size_t> (dst)].store (true);
            return result;
        };
        switch (dst)
        {
            case ndlr::modulation::harmonyKey:
                harmonySettings.root = m (harmonySettings.root, 0, 11); break;
            case ndlr::modulation::harmonyMode:
                harmonySettings.scale = m (harmonySettings.scale, 0, 27); break;
            case ndlr::modulation::harmonyDegree:
                harmonySettings.degree = m (harmonySettings.degree, 0, 6); break;
            case ndlr::modulation::harmonyChordType:
                harmonySettings.chordType = m (harmonySettings.chordType, 0, 17); break;

            case ndlr::modulation::padOn:
                padSettings.enabled = m (padSettings.enabled ? 1 : 0, 0, 1) > 0; break;
            case ndlr::modulation::padPosition:
                padSettings.position = m (padSettings.position, 0, 100); break;
            case ndlr::modulation::padRange:
                padSettings.range = m (padSettings.range, 1, 22); break;
            case ndlr::modulation::padSpread:
                padSettings.spread = m (padSettings.spread, 1, 6); break;
            case ndlr::modulation::padStrum:
                padSettings.strumDivision = m (padSettings.strumDivision, 0, 20); break;
            case ndlr::modulation::padVelocity:
                padSettings.velocity = m (padSettings.velocity, 1, 127); break;

            case ndlr::modulation::droneOn:
                droneSettings.enabled = m (droneSettings.enabled ? 1 : 0, 0, 1) > 0; break;
            case ndlr::modulation::dronePosition:
                droneSettings.position = m (droneSettings.position, 0, 3); break;
            case ndlr::modulation::droneType:
                droneSettings.type = m (droneSettings.type, 0, 3); break;
            case ndlr::modulation::droneTrigger:
                droneSettings.trigger = m (droneSettings.trigger, 1, 19); break;
            case ndlr::modulation::droneVelocity:
                droneSettings.velocity = m (droneSettings.velocity, 1, 127); break;

            case ndlr::modulation::motif1On:
                motif1Settings.enabled = m (motif1Settings.enabled ? 1 : 0, 0, 1) > 0; break;
            case ndlr::modulation::motif1Position:
                motif1Settings.position = m (motif1Settings.position, 0, 4); break;
            case ndlr::modulation::motif1Pattern:
                motif1Settings.pattern = m (motif1Settings.pattern, 0, 39); break;
            case ndlr::modulation::motif1ActivePattern:
                motif1Settings.patternAmplitude = m (motif1Settings.patternAmplitude, 0, 100); break;
            case ndlr::modulation::motif1Length:
                motif1Settings.length = m (motif1Settings.length, 1, 16); break;
            case ndlr::modulation::motif1Variation:
                motif1Settings.variation = m (motif1Settings.variation, 0, 5); break;
            case ndlr::modulation::motif1Division:
                motif1Settings.divisionIndex = m (motif1Settings.divisionIndex, 0, 20); break;
            case ndlr::modulation::motif1Velocity:
                motif1Settings.velocity = m (motif1Settings.velocity, 1, 127); break;
            case ndlr::modulation::motif1Gate:
                motif1Settings.gate = m (motif1Settings.gate, 5, 127); break;
            case ndlr::modulation::motif1Accent:
                motif1Settings.accent = m (motif1Settings.accent, 1, 10); break;
            case ndlr::modulation::motif1Rhythm:
                motif1RhythmSlot = m (motif1RhythmSlot, 1, 40);
                applyRhythmMemory (motif1Settings, motif1RhythmSlot); break;

            case ndlr::modulation::motif2On:
                motif2Settings.enabled = m (motif2Settings.enabled ? 1 : 0, 0, 1) > 0; break;
            case ndlr::modulation::motif2Position:
                motif2Settings.position = m (motif2Settings.position, 0, 4); break;
            case ndlr::modulation::motif2Pattern:
                motif2Settings.pattern = m (motif2Settings.pattern, 0, 39); break;
            case ndlr::modulation::motif2ActivePattern:
                motif2Settings.patternAmplitude = m (motif2Settings.patternAmplitude, 0, 100); break;
            case ndlr::modulation::motif2Length:
                motif2Settings.length = m (motif2Settings.length, 1, 16); break;
            case ndlr::modulation::motif2Variation:
                motif2Settings.variation = m (motif2Settings.variation, 0, 5); break;
            case ndlr::modulation::motif2Division:
                motif2Settings.divisionIndex = m (motif2Settings.divisionIndex, 0, 20); break;
            case ndlr::modulation::motif2Velocity:
                motif2Settings.velocity = m (motif2Settings.velocity, 1, 127); break;
            case ndlr::modulation::motif2Gate:
                motif2Settings.gate = m (motif2Settings.gate, 5, 127); break;
            case ndlr::modulation::motif2Accent:
                motif2Settings.accent = m (motif2Settings.accent, 1, 10); break;
            case ndlr::modulation::motif2Rhythm:
                motif2RhythmSlot = m (motif2RhythmSlot, 1, 40);
                applyRhythmMemory (motif2Settings, motif2RhythmSlot); break;

            case ndlr::modulation::perlinZoom:
                perlinZoomValue = m (perlinZoomValue, 1, 100); break;
            case ndlr::modulation::perlinSpacing:
                perlinSpacingValue = m (perlinSpacingValue, 0, 100); break;
            case ndlr::modulation::perlinRoughness:
                perlinRoughnessValue = m (perlinRoughnessValue, 1, 100); break;
            case ndlr::modulation::perlinPersistence:
                perlinPersistenceValue = m (perlinPersistenceValue, 1, 100); break;
            case ndlr::modulation::perlinSeed:
                perlinSeedValue = m (perlinSeedValue, 0, 999); break;
            case ndlr::modulation::perlinResolution:
                perlinResolutionValue = m (perlinResolutionValue, 10, 80); break;
            case ndlr::modulation::perlinBrightness:
                perlinBrightnessValue = m (perlinBrightnessValue, 0, 100); break;
            default: break;
        }
    }

    if (perlinEnabled)
    {
        const auto x = perlinX->load();
        const auto y = perlinY->load();
        const auto brightness = static_cast<float> (perlinBrightnessValue) / 100.0f;
        const auto roughness = 1.0f + static_cast<float> (perlinRoughnessValue) * 0.035f;
        const auto persistence = 0.2f + static_cast<float> (perlinPersistenceValue) * 0.0055f;
        perlinNoise.setSeed (perlinSeedValue);
        auto applyPerlin = [&] (NdlrEngine::MotifSettings& settings, int motif)
        {
            static constexpr std::array<int, 3> maxima { 20, 40, 60 };
            const auto maximum = maxima[static_cast<size_t> (
                juce::jlimit (0, 2, settings.patternType))];
            for (auto step = 0; step < settings.length; ++step)
                settings.patternSteps[static_cast<size_t> (step)] = perlinNoise.patternValue (
                    step, settings.length, motif, maximum, x, y,
                    static_cast<float> (perlinZoomValue),
                    static_cast<float> (perlinSpacingValue),
                    roughness, persistence, brightness);
            settings.customPattern = true;
            settings.customPatternSource = settings.pattern;
        };
        if (perlinM1->load() >= 0.5f) applyPerlin (motif1Settings, 0);
        if (perlinM2->load() >= 0.5f) applyPerlin (motif2Settings, 1);
    }

    engine.process (midi, audio.getNumSamples(), position, hasHostPosition,
                    transportSettings, harmonySettings, motif1Settings, motif2Settings, droneSettings, padSettings);
}

juce::AudioProcessorEditor* NdlrAudioProcessor::createEditor()
{
    return new NdlrAudioProcessorEditor (*this);
}

void NdlrAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void NdlrAudioProcessor::setStateInformation (const void* data, int size)
{
    if (const auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto restoredState = juce::ValueTree::fromXml (*xml);
            migrateStateTree (restoredState);
            parameters.replaceState (restoredState);
            pendingMidiKey.store (-1);
            pendingMidiDegree.store (-1);
            refreshRhythmMemoryCache();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NdlrAudioProcessor();
}
