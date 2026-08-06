#pragma once

#include <JuceHeader.h>

namespace ndlr::ui
{
inline const juce::Colour background { 0xff0d1415 };
inline const juce::Colour panel { 0xff121f20 };
inline const juce::Colour panelEdge { 0xff244043 };
inline const juce::Colour cyan { 0xff00a6ff };
inline const juce::Colour green { 0xff00e66f };
inline const juce::Colour orange { 0xffee8451 };
inline const juce::Colour text { 0xffecf8f8 };
inline const juce::Colour muted { 0xff789092 };

// Propriétés visuelles facultatives, sauvegardées par le mode Layout sur les
// sliders +/- individuellement. L'absence d'une propriété conserve le style
// historique afin que les anciens layouts restent identiques.
inline const juce::Identifier incDecValueWidthProperty { "layoutValueWidth" };
inline const juce::Identifier incDecButtonWidthProperty { "layoutButtonWidth" };
inline const juce::Identifier incDecCornerRadiusProperty { "layoutButtonCornerRadius" };
inline const juce::Identifier incDecFillAlphaProperty { "layoutButtonFillAlpha" };
inline const juce::Identifier incDecOutlineAlphaProperty { "layoutButtonOutlineAlpha" };
inline const juce::Identifier incDecGlyphScaleProperty { "layoutButtonGlyphScale" };
inline const juce::Identifier incDecLineWidthProperty { "layoutButtonLineWidth" };

inline juce::Rectangle<int> getIncDecValueBounds (juce::Slider& slider)
{
    const auto bounds = slider.getLocalBounds();
    if (bounds.isEmpty() || slider.getTextBoxPosition() == juce::Slider::NoTextBox)
        return {};

    // Réserve toujours une cible à chacun des boutons, même dans les petites
    // cellules Perlin, puis centre la valeur dans l'espace restant.
    const auto defaultButtonWidth = juce::jlimit (8, 16, juce::jmax (8, bounds.getWidth() / 5));
    const auto minimumButtonWidth = juce::jlimit (6, 64, static_cast<int> (
        slider.getProperties().getWithDefault (incDecButtonWidthProperty,
                                                defaultButtonWidth)));
    const auto configuredValueWidth = juce::jlimit (12, 180, static_cast<int> (
        slider.getProperties().getWithDefault (incDecValueWidthProperty,
                                                slider.getTextBoxWidth())));
    const auto valueWidth = juce::jmin (configuredValueWidth,
                                       juce::jmax (0, bounds.getWidth() - 2 * minimumButtonWidth));
    const auto valueHeight = juce::jmin (slider.getTextBoxHeight(), bounds.getHeight());
    return juce::Rectangle<int> (valueWidth, valueHeight).withCentre (bounds.getCentre());
}

class IncDecButton final : public juce::Button
{
public:
    IncDecButton (juce::Slider& sliderToUse, bool isIncrementToUse)
        : juce::Button (isIncrementToUse ? "Increment" : "Decrement"),
          slider (sliderToUse), isIncrement (isIncrementToUse)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    bool hitTest (int x, int y) override
    {
        return getActionBounds().contains (x, y);
    }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = getActionBounds().toFloat().reduced (0.5f);
        if (bounds.isEmpty())
            return;

        const auto accent = slider.findColour (juce::Slider::thumbColourId)
                                  .withMultipliedAlpha (isEnabled() ? 1.0f : 0.38f);
        const auto& properties = slider.getProperties();
        const auto cornerRadius = juce::jlimit (0.0f, 16.0f, static_cast<float> (
            properties.getWithDefault (incDecCornerRadiusProperty, 3.0f)));
        const auto restingFillAlpha = juce::jlimit (0.0f, 1.0f, static_cast<float> (
            properties.getWithDefault (incDecFillAlphaProperty, 0.11f)));
        const auto restingOutlineAlpha = juce::jlimit (0.0f, 1.0f, static_cast<float> (
            properties.getWithDefault (incDecOutlineAlphaProperty, 0.72f)));
        const auto fillAlpha = juce::jmin (1.0f, restingFillAlpha
            + (isButtonDown ? 0.23f : (isMouseOverButton ? 0.11f : 0.0f)));
        const auto outlineAlpha = juce::jmin (1.0f, restingOutlineAlpha
            + (isMouseOverButton ? 0.23f : 0.0f));
        const auto lineWidth = juce::jlimit (0.5f, 5.0f, static_cast<float> (
            properties.getWithDefault (incDecLineWidthProperty, 1.6f)));

        g.setColour (accent.withAlpha (fillAlpha));
        g.fillRoundedRectangle (bounds, cornerRadius);
        g.setColour (accent.withAlpha (outlineAlpha));
        g.drawRoundedRectangle (bounds, cornerRadius,
                                isButtonDown ? lineWidth : juce::jmax (0.5f, lineWidth * 0.63f));

        const auto centre = bounds.getCentre();
        const auto glyphScale = juce::jlimit (0.08f, 0.48f, static_cast<float> (
            properties.getWithDefault (incDecGlyphScaleProperty, 0.24f)));
        const auto halfLength = juce::jlimit (1.5f, 12.0f, bounds.getWidth() * glyphScale);
        g.drawLine (centre.x - halfLength, centre.y,
                    centre.x + halfLength, centre.y, lineWidth);
        if (isIncrement)
            g.drawLine (centre.x, centre.y - halfLength,
                        centre.x, centre.y + halfLength, lineWidth);
    }

private:
    juce::Rectangle<int> getActionBounds() const
    {
        const auto valueBounds = getIncDecValueBounds (slider);
        const auto sideInSlider = isIncrement
            ? slider.getLocalBounds().withLeft (valueBounds.getRight())
            : slider.getLocalBounds().withRight (valueBounds.getX());
        return getBounds().getIntersection (sideInSlider).translated (-getX(), -getY());
    }

    juce::Slider& slider;
    const bool isIncrement;
};

class DraggableSliderLabel final : public juce::Label
{
public:
    explicit DraggableSliderLabel (juce::Slider& sliderToUse)
        : slider (sliderToUse)
    {
        setJustificationType (juce::Justification::centred);
        setKeyboardType (juce::TextInputTarget::decimalKeyboard);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isLeftButtonDown() && isEnabled())
        {
            valueAtDragStart = slider.getValue();
            return;
        }

        juce::Label::mouseDown (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! event.mods.isLeftButtonDown() || ! isEnabled())
        {
            juce::Label::mouseDrag (event);
            return;
        }

        if (! event.mouseWasDraggedSinceMouseDown())
            return;

        if (! dragNotification)
            dragNotification = std::make_unique<juce::Slider::ScopedDragNotification> (slider);

        auto sensitivity = juce::jmax (1, slider.getMouseDragSensitivity());
        if (event.mods.isShiftDown())
            sensitivity *= 4;

        const auto start = slider.valueToProportionOfLength (valueAtDragStart);
        const auto movement = -static_cast<double> (event.getDistanceFromDragStartY())
                              / static_cast<double> (sensitivity);
        slider.setValue (slider.proportionOfLengthToValue (juce::jlimit (0.0, 1.0, start + movement)),
                         juce::sendNotificationSync);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        // Label ouvre son éditeur sur un clic simple, mais pas après un drag.
        juce::Label::mouseUp (event);
        dragNotification.reset();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override {}

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return createIgnoredAccessibilityHandler (*this);
    }

private:
    juce::Slider& slider;
    std::unique_ptr<juce::Slider::ScopedDragNotification> dragNotification;
    double valueAtDragStart = 0.0;
};

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::Label::textColourId, text);
        setColour (juce::ComboBox::backgroundColourId, juce::Colour { 0xff091011 });
        setColour (juce::ComboBox::outlineColourId, panelEdge);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, cyan);
        setColour (juce::Slider::thumbColourId, cyan);
        setColour (juce::Slider::trackColourId, juce::Colour { 0xff29484b });
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour { 0xff091011 });
        setColour (juce::Slider::textBoxOutlineColourId, panelEdge);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxHighlightColourId, cyan.withAlpha (0.35f));
        setColour (juce::TextButton::buttonColourId, green.withAlpha (0.30f));
        setColour (juce::TextButton::buttonOnColourId, green);
        setColour (juce::TextButton::textColourOffId, green.withAlpha (0.30f));
        setColour (juce::TextButton::textColourOnId, background);
        setColour (juce::TabbedButtonBar::tabOutlineColourId, panelEdge);
        setColour (juce::TabbedButtonBar::frontOutlineColourId, green);
        setColour (juce::TabbedButtonBar::tabTextColourId, muted);
        setColour (juce::TabbedButtonBar::frontTextColourId, green);
    }

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override
    {
        const auto selected = button.isFrontTab();
        const auto accent = button.findColour (selected
                                                   ? juce::TabbedButtonBar::frontTextColourId
                                                   : juce::TabbedButtonBar::tabTextColourId)
                                  .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.35f);
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 1.0f);

        auto fill = panel;
        if (selected)
            fill = panel.interpolatedWith (accent, 0.16f);
        else if (isMouseOver || isMouseDown)
            fill = panel.interpolatedWith (accent, isMouseDown ? 0.14f : 0.09f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (accent.withMultipliedAlpha (selected ? 0.95f : 0.30f));
        g.drawRoundedRectangle (bounds, 5.0f, selected ? 1.6f : 1.0f);

        if (selected)
        {
            auto accentLine = bounds.removeFromBottom (3.0f).reduced (5.0f, 0.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (accentLine, 1.5f);
        }

        g.setColour (accent.withMultipliedAlpha (selected ? 1.0f : 0.30f));
        g.setFont (juce::FontOptions { 18.0f, juce::Font::bold });
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (6, 2),
                          juce::Justification::centred, 1);
    }

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override
    {
        if (button.getName().startsWith ("Motif Source")
            || button.getName().startsWith ("Motif Register")
            || button.getName().startsWith ("Rhythm Mode"))
            return juce::Font { juce::FontOptions { 11.0f, juce::Font::bold } };
        return juce::LookAndFeel_V4::getTextButtonFont (button, buttonHeight);
    }

    juce::Button* createSliderButton (juce::Slider& slider, bool isIncrement) override
    {
        return new IncDecButton (slider, isIncrement);
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        if (slider.getSliderStyle() != juce::Slider::IncDecButtons)
            return juce::LookAndFeel_V4::createSliderTextBox (slider);

        auto* label = new DraggableSliderLabel (slider);
        label->setColour (juce::Label::textColourId,
                          slider.findColour (juce::Slider::textBoxTextColourId));
        label->setColour (juce::Label::backgroundColourId,
                          slider.findColour (juce::Slider::textBoxBackgroundColourId));
        label->setColour (juce::Label::outlineColourId,
                          slider.findColour (juce::Slider::textBoxOutlineColourId));
        label->setColour (juce::TextEditor::textColourId,
                          slider.findColour (juce::Slider::textBoxTextColourId));
        label->setColour (juce::TextEditor::backgroundColourId,
                          slider.findColour (juce::Slider::textBoxBackgroundColourId));
        label->setColour (juce::TextEditor::outlineColourId,
                          slider.findColour (juce::Slider::textBoxOutlineColourId));
        label->setColour (juce::TextEditor::highlightColourId,
                          slider.findColour (juce::Slider::textBoxHighlightColourId));
        return label;
    }

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& slider) override
    {
        if (slider.getSliderStyle() != juce::Slider::IncDecButtons)
            return juce::LookAndFeel_V4::getSliderLayout (slider);

        return { slider.getLocalBounds(), getIncDecValueBounds (slider) };
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        const auto compact = box.getName().startsWith ("Mod MIDI CC");
        const auto arrowSpace = compact ? 15 : juce::jmin (22, box.getHeight() + 3);
        label.setBounds (4, 1, juce::jmax (1, box.getWidth() - arrowSpace - 5), box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width),
                                               static_cast<float> (height)).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
        const auto compact = box.getName().startsWith ("Mod MIDI CC");
        const auto squareSize = juce::jmin (compact ? 13.0f : 17.0f,
                                            juce::jmax (9.0f, static_cast<float> (height) - 4.0f));
        const auto square = juce::Rectangle<float> { squareSize, squareSize }
                                .withCentre ({ static_cast<float> (width) - squareSize * 0.5f - 2.0f,
                                               static_cast<float> (height) * 0.5f });
        const auto themeColour = box.findColour (juce::ComboBox::arrowColourId)
                                    .withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f);
        g.setColour (themeColour.withAlpha (isButtonDown ? 0.68f : 0.5f));
        g.fillRoundedRectangle (square, 2.5f);
        g.setColour (themeColour.withAlpha (0.8f));
        g.drawRoundedRectangle (square, 2.5f, 1.0f);

        const auto centre = square.getCentre();
        const auto arrowHalfWidth = juce::jmax (2.5f, squareSize * 0.23f);
        juce::Path arrow;
        arrow.addTriangle (centre.x - arrowHalfWidth, centre.y - 1.5f,
                           centre.x + arrowHalfWidth, centre.y - 1.5f,
                           centre.x, centre.y + 2.5f);
        g.setColour (background.withAlpha (0.9f));
        g.fillPath (arrow);
        juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH, isButtonDown);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float position, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                     static_cast<float> (width), static_cast<float> (height))
                                .reduced (6.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        g.setColour (panelEdge);
        g.drawEllipse (bounds.withSizeKeepingCentre (radius * 2.0f, radius * 2.0f), 3.0f);
        const auto angle = startAngle + position * (endAngle - startAngle);
        juce::Path pointer;
        pointer.addRoundedRectangle (-2.0f, -radius + 7.0f, 4.0f, radius * 0.55f, 2.0f);
        g.setColour (cyan);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::TwoValueHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                    minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto centreY = static_cast<float> (y + height / 2);
        const auto left = static_cast<float> (x + 6);
        const auto right = static_cast<float> (x + width - 6);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.drawLine (left, centreY, right, centreY, 4.0f);
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.drawLine (minSliderPos, centreY, maxSliderPos, centreY, 5.0f);
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillEllipse (minSliderPos - 5.0f, centreY - 5.0f, 10.0f, 10.0f);
        g.fillEllipse (maxSliderPos - 5.0f, centreY - 5.0f, 10.0f, 10.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override
    {
        if (button.getButtonText().isNotEmpty())
        {
            const auto enabledAlpha = button.isEnabled() ? 1.0f : 0.4f;
            const auto stateAlpha = (button.getToggleState() ? 1.0f : 0.30f) * enabledAlpha;
            const auto accent = button.findColour (juce::ToggleButton::tickColourId);
            const auto boxSize = static_cast<float> (
                juce::jlimit (12, 18, button.getHeight() - 6));
            const auto box = juce::Rectangle<float> { boxSize, boxSize }
                                 .withCentre ({ 4.0f + boxSize * 0.5f,
                                                static_cast<float> (button.getHeight()) * 0.5f });

            g.setColour (accent.withAlpha (button.getToggleState() ? 0.20f : 0.06f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (accent.withAlpha (stateAlpha));
            g.drawRoundedRectangle (box, 3.0f, shouldDrawButtonAsDown ? 2.0f : 1.2f);
            if (button.getToggleState())
            {
                juce::Path tick;
                tick.startNewSubPath (box.getX() + boxSize * 0.22f, box.getCentreY());
                tick.lineTo (box.getX() + boxSize * 0.43f,
                             box.getBottom() - boxSize * 0.24f);
                tick.lineTo (box.getRight() - boxSize * 0.18f,
                             box.getY() + boxSize * 0.23f);
                g.strokePath (tick, juce::PathStrokeType (2.0f));
            }

            if (shouldDrawButtonAsHighlighted)
                g.setColour (accent.withAlpha (juce::jmin (1.0f, stateAlpha + 0.15f)));
            g.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
            g.drawFittedText (button.getButtonText(),
                              button.getLocalBounds().withTrimmedLeft (
                                  juce::roundToInt (box.getRight() + 6.0f)),
                              juce::Justification::centredLeft, 1);
            return;
        }

        const auto size = static_cast<float> (juce::jlimit (10, 16, button.getHeight() - 4));
        const auto box = juce::Rectangle<float> { size, size }.withCentre (
            button.getLocalBounds().toFloat().getCentre());
        const auto activeColour = button.findColour (juce::ToggleButton::tickColourId)
                                        .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f);
        g.setColour (button.getToggleState() ? activeColour.withAlpha (0.22f) : panelEdge);
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (button.getToggleState() ? activeColour : muted);
        g.drawRoundedRectangle (box, 3.0f, shouldDrawButtonAsDown ? 2.0f : 1.2f);
        if (button.getToggleState())
        {
            juce::Path tick;
            tick.startNewSubPath (box.getX() + size * 0.22f, box.getCentreY());
            tick.lineTo (box.getX() + size * 0.43f, box.getBottom() - size * 0.24f);
            tick.lineTo (box.getRight() - size * 0.18f, box.getY() + size * 0.23f);
            g.strokePath (tick, juce::PathStrokeType (2.0f));
        }
        juce::ignoreUnused (shouldDrawButtonAsHighlighted);
    }
};

inline void configureIncDecSliders (juce::Component& component)
{
    if (auto* slider = dynamic_cast<juce::Slider*> (&component);
        slider != nullptr && slider->getSliderStyle() == juce::Slider::IncDecButtons)
    {
        // TextBoxAbove empêche JUCE de rogner horizontalement les deux cibles.
        // Le LookAndFeel place malgré tout la valeur au centre.
        slider->setTextBoxStyle (juce::Slider::TextBoxAbove, ! slider->isTextBoxEditable(),
                                 slider->getTextBoxWidth(), slider->getTextBoxHeight());
        slider->setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        slider->setMouseDragSensitivity (160);
    }

    for (auto i = 0; i < component.getNumChildComponents(); ++i)
        configureIncDecSliders (*component.getChildComponent (i));
}

class SectionPanel final : public juce::Component
{
public:
    std::function<void()> onPanelClicked;
    SectionPanel (juce::String titleToUse, juce::Colour accentToUse)
        : title (std::move (titleToUse)), accent (accentToUse)
    {
        // Reçoit aussi les clics effectués sur les boutons, menus et sliders
        // contenus dans le panneau.
        addMouseListener (this, true);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (panel);
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (accent.withAlpha (0.30f));
        g.drawRoundedRectangle (bounds, 10.0f, 1.5f);
        g.setColour (accent);
        g.fillRoundedRectangle ({ bounds.getX(), bounds.getY(), 5.0f, bounds.getHeight() },
                                2.5f);
        g.setFont (juce::FontOptions { 18.0f, juce::Font::bold });
        g.drawText (title.toUpperCase(), 17, 6, getWidth() - 26, 26, juce::Justification::left);
    }

    void setAccentColour (juce::Colour newAccent) { accent = newAccent; repaint(); }
    juce::Colour getAccentColour() const noexcept { return accent; }
    void mouseDown (const juce::MouseEvent&) override { if (onPanelClicked) onPanelClicked(); }

private:
    juce::String title;
    juce::Colour accent;
};
}
