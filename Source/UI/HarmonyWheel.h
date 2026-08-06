#pragma once

#include <JuceHeader.h>

namespace ndlr::ui
{
class HarmonyWheel final : public juce::Component
{
public:
    HarmonyWheel();

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;

    void setSelection (int keyIndex, int degreeIndex);
    void setHarmonyNames (juce::String scaleName, juce::String chordName);

    std::function<void (int)> onKeyChanged;
    std::function<void (int)> onDegreeChanged;

private:
    static juce::Path makeSector (juce::Point<float> centre, float innerRadius,
                                  float outerRadius, float startAngle, float endAngle);
    static int sectorFromPoint (juce::Point<float> point, juce::Point<float> centre,
                                int sectorCount) noexcept;

    int selectedKey = 0;
    int selectedDegree = 0;
    juce::String scale { "Major" };
    juce::String chord { "Triad" };
};
}
