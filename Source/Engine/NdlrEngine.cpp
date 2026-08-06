#include "NdlrEngine.h"
#include "RhythmUtilities.h"
#include "TimingUtilities.h"

#include <array>
#include <cmath>
#include <limits>

void NdlrEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void NdlrEngine::reset()
{
    motif1Runtime.reset();
    motif2Runtime.reset();
    droneNoteCount = 0;
    droneLastBoundary = -1;
    padNoteCount = 0;
    padNoteSounded.fill (false);
    padNextStrumNote = 0;
    padPendingTrigger = false;
    padPendingTriggerPpq = 0.0;
    padLastVelocity = padLastChannel = padLastPolyChain = padLastStrumDivision = -1;
    padLastQuantize = -1;
    lastHarmonyTriggerCounter = 0;
    for (auto& channel : noteOwners) channel.fill (0);
    transportRunning.store (false);
    motif1Runtime.patternStep.store (-1);
    motif1Runtime.rhythmStep.store (-1);
    internalPpq = 0.0;
    wasInternalPlaying = false;
}

void NdlrEngine::panic (juce::MidiBuffer& midi)
{
    stopActiveNote (motif1Runtime, midi, 0);
    stopActiveNote (motif2Runtime, midi, 0);
    for (auto channel = 1; channel <= 16; ++channel)
        midi.addEvent (juce::MidiMessage::allNotesOff (channel), 0);
    motif1Runtime.reset();
    motif2Runtime.reset();
    droneNoteCount = 0;
    droneLastBoundary = -1;
    padNoteCount = 0;
    padNoteSounded.fill (false);
    padNextStrumNote = 0;
    padPendingTrigger = false;
    padPendingTriggerPpq = 0.0;
    lastHarmonyTriggerCounter = 0;
    for (auto& channel : noteOwners) channel.fill (0);
}

void NdlrEngine::process (juce::MidiBuffer& midi, int numSamples,
                          const juce::AudioPlayHead::PositionInfo& position,
                          bool hasHostPosition,
                          const TransportSettings& transportSettings,
                          const HarmonySettings& harmonySettings,
                          const MotifSettings& motif1Settings,
                          const MotifSettings& motif2Settings,
                          const DroneSettings& droneSettings,
                          const PadSettings& padSettings)
{
    const auto hostPpq = position.getPpqPosition();
    const auto hostIsPlaying = hasHostPosition && position.getIsPlaying()
                            && static_cast<bool> (hostPpq);
    const auto internalIsPlaying = ! hostIsPlaying && transportSettings.internalPlaying;
    const auto isPlaying = hostIsPlaying || internalIsPlaying;
    double currentPpq = 0.0;

    if (hostIsPlaying)
    {
        currentPpq = *hostPpq;
        if (const auto bpm = position.getBpm())
            tempo.store (*bpm);
        wasInternalPlaying = false;
    }
    else
    {
        const auto internalTempo = juce::jlimit (20.0, 300.0, transportSettings.internalTempo);
        tempo.store (internalTempo);

        if (internalIsPlaying && ! wasInternalPlaying)
            internalPpq = 0.0;

        currentPpq = internalPpq;
        if (internalIsPlaying)
            internalPpq += static_cast<double> (numSamples) * internalTempo
                         / (60.0 * sampleRate);

        wasInternalPlaying = internalIsPlaying;
    }

    transportRunning.store (isPlaying);
    const auto harmonyRetrigger = harmonySettings.triggerCounter != lastHarmonyTriggerCounter;
    lastHarmonyTriggerCounter = harmonySettings.triggerCounter;
    processMotif (motif1Runtime, midi, numSamples, currentPpq, isPlaying,
                  harmonySettings, motif1Settings);
    processMotif (motif2Runtime, midi, numSamples, currentPpq, isPlaying,
                  harmonySettings, motif2Settings);

    auto stopPad = [&]
    {
        for (auto i = 0; i < padNoteCount; ++i)
            if (padNoteSounded[static_cast<size_t> (i)])
                emitNoteOff (midi, padChannels[static_cast<size_t> (i)],
                             padNotes[static_cast<size_t> (i)], 0);
        padNoteCount = 0;
        padNextStrumNote = 0;
        padPendingTrigger = false;
        padNoteSounded.fill (false);
    };

    const auto padSamplesPerQuarter = sampleRate * 60.0 / tempo.load();
    const auto padEndPpq = currentPpq + static_cast<double> (numSamples) / padSamplesPerQuarter;
    const auto startPad = [&] (double triggerPpq)
    {
        const auto offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
            juce::roundToInt ((triggerPpq - currentPpq) * padSamplesPerQuarter));
        const auto initialCount = padSettings.strum
            ? juce::jmin (padNoteCount, padSettings.group ? 3 : 1) : padNoteCount;
        for (auto i = 0; i < initialCount; ++i)
        {
            const auto index = static_cast<size_t> (i);
            emitNoteOn (midi, padChannels[index], padNotes[index],
                        juce::jlimit (1, 127, padSettings.velocity), offset);
            padNoteSounded[index] = true;
        }
        padNextStrumNote = initialCount;
        padNextStrumPpq = triggerPpq + padStrumIntervalPpq;
        padPendingTrigger = false;
    };

    if (isPlaying && padSettings.enabled && padPendingTrigger
        && padPendingTriggerPpq < padEndPpq - 1.0e-12)
        startPad (padPendingTriggerPpq);

    while (isPlaying && padSettings.enabled
           && padNextStrumNote > 0 && padNextStrumNote < padNoteCount
           && padNextStrumPpq < padEndPpq - 1.0e-12)
    {
        const auto offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
            juce::roundToInt ((padNextStrumPpq - currentPpq) * padSamplesPerQuarter));
        const auto index = static_cast<size_t> (padNextStrumNote);
        emitNoteOn (midi, padChannels[index], padNotes[index],
                    juce::jlimit (1, 127, padSettings.velocity), offset);
        padNoteSounded[index] = true;
        ++padNextStrumNote;
        padNextStrumPpq += padStrumIntervalPpq;
    }

    if (! isPlaying || ! padSettings.enabled)
        stopPad();
    else
    {
        const auto voicing = calculatePadVoicing (harmonySettings, padSettings);
        auto wanted = voicing.notes;
        auto wantedCount = voicing.noteCount;

        const auto inversionMode = juce::jlimit (0, 3, padSettings.inversionMode);
        if (inversionMode == 1 || inversionMode == 2)
            wanted = rotatePadVoicing (wanted, wantedCount, inversionMode);
        else if (inversionMode == 3 && padNoteCount > 0)
            wanted = minimisePadVoiceMovement (
                wanted, wantedCount, padNotes, padNoteCount);

        auto changed = wantedCount != padNoteCount;
        for (auto i = 0; i < wantedCount && ! changed; ++i)
            changed = wanted[static_cast<size_t> (i)] != padNotes[static_cast<size_t> (i)];
        const auto articulationChanged = padSettings.velocity != padLastVelocity
            || padSettings.midiChannel != padLastChannel || padSettings.polyChain != padLastPolyChain
            || padSettings.strum != padLastStrum || padSettings.group != padLastGroup
            || inversionMode != padLastInversionMode
            || padSettings.strumDivision != padLastStrumDivision
            || padSettings.quantize != padLastQuantize;

        if (changed || articulationChanged || harmonyRetrigger)
        {
            stopPad();
            padNotes = wanted;
            padNoteCount = wantedCount;
            padNoteSounded.fill (false);
            for (auto i = 0; i < padNoteCount; ++i)
                padChannels[static_cast<size_t> (i)] =
                    (juce::jlimit (1, 16, padSettings.midiChannel) - 1
                     + i % juce::jlimit (1, 4, padSettings.polyChain)) % 16 + 1;

            static constexpr std::array<int, 21> ticks {
                576,384,256,288,192,128,144,96,64,72,48,32,36,24,16,18,12,8,9,6,3
            };
            padStrumIntervalPpq = static_cast<double> (
                ticks[static_cast<size_t> (juce::jlimit (0, 20, padSettings.strumDivision))]) / ppqn;
            const auto quantize = juce::jlimit (0, 2, padSettings.quantize);
            const auto grid = quantize == 1 ? 1.0 : 0.5;
            const auto triggerPpq = quantize == 0 ? currentPpq
                : std::ceil (currentPpq / grid - 1.0e-9) * grid;
            if (triggerPpq < padEndPpq - 1.0e-12)
                startPad (triggerPpq);
            else
            {
                padPendingTrigger = true;
                padPendingTriggerPpq = triggerPpq;
            }
        }

        padLastVelocity = padSettings.velocity;
        padLastChannel = padSettings.midiChannel;
        padLastPolyChain = padSettings.polyChain;
        padLastStrum = padSettings.strum;
        padLastGroup = padSettings.group;
        padLastInversionMode = inversionMode;
        padLastStrumDivision = padSettings.strumDivision;
        padLastQuantize = padSettings.quantize;
    }

    auto stopDrone = [&]
    {
        for (auto i = 0; i < droneNoteCount; ++i)
            emitNoteOff (midi, droneActiveChannel,
                         droneNotes[static_cast<size_t> (i)], 0);
        droneNoteCount = 0;
    };
    if (! isPlaying || ! droneSettings.enabled)
    {
        stopDrone();
        droneLastBoundary = -1;
        return;
    }

    const auto trigger = juce::jlimit (1, 19, droneSettings.trigger);
    const auto chordMode = trigger >= 9;
    MotifSettings rootLookup;
    rootLookup.patternType = 1;
    rootLookup.position = 2;
    const auto rootClass = getMidiNote (harmonySettings, rootLookup,
                                        chordMode ? harmonySettings.degree + 1 : 1) % 12;
    auto base = 12 * (juce::jlimit (0, 3, droneSettings.position) + 1) + rootClass;
    std::array<int, 3> wanted { base, base + 12, base + 7 };
    auto wantedCount = 1;
    if (droneSettings.type == 1) wanted[1] = base + 12, wantedCount = 2;
    if (droneSettings.type == 2) wanted[1] = base + 7, wantedCount = 2;
    if (droneSettings.type >= 3) wanted[1] = base + 7, wanted[2] = base + 12, wantedCount = 3;

    const auto sustain = trigger == 1 || trigger == 9;
    const auto grid = trigger >= 17 ? 0.5 : 1.0;
    const auto boundary = static_cast<int64_t> (std::floor (currentPpq / grid + 1.0e-9));
    const auto requestedDroneChannel = juce::jlimit (1, 16, droneSettings.midiChannel);
    auto shouldFire = sustain && (wantedCount != droneNoteCount || wanted != droneNotes
                                  || requestedDroneChannel != droneActiveChannel
                                  || harmonyRetrigger);
    if (! sustain && boundary != droneLastBoundary)
    {
        const auto beat = static_cast<int> (std::floor (currentPpq + 1.0e-9));
        const auto cadence = trigger <= 8 ? trigger - 1 : trigger - 9;
        if (trigger >= 17)
        {
            const auto tick = static_cast<int> (std::llround (currentPpq * ppqn)) % (ppqn * 4);
            shouldFire = trigger == 17 ? (tick == 0 || tick == 144 || tick == 240 || tick == 336)
                       : trigger == 18 ? (tick == 144 || tick == 240 || tick == 288)
                                       : (tick == 0 || tick == 144 || tick == 192 || tick == 288);
        }
        else
            shouldFire = cadence == 1 ? beat % 4 == 0
                       : cadence == 2 ? (beat % 4 == 0 || beat % 4 == 1)
                       : cadence == 3 ? (beat % 4 == 0 || beat % 4 == 2)
                       : cadence == 4 ? (beat % 4 == 1 || beat % 4 == 3)
                       : cadence == 5 ? true : cadence == 6 ? beat % 3 == 0
                       : cadence == 7 ? beat % 5 == 0 : false;
    }
    droneLastBoundary = boundary;
    if (shouldFire)
    {
        stopDrone();
        droneNotes = wanted;
        droneNoteCount = wantedCount;
        droneActiveChannel = requestedDroneChannel;
        for (auto i = 0; i < droneNoteCount; ++i)
            emitNoteOn (midi, droneActiveChannel,
                        droneNotes[static_cast<size_t> (i)],
                        juce::jlimit (1, 127, droneSettings.velocity), 0);
    }
}

void NdlrEngine::processMotif (MotifRuntimeState& runtime,
                               juce::MidiBuffer& midi, int numSamples,
                               double currentPpq, bool isPlaying,
                               const HarmonySettings& harmonySettings,
                               const MotifSettings& settings)
{
    const auto length = juce::jlimit (1, 16, settings.length);
    runtime.patternLength.store (length);

    if (! isPlaying || ! settings.enabled)
    {
        stopActiveNote (runtime, midi, 0);
        runtime.patternStep.store (-1);
        runtime.rhythmStep.store (-1);
        runtime.noteCounter = 0;
        runtime.lastRhythmBoundary = -1;
        runtime.pendingRatchets = 0;
        runtime.scheduledNoteOff = false;
        return;
    }

    const auto divisionTicks = getDivisionTicks (settings.divisionIndex);
    const auto stepPpq = static_cast<double> (divisionTicks) / ppqn;
    const auto absoluteTick = static_cast<int64_t> (std::floor (currentPpq * ppqn + 1.0e-9));
    const auto stepNumber = absoluteTick / divisionTicks;
    const auto rhythmLength = juce::jlimit (4, 32, settings.rhythmLength);
    const auto signedRotation = juce::jlimit (-(rhythmLength - 1), rhythmLength - 1,
                                              settings.rotation);
    // Convention UI : une valeur positive tourne la roue dans le sens horaire.
    const auto rotation = (((-signedRotation) % rhythmLength) + rhythmLength) % rhythmLength;
    const auto euclideanPattern = ndlr::rhythm::euclidean (
        rhythmLength, juce::jlimit (1, rhythmLength, settings.euclideanPulses), rotation);
    runtime.rhythmStep.store (static_cast<int> (
        ndlr::timing::positiveModulo (stepNumber, static_cast<int64_t> (rhythmLength))));

    const auto currentTempo = tempo.load();
    const auto samplesPerQuarter = sampleRate * 60.0 / currentTempo;
    const auto endPpq = currentPpq + static_cast<double> (numSamples) / samplesPerQuarter;
    auto boundaryNumber = static_cast<int64_t> (std::ceil (currentPpq / stepPpq - 1.0e-9));

    if (runtime.scheduledNoteOff && runtime.scheduledNoteOffPpq < endPpq - 1.0e-12)
    {
        const auto offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
            juce::roundToInt ((runtime.scheduledNoteOffPpq - currentPpq) * samplesPerQuarter));
        emitNoteOff (midi, runtime.scheduledChannel, runtime.scheduledNote, offset);
        runtime.scheduledNoteOff = false;
        runtime.noteIsActive = false;
    }

    auto scheduleGateOff = [&] (double startPpq, double durationPpq, int channel, int note)
    {
        const auto offPpq = startPpq + durationPpq;
        if (offPpq < endPpq - 1.0e-12)
        {
            const auto offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                juce::roundToInt ((offPpq - currentPpq) * samplesPerQuarter));
            emitNoteOff (midi, channel, note, offset);
            runtime.noteIsActive = false;
        }
        else
        {
            runtime.scheduledNoteOff = true;
            runtime.scheduledNoteOffPpq = offPpq;
            runtime.scheduledChannel = channel;
            runtime.scheduledNote = note;
        }
    };

    auto emitPendingRatchets = [&]
    {
        while (runtime.pendingRatchets > 0 && runtime.nextRatchetPpq < endPpq - 1.0e-12)
        {
            if (runtime.nextRatchetPpq >= currentPpq - 1.0e-12)
            {
                const auto offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                    juce::roundToInt ((runtime.nextRatchetPpq - currentPpq) * samplesPerQuarter));
                runtime.scheduledNoteOff = false;
                stopActiveNote (runtime, midi, offset);
                auto hitVelocity = runtime.ratchetVelocity;
                if (settings.ratchetVelocityMode == 1)
                    hitVelocity = juce::jmax (1, runtime.ratchetVelocity * (runtime.ratchetHit + 1) / runtime.ratchetTotal);
                else if (settings.ratchetVelocityMode == 2)
                    hitVelocity = juce::jmax (1, runtime.ratchetVelocity * (runtime.ratchetTotal - runtime.ratchetHit) / runtime.ratchetTotal);

                emitNoteOn (midi, runtime.ratchetChannel, runtime.ratchetNote,
                            hitVelocity, offset);
                runtime.noteIsActive = true;
                runtime.activeNote = runtime.ratchetNote;
                runtime.activeChannel = runtime.ratchetChannel;
                if (! (runtime.ratchetTieAfter && runtime.ratchetHit == runtime.ratchetTotal - 1))
                    scheduleGateOff (runtime.nextRatchetPpq,
                                     runtime.ratchetIntervalPpq * juce::jlimit (5, 127, settings.gate) / 127.0,
                                     runtime.ratchetChannel, runtime.ratchetNote);
            }

            runtime.nextRatchetPpq += runtime.ratchetIntervalPpq;
            --runtime.pendingRatchets;
            ++runtime.ratchetHit;
        }
    };

    // Termine d'abord les sous-frappes commencées dans le bloc précédent.
    emitPendingRatchets();

    for (;; ++boundaryNumber)
    {
        const auto boundaryPpq = static_cast<double> (boundaryNumber) * stepPpq;
        if (boundaryPpq >= endPpq - 1.0e-12)
            break;

        const auto sampleOffset = juce::jlimit (
            0, juce::jmax (0, numSamples - 1),
            juce::roundToInt ((boundaryPpq - currentPpq) * samplesPerQuarter));
        if (runtime.lastRhythmBoundary >= 0 && boundaryNumber <= runtime.lastRhythmBoundary)
            runtime.noteCounter = 0;
        runtime.lastRhythmBoundary = boundaryNumber;

        const auto rhythmStep = static_cast<int> (
            ndlr::timing::positiveModulo (boundaryNumber, static_cast<int64_t> (rhythmLength)));
        runtime.rhythmStep.store (rhythmStep);
        const auto sourceStep = settings.rhythmMode == 1
                              ? rhythmStep : (rhythmStep + rotation) % rhythmLength;
        const auto rhythmState = settings.rhythmMode == 1
                               ? euclideanPattern[static_cast<size_t> (rhythmStep)]
                               : settings.rhythmState[static_cast<size_t> (sourceStep)];

        if (rhythmState == 0) // REST coupe la note et n'avance pas le pattern.
        {
            stopActiveNote (runtime, midi, sampleOffset);
            continue;
        }

        if (rhythmState == 2) // TIE prolonge la note et n'avance pas le pattern.
            continue;

        const auto variedStep = applyVariation (runtime.noteCounter++, length, settings.variation);
        const auto useStoredPattern = settings.pattern >= 20
                                   || (settings.customPattern
                                       && settings.pattern == settings.customPatternSource);
        const auto originalPatternValue = useStoredPattern
            ? juce::jmax (1, settings.patternSteps[static_cast<size_t> (variedStep)])
            : getPatternValue (settings.pattern, variedStep);
        // 50 % conserve la forme d'origine. Les deux extremites poussent tous
        // les pas vers le minimum ou le maximum du type de pattern actif.
        static constexpr std::array<int, 3> patternMaxima { 20, 40, 60 };
        const auto patternMaximum = patternMaxima[static_cast<size_t> (
            juce::jlimit (0, 2, settings.patternType))];
        const auto patternValue = applyPatternAmplitude (
            originalPatternValue, settings.patternAmplitude, patternMaximum);
        const auto note = getMidiNote (harmonySettings, settings, patternValue);
        const auto channel = juce::jlimit (1, 16, settings.midiChannel);
        const auto stepVelocity = juce::jlimit (1, 127,
            settings.rhythmVelocity[static_cast<size_t> (sourceStep)]);
        auto velocity = juce::roundToInt (static_cast<float> (stepVelocity * settings.velocity) / 127.0f);
        if (settings.accent == 3)
            velocity = settings.velocity;
        else if (settings.accent >= 4)
        {
            static constexpr std::array<std::array<int, 8>, 7> accentPatterns {{
                {{1,0,0,0,0,0,0,0}}, {{1,0,0,0,0,0,0,0}}, {{1,0,1,0,0,0,0,0}},
                {{1,0,0,0,0,0,0,0}}, {{1,1,0,0,0,0,0,0}}, {{1,0,0,1,0,0,0,0}},
                {{1,0,1,0,1,0,0,0}}
            }};
            static constexpr std::array<int, 7> accentLengths { 2, 4, 6, 3, 4, 6, 8 };
            const auto accentIndex = juce::jlimit (4, 10, settings.accent) - 4;
            const auto& pattern = accentPatterns[static_cast<size_t> (juce::jlimit (4, 10, settings.accent) - 4)];
            velocity = pattern[static_cast<size_t> (rhythmStep % accentLengths[static_cast<size_t> (accentIndex)])] != 0
                     ? settings.velocity
                     : juce::roundToInt (static_cast<float> (settings.velocity) * 0.55f);
        }
        // Accent 2 is the dedicated humanized-rhythm mode. Keeping the jitter
        // here makes Accent 1 (raw rhythm velocity) and the fixed/preset modes
        // behave distinctly, as their explicit menu labels indicate.
        if (settings.accent == 2 && settings.humanize > 0)
        {
            auto hash = static_cast<uint32_t> (boundaryNumber * 747796405u + 2891336453u);
            hash = (hash >> ((hash >> 28) + 4)) ^ hash;
            const auto range = juce::jmax (1, juce::roundToInt (
                static_cast<float> (settings.velocity * settings.humanize) / 10.0f));
            const auto jitter = static_cast<int> (hash % static_cast<uint32_t> (range * 2 + 1)) - range;
            velocity += jitter;
        }
        velocity = juce::jlimit (1, 127, velocity);

        const auto ratchetCount = juce::jlimit (1, 4,
            settings.rhythmRatchet[static_cast<size_t> (sourceStep)]);
        const auto nextRhythmStep = (rhythmStep + 1) % rhythmLength;
        const auto nextSourceStep = settings.rhythmMode == 1
                                  ? nextRhythmStep : (nextRhythmStep + rotation) % rhythmLength;
        const auto nextState = settings.rhythmMode == 1
                             ? euclideanPattern[static_cast<size_t> (nextRhythmStep)]
                             : settings.rhythmState[static_cast<size_t> (nextSourceStep)];
        const auto tieAfter = nextState == 2;
        auto firstVelocity = velocity;
        if (settings.ratchetVelocityMode == 1 && ratchetCount > 1)
            firstVelocity = juce::jmax (1, velocity / ratchetCount);

        runtime.scheduledNoteOff = false;
        stopActiveNote (runtime, midi, sampleOffset);
        emitNoteOn (midi, channel, note, firstVelocity, sampleOffset);
        runtime.noteIsActive = true;
        runtime.activeNote = note;
        runtime.activeChannel = channel;
        runtime.patternStep.store (variedStep);
        if (! (tieAfter && ratchetCount == 1))
            scheduleGateOff (boundaryPpq,
                             stepPpq / static_cast<double> (ratchetCount)
                                * juce::jlimit (5, 127, settings.gate) / 127.0,
                             channel, note);

        runtime.pendingRatchets = ratchetCount - 1;
        runtime.ratchetNote = note;
        runtime.ratchetChannel = channel;
        runtime.ratchetVelocity = velocity;
        runtime.ratchetTotal = ratchetCount;
        runtime.ratchetHit = 1;
        runtime.ratchetTieAfter = tieAfter;
        runtime.ratchetIntervalPpq = stepPpq / static_cast<double> (ratchetCount);
        runtime.nextRatchetPpq = boundaryPpq + runtime.ratchetIntervalPpq;
        emitPendingRatchets();
    }
}
int NdlrEngine::getDivisionTicks (int divisionIndex) noexcept
{
    // Musical divisions expressed on NDLR's 96 PPQN clock.
    static constexpr std::array<int, 21> ticks {
        576, 384, 256, 288, 192, 128, 144, 96, 64, 72, 48,
        32, 36, 24, 16, 18, 12, 8, 9, 6, 3
    };

    return ticks[static_cast<size_t> (juce::jlimit (0, static_cast<int> (ticks.size()) - 1,
                                                    divisionIndex))];
}

int NdlrEngine::applyVariation (int64_t stepNumber, int length, int variation) noexcept
{
    const auto forward = static_cast<int> (stepNumber % length);

    switch (variation)
    {
        case 1: // Reverse
            return length - 1 - forward;

        case 2: // Ping-pong
        {
            const auto period = juce::jmax (1, length * 2 - 2);
            const auto position = static_cast<int> (stepNumber % period);
            return position < length ? position : period - position;
        }

        case 3: // Ping-pong with held end points
        {
            const auto period = length * 2;
            const auto position = static_cast<int> (stepNumber % period);
            return position < length ? position : period - 1 - position;
        }

        case 4: // Odd steps, then even steps
        {
            const auto oddCount = length / 2;
            return forward < oddCount ? forward * 2 + 1 : (forward - oddCount) * 2;
        }

        case 5: // Deterministic random order, stable when the host seeks
        {
            auto value = static_cast<uint32_t> (stepNumber);
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return static_cast<int> (value % static_cast<uint32_t> (length));
        }

        default:
            return forward;
    }
}

int NdlrEngine::getPatternValue (int pattern, int motifStep) noexcept
{
    static constexpr std::array<std::array<int, 16>, 20> patterns {{
        {{0,1,2,3,4,5,6,7,6,5,4,3,2,1,0,1}}, {{0,2,4,1,3,5,2,4,6,3,5,7,4,6,5,3}},
        {{0,2,0,3,0,4,0,5,0,4,0,3,0,2,0,1}}, {{0,2,1,3,2,4,3,5,4,6,5,7,6,5,4,3}},
        {{0,4,1,4,2,4,3,4,2,4,1,4,0,4,1,2}}, {{0,1,2,0,1,2,3,1,2,3,4,2,3,4,5,3}},
        {{0,2,4,6,1,3,5,7,0,2,4,6,1,3,5,4}}, {{0,2,1,2,0,2,1,2,0,3,1,3,0,3,1,3}},
        {{0,1,3,0,1,3,5,0,1,3,5,7,3,5,7,5}}, {{0,3,5,3,0,4,6,4,0,3,5,3,1,4,6,4}},
        {{0,0,1,1,2,2,3,3,4,4,5,5,4,3,2,1}}, {{0,7,1,6,2,5,3,4,3,5,2,6,1,7,0,4}},
        {{0,1,0,0,1,0,1,0,0,1,0,1,0,0,1,2}}, {{0,1,2,4,0,1,3,4,0,2,3,4,0,1,2,3}},
        {{0,2,4,7,0,2,4,7,0,2,5,7,0,3,5,7}}, {{0,4,2,6,1,5,3,7,0,4,2,6,1,5,3,5}},
        {{0,1,3,2,4,3,5,4,6,5,7,6,8,7,6,5}}, {{0,1,0,2,1,3,2,4,3,5,4,6,5,7,6,4}},
        {{0,3,1,4,2,5,3,6,4,7,5,4,3,5,2,4}}, {{0,2,5,3,0,4,6,2,0,3,5,7,2,4,6,0}}
    }};

    return patterns[static_cast<size_t> (juce::jlimit (0, 19, pattern))]
                   [static_cast<size_t> (juce::jlimit (0, 15, motifStep))] + 1;
}

int NdlrEngine::getMidiNote (const HarmonySettings& harmony,
                             const MotifSettings& motif, int patternValue) noexcept
{
    struct Scale
    {
        std::array<int, 8> intervals;
        int length;
    };

    using Formula = std::array<int, 7>;
    static constexpr int x = -1;
    static constexpr std::array<int, 12> keyMidi { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };
    static constexpr std::array<Scale, 28> scales {{
        {{{ 0,2,4,5,7,9,11,x }},7}, {{{ 0,2,3,5,7,9,10,x }},7}, {{{ 0,1,3,5,7,8,10,x }},7},
        {{{ 0,2,4,6,7,9,11,x }},7}, {{{ 0,2,4,5,7,9,10,x }},7}, {{{ 0,2,3,5,7,8,10,x }},7},
        {{{ 0,1,3,5,6,8,10,x }},7}, {{{ 0,2,3,6,7,8,10,x }},7}, {{{ 0,2,3,5,7,8,11,x }},7},
        {{{ 0,3,5,7,10,x,x,x }},5}, {{{ 0,2,4,6,8,10,x,x }},6}, {{{ 0,2,x,x,x,x,x,x }},2},
        {{{ 0,4,x,x,x,x,x,x }},2}, {{{ 0,5,x,x,x,x,x,x }},2}, {{{ 0,9,x,x,x,x,x,x }},2},
        {{{ 0,2,4,7,9,x,x,x }},5}, {{{ 0,3,5,6,7,10,x,x }},6}, {{{ 0,2,3,5,7,9,11,x }},7},
        {{{ 0,1,3,5,7,9,10,x }},7}, {{{ 0,2,4,6,8,9,11,x }},7}, {{{ 0,2,4,6,7,9,10,x }},7},
        {{{ 0,2,4,5,7,8,10,x }},7}, {{{ 0,2,3,5,6,8,10,x }},7}, {{{ 0,1,3,4,6,8,10,x }},7},
        {{{ 0,1,4,5,7,8,10,x }},7}, {{{ 0,1,4,5,7,8,11,x }},7}, {{{ 0,1,3,4,6,7,9,10 }},8},
        {{{ 0,3,4,7,8,11,x,x }},6}
    }};

    static constexpr std::array<Formula, 18> formulas7 {{
        {{0,2,4,x,x,x,x}}, {{0,2,4,6,x,x,x}}, {{0,1,4,x,x,x,x}}, {{0,3,4,x,x,x,x}},
        {{0,2,4,5,x,x,x}}, {{0,1,2,4,x,x,x}}, {{0,2,4,8,x,x,x}}, {{0,2,4,6,8,x,x}},
        {{0,3,6,x,x,x,x}}, {{0,4,x,x,x,x,x}}, {{0,2,6,x,x,x,x}}, {{0,4,8,x,x,x,x}},
        {{0,2,4,6,8,10,x}}, {{0,2,4,6,8,10,12}}, {{0,1,2,x,x,x,x}}, {{0,2,6,8,x,x,x}},
        {{0,4,6,8,x,x,x}}, {{0,3,6,9,11,x,x}}
    }};
    static constexpr std::array<Formula, 18> formulas8 {{
        {{0,2,4,x,x,x,x}}, {{0,2,4,6,x,x,x}}, {{0,1,4,x,x,x,x}}, {{0,3,4,x,x,x,x}},
        {{0,2,4,5,x,x,x}}, {{0,1,2,4,x,x,x}}, {{0,2,4,7,x,x,x}}, {{0,2,4,6,8,x,x}},
        {{0,3,6,x,x,x,x}}, {{0,4,x,x,x,x,x}}, {{0,2,6,x,x,x,x}}, {{0,4,8,x,x,x,x}},
        {{0,2,4,6,8,10,x}}, {{0,2,4,6,8,10,12}}, {{0,1,2,x,x,x,x}}, {{0,2,6,8,x,x,x}},
        {{0,4,6,8,x,x,x}}, {{0,3,6,9,11,x,x}}
    }};
    static constexpr std::array<Formula, 18> formulas6 {{
        {{0,2,4,x,x,x,x}}, {{0,2,4,5,x,x,x}}, {{0,1,4,x,x,x,x}}, {{0,3,4,x,x,x,x}},
        {{0,2,4,5,x,x,x}}, {{0,1,2,4,x,x,x}}, {{0,2,4,7,x,x,x}}, {{0,2,4,5,7,x,x}},
        {{0,3,5,x,x,x,x}}, {{0,3,x,x,x,x,x}}, {{0,2,5,x,x,x,x}}, {{0,3,7,x,x,x,x}},
        {{0,1,2,3,4,5,x}}, {{0,1,2,3,4,5,7}}, {{0,1,2,x,x,x,x}}, {{0,2,5,7,x,x,x}},
        {{0,3,5,7,x,x,x}}, {{0,2,4,6,x,x,x}}
    }};
    static constexpr std::array<Formula, 18> formulas5 {{
        {{0,1,3,x,x,x,x}}, {{0,1,3,4,x,x,x}}, {{0,1,3,x,x,x,x}}, {{0,2,3,x,x,x,x}},
        {{0,1,3,4,x,x,x}}, {{0,1,2,3,x,x,x}}, {{0,1,3,5,x,x,x}}, {{0,1,3,4,5,x,x}},
        {{0,2,4,x,x,x,x}}, {{0,3,x,x,x,x,x}}, {{0,1,4,x,x,x,x}}, {{0,3,6,x,x,x,x}},
        {{0,1,2,3,4,x,x}}, {{0,1,2,3,4,5,x}}, {{0,1,2,x,x,x,x}}, {{0,1,4,5,x,x,x}},
        {{0,3,4,5,x,x,x}}, {{0,2,3,5,x,x,x}}
    }};
    static constexpr Formula formula2 {{ 0, 1, x, x, x, x, x }};

    const auto& scale = scales[static_cast<size_t> (juce::jlimit (0, 27, harmony.scale))];
    const auto type = juce::jlimit (0, 17, harmony.chordType);
    const Formula* formula = &formulas7[static_cast<size_t> (type)];
    if (scale.length == 8) formula = &formulas8[static_cast<size_t> (type)];
    if (scale.length == 6) formula = &formulas6[static_cast<size_t> (type)];
    if (scale.length == 5) formula = &formulas5[static_cast<size_t> (type)];
    if (scale.length == 2) formula = &formula2;

    std::array<bool, 12> chordClasses {};
    for (const auto formulaIndex : *formula)
    {
        if (formulaIndex < 0)
            break;

        const auto rawIndex = juce::jlimit (0, 6, harmony.degree) + formulaIndex;
        const auto scaleIndex = rawIndex % scale.length;
        const auto octave = rawIndex / scale.length;
        const auto note = 60 + keyMidi[static_cast<size_t> (juce::jlimit (0, 11, harmony.root))]
                        + scale.intervals[static_cast<size_t> (scaleIndex)] + octave * 12;
        chordClasses[static_cast<size_t> (note % 12)] = true;
    }

    static constexpr std::array<std::array<int, 2>, 5> positionRanges {{
        {{12,47}}, {{24,59}}, {{48,71}}, {{60,83}}, {{72,95}}
    }};
    const auto range = positionRanges[static_cast<size_t> (juce::jlimit (0, 4, motif.position))];
    const auto value = juce::jmax (1, patternValue);

    if (motif.patternType == 1)
    {
        // Scale values are degrees, not indices into the subset of notes that
        // happens to fit below the current register ceiling. Keeping a fixed
        // tonic anchor means 1..7 always plays a complete seven-note scale;
        // higher values continue naturally into following octaves.
        const auto degreeIndex = value - 1;
        const auto scaleIndex = degreeIndex % scale.length;
        const auto octave = degreeIndex / scale.length;
        const auto registerRoot = 36 + juce::jlimit (0, 4, motif.position) * 12
                                + keyMidi[static_cast<size_t> (
                                    juce::jlimit (0, 11, harmony.root))];
        return juce::jlimit (0, 127,
            registerRoot + scale.intervals[static_cast<size_t> (scaleIndex)]
                         + octave * 12);
    }

    std::array<int, 36> pool {};
    auto poolSize = 0;
    for (auto note = range[0]; note <= range[1]; ++note)
        if (chordClasses[static_cast<size_t> (note % 12)])
            pool[static_cast<size_t> (poolSize++)] = note;

    if (poolSize == 0)
        return 48;

    if (motif.patternType == 2)
        return juce::jlimit (0, 127, pool[0] + value - 1);

    // Chord values are successive chord tones, not indices that wrap inside
    // the small register preview window. Continue collecting chord tones up
    // to the MIDI ceiling so 1..20 can naturally span several octaves.
    std::array<int, 128> chordPool {};
    auto chordPoolSize = 0;
    for (auto note = range[0]; note <= 127; ++note)
        if (chordClasses[static_cast<size_t> (note % 12)])
            chordPool[static_cast<size_t> (chordPoolSize++)] = note;

    if (chordPoolSize == 0)
        return 48;

    return chordPool[static_cast<size_t> (
        juce::jmin (value - 1, chordPoolSize - 1))];
}

NdlrEngine::PadVoicing NdlrEngine::calculatePadVoicing (
    const HarmonySettings& harmony, const PadSettings& pad) noexcept
{
    MotifSettings lookup;
    lookup.patternType = 0;
    lookup.position = 2;
    std::array<bool, 12> chordClasses {};
    for (auto voice = 1; voice <= 7; ++voice)
        chordClasses[static_cast<size_t> (getMidiNote (harmony, lookup, voice) % 12)] = true;

    std::array<int, 128> pool {};
    auto poolSize = 0;
    for (auto note = 0; note < 128; ++note)
        if (chordClasses[static_cast<size_t> (note % 12)])
            pool[static_cast<size_t> (poolSize++)] = note;

    PadVoicing result;
    result.targetNote = juce::roundToInt (
        static_cast<float> (juce::jlimit (0, 100, pad.position)) * 1.27f);
    auto closest = 0;
    for (auto i = 1; i < poolSize; ++i)
        if (std::abs (pool[static_cast<size_t> (i)] - result.targetNote)
            < std::abs (pool[static_cast<size_t> (closest)] - result.targetNote))
            closest = i;
    result.centreNote = pool[static_cast<size_t> (closest)];

    result.candidateCount = juce::jmin (juce::jlimit (1, 22, pad.range), poolSize);
    const auto first = juce::jlimit (0, poolSize - result.candidateCount,
                                    closest - result.candidateCount / 2);
    for (auto i = 0; i < result.candidateCount; ++i)
    {
        const auto note = pool[static_cast<size_t> (first + i)];
        result.candidates[static_cast<size_t> (i)] = note;
        const auto keep = (pad.spread <= 1 || result.candidateCount <= 4) ? true
                       : pad.spread == 2 ? (i % 2 == 0 || i == result.candidateCount - 1)
                       : pad.spread == 3 ? (i >= result.candidateCount / 3
                                            && i < result.candidateCount / 3 + 4)
                       : pad.spread == 4 ? ((i * 37 + harmony.degree * 11) % 10 < 7)
                       : pad.spread == 5 ? i % 2 == 0 : i % 3 != 1;
        if (keep)
            result.notes[static_cast<size_t> (result.noteCount++)] = note;
    }
    if (result.noteCount == 0)
        result.notes[static_cast<size_t> (result.noteCount++)] = result.centreNote;
    return result;
}

std::array<int, 22> NdlrEngine::minimisePadVoiceMovement (
    const std::array<int, 22>& notes, int noteCount,
    const std::array<int, 22>& previousNotes, int previousNoteCount) noexcept
{
    noteCount = juce::jlimit (0, static_cast<int> (notes.size()), noteCount);
    previousNoteCount = juce::jlimit (
        0, static_cast<int> (previousNotes.size()), previousNoteCount);
    if (noteCount < 1 || previousNoteCount < 1)
        return notes;

    auto requestedRegisterSum = 0;
    for (auto voice = 0; voice < noteCount; ++voice)
        requestedRegisterSum += notes[static_cast<size_t> (voice)];

    auto best = notes;
    auto bestScore = std::numeric_limits<double>::max();
    for (auto rotation = 0; rotation < noteCount; ++rotation)
    {
        std::array<int, 22> rotated {};
        for (auto voice = 0; voice < noteCount; ++voice)
        {
            auto note = notes[static_cast<size_t> ((voice + rotation) % noteCount)];
            if (voice > 0)
                while (note <= rotated[static_cast<size_t> (voice - 1)])
                    note += 12;
            rotated[static_cast<size_t> (voice)] = note;
        }

        // AUTO choisit l'inversion la plus proche du voicing precedent, mais
        // doit rester dans le registre demandé par POS. Pour chaque inversion,
        // on ne conserve donc que l'octave dont le centre est le plus proche
        // du voicing brut calculé à partir de la position courante.
        std::array<int, 22> registerCandidate {};
        auto registerCandidateIsValid = false;
        auto bestRegisterDistance = std::numeric_limits<int>::max();
        auto bestOctaveDistance = std::numeric_limits<int>::max();
        for (auto octave = -5; octave <= 5; ++octave)
        {
            std::array<int, 22> candidate {};
            auto valid = true;
            auto candidateRegisterSum = 0;
            for (auto voice = 0; voice < noteCount; ++voice)
            {
                const auto note = rotated[static_cast<size_t> (voice)] + octave * 12;
                if (note < 0 || note > 127)
                {
                    valid = false;
                    break;
                }
                candidate[static_cast<size_t> (voice)] = note;
                candidateRegisterSum += note;
            }
            if (! valid)
                continue;

            const auto registerDistance = std::abs (
                candidateRegisterSum - requestedRegisterSum);
            const auto octaveDistance = std::abs (octave);
            if (registerDistance < bestRegisterDistance
                || (registerDistance == bestRegisterDistance
                    && octaveDistance < bestOctaveDistance))
            {
                registerCandidate = candidate;
                registerCandidateIsValid = true;
                bestRegisterDistance = registerDistance;
                bestOctaveDistance = octaveDistance;
            }
        }

        if (! registerCandidateIsValid)
            continue;

        double score = rotation * 0.001;
        for (auto voice = 0; voice < noteCount; ++voice)
        {
            const auto targetIndex = noteCount == 1 ? 0
                : juce::roundToInt (static_cast<float> (voice * (previousNoteCount - 1))
                                   / static_cast<float> (noteCount - 1));
            score += std::abs (
                registerCandidate[static_cast<size_t> (voice)]
                - previousNotes[static_cast<size_t> (targetIndex)]);
        }
        if (score < bestScore)
        {
            best = registerCandidate;
            bestScore = score;
        }
    }
    return best;
}

std::array<int, 22> NdlrEngine::rotatePadVoicing (
    const std::array<int, 22>& notes, int noteCount, int rotation) noexcept
{
    noteCount = juce::jlimit (0, static_cast<int> (notes.size()), noteCount);
    if (noteCount < 2)
        return notes;
    rotation = ndlr::timing::positiveModulo (rotation, noteCount);
    if (rotation == 0)
        return notes;

    std::array<int, 22> rotated {};
    for (auto voice = 0; voice < noteCount; ++voice)
    {
        auto note = notes[static_cast<size_t> ((voice + rotation) % noteCount)];
        if (voice > 0)
            while (note <= rotated[static_cast<size_t> (voice - 1)])
                note += 12;
        rotated[static_cast<size_t> (voice)] = note;
    }

    auto best = notes;
    auto bestScore = std::numeric_limits<int>::max();
    for (auto octave = -5; octave <= 5; ++octave)
    {
        std::array<int, 22> candidate {};
        auto valid = true;
        auto score = 0;
        for (auto voice = 0; voice < noteCount; ++voice)
        {
            const auto note = rotated[static_cast<size_t> (voice)] + octave * 12;
            if (note < 0 || note > 127)
            {
                valid = false;
                break;
            }
            candidate[static_cast<size_t> (voice)] = note;
            score += std::abs (note - notes[static_cast<size_t> (voice)]);
        }
        if (valid && score < bestScore)
        {
            best = candidate;
            bestScore = score;
        }
    }
    return best;
}

void NdlrEngine::stopActiveNote (MotifRuntimeState& runtime,
                                 juce::MidiBuffer& midi, int sampleOffset)
{
    if (! runtime.noteIsActive)
        return;

    emitNoteOff (midi, runtime.activeChannel, runtime.activeNote, sampleOffset);
    runtime.noteIsActive = false;
}

void NdlrEngine::emitNoteOn (juce::MidiBuffer& midi, int channel, int note,
                             int velocity, int sampleOffset)
{
    channel = juce::jlimit (1, 16, channel);
    note = juce::jlimit (0, 127, note);
    auto& owners = noteOwners[static_cast<size_t> (channel - 1)][static_cast<size_t> (note)];
    // Certains synthés empilent les Note On identiques et réclament autant de
    // Note Off. Une réarticulation explicite évite cette pile tout en gardant
    // la note tenue par l'autre module.
    if (owners > 0)
        midi.addEvent (juce::MidiMessage::noteOff (channel, note), sampleOffset);
    owners = juce::jmin (owners + 1, 1024);
    midi.addEvent (juce::MidiMessage::noteOn (channel, note,
                                              static_cast<juce::uint8> (juce::jlimit (1, 127, velocity))),
                   sampleOffset);
}

void NdlrEngine::emitNoteOff (juce::MidiBuffer& midi, int channel, int note,
                              int sampleOffset)
{
    channel = juce::jlimit (1, 16, channel);
    note = juce::jlimit (0, 127, note);
    auto& owners = noteOwners[static_cast<size_t> (channel - 1)][static_cast<size_t> (note)];
    if (owners > 0)
        --owners;
    if (owners == 0)
        midi.addEvent (juce::MidiMessage::noteOff (channel, note), sampleOffset);
}
