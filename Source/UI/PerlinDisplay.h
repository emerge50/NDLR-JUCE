#pragma once

#include <JuceHeader.h>
#include "../Engine/PerlinNoise.h"
#include "NdlrLookAndFeel.h"

namespace ndlr::ui
{
class PerlinDisplay final : public juce::Component
{
public:
    std::function<void (int x, int y)> onPositionChanged;

    void setParameters (int seedValue, int xValue, int yValue, int zoomValue,
                        int spacingValue, int roughnessValue, int persistenceValue,
                        int resolutionValue, int brightnessValue)
    {
        noise.setSeed (seedValue); x = static_cast<float> (xValue); y = static_cast<float> (yValue);
        zoom = static_cast<float> (zoomValue); spacing = static_cast<float> (spacingValue);
        roughness = static_cast<float> (roughnessValue); persistence = static_cast<float> (persistenceValue);
        resolution = juce::jlimit (10, 80, resolutionValue);
        brightness = juce::jlimit (0.0f, 1.0f, static_cast<float> (brightnessValue) / 100.0f);
        rebuildProfiles();
        repaint();
    }

    void setProfiles (int newLength1, int type1, bool enabled1,
                      int newLength2, int type2, bool enabled2)
    {
        length1 = juce::jlimit (1, 16, newLength1); length2 = juce::jlimit (1, 16, newLength2);
        maximum1 = type1 == 0 ? 20 : type1 == 1 ? 40 : 60;
        maximum2 = type2 == 0 ? 20 : type2 == 1 ? 40 : 60;
        motif1Enabled = enabled1; motif2Enabled = enabled2;
        rebuildProfiles(); repaint();
    }

    [[nodiscard]] int getPatternValue (int motif, int step) const noexcept
    { return motif == 0 ? profile1[static_cast<size_t> (step)] : profile2[static_cast<size_t> (step)]; }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! getFieldBounds().contains (event.position)) return;
        dragStart = event.position; dragStartX = x; dragStartY = y; dragging = true;
    }
    void mouseDrag (const juce::MouseEvent& event) override { dragFieldTo (event.position); }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    void paint (juce::Graphics& g) override
    {
        const auto field = getFieldBounds();
        const auto columns = resolution;
        const auto cellWidth = field.getWidth() / static_cast<float> (columns);
        // Derive the row count from the drawable aspect ratio so the sampled
        // cells remain square. A 216 x 216 display therefore uses the same
        // number of rows and columns instead of stretching 55% as many rows.
        const auto rows = juce::jmax (5, juce::roundToInt (
            field.getHeight() / juce::jmax (0.01f, cellWidth)));
        const auto cellHeight = field.getHeight() / static_cast<float> (rows);
        const auto scale = 0.03f + zoom * 0.0045f;
        for (auto row = 0; row < rows; ++row)
            for (auto column = 0; column < columns; ++column)
            {
                const auto value = brightness * noise.sample (x * 0.005f + static_cast<float> (column) * scale,
                                                 y * 0.005f + static_cast<float> (row) * scale,
                                                 0.2f + persistence * 0.0055f,
                                                 1.0f + roughness * 0.035f);
                g.setColour (juce::Colour::fromFloatRGBA (value, value, value, 1.0f));
                g.fillRect (juce::Rectangle<float> {
                    field.getX() + static_cast<float> (column) * cellWidth,
                    field.getY() + static_cast<float> (row) * cellHeight,
                    cellWidth + 0.5f, cellHeight + 0.5f });
            }
        const auto m1Y = juce::roundToInt (field.getCentreY());
        const auto m2Y = juce::roundToInt (juce::jlimit (field.getY(), field.getBottom(),
            static_cast<float> (m1Y) + spacing * 0.01f * field.getHeight() * 0.4f));
        g.setColour (cyan); g.drawHorizontalLine (m1Y, field.getX(), field.getRight());
        g.setColour (orange); g.drawHorizontalLine (m2Y, field.getX(), field.getRight());

    }

private:
    void dragFieldTo (juce::Point<float> point)
    {
        if (! dragging) return;
        const auto field = getFieldBounds();
        const auto newX = juce::jlimit (1, 1000, juce::roundToInt (
            dragStartX - (point.x - dragStart.x) / juce::jmax (1.0f, field.getWidth()) * 1000.0f));
        const auto newY = juce::jlimit (1, 1000, juce::roundToInt (
            dragStartY - (point.y - dragStart.y) / juce::jmax (1.0f, field.getHeight()) * 1000.0f));
        if (onPositionChanged) onPositionChanged (newX, newY);
    }

    [[nodiscard]] juce::Rectangle<float> getFieldBounds() const
    {
        return getLocalBounds().reduced (2).toFloat();
    }

    void rebuildProfiles()
    {
        for (auto step = 0; step < 16; ++step)
        {
            profile1[static_cast<size_t> (step)] = noise.patternValue (
                step, length1, 0, maximum1, x, y, zoom, spacing,
                1.0f + roughness * 0.035f, 0.2f + persistence * 0.0055f, brightness);
            profile2[static_cast<size_t> (step)] = noise.patternValue (
                step, length2, 1, maximum2, x, y, zoom, spacing,
                1.0f + roughness * 0.035f, 0.2f + persistence * 0.0055f, brightness);
        }
    }

    ndlr::PerlinNoise noise;
    float x = 1.0f, y = 1.0f, zoom = 1.0f, spacing = 0.0f, roughness = 1.0f, persistence = 1.0f;
    float brightness = 1.0f;
    int resolution = 52;
    std::array<int, 16> profile1 {}, profile2 {};
    int length1 = 8, length2 = 8, maximum1 = 20, maximum2 = 20;
    bool motif1Enabled = true, motif2Enabled = true;
    juce::Point<float> dragStart;
    float dragStartX = 1.0f, dragStartY = 1.0f;
    bool dragging = false;
};

class PerlinProfiles final : public juce::Component
{
public:
    void setProfiles (const std::array<int, 16>& newM1, int newLength1, int newMaximum1, bool enabled1,
                      const std::array<int, 16>& newM2, int newLength2, int newMaximum2, bool enabled2)
    {
        m1 = newM1; m2 = newM2; length1 = newLength1; length2 = newLength2;
        maximum1 = newMaximum1; maximum2 = newMaximum2;
        m1Enabled = enabled1; m2Enabled = enabled2; repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        auto first = area.removeFromTop (area.getHeight() * 0.5f);
        drawProfile (g, first, m1, length1, maximum1, cyan, m1Enabled, "M1");
        drawProfile (g, area, m2, length2, maximum2, orange, m2Enabled, "M2");
    }

private:
    static void drawProfile (juce::Graphics& g, juce::Rectangle<float> bounds,
                             const std::array<int, 16>& values, int length, int maximum,
                             juce::Colour colour, bool enabled, const juce::String& label)
    {
        length = juce::jlimit (1, 16, length); maximum = juce::jmax (1, maximum);
        const auto fullBounds = bounds;
        bounds.removeFromLeft (32.0f);
        bounds = bounds.reduced (1.0f, 3.0f);
        const auto baseline = bounds.getBottom() - 2.0f;
        const auto usableHeight = juce::jmax (1.0f, bounds.getHeight() - 4.0f);
        const auto slot = bounds.getWidth() / static_cast<float> (length);

        // Une cellule verticale par step, avec un repere plus visible tous les
        // quatre pas, comme dans le Pattern Editor.
        for (auto step = 0; step <= length; ++step)
        {
            const auto x = bounds.getX() + static_cast<float> (step) * slot;
            g.setColour (juce::Colour { 0xff527078 }.withAlpha (
                step % 4 == 0 ? 0.28f : 0.13f));
            g.drawVerticalLine (juce::roundToInt (x), bounds.getY(), baseline);
        }

        // L'echelle de chaque courbe suit son mode : 1-20 pour Chord,
        // 1-40 pour Scale et 1-60 pour Chromatic.
        const std::array<int, 5> ticks {
            1, maximum / 4, maximum / 2, maximum * 3 / 4, maximum
        };
        g.setFont (juce::FontOptions { 7.5f, juce::Font::bold });
        for (const auto tick : ticks)
        {
            const auto y = baseline - usableHeight * static_cast<float> (tick)
                                               / static_cast<float> (maximum);
            g.setColour (juce::Colour { 0xff527078 }.withAlpha (
                tick == 1 || tick == maximum ? 0.42f : 0.24f));
            g.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
            g.setColour (juce::Colour { 0xff8aa0a5 }.withAlpha (0.82f));
            g.drawText (juce::String (tick),
                        juce::Rectangle<float> { fullBounds.getX() + 13.0f, y - 5.0f,
                                                 17.0f, 10.0f },
                        juce::Justification::centredRight);
        }

        juce::Path line;
        std::array<juce::Point<float>, 16> points {};
        for (auto step = 0; step < length; ++step)
        {
            const auto px = bounds.getX() + (static_cast<float> (step) + 0.5f) * slot;
            const auto py = baseline
                - static_cast<float> (values[static_cast<size_t> (step)]) / static_cast<float> (maximum)
                    * usableHeight;
            points[static_cast<size_t> (step)] = { px, py };
            if (step == 0) line.startNewSubPath (px, py); else line.lineTo (px, py);
        }
        const auto drawColour = colour.withAlpha (enabled ? 0.95f : 0.3f);
        g.setColour (drawColour); g.strokePath (line, juce::PathStrokeType (1.2f));
        for (auto step = 0; step < length; ++step)
            g.fillEllipse (points[static_cast<size_t> (step)].x - 2.2f,
                           points[static_cast<size_t> (step)].y - 2.2f, 4.4f, 4.4f);
        g.setFont (juce::FontOptions { 8.0f, juce::Font::bold });
        g.setColour (drawColour);
        g.drawText (label, juce::Rectangle<float> { fullBounds.getX(), fullBounds.getY(),
                                                    14.0f, 12.0f },
                    juce::Justification::centredLeft);
    }

    std::array<int, 16> m1 {}, m2 {};
    int length1 = 8, length2 = 8, maximum1 = 20, maximum2 = 20;
    bool m1Enabled = true, m2Enabled = true;
};
}
