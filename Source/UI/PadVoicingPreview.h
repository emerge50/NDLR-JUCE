#pragma once

#include <JuceHeader.h>
#include "../Engine/NdlrEngine.h"
#include "NdlrLookAndFeel.h"

namespace ndlr::ui
{
class PadVoicingPreview final : public juce::Component
{
public:
    void setData (const NdlrEngine::PadVoicing& newVoicing, int newPosition,
                  int newRange, int newSpread, juce::String newHarmony)
    {
        voicing = newVoicing;
        position = newPosition;
        range = newRange;
        spread = newSpread;
        harmony = std::move (newHarmony);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (background);
        auto area = getLocalBounds().toFloat().reduced (18.0f);

        g.setColour (green);
        g.setFont (juce::FontOptions { 22.0f, juce::Font::bold });
        g.drawText ("PAD VOICING", area.removeFromTop (28.0f),
                    juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
        g.drawText ("PROTOTYPE WINDOW  -  current controls remain unchanged",
                    area.removeFromTop (18.0f), juce::Justification::centredLeft);

        auto summary = area.removeFromTop (54.0f);
        drawSummary (g, summary.removeFromLeft (summary.getWidth() / 4.0f),
                     "HARMONY", harmony, green);
        drawSummary (g, summary.removeFromLeft (summary.getWidth() / 3.0f),
                     "CENTER", juce::String (position) + "  ->  " + noteName (voicing.centreNote),
                     orange);
        drawSummary (g, summary.removeFromLeft (summary.getWidth() / 2.0f),
                     "WINDOW", juce::String (range) + " candidates", cyan);
        drawSummary (g, summary, "ACTIVE", juce::String (voicing.noteCount) + " notes", green);
        area.removeFromTop (12.0f);

        auto keyboardArea = area.removeFromTop (170.0f);
        drawKeyboard (g, keyboardArea);
        area.removeFromTop (8.0f);

        auto legend = area.removeFromTop (26.0f);
        drawLegendItem (g, legend.removeFromLeft (175.0f), muted, false, "Chord candidate");
        drawLegendItem (g, legend.removeFromLeft (150.0f), green, true, "Active note");
        drawLegendItem (g, legend.removeFromLeft (175.0f), orange, false,
                        "Position / center");

        area.removeFromTop (4.0f);
        drawSpreadChoices (g, area.removeFromTop (60.0f));
    }

private:
    static bool isBlack (int note) noexcept
    {
        const auto pitch = note % 12;
        return pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
    }

    static juce::String noteName (int note)
    {
        static const std::array<const char*, 12> names {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        note = juce::jlimit (0, 127, note);
        return juce::String (names[static_cast<size_t> (note % 12)])
             + juce::String (note / 12 - 1);
    }

    static void drawSummary (juce::Graphics& g, juce::Rectangle<float> bounds,
                             const juce::String& label, const juce::String& value,
                             juce::Colour accent)
    {
        bounds = bounds.reduced (3.0f, 2.0f);
        g.setColour (panel);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (accent.withAlpha (0.58f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
        auto textArea = bounds.reduced (9.0f, 4.0f);
        g.setColour (accent.withAlpha (0.78f));
        g.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
        g.drawText (label, textArea.removeFromTop (16.0f), juce::Justification::centredLeft);
        g.setColour (text);
        g.setFont (juce::FontOptions { 14.0f, juce::Font::bold });
        g.drawFittedText (value, textArea.toNearestInt(),
                          juce::Justification::centredLeft, 1);
    }

    void drawKeyboard (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        auto minimum = voicing.candidateCount > 0 ? voicing.candidates[0] : voicing.centreNote;
        auto maximum = voicing.candidateCount > 0
            ? voicing.candidates[static_cast<size_t> (voicing.candidateCount - 1)]
            : voicing.centreNote;
        auto firstOctave = juce::jmax (0, minimum / 12 - 1);
        auto lastOctave = juce::jmin (10, maximum / 12 + 1);
        if (lastOctave - firstOctave < 5)
        {
            const auto missing = 5 - (lastOctave - firstOctave);
            firstOctave = juce::jmax (0, firstOctave - missing / 2);
            lastOctave = juce::jmin (10, firstOctave + 5);
            firstOctave = juce::jmax (0, lastOctave - 5);
        }
        const auto firstNote = firstOctave * 12;
        const auto lastNote = juce::jmin (127, (lastOctave + 1) * 12 - 1);
        const auto octaveCount = lastOctave - firstOctave + 1;
        auto frame = bounds.reduced (1.0f);
        const auto controlHeight = 28.0f;
        const auto keyboardTop = frame.getY() + controlHeight;
        const auto keyboardBounds = frame.withTop (keyboardTop);
        const auto whiteWidth = frame.getWidth() / static_cast<float> (octaveCount * 7);
        const auto blackWidth = whiteWidth * 0.62f;
        const auto blackHeight = keyboardBounds.getHeight() * 0.52f;
        std::array<juce::Rectangle<float>, 128> keyBounds {};

        g.setColour (panel);
        g.fillRoundedRectangle (frame, 11.0f);
        g.setColour (green.withAlpha (0.68f));
        g.drawRoundedRectangle (frame, 11.0f, 2.0f);
        g.drawHorizontalLine (juce::roundToInt (keyboardTop), frame.getX(), frame.getRight());

        const auto drawReadout = [&] (juce::Rectangle<float> area, juce::Colour accent,
                                      const juce::String& name, const juce::String& value)
        {
            g.setColour (accent.withAlpha (0.16f));
            g.fillRoundedRectangle (area.reduced (3.0f, 4.0f), 4.0f);
            g.setColour (accent.withAlpha (0.78f));
            g.drawRoundedRectangle (area.reduced (3.0f, 4.0f), 4.0f, 1.0f);
            g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            g.drawText (name, area.reduced (9.0f, 0.0f), juce::Justification::centredLeft);
            g.setColour (text);
            g.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
            g.drawText (value, area.reduced (9.0f, 0.0f), juce::Justification::centredRight);
        };

        auto controls = frame.removeFromTop (controlHeight).reduced (7.0f, 0.0f);
        const auto readoutWidth = juce::jmin (145.0f, controls.getWidth() * 0.18f);
        drawReadout (controls.removeFromLeft (readoutWidth), orange, "POSITION",
                     juce::String (position));
        drawReadout (controls.removeFromLeft (readoutWidth), cyan, "RANGE",
                     juce::String (range));
        drawReadout (controls.removeFromLeft (readoutWidth), green, "SPREAD",
                     juce::String (spread));

        g.setColour (muted.withAlpha (0.34f));
        const auto glyphY = controls.getCentreY();
        const auto glyphX = controls.getRight() - 64.0f;
        for (auto row = -1; row <= 1; ++row)
        {
            const auto y = glyphY + static_cast<float> (row) * 7.0f;
            g.drawLine (glyphX, y, glyphX + 13.0f, y, 2.0f);
            g.drawLine (glyphX + 26.0f, y, glyphX + 39.0f, y, 2.0f);
        }

        const auto whiteIndex = [] (int pitch)
        {
            static constexpr std::array<int, 12> indices { 0,0,1,1,2,3,3,4,4,5,5,6 };
            return indices[static_cast<size_t> (pitch)];
        };
        for (auto note = firstNote; note <= lastNote; ++note)
        {
            if (isBlack (note))
                continue;
            const auto octave = note / 12 - firstOctave;
            const auto index = octave * 7 + whiteIndex (note % 12);
            auto key = juce::Rectangle<float> {
                keyboardBounds.getX() + static_cast<float> (index) * whiteWidth,
                keyboardBounds.getY(), whiteWidth, keyboardBounds.getHeight()
            };
            keyBounds[static_cast<size_t> (note)] = key;
            g.setColour (green.withAlpha (0.30f));
            if (index > 0)
                g.drawVerticalLine (juce::roundToInt (key.getX()), key.getY(), key.getBottom());
            if (note % 12 == 0)
            {
                g.setColour (muted.withAlpha (0.55f));
                g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
                g.drawText (noteName (note), key.removeFromBottom (17.0f),
                            juce::Justification::centred);
            }
        }
        for (auto note = firstNote; note <= lastNote; ++note)
        {
            if (! isBlack (note))
                continue;
            const auto octave = note / 12 - firstOctave;
            const auto leftWhite = whiteIndex (note % 12);
            const auto boundary = keyboardBounds.getX()
                + static_cast<float> (octave * 7 + leftWhite + 1) * whiteWidth;
            auto key = juce::Rectangle<float> { boundary - blackWidth * 0.5f, keyboardBounds.getY(),
                                                blackWidth, blackHeight };
            keyBounds[static_cast<size_t> (note)] = key;

            const auto radius = juce::jmin (7.0f, blackWidth * 0.25f);
            juce::Path blackKey;
            blackKey.startNewSubPath (key.getX(), key.getY());
            blackKey.lineTo (key.getX(), key.getBottom() - radius);
            blackKey.quadraticTo (key.getX(), key.getBottom(),
                                  key.getX() + radius, key.getBottom());
            blackKey.lineTo (key.getRight() - radius, key.getBottom());
            blackKey.quadraticTo (key.getRight(), key.getBottom(),
                                  key.getRight(), key.getBottom() - radius);
            blackKey.lineTo (key.getRight(), key.getY());
            g.setColour (green.withAlpha (0.48f));
            g.strokePath (blackKey, juce::PathStrokeType { 2.0f });
        }

        const auto keyCentre = [&] (int note)
        {
            const auto key = keyBounds[static_cast<size_t> (juce::jlimit (0, 127, note))];
            return juce::Point<float> { key.getCentreX(), isBlack (note)
                ? key.getBottom() - 12.0f : keyboardBounds.getBottom() - 24.0f };
        };

        if (voicing.candidateCount > 0)
        {
            const auto left = keyCentre (voicing.candidates[0]).x;
            const auto right = keyCentre (
                voicing.candidates[static_cast<size_t> (voicing.candidateCount - 1)]).x;
            const auto bracket = juce::Rectangle<float> { left, keyboardBounds.getY() + 5.0f,
                                                          juce::jmax (2.0f, right - left), 5.0f };
            g.setColour (cyan.withAlpha (0.18f));
            g.fillRoundedRectangle (bracket, 3.0f);
            g.setColour (cyan.withAlpha (0.88f));
            g.drawRoundedRectangle (bracket, 3.0f, 1.4f);
            g.drawVerticalLine (juce::roundToInt (left), bracket.getY(), bracket.getBottom() + 3.0f);
            g.drawVerticalLine (juce::roundToInt (right), bracket.getY(), bracket.getBottom() + 3.0f);
        }

        for (auto i = 0; i < voicing.candidateCount; ++i)
        {
            const auto point = keyCentre (voicing.candidates[static_cast<size_t> (i)]);
            g.setColour (muted.withAlpha (0.48f));
            g.drawEllipse ({ point.x - 4.5f, point.y - 4.5f, 9.0f, 9.0f }, 1.2f);
        }
        for (auto i = 0; i < voicing.noteCount; ++i)
        {
            const auto point = keyCentre (voicing.notes[static_cast<size_t> (i)]);
            g.setColour (green.withAlpha (0.18f));
            g.fillEllipse ({ point.x - 7.0f, point.y - 7.0f, 14.0f, 14.0f });
            g.setColour (green);
            g.fillEllipse ({ point.x - 4.0f, point.y - 4.0f, 8.0f, 8.0f });
        }

        if (voicing.targetNote >= firstNote && voicing.targetNote <= lastNote)
        {
            const auto target = keyCentre (voicing.targetNote);
            g.setColour (orange.withAlpha (0.72f));
            g.drawVerticalLine (juce::roundToInt (target.x), keyboardBounds.getY(),
                                keyboardBounds.getBottom());
        }
        const auto centre = keyCentre (voicing.centreNote);
        g.setColour (orange);
        g.drawEllipse ({ centre.x - 8.0f, centre.y - 8.0f, 16.0f, 16.0f }, 2.0f);
    }

    static void drawLegendItem (juce::Graphics& g, juce::Rectangle<float> bounds,
                                juce::Colour colour, bool filled,
                                const juce::String& label)
    {
        const auto centre = juce::Point<float> { bounds.getX() + 9.0f, bounds.getCentreY() };
        g.setColour (colour);
        if (filled) g.fillEllipse ({ centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f });
        else g.drawEllipse ({ centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f }, 1.4f);
        g.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
        g.drawText (label, bounds.withTrimmedLeft (20.0f), juce::Justification::centredLeft);
    }

    void drawSpreadChoices (juce::Graphics& g, juce::Rectangle<float> bounds) const
    {
        static const std::array<juce::String, 6> names {
            "FULL", "ALT + LAST", "CLUSTER", "DYNAMIC", "ALTERNATE", "2 OF 3"
        };
        const auto cellWidth = bounds.getWidth() / 6.0f;
        for (auto choice = 1; choice <= 6; ++choice)
        {
            auto cell = bounds.removeFromLeft (cellWidth).reduced (3.0f, 2.0f);
            const auto selected = choice == spread;
            g.setColour (selected ? panel.interpolatedWith (green, 0.16f) : panel);
            g.fillRoundedRectangle (cell, 5.0f);
            g.setColour ((selected ? green : panelEdge).withAlpha (selected ? 0.95f : 0.72f));
            g.drawRoundedRectangle (cell, 5.0f, selected ? 1.6f : 1.0f);
            auto dots = cell.removeFromTop (27.0f).reduced (10.0f, 6.0f);
            for (auto i = 0; i < 8; ++i)
            {
                const auto keep = choice == 1 ? true
                    : choice == 2 ? (i % 2 == 0 || i == 7)
                    : choice == 3 ? (i >= 2 && i < 6)
                    : choice == 4 ? ((i * 37 + 11) % 10 < 7)
                    : choice == 5 ? i % 2 == 0 : i % 3 != 1;
                const auto x = dots.getX() + (static_cast<float> (i) + 0.5f)
                                           * dots.getWidth() / 8.0f;
                g.setColour ((keep ? green : muted).withAlpha (keep ? 0.88f : 0.30f));
                g.fillEllipse ({ x - 2.2f, dots.getCentreY() - 2.2f, 4.4f, 4.4f });
            }
            g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            g.setColour (selected ? green : muted);
            g.drawFittedText (names[static_cast<size_t> (choice - 1)], cell.toNearestInt(),
                              juce::Justification::centred, 1);
        }
    }

    NdlrEngine::PadVoicing voicing;
    int position = 50;
    int range = 13;
    int spread = 4;
    juce::String harmony { "C / Major / Triad" };
};

struct PadVoicingControlState
{
    bool enabled = false;
    bool strum = false;
    bool group = false;
    juce::String inversionMode { "ROOT" };
    int velocity = 100;
    int midiChannel = 1;
    int polyChain = 1;
    juce::String strumDivision { "16n" };
    juce::String quantize { "Off" };
};

class PadSpreadButton final : public juce::Button
{
public:
    explicit PadSpreadButton (int spreadChoice)
        : juce::Button ("Pad spread " + juce::String (spreadChoice)), choice (spreadChoice)
    {
        jassert (choice >= 1 && choice <= 6);
        setClickingTogglesState (true);
        setRadioGroupId (0x504144, juce::dontSendNotification);
        setTooltip (names[static_cast<size_t> (choice - 1)]);
    }

    int getChoice() const noexcept { return choice; }

    void paintButton (juce::Graphics& g, bool isMouseOverButton,
                      bool isButtonDown) override
    {
        auto cell = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
        const auto selected = getToggleState();
        const auto inactiveAlpha = isMouseOverButton ? 0.45f : 0.30f;
        const auto borderAlpha = selected ? 0.95f : inactiveAlpha;

        g.setColour (selected ? panel.interpolatedWith (green, 0.16f)
                              : panel.withAlpha (isButtonDown ? 0.92f : 0.72f));
        g.fillRoundedRectangle (cell, 5.0f);
        g.setColour ((selected ? green : panelEdge).withAlpha (borderAlpha));
        g.drawRoundedRectangle (cell, 5.0f, selected ? 1.6f : 1.0f);

        auto buttonContent = cell;
        const auto labelHeight = juce::jmin (11.0f, buttonContent.getHeight() * 0.5f);
        const auto labelBounds = buttonContent.removeFromTop (labelHeight);
        auto dots = buttonContent.reduced (8.0f, 2.0f);
        const auto dotRadius = juce::jlimit (1.2f, 2.0f, dots.getHeight() * 0.24f);
        for (auto index = 0; index < 8; ++index)
        {
            const auto keep = choice == 1 ? true
                : choice == 2 ? (index % 2 == 0 || index == 7)
                : choice == 3 ? (index >= 2 && index < 6)
                : choice == 4 ? ((index * 37 + 11) % 10 < 7)
                : choice == 5 ? index % 2 == 0 : index % 3 != 1;
            const auto x = dots.getX() + (static_cast<float> (index) + 0.5f)
                                       * dots.getWidth() / 8.0f;
            const auto colour = keep ? green : muted;
            const auto alpha = selected ? (keep ? 0.92f : 0.28f)
                                        : (keep ? inactiveAlpha : 0.16f);
            g.setColour (colour.withAlpha (alpha));
            g.fillEllipse ({ x - dotRadius, dots.getCentreY() - dotRadius,
                             dotRadius * 2.0f, dotRadius * 2.0f });
        }

        g.setFont (juce::FontOptions { 8.0f, juce::Font::bold });
        g.setColour ((selected ? green : muted).withAlpha (selected ? 1.0f : inactiveAlpha));
        g.drawFittedText (names[static_cast<size_t> (choice - 1)], labelBounds.toNearestInt(),
                          juce::Justification::centred, 1);
    }

private:
    inline static const std::array<juce::String, 6> names {
        "FULL", "ALT + LAST", "CLUSTER", "DYNAMIC", "ALTERNATE", "2 OF 3"
    };

    int choice = 1;
};

class PadVoicingMiniView final : public juce::Component
{
public:
    std::function<void(int)> onPositionChange;
    std::function<void(int)> onRangeChange;
    std::function<void(bool)> onPositionGesture;
    std::function<void(bool)> onRangeGesture;

    void setEmbeddedMode (bool shouldBeEmbedded)
    {
        embedded = shouldBeEmbedded;
        repaint();
    }

    void setData (const NdlrEngine::PadVoicing& newVoicing, int newPosition,
                  int newRange, int newSpread, juce::String newHarmony,
                  PadVoicingControlState newControls)
    {
        voicing = newVoicing;
        position = newPosition;
        range = newRange;
        spread = newSpread;
        harmony = std::move (newHarmony);
        controls = std::move (newControls);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (embedded)
        {
            drawKeyboard (g, getLocalBounds().toFloat(), true);
            return;
        }

        g.fillAll (background);
        auto area = getLocalBounds().toFloat().reduced (6.0f);

        g.setColour (panel);
        g.fillRoundedRectangle (area, 8.0f);
        g.setColour (green.withAlpha (0.68f));
        g.drawRoundedRectangle (area, 8.0f, 1.5f);
        area = area.reduced (7.0f, 4.0f);

        auto header = area.removeFromTop (24.0f);
        g.setColour (green);
        g.setFont (juce::FontOptions { 18.0f, juce::Font::bold });
        g.drawText ("PAD", header.removeFromLeft (54.0f), juce::Justification::centredLeft);
        g.setColour (muted.withAlpha (0.72f));
        g.setFont (juce::FontOptions { 9.5f, juce::Font::bold });
        g.drawFittedText (harmony + "  |  CENTER " + noteName (voicing.centreNote)
                              + "  |  " + juce::String (voicing.noteCount) + " NOTES",
                          header.toNearestInt(), juce::Justification::centredRight, 1);

        area.removeFromTop (2.0f);
        drawKeyboard (g, area.removeFromTop (70.0f), true);
        area.removeFromTop (3.0f);

        auto row1 = area.removeFromTop (27.0f);
        drawToggle (g, takeCell (row1, 4), "PAD", controls.enabled);
        drawValue (g, takeCell (row1, 3), "VELOCITY", juce::String (controls.velocity));
        drawValue (g, takeCell (row1, 2), "MIDI", juce::String (controls.midiChannel));
        drawValue (g, row1, "POLY", juce::String (controls.polyChain));

        area.removeFromTop (2.0f);
        auto row2 = area.removeFromTop (27.0f);
        const auto cellWidth = row2.getWidth() / 5.0f;
        drawToggle (g, row2.removeFromLeft (cellWidth), "STRUM", controls.strum);
        drawValue (g, row2.removeFromLeft (cellWidth), "DIV", controls.strumDivision);
        drawToggle (g, row2.removeFromLeft (cellWidth), "GROUP", controls.group);
        drawValue (g, row2.removeFromLeft (cellWidth), "INV", controls.inversionMode);
        drawValue (g, row2, "QUANTIZE", controls.quantize);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! embedded || lastKeyboardBounds.isEmpty())
            return;

        const auto point = event.position;
        const auto inHandleBand = point.y >= lastKeyboardBounds.getY() - 4.0f
                               && point.y <= lastKeyboardBounds.getY() + 24.0f;
        const auto leftDistance = std::abs (point.x - lastRangeLeftX);
        const auto rightDistance = std::abs (point.x - lastRangeRightX);
        if (inHandleBand && juce::jmin (leftDistance, rightDistance) <= 12.0f)
        {
            dragMode = leftDistance <= rightDistance ? DragMode::rangeLeft
                                                     : DragMode::rangeRight;
            dragStartX = point.x;
            dragStartRange = range;
            const auto bracketWidth = std::abs (lastRangeRightX - lastRangeLeftX);
            pixelsPerRangeStep = juce::jlimit (
                8.0f, 30.0f, bracketWidth / static_cast<float> (juce::jmax (1, range - 1)));
            if (onRangeGesture != nullptr)
                onRangeGesture (true);
            return;
        }

        if (lastKeyboardBounds.contains (point))
        {
            dragMode = DragMode::position;
            dragStartX = point.x;
            dragStartPosition = position;
            dragFirstVisibleNote = lastFirstNote;
            dragLastVisibleNote = lastLastNote;
            if (onPositionGesture != nullptr)
                onPositionGesture (true);
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragMode == DragMode::position)
        {
            constexpr auto positionSpan = 48.0f / 1.27f;
            const auto delta = (event.position.x - dragStartX)
                             / juce::jmax (1.0f, lastKeyboardBounds.getWidth())
                             * positionSpan;
            if (onPositionChange != nullptr)
                onPositionChange (juce::jlimit (
                    0, 100, juce::roundToInt (static_cast<float> (dragStartPosition) + delta)));
            return;
        }

        if (dragMode == DragMode::rangeLeft || dragMode == DragMode::rangeRight)
        {
            const auto direction = dragMode == DragMode::rangeLeft ? -1.0f : 1.0f;
            const auto delta = direction * (event.position.x - dragStartX);
            const auto newRange = juce::jlimit (
                1, 22, dragStartRange + juce::roundToInt (delta / pixelsPerRangeStep));
            if (onRangeChange != nullptr)
                onRangeChange (newRange);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragMode == DragMode::position && onPositionGesture != nullptr)
            onPositionGesture (false);
        else if ((dragMode == DragMode::rangeLeft || dragMode == DragMode::rangeRight)
                 && onRangeGesture != nullptr)
            onRangeGesture (false);

        dragMode = DragMode::none;
        repaint();
        updateMouseCursor ({ -1000.0f, -1000.0f });
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        updateMouseCursor (event.position);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        updateMouseCursor ({ -1000.0f, -1000.0f });
    }

private:
    enum class DragMode { none, position, rangeLeft, rangeRight };

    void updateMouseCursor (juce::Point<float> point)
    {
        const auto inHandleBand = point.y >= lastKeyboardBounds.getY() - 4.0f
                               && point.y <= lastKeyboardBounds.getY() + 24.0f;
        const auto nearHandle = inHandleBand
            && (std::abs (point.x - lastRangeLeftX) <= 12.0f
                || std::abs (point.x - lastRangeRightX) <= 12.0f);
        setMouseCursor (nearHandle ? juce::MouseCursor::LeftRightResizeCursor
                                  : lastKeyboardBounds.contains (point)
                                      ? juce::MouseCursor::DraggingHandCursor
                                      : juce::MouseCursor::NormalCursor);
    }

    static juce::Rectangle<float> takeCell (juce::Rectangle<float>& row, int remainingCells)
    {
        return row.removeFromLeft (row.getWidth() / static_cast<float> (remainingCells));
    }

    static bool isBlack (int note) noexcept
    {
        const auto pitch = note % 12;
        return pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
    }

    static juce::String noteName (int note)
    {
        static const std::array<const char*, 12> names {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        note = juce::jlimit (0, 127, note);
        return juce::String (names[static_cast<size_t> (note % 12)])
             + juce::String (note / 12 - 1);
    }

    static void drawChip (juce::Graphics& g, juce::Rectangle<float> bounds,
                          juce::Colour accent, float alpha)
    {
        bounds = bounds.reduced (2.0f, 1.5f);
        g.setColour (accent.withAlpha (0.07f + alpha * 0.11f));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (accent.withAlpha (alpha));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    }

    static void drawToggle (juce::Graphics& g, juce::Rectangle<float> bounds,
                            const juce::String& label, bool active)
    {
        const auto accent = active ? green : muted;
        drawChip (g, bounds, accent, active ? 0.92f : 0.30f);
        g.setColour (accent.withAlpha (active ? 1.0f : 0.38f));
        g.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
        g.drawFittedText (label + (active ? "  ON" : "  OFF"),
                          bounds.reduced (6.0f, 1.0f).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    static void drawValue (juce::Graphics& g, juce::Rectangle<float> bounds,
                           const juce::String& label, const juce::String& value)
    {
        drawChip (g, bounds, green, 0.44f);
        auto textBounds = bounds.reduced (7.0f, 1.0f);
        g.setColour (muted.withAlpha (0.76f));
        g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        g.drawText (label, textBounds, juce::Justification::centredLeft);
        g.setColour (green);
        g.setFont (juce::FontOptions { 10.5f, juce::Font::bold });
        g.drawText (value, textBounds, juce::Justification::centredRight);
    }

    void drawKeyboard (juce::Graphics& g, juce::Rectangle<float> bounds,
                       bool showReadoutValues)
    {
        bounds = bounds.reduced (2.0f, 0.0f);
        g.setColour (background.withAlpha (0.58f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (green.withAlpha (0.52f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.2f);

        auto firstNote = 0;
        auto lastNote = 47;
        if (dragMode == DragMode::position)
        {
            // Keep the keyboard scale stable during a position gesture.
            firstNote = dragFirstVisibleNote;
            lastNote = dragLastVisibleNote;
        }
        else
        {
            const auto candidateMinimum = voicing.candidateCount > 0
                ? voicing.candidates[0] : voicing.centreNote;
            const auto candidateMaximum = voicing.candidateCount > 0
                ? voicing.candidates[static_cast<size_t> (voicing.candidateCount - 1)]
                : voicing.centreNote;
            auto firstOctave = juce::jlimit (0, 10, candidateMinimum / 12);
            auto lastOctave = juce::jlimit (0, 10, candidateMaximum / 12);

            // Preserve the original four-octave view for small ranges, but grow
            // it when necessary so Range 13...22 never gets clipped.
            constexpr auto minimumOctaves = 4;
            while (lastOctave - firstOctave + 1 < minimumOctaves)
            {
                if (firstOctave > 0)
                    --firstOctave;
                if (lastOctave - firstOctave + 1 < minimumOctaves && lastOctave < 10)
                    ++lastOctave;
            }

            firstNote = firstOctave * 12;
            lastNote = juce::jmin (127, (lastOctave + 1) * 12 - 1);
        }
        const auto octaveCount = juce::jmax (1, (lastNote - firstNote + 12) / 12);
        auto readouts = bounds.removeFromTop (20.0f);
        auto keyboard = bounds;
        lastKeyboardBounds = keyboard;
        lastFirstNote = firstNote;
        lastLastNote = lastNote;
        lastRangeLeftX = -1000.0f;
        lastRangeRightX = -1000.0f;

        const auto polyChainCount = juce::jlimit (1, 4, controls.polyChain);
        const auto firstMidiChannel = juce::jlimit (1, 16, controls.midiChannel);
        const std::array<juce::Colour, 4> channelColours {
            green, cyan, orange, muted
        };
        const auto midiChannelForSlot = [firstMidiChannel] (int slot)
        {
            return (firstMidiChannel - 1 + slot) % 16 + 1;
        };

        const auto drawReadout = [&] (juce::Rectangle<float> cell, juce::Colour accent,
                                      const juce::String& label, const juce::String& value)
        {
            cell = cell.reduced (2.0f, 2.0f);
            g.setColour (accent.withAlpha (0.13f));
            g.fillRoundedRectangle (cell, 3.0f);
            g.setColour (accent.withAlpha (0.76f));
            g.drawRoundedRectangle (cell, 3.0f, 0.9f);
            auto textArea = cell.reduced (5.0f, 0.0f);
            g.setFont (juce::FontOptions { 8.0f, juce::Font::bold });
            g.drawText (label, textArea, juce::Justification::centredLeft);
            if (showReadoutValues)
            {
                g.setColour (text);
                g.setFont (juce::FontOptions { 9.5f, juce::Font::bold });
                g.drawText (value, textArea, juce::Justification::centredRight);
            }
        };

        const auto readoutWidth = juce::jmin (105.0f, readouts.getWidth() / 3.7f);
        drawReadout (readouts.removeFromLeft (readoutWidth), orange, "POS", juce::String (position));
        drawReadout (readouts.removeFromLeft (readoutWidth), cyan, "RANGE", juce::String (range));
        if (! embedded)
            drawReadout (readouts.removeFromLeft (readoutWidth), green, "SPREAD",
                         juce::String (spread));

        // Keep the legend tied to the same round-robin channel allocation as
        // NdlrEngine::updatePadVoicing(). This also makes channel 16 -> 1 wrap
        // visible when a Poly Chain crosses the end of the MIDI channel range.
        if (readouts.getWidth() > 48.0f)
        {
            readouts.removeFromLeft (3.0f);
            auto legend = readouts;
            const auto labelWidth = juce::jmin (34.0f, legend.getWidth() * 0.18f);
            auto label = legend.removeFromLeft (labelWidth);
            g.setColour (muted.withAlpha (0.72f));
            g.setFont (juce::FontOptions { 7.5f, juce::Font::bold });
            g.drawText ("POLY", label, juce::Justification::centred);

            const auto chipWidth = juce::jmin (48.0f,
                                               legend.getWidth()
                                                   / static_cast<float> (polyChainCount));
            for (auto slot = 0; slot < polyChainCount; ++slot)
            {
                auto chip = legend.removeFromLeft (chipWidth).reduced (2.0f, 2.0f);
                const auto accent = channelColours[static_cast<size_t> (slot)];
                const auto isUsed = slot < voicing.noteCount;
                g.setColour (accent.withAlpha (isUsed ? 0.16f : 0.07f));
                g.fillRoundedRectangle (chip, 3.0f);
                g.setColour (accent.withAlpha (isUsed ? 0.82f : 0.34f));
                g.drawRoundedRectangle (chip, 3.0f, 0.9f);
                g.setFont (juce::FontOptions { 8.0f, juce::Font::bold });
                g.drawText ("CH " + juce::String (midiChannelForSlot (slot)), chip,
                            juce::Justification::centred);
            }
        }

        const auto whiteWidth = keyboard.getWidth() / static_cast<float> (octaveCount * 7);
        const auto blackWidth = whiteWidth * 0.58f;
        const auto blackHeight = keyboard.getHeight() * 0.54f;
        std::array<juce::Rectangle<float>, 128> keyBounds {};
        const auto whiteIndex = [] (int pitch)
        {
            static constexpr std::array<int, 12> indices { 0,0,1,1,2,3,3,4,4,5,5,6 };
            return indices[static_cast<size_t> (pitch)];
        };

        g.setColour (green.withAlpha (0.22f));
        for (auto i = 1; i < octaveCount * 7; ++i)
        {
            const auto x = keyboard.getX() + static_cast<float> (i) * whiteWidth;
            g.drawVerticalLine (juce::roundToInt (x), keyboard.getY(), keyboard.getBottom());
        }

        for (auto note = firstNote; note <= lastNote; ++note)
        {
            const auto octave = (note - firstNote) / 12;
            const auto boundary = keyboard.getX()
                + static_cast<float> (octave * 7 + whiteIndex (note % 12)) * whiteWidth;
            if (! isBlack (note))
            {
                keyBounds[static_cast<size_t> (note)] = {
                    boundary, keyboard.getY(), whiteWidth, keyboard.getHeight()
                };
                continue;
            }

            const auto keyBoundary = boundary + whiteWidth;
            const juce::Rectangle<float> key { keyBoundary - blackWidth * 0.5f,
                                               keyboard.getY(), blackWidth, blackHeight };
            keyBounds[static_cast<size_t> (note)] = key;
            const auto radius = juce::jmin (5.0f, blackWidth * 0.24f);
            juce::Path path;
            path.startNewSubPath (key.getX(), key.getY());
            path.lineTo (key.getX(), key.getBottom() - radius);
            path.quadraticTo (key.getX(), key.getBottom(), key.getX() + radius, key.getBottom());
            path.lineTo (key.getRight() - radius, key.getBottom());
            path.quadraticTo (key.getRight(), key.getBottom(), key.getRight(), key.getBottom() - radius);
            path.lineTo (key.getRight(), key.getY());
            g.setColour (green.withAlpha (0.44f));
            g.strokePath (path, juce::PathStrokeType { 1.4f });
        }

        // A voice is assigned to the Poly Chain channels in note order, exactly
        // as in the engine. Tinting the complete key shows each channel's real
        // footprint while preserving the keyboard and range graphics beneath it.
        for (auto i = 0; i < voicing.noteCount; ++i)
        {
            const auto note = voicing.notes[static_cast<size_t> (i)];
            if (note < firstNote || note > lastNote)
                continue;

            auto key = keyBounds[static_cast<size_t> (note)].reduced (0.8f);
            if (key.isEmpty())
                continue;

            const auto accent = channelColours[static_cast<size_t> (i % polyChainCount)];
            g.setColour (accent.withAlpha (isBlack (note) ? 0.25f : 0.15f));
            if (isBlack (note))
                g.fillRoundedRectangle (key, juce::jmin (4.0f, key.getWidth() * 0.20f));
            else
                g.fillRect (key);
        }

        g.setColour (muted.withAlpha (0.90f));
        g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        const auto firstVisibleC = firstNote + (12 - firstNote % 12) % 12;
        for (auto note = firstVisibleC; note <= lastNote; note += 12)
        {
            auto key = keyBounds[static_cast<size_t> (note)];
            if (! key.isEmpty())
                g.drawText (noteName (note), key.removeFromBottom (14.0f),
                            juce::Justification::centred);
        }

        const auto notePoint = [&] (int note)
        {
            const auto key = keyBounds[static_cast<size_t> (juce::jlimit (0, 127, note))];
            return juce::Point<float> { key.getCentreX(), isBlack (note)
                ? key.getBottom() - 6.0f : keyboard.getBottom() - 22.0f };
        };
        const auto isVisible = [firstNote, lastNote] (int note)
        {
            return note >= firstNote && note <= lastNote;
        };

        auto firstVisibleCandidate = -1;
        auto lastVisibleCandidate = -1;
        for (auto i = 0; i < voicing.candidateCount; ++i)
        {
            const auto note = voicing.candidates[static_cast<size_t> (i)];
            if (! isVisible (note))
                continue;
            if (firstVisibleCandidate < 0)
                firstVisibleCandidate = note;
            lastVisibleCandidate = note;
        }
        if (firstVisibleCandidate >= 0)
        {
            const auto left = notePoint (firstVisibleCandidate).x;
            const auto right = notePoint (lastVisibleCandidate).x;
            lastRangeLeftX = left;
            lastRangeRightX = right;
            const juce::Rectangle<float> bracket { left, keyboard.getY() + 2.0f,
                                                   juce::jmax (2.0f, right - left), 4.0f };
            g.setColour (cyan.withAlpha (0.86f));
            g.drawRoundedRectangle (bracket, 2.0f, 1.2f);
            for (const auto handleX : { left, right })
            {
                g.setColour (background);
                g.fillEllipse ({ handleX - 4.0f, bracket.getCentreY() - 4.0f, 8.0f, 8.0f });
                g.setColour (cyan);
                g.drawEllipse ({ handleX - 4.0f, bracket.getCentreY() - 4.0f, 8.0f, 8.0f },
                               1.4f);
            }
        }

        for (auto i = 0; i < voicing.candidateCount; ++i)
        {
            const auto note = voicing.candidates[static_cast<size_t> (i)];
            if (! isVisible (note))
                continue;
            const auto point = notePoint (note);
            g.setColour (muted.withAlpha (0.45f));
            g.drawEllipse ({ point.x - 3.0f, point.y - 3.0f, 6.0f, 6.0f }, 0.9f);
        }
        for (auto i = 0; i < voicing.noteCount; ++i)
        {
            const auto note = voicing.notes[static_cast<size_t> (i)];
            if (! isVisible (note))
                continue;
            const auto point = notePoint (note);
            const auto accent = channelColours[static_cast<size_t> (i % polyChainCount)];
            g.setColour (accent.withAlpha (0.20f));
            g.fillEllipse ({ point.x - 5.0f, point.y - 5.0f, 10.0f, 10.0f });
            g.setColour (accent);
            g.fillEllipse ({ point.x - 2.7f, point.y - 2.7f, 5.4f, 5.4f });
        }

        if (isVisible (voicing.targetNote))
        {
            const auto target = notePoint (voicing.targetNote);
            g.setColour (orange.withAlpha (0.72f));
            g.drawVerticalLine (juce::roundToInt (target.x), keyboard.getY(), keyboard.getBottom());
        }
        if (isVisible (voicing.centreNote))
        {
            const auto centre = notePoint (voicing.centreNote);
            g.setColour (orange);
            g.drawEllipse ({ centre.x - 5.5f, centre.y - 5.5f, 11.0f, 11.0f }, 1.5f);
        }
    }

    NdlrEngine::PadVoicing voicing;
    PadVoicingControlState controls;
    int position = 50;
    int range = 13;
    int spread = 4;
    juce::String harmony { "C / Major / Triad" };
    bool embedded = false;
    DragMode dragMode = DragMode::none;
    float dragStartX = 0.0f;
    float pixelsPerRangeStep = 12.0f;
    int dragStartPosition = 50;
    int dragStartRange = 1;
    int dragFirstVisibleNote = 0;
    int dragLastVisibleNote = 47;
    juce::Rectangle<float> lastKeyboardBounds;
    int lastFirstNote = 0;
    int lastLastNote = 47;
    float lastRangeLeftX = -1000.0f;
    float lastRangeRightX = -1000.0f;
};

class PadVoicingWindow final : public juce::DocumentWindow
{
public:
    explicit PadVoicingWindow (juce::LookAndFeel* lookAndFeel)
        : juce::DocumentWindow ("Pad Compact Prototype", panel,
                                juce::DocumentWindow::closeButton)
    {
        setLookAndFeel (lookAndFeel);
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        setContentNonOwned (&preview, false);
        centreWithSize (575, 212);
    }

    ~PadVoicingWindow() override
    {
        clearContentComponent();
        setLookAndFeel (nullptr);
    }

    void closeButtonPressed() override { setVisible (false); }

    void setData (const NdlrEngine::PadVoicing& voicing, int position,
                  int range, int spread, const juce::String& harmony,
                  PadVoicingControlState controls)
    {
        preview.setData (voicing, position, range, spread, harmony, std::move (controls));
    }

private:
    PadVoicingMiniView preview;
};
}
