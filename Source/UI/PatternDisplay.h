#pragma once
#include <JuceHeader.h>

namespace ndlr::ui
{
class PatternDisplay final : public juce::Component
{
public:
    std::function<void (int step, int value)> onStepChanged;
    void setAccentColour (juce::Colour colour) { accentColour = colour; repaint(); }

    void setPattern (std::array<int, 16> values, int length, int maximum, int playhead)
    {
        pattern = values; activeLength = juce::jlimit (1, 16, length);
        maxValue = juce::jmax (1, maximum); currentStep = playhead; repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override { editFromPointer (event.position); }
    void mouseDrag (const juce::MouseEvent& event) override { editFromPointer (event.position); }

    void paint (juce::Graphics& g) override
    {
        const auto area = getPlotArea();
        const auto baseline = area.getBottom();
        const auto width = area.getWidth() / 16.0f;
        const auto usableHeight = area.getHeight() - 4.0f;

        // Grille verticale : une cellule par step, avec un repere legerement
        // plus present tous les quatre pas.
        for (auto step = 0; step <= 16; ++step)
        {
            const auto x = area.getX() + static_cast<float> (step) * width;
            g.setColour (juce::Colour { 0xff527078 }.withAlpha (
                step % 4 == 0 ? 0.28f : 0.13f));
            g.drawVerticalLine (juce::roundToInt (x), area.getY(), baseline);
        }

        // L'echelle suit le range du mode courant : 1-20 pour Chord,
        // 1-40 pour Scale et 1-60 pour Chromatic.
        const std::array<int, 5> ticks {
            1, maxValue / 4, maxValue / 2, maxValue * 3 / 4, maxValue
        };
        g.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        for (const auto tick : ticks)
        {
            const auto y = baseline - usableHeight * static_cast<float> (tick)
                                           / static_cast<float> (maxValue);
            g.setColour (juce::Colour { 0xff527078 }.withAlpha (
                tick == 1 || tick == maxValue ? 0.42f : 0.24f));
            g.drawHorizontalLine (juce::roundToInt (y), area.getX(), area.getRight());
            g.setColour (juce::Colour { 0xff8aa0a5 }.withAlpha (0.82f));
            g.drawText (juce::String (tick),
                        juce::Rectangle<float> { 0.0f, y - 6.0f, area.getX() - 5.0f, 12.0f },
                        juce::Justification::centredRight);
        }

        for (auto i = 0; i < 16; ++i)
        {
            const auto value = pattern[static_cast<size_t> (i)];
            // Échelle fixe et linéaire : deux fois la valeur produit exactement
            // deux fois la hauteur, sans redimensionner les autres barres.
            const auto normalised = static_cast<float> (value) / static_cast<float> (maxValue);
            const auto height = usableHeight * normalised;
            auto bar = juce::Rectangle<float> { area.getX() + static_cast<float> (i) * width + 2.0f,
                                                baseline - height, width - 4.0f, height };
            g.setColour (i >= activeLength ? juce::Colour { 0xff293238 }
                                           : i == currentStep ? juce::Colours::yellow : accentColour);
            g.fillRect (bar);
            g.setColour (i < activeLength ? accentColour : juce::Colour { 0xff526069 });
            g.setFont (juce::FontOptions { 10.0f });
            g.drawText (juce::String (value), juce::Rectangle<float> { bar.getX(), baseline, bar.getWidth(), 14.0f },
                        juce::Justification::centred);
        }
        g.setColour (juce::Colour { 0xff17443e });
        g.drawHorizontalLine (juce::roundToInt (baseline), area.getX(), area.getRight());
    }

private:
    void editFromPointer (juce::Point<float> point)
    {
        const auto area = getPlotArea();
        const auto width = area.getWidth() / 16.0f;
        const auto step = juce::jlimit (0, activeLength - 1,
            static_cast<int> ((point.x - area.getX()) / width));
        const auto baseline = area.getBottom();
        const auto usableHeight = area.getHeight() - 4.0f;
        const auto visualPosition = juce::jlimit (0.0f, 1.0f, (baseline - point.y) / usableHeight);
        const auto value = juce::jlimit (1, maxValue, juce::roundToInt (
            visualPosition * static_cast<float> (maxValue)));
        if (pattern[static_cast<size_t> (step)] == value) return;
        pattern[static_cast<size_t> (step)] = value;
        if (onStepChanged) onStepChanged (step, value);
        repaint();
    }

    [[nodiscard]] juce::Rectangle<float> getPlotArea() const
    {
        auto area = getLocalBounds().toFloat();
        area.removeFromLeft (34.0f);
        area.removeFromRight (8.0f);
        area.removeFromTop (6.0f);
        area.removeFromBottom (16.0f);
        return area;
    }

    std::array<int, 16> pattern {};
    int activeLength = 8, maxValue = 20, currentStep = -1;
    juce::Colour accentColour { 0xff00a6ff };
};
}
