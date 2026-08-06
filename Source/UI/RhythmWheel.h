#pragma once

#include <JuceHeader.h>

namespace ndlr::ui
{
class RhythmWheel final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setLength (int);
    void setStates (const std::array<int, 32>&);
    void setVelocities (const std::array<int, 32>&);
    void setRatchets (const std::array<int, 32>&);
    void setPlayhead (int);
    void setDivisionLabel (juce::String);
    void setRatchetVelocityMode (int);
    void setAccentColour (juce::Colour colour) { accentColour = colour; repaint(); }

    std::function<void (int step, int state)> onStateChanged;
    std::function<void (int step, int velocity)> onVelocityChanged;
    std::function<void (int step, int ratchet)> onRatchetChanged;
    std::function<void (int delta)> onDivisionStep;
    std::function<void (int mode)> onRatchetVelocityModeChanged;

private:
    juce::Point<float> nodePosition (int step) const;

    int length = 8;
    int playhead = -1;
    int selectedStep = 0;
    int dragStep = -1;
    juce::String divisionLabel { "4n" };
    juce::Colour accentColour { 0xff00a6ff };
    int ratchetVelocityMode = 0;
    std::array<int, 32> velocities = []
    {
        std::array<int, 32> result {};
        result.fill (100);
        return result;
    }();
    std::array<int, 32> ratchets = []
    {
        std::array<int, 32> result {};
        result.fill (1);
        return result;
    }();
    std::array<int, 32> states = []
    {
        std::array<int, 32> result {};
        result.fill (1);
        return result;
    }();
};
}
