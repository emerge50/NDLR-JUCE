#include "RhythmWheel.h"
#include "NdlrLookAndFeel.h"

#include <cmath>

namespace ndlr::ui
{
void RhythmWheel::paint (juce::Graphics& g)
{
    // Géométrie et couleurs reprises de ndlr_rhythm_orbit.js.
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto radius = juce::jmax (15.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.43f);
    const auto ringColour = juce::Colour::fromFloatRGBA (0.12f, 0.23f, 0.24f, 1.0f);
    const auto spokeColour = accentColour.withAlpha (0.85f);
    const auto noteColour = accentColour;
    const auto tieColour = juce::Colour::fromFloatRGBA (0.0f, 0.90f, 0.38f, 1.0f);
    const auto restColour = juce::Colour::fromFloatRGBA (1.0f, 0.05f, 0.08f, 1.0f);
    const auto selectColour = juce::Colour::fromFloatRGBA (0.96f, 1.0f, 0.0f, 1.0f);
    const auto playColour = juce::Colour::fromFloatRGBA (0.9882f, 0.9882f, 0.9882f, 0.372f);

    g.setColour (ringColour);
    g.drawEllipse (juce::Rectangle<float> { radius * 2.0f, radius * 2.0f }.withCentre (centre), 1.2f);

    for (auto step = 0; step < length; ++step)
    {
        const auto point = nodePosition (step);
        const auto state = states[static_cast<size_t> (step)];
        const auto isSelected = step == selectedStep;

        if (state == 1)
        {
            const auto angle = juce::MathConstants<float>::twoPi * static_cast<float> (step)
                             / static_cast<float> (length);
            const auto ratchetCount = juce::jlimit (1, 4, ratchets[static_cast<size_t> (step)]);
            auto spokeLength = static_cast<float> (velocities[static_cast<size_t> (step)])
                             / 127.0f * radius * 0.70f;
            // Les segments de ratchet ne doivent jamais traverser le cercle
            // central (rayon 41 px). La valeur MIDI de vélocité ne change pas.
            if (ratchetCount > 1)
                spokeLength = juce::jmin (spokeLength, juce::jmax (0.0f, radius - 45.0f));
            const auto end = point - juce::Point<float> { std::sin (angle), -std::cos (angle) }
                                           * spokeLength;
            g.setColour (isSelected ? selectColour : spokeColour);
            const auto delta = end - point;
            for (auto hit = 0; hit < ratchetCount; ++hit)
            {
                const auto segmentStart = static_cast<float> (hit) / static_cast<float> (ratchetCount);
                const auto segmentEnd = segmentStart + 0.8f / static_cast<float> (ratchetCount);
                g.drawLine ({ point + delta * segmentStart, point + delta * segmentEnd },
                            isSelected ? 3.0f : 2.0f);
            }
        }
        else if (state == 2)
        {
            const auto previous = nodePosition ((step - 1 + length) % length);
            g.setColour (isSelected ? selectColour : tieColour);
            g.drawLine ({ previous, point }, isSelected ? 3.0f : 2.0f);
        }

        if (step == playhead)
        {
            g.setColour (playColour);
            g.fillEllipse (juce::Rectangle<float> { 20.0f, 20.0f }.withCentre (point));
        }

        g.setColour (isSelected ? selectColour : state == 0 ? restColour
                                                             : state == 2 ? tieColour : noteColour);
        const auto nodeDiameter = isSelected ? 14.0f : 11.0f;
        g.fillEllipse (juce::Rectangle<float> { nodeDiameter, nodeDiameter }.withCentre (point));
        if (isSelected)
        {
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> { 4.0f, 4.0f }.withCentre (point));
        }
    }

    g.setColour (accentColour);
    g.drawEllipse (juce::Rectangle<float> { 82.0f, 82.0f }.withCentre (centre), 1.5f);
    g.setFont (juce::FontOptions { 8.0f, juce::Font::bold });
    g.drawText ("DIVISION", juce::Rectangle<float> { 56.0f, 12.0f }.withCentre (
                    centre.translated (0.0f, -25.0f)), juce::Justification::centred);
    g.setFont (juce::FontOptions { 16.0f, juce::Font::bold });
    g.drawText ("-", juce::Rectangle<float> { 18.0f, 22.0f }.withCentre (
                    centre.translated (-27.0f, -7.0f)), juce::Justification::centred);
    g.drawText ("+", juce::Rectangle<float> { 18.0f, 22.0f }.withCentre (
                    centre.translated (27.0f, -7.0f)), juce::Justification::centred);
    g.setFont (juce::FontOptions { 15.0f, juce::Font::bold });
    g.drawText (divisionLabel, juce::Rectangle<float> { 48.0f, 22.0f }.withCentre (
                    centre.translated (0.0f, -7.0f)),
                juce::Justification::centred);
    g.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
    static const juce::StringArray velocityModeLabels { "=", "UP", "DN" };
    g.drawText (velocityModeLabels[ratchetVelocityMode],
                juce::Rectangle<float> { 42.0f, 18.0f }.withCentre (centre.translated (0.0f, 13.0f)),
                juce::Justification::centred);
}

void RhythmWheel::mouseDown (const juce::MouseEvent& event)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    if (event.position.getDistanceFrom (centre) < 41.0f)
    {
        if (event.mods.isAltDown())
        {
            ratchetVelocityMode = (ratchetVelocityMode + 1) % 3;
            if (onRatchetVelocityModeChanged) onRatchetVelocityModeChanged (ratchetVelocityMode);
        }
        else if (onDivisionStep)
        {
            onDivisionStep (event.position.x < centre.x ? -1 : 1);
        }
        repaint();
        return;
    }

    auto closest = -1;
    auto closestDistance = 18.0f;
    for (auto step = 0; step < length; ++step)
    {
        const auto distance = event.position.getDistanceFrom (nodePosition (step));
        if (distance < closestDistance)
        {
            closest = step;
            closestDistance = distance;
        }
    }

    if (closest < 0) return;
    selectedStep = closest;
    dragStep = closest;
    if (event.mods.isAltDown() && states[static_cast<size_t> (closest)] == 1)
    {
        auto& ratchet = ratchets[static_cast<size_t> (closest)];
        ratchet = ratchet % 4 + 1;
        if (onRatchetChanged) onRatchetChanged (closest, ratchet);
    }
    else if (event.mods.isShiftDown())
    {
        auto& state = states[static_cast<size_t> (closest)];
        state = (state + 1) % 3; // NOTE -> TIE -> REST -> NOTE
        if (onStateChanged) onStateChanged (closest, state);
    }
    repaint();
}

void RhythmWheel::mouseWheelMove (const juce::MouseEvent& event,
                                  const juce::MouseWheelDetails& wheel)
{
    const auto centre = getLocalBounds().toFloat().getCentre();
    if (event.position.getDistanceFrom (centre) >= 41.0f || wheel.deltaY == 0.0f) return;
    if (onDivisionStep) onDivisionStep (wheel.deltaY > 0.0f ? 1 : -1);
}

void RhythmWheel::mouseDrag (const juce::MouseEvent& event)
{
    if (dragStep < 0 || states[static_cast<size_t> (dragStep)] != 1) return;

    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto anchor = nodePosition (dragStep);
    auto axis = centre - anchor;
    const auto magnitude = axis.getDistanceFromOrigin();
    if (magnitude <= 0.0f) return;
    axis /= magnitude;

    const auto pointer = event.position - anchor;
    const auto projection = pointer.x * axis.x + pointer.y * axis.y;
    const auto radius = juce::jmax (15.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.43f);
    const auto velocity = juce::jlimit (1, 127,
        juce::roundToInt (projection / (radius * 0.70f) * 127.0f));

    auto& current = velocities[static_cast<size_t> (dragStep)];
    if (current == velocity) return;
    current = velocity;
    if (onVelocityChanged) onVelocityChanged (dragStep, velocity);
    repaint();
}

void RhythmWheel::setLength (int newLength)
{
    newLength = juce::jlimit (4, 32, newLength);
    if (length == newLength) return;
    length = newLength;
    selectedStep = juce::jlimit (0, length - 1, selectedStep);
    dragStep = dragStep < 0 ? -1 : juce::jlimit (0, length - 1, dragStep);
    repaint();
}

void RhythmWheel::setStates (const std::array<int, 32>& newStates)
{
    if (states == newStates) return;
    states = newStates;
    repaint();
}

void RhythmWheel::setVelocities (const std::array<int, 32>& newVelocities)
{
    if (velocities == newVelocities) return;
    velocities = newVelocities;
    repaint();
}

void RhythmWheel::setRatchets (const std::array<int, 32>& newRatchets)
{
    if (ratchets == newRatchets) return;
    ratchets = newRatchets;
    repaint();
}

void RhythmWheel::setPlayhead (int newPlayhead)
{
    if (playhead == newPlayhead) return;
    playhead = newPlayhead;
    repaint();
}

void RhythmWheel::setDivisionLabel (juce::String newLabel)
{
    if (divisionLabel == newLabel) return;
    divisionLabel = std::move (newLabel);
    repaint();
}

void RhythmWheel::setRatchetVelocityMode (int newMode)
{
    newMode = juce::jlimit (0, 2, newMode);
    if (ratchetVelocityMode == newMode) return;
    ratchetVelocityMode = newMode;
    repaint();
}

juce::Point<float> RhythmWheel::nodePosition (int step) const
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto radius = juce::jmax (15.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.43f);
    const auto angle = juce::MathConstants<float>::twoPi * static_cast<float> (step)
                     / static_cast<float> (length);
    return centre + juce::Point<float> { std::sin (angle), -std::cos (angle) } * radius;
}
}
