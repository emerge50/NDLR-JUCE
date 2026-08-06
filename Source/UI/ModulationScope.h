#pragma once
#include <JuceHeader.h>

namespace ndlr::ui
{
class ModulationScope final : public juce::Component
{
public:
    void markRestart()
    {
        history.fill (0.0f);
        samplesSinceRetrigger = 0;
        repaint();
    }

    void pushValue (float value)
    {
        for (size_t i = 1; i < history.size(); ++i)
            history[i - 1] = history[i];
        history.back() = juce::jlimit (-1.0f, 1.0f, value);
        if (samplesSinceRetrigger >= 0
            && ++samplesSinceRetrigger >= static_cast<int> (history.size()))
            samplesSinceRetrigger = -1;
        repaint();
    }

    void setPattern (const std::array<float, 16>& values, int length)
    {
        length = juce::jlimit (1, 16, length);
        if (showsPattern && pattern == values && patternLength == length) return;
        pattern = values;
        patternLength = length;
        showsPattern = true;
        repaint();
    }

    void clearPattern()
    {
        if (! showsPattern) return;
        showsPattern = false;
        repaint();
    }

    void setStepped (bool shouldBeStepped)
    {
        if (stepped == shouldBeStepped) return;
        stepped = shouldBeStepped;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour { 0xff091011 });
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (juce::Colour { 0xff244043 });
        g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX(), area.getRight());

        juce::Path path;
        if (showsPattern)
        {
            // Un pattern est une suite de cellules, pas un historique
            // echantillonne. Les separations restent visibles meme lorsque
            // deux pas consecutifs portent la meme valeur.
            g.setColour (juce::Colour { 0xff244043 }.withAlpha (0.55f));
            for (auto step = 1; step < patternLength; ++step)
            {
                const auto x = area.getX() + area.getWidth() * static_cast<float> (step)
                                                   / static_cast<float> (patternLength);
                g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
            }

            for (auto step = 0; step < patternLength; ++step)
            {
                const auto left = area.getX() + area.getWidth() * static_cast<float> (step)
                                                      / static_cast<float> (patternLength);
                const auto right = area.getX() + area.getWidth() * static_cast<float> (step + 1)
                                                       / static_cast<float> (patternLength);
                const auto y = juce::jmap (pattern[static_cast<size_t> (step)], -1.0f, 1.0f,
                                           area.getBottom(), area.getY());
                if (step == 0)
                    path.startNewSubPath (left, y);
                else
                    path.lineTo (left, y);
                path.lineTo (right, y);
            }
        }
        else
        {
        for (size_t i = 0; i < history.size(); ++i)
        {
            const auto x = area.getX() + area.getWidth() * static_cast<float> (i)
                                         / static_cast<float> (history.size() - 1);
            const auto y = juce::jmap (history[i], -1.0f, 1.0f, area.getBottom(), area.getY());
            if (i == 0)
                path.startNewSubPath (x, y);
            else if (stepped)
            {
                const auto previousY = juce::jmap (history[i - 1], -1.0f, 1.0f,
                                                   area.getBottom(), area.getY());
                path.lineTo (x, previousY);
                path.lineTo (x, y);
            }
            else
                path.lineTo (x, y);
        }
        }
        g.setColour (juce::Colour { 0xff00e6a0 });
        g.strokePath (path, juce::PathStrokeType { 1.5f });

        if (samplesSinceRetrigger >= 0)
        {
            const auto x = area.getRight()
                - area.getWidth() * static_cast<float> (samplesSinceRetrigger)
                                  / static_cast<float> (history.size() - 1);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        }
    }

private:
    std::array<float, 64> history {};
    std::array<float, 16> pattern {};
    int patternLength = 16;
    bool stepped = false;
    bool showsPattern = false;
    int samplesSinceRetrigger = -1;
};
}
