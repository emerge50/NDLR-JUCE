#pragma once
#include <JuceHeader.h>
#include "NdlrLookAndFeel.h"

namespace ndlr::ui
{
class LayoutNumericField final : public juce::Component
{
public:
    LayoutNumericField (juce::String labelText, int decimalPlacesToUse)
        : decimalPlaces (decimalPlacesToUse)
    {
        label.setText (std::move (labelText), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, text.withAlpha (0.82f));
        label.setFont (juce::FontOptions { 11.5f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centredLeft);
        label.setMinimumHorizontalScale (0.78f);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        editor.setJustification (juce::Justification::centredRight);
        editor.setInputRestrictions (8, decimalPlaces > 0 ? "-0123456789." : "-0123456789");
        editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour { 0xff091011 });
        editor.setColour (juce::TextEditor::outlineColourId, panelEdge);
        editor.setColour (juce::TextEditor::focusedOutlineColourId, cyan);
        editor.onReturnKey = [this] { commit(); };
        editor.onFocusLost = [this] { commit(); };
        addAndMakeVisible (editor);
    }

    std::function<void(double)> onValueChanged;

    void setValue (double value)
    {
        editor.setText (juce::String (value, decimalPlaces), false);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromLeft (145));
        editor.setBounds (area.reduced (1, 2));
    }

private:
    void commit()
    {
        if (onValueChanged != nullptr)
            onValueChanged (editor.getText().getDoubleValue());
    }

    juce::Label label;
    juce::TextEditor editor;
    int decimalPlaces = 0;
};

class LayoutAttributePanel final : public juce::Component
{
public:
    enum GeometryAttribute { positionX, positionY, width, height };
    enum IncDecAttribute
    {
        valueWidth, buttonWidth, cornerRadius, fillOpacity,
        outlineOpacity, glyphScale, lineWidth
    };

    LayoutAttributePanel()
    {
        setName ("Layout Attributes");
        title.setText ("ATTRIBUTS DU LAYOUT", juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, green);
        title.setFont (juce::FontOptions { 16.0f, juce::Font::bold });
        title.setJustificationType (juce::Justification::centredLeft);
        title.setMinimumHorizontalScale (0.80f);
        title.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (title);

        selection.setText ("Selectionnez un element", juce::dontSendNotification);
        selection.setColour (juce::Label::textColourId, text);
        selection.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
        selection.setJustificationType (juce::Justification::centredLeft);
        selection.setMinimumHorizontalScale (0.58f);
        selection.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (selection);

        colourLabel.setText ("COULEUR", juce::dontSendNotification);
        colourLabel.setColour (juce::Label::textColourId, text.withAlpha (0.82f));
        colourLabel.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
        colourLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (colourLabel);
        colour.addItem ("Vert", 1);
        colour.addItem ("Bleu", 2);
        colour.addItem ("Orange", 3);
        colour.addItem ("Jaune", 4);
        colour.addItem ("Blanc", 5);
        colour.addItem ("Violet", 6);
        colour.addSeparator();
        colour.addItem ("Gris clair", 7);
        colour.addItem ("Gris moyen", 8);
        colour.addItem ("Gris fonce", 9);
        colour.addItem ("Anthracite", 10);
        colour.onChange = [this]
        {
            if (! updating && onColourChanged != nullptr && colour.getSelectedId() > 0)
                onColourChanged (colourForId (colour.getSelectedId()));
        };
        addAndMakeVisible (colour);

        sectionGeometry.setText ("GEOMETRIE", juce::dontSendNotification);
        sectionIncDec.setText ("BOUTONS  - / +", juce::dontSendNotification);
        for (auto* label : { &sectionGeometry, &sectionIncDec })
        {
            label->setColour (juce::Label::textColourId, cyan.withAlpha (0.82f));
            label->setFont (juce::FontOptions { 12.0f, juce::Font::bold });
            label->setJustificationType (juce::Justification::centredLeft);
            label->setMinimumHorizontalScale (0.80f);
            label->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*label);
        }

        const std::array<LayoutNumericField*, 4> geometryFields {
            &xField, &yField, &widthField, &heightField
        };
        for (auto index = 0; index < static_cast<int> (geometryFields.size()); ++index)
        {
            auto* field = geometryFields[static_cast<size_t> (index)];
            field->onValueChanged = [this, index] (double value)
            {
                if (! updating && onGeometryChanged != nullptr)
                    onGeometryChanged (index, value);
            };
            addAndMakeVisible (*field);
        }

        const std::array<LayoutNumericField*, 7> styleFields {
            &valueWidthField, &buttonWidthField, &cornerField, &fillField,
            &outlineField, &glyphField, &lineField
        };
        for (auto index = 0; index < static_cast<int> (styleFields.size()); ++index)
        {
            auto* field = styleFields[static_cast<size_t> (index)];
            field->onValueChanged = [this, index] (double value)
            {
                if (! updating && onIncDecChanged != nullptr)
                    onIncDecChanged (index, value);
            };
            addAndMakeVisible (*field);
        }
        showIncDecFields (false);
    }

    std::function<void(int, double)> onGeometryChanged;
    std::function<void(int, double)> onIncDecChanged;
    std::function<void(juce::Colour)> onColourChanged;

    void setSelection (const juce::String& name, juce::Component* component,
                       juce::Colour accent, bool colourIsEditable)
    {
        updating = true;
        selectedComponent = component;
        selection.setText (component != nullptr ? name : "Selectionnez un element",
                           juce::dontSendNotification);
        colour.setEnabled (component != nullptr && colourIsEditable);
        colour.setSelectedId (colourIsEditable ? idForColour (accent) : 0,
                              juce::dontSendNotification);
        refreshGeometry();

        auto* slider = dynamic_cast<juce::Slider*> (component);
        const auto hasIncDec = slider != nullptr
            && slider->getSliderStyle() == juce::Slider::IncDecButtons;
        showIncDecFields (hasIncDec);
        if (hasIncDec)
        {
            const auto& properties = slider->getProperties();
            const auto defaultButtonWidth = juce::jlimit (8, 16,
                juce::jmax (8, slider->getWidth() / 5));
            valueWidthField.setValue (static_cast<int> (properties.getWithDefault (
                incDecValueWidthProperty, slider->getTextBoxWidth())));
            buttonWidthField.setValue (static_cast<int> (properties.getWithDefault (
                incDecButtonWidthProperty, defaultButtonWidth)));
            cornerField.setValue (static_cast<double> (properties.getWithDefault (
                incDecCornerRadiusProperty, 3.0)));
            fillField.setValue (100.0 * static_cast<double> (properties.getWithDefault (
                incDecFillAlphaProperty, 0.11)));
            outlineField.setValue (100.0 * static_cast<double> (properties.getWithDefault (
                incDecOutlineAlphaProperty, 0.72)));
            glyphField.setValue (100.0 * static_cast<double> (properties.getWithDefault (
                incDecGlyphScaleProperty, 0.24)));
            lineField.setValue (static_cast<double> (properties.getWithDefault (
                incDecLineWidthProperty, 1.6)));
        }
        updating = false;
        resized();
        repaint();
    }

    void refreshGeometry()
    {
        if (selectedComponent == nullptr)
            return;
        updating = true;
        xField.setValue (selectedComponent->getX());
        yField.setValue (selectedComponent->getY());
        widthField.setValue (selectedComponent->getWidth());
        heightField.setValue (selectedComponent->getHeight());
        updating = false;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour { 0xf20d1415 });
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (green.withAlpha (0.72f));
        g.drawRoundedRectangle (bounds, 7.0f, 1.2f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10, 7);
        title.setBounds (area.removeFromTop (28));
        selection.setBounds (area.removeFromTop (34));
        auto colourRow = area.removeFromTop (30);
        colourLabel.setBounds (colourRow.removeFromLeft (145));
        colour.setBounds (colourRow.reduced (1, 1));
        area.removeFromTop (6);
        sectionGeometry.setBounds (area.removeFromTop (24));
        for (auto* field : { &xField, &yField, &widthField, &heightField })
            field->setBounds (area.removeFromTop (25));
        area.removeFromTop (6);
        sectionIncDec.setBounds (area.removeFromTop (24));
        for (auto* field : { &valueWidthField, &buttonWidthField, &cornerField,
                             &fillField, &outlineField, &glyphField, &lineField })
            field->setBounds (area.removeFromTop (25));
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragger.startDraggingComponent (this, event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        dragger.dragComponent (this, event, nullptr);
    }

private:
    static juce::Colour colourForId (int id)
    {
        static const std::array<juce::Colour, 10> colours {
            green, cyan, orange, juce::Colours::yellow,
            juce::Colours::white, juce::Colour { 0xffa66cff },
            juce::Colour { 0xffc5cecf }, juce::Colour { 0xff7f9193 },
            juce::Colour { 0xff4d6062 }, juce::Colour { 0xff29383a }
        };
        return colours[static_cast<size_t> (juce::jlimit (1, 10, id) - 1)];
    }

    static int idForColour (juce::Colour value)
    {
        for (auto id = 1; id <= 10; ++id)
            if (colourForId (id).getARGB() == value.getARGB())
                return id;
        return 0;
    }

    void showIncDecFields (bool shouldShow)
    {
        sectionIncDec.setVisible (shouldShow);
        for (auto* field : { &valueWidthField, &buttonWidthField, &cornerField,
                             &fillField, &outlineField, &glyphField, &lineField })
            field->setVisible (shouldShow);
    }

    juce::Label title, selection, colourLabel, sectionGeometry, sectionIncDec;
    juce::ComboBox colour;
    LayoutNumericField xField { "X", 0 }, yField { "Y", 0 };
    LayoutNumericField widthField { "Largeur", 0 }, heightField { "Hauteur", 0 };
    LayoutNumericField valueWidthField { "Largeur valeur", 0 };
    LayoutNumericField buttonWidthField { "Largeur bouton", 0 };
    LayoutNumericField cornerField { "Arrondi", 1 };
    LayoutNumericField fillField { "Fond %", 0 };
    LayoutNumericField outlineField { "Contour %", 0 };
    LayoutNumericField glyphField { "Symbole %", 0 };
    LayoutNumericField lineField { "Epaisseur", 1 };
    juce::Component* selectedComponent = nullptr;
    juce::ComponentDragger dragger;
    bool updating = false;
};

class LayoutDebugOverlay final : public juce::Component
{
public:
    std::function<void()> onLayoutChanged;

    LayoutDebugOverlay()
    {
        addAndMakeVisible (attributePanel);
        attributePanel.onGeometryChanged = [this] (int attribute, double value)
        {
            applyGeometryAttribute (attribute, value);
        };
        attributePanel.onIncDecChanged = [this] (int attribute, double value)
        {
            applyIncDecAttribute (attribute, value);
        };
        attributePanel.onColourChanged = [this] (juce::Colour colour)
        {
            if (selectedTarget == nullptr)
                return;
            applyEditableColour (*selectedTarget->component, colour);
            changed();
        };
    }

    void addTarget (juce::Component& component, juce::String name,
                    juce::String legacyName = {})
    {
        targets.push_back ({ &component, std::move (name), std::move (legacyName), 0, false });
    }

    void resized() override
    {
        if (! panelPositioned)
        {
            attributePanel.setBounds (juce::jmax (8, getWidth() - 346), 8, 338, 452);
            panelPositioned = true;
        }
        else
        {
            auto position = attributePanel.getPosition();
            position.x = juce::jlimit (0, juce::jmax (0, getWidth() - attributePanel.getWidth()), position.x);
            position.y = juce::jlimit (0, juce::jmax (0, getHeight() - attributePanel.getHeight()), position.y);
            attributePanel.setTopLeftPosition (position);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.12f));
        // Grille de contrainte 8 px.
        g.setColour (juce::Colours::white.withAlpha (0.055f));
        for (auto x = 0; x < getWidth(); x += gridSize) g.drawVerticalLine (x, 0.0f, static_cast<float> (getHeight()));
        for (auto y = 0; y < getHeight(); y += gridSize) g.drawHorizontalLine (y, 0.0f, static_cast<float> (getWidth()));

        for (const auto& target : targets)
        {
            if (! target.component->isShowing())
                continue;

            auto bounds = getLocalArea (target.component->getParentComponent(), target.component->getBounds());
            const auto selected = selectedTarget != nullptr
                && target.component == selectedTarget->component;
            g.setColour (selected ? juce::Colours::yellow : juce::Colours::cyan.withAlpha (0.85f));
            g.drawRect (bounds, selected ? 3 : 1);
            g.fillRect (bounds.getRight() - 10, bounds.getBottom() - 10, 10, 10);
            g.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
            const auto infoText = target.name + "  x:" + juce::String (target.component->getX())
                                + " y:" + juce::String (target.component->getY())
                                + " w:" + juce::String (target.component->getWidth())
                                + " h:" + juce::String (target.component->getHeight());
            g.drawText (infoText, bounds.removeFromTop (20).reduced (4, 0), juce::Justification::centredLeft);
        }

        if (active != nullptr)
        {
            const auto bounds = getLocalArea (active->getParentComponent(), active->getBounds());
            g.setColour (juce::Colours::yellow.withAlpha (0.45f));
            g.drawVerticalLine (bounds.getX(), 0.0f, static_cast<float> (getHeight()));
            g.drawVerticalLine (bounds.getRight(), 0.0f, static_cast<float> (getHeight()));
            g.drawHorizontalLine (bounds.getY(), 0.0f, static_cast<float> (getWidth()));
            g.drawHorizontalLine (bounds.getBottom(), 0.0f, static_cast<float> (getWidth()));
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        active = nullptr;
        activeTarget = nullptr;
        for (auto iterator = targets.rbegin(); iterator != targets.rend(); ++iterator)
        {
            if (! iterator->component->isShowing())
                continue;

            const auto localBounds = getLocalArea (iterator->component->getParentComponent(),
                                                   iterator->component->getBounds());
            if (localBounds.contains (event.position.toInt()))
            {
                active = iterator->component;
                activeTarget = &*iterator;
                selectedTarget = activeTarget;
                updateAttributePanel();
                const auto pointInParent = active->getParentComponent()->getLocalPoint (
                    this, event.position.toInt());
                dragOffset = pointInParent - active->getPosition();
                const auto cornerDelta = localBounds.getBottomRight() - event.position.toInt();
                resizing = cornerDelta.x * cornerDelta.x + cornerDelta.y * cornerDelta.y <= 256;
                if (event.mods.isPopupMenu())
                {
                    cycleColour (*activeTarget);
                    updateAttributePanel();
                    changed();
                    active = nullptr;
                    activeTarget = nullptr;
                }
                break;
            }
        }
        if (active == nullptr && activeTarget == nullptr && ! event.mods.isPopupMenu())
        {
            selectedTarget = nullptr;
            updateAttributePanel();
        }
        attributePanel.toFront (false);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (active == nullptr) return;
        const auto pointInParent = active->getParentComponent()->getLocalPoint (
            this, event.position.toInt());
        if (resizing)
        {
            auto size = pointInParent - active->getPosition();
            if (! event.mods.isAltDown()) size = snapPoint (size);
            active->setSize (juce::jmax (20, size.x), juce::jmax (20, size.y));
        }
        else
        {
            auto position = pointInParent - dragOffset;
            if (! event.mods.isAltDown()) position = snapPoint (position);
            active->setTopLeftPosition (position);
        }
        attributePanel.refreshGeometry();
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (active != nullptr) changed();
        active = nullptr;
        activeTarget = nullptr;
    }

    void copyTargetLayout (juce::Component& source, juce::Component& destination,
                           bool copyColour = true)
    {
        destination.setBounds (source.getBounds());

        bool sourceColourIsEditable = false;
        bool destinationColourIsEditable = false;
        const auto sourceColour = getEditableColour (source, sourceColourIsEditable);
        getEditableColour (destination, destinationColourIsEditable);
        if (copyColour && sourceColourIsEditable && destinationColourIsEditable)
            applyEditableColour (destination, sourceColour);

        auto* sourceSlider = dynamic_cast<juce::Slider*> (&source);
        auto* destinationSlider = dynamic_cast<juce::Slider*> (&destination);
        if (sourceSlider != nullptr && destinationSlider != nullptr
            && sourceSlider->getSliderStyle() == juce::Slider::IncDecButtons
            && destinationSlider->getSliderStyle() == juce::Slider::IncDecButtons)
        {
            const std::array<juce::Identifier, 7> styleProperties {
                incDecValueWidthProperty, incDecButtonWidthProperty,
                incDecCornerRadiusProperty, incDecFillAlphaProperty,
                incDecOutlineAlphaProperty, incDecGlyphScaleProperty,
                incDecLineWidthProperty
            };
            auto& sourceProperties = sourceSlider->getProperties();
            auto& destinationProperties = destinationSlider->getProperties();
            for (const auto& property : styleProperties)
            {
                if (sourceProperties.contains (property))
                    destinationProperties.set (property, sourceProperties[property]);
                else
                    destinationProperties.remove (property);
            }
            destinationSlider->setTextBoxStyle (
                sourceSlider->getTextBoxPosition(), ! sourceSlider->isTextBoxEditable(),
                sourceSlider->getTextBoxWidth(), sourceSlider->getTextBoxHeight());
        }
        destination.repaint();
    }

    bool restoreTargetFromCompiledDefault (juce::Component& component)
    {
        const auto layout = getCompiledDefaultLayout();
        if (! layout.isValid())
            return false;

        for (auto& target : targets)
        {
            if (target.component != &component)
                continue;

            for (auto item : layout)
                if (const auto id = item["id"].toString();
                    id == target.name
                        || (! target.legacyName.isEmpty() && id == target.legacyName))
                {
                    component.setBounds (static_cast<int> (item["x"]),
                                         static_cast<int> (item["y"]),
                                         static_cast<int> (item["w"]),
                                         static_cast<int> (item["h"]));
                    restoreAttributes (target, item);
                    target.attributesRestored = true;
                    component.repaint();
                    return true;
                }
            return false;
        }
        return false;
    }

    void saveLayout (juce::ValueTree state) const
    {
        auto previous = state.getChildWithName ("UILayout");
        if (previous.isValid()) state.removeChild (previous, nullptr);
        juce::ValueTree layout { "UILayout" };
        for (const auto& target : targets)
        {
            juce::ValueTree item { "Item" };
            item.setProperty ("id", target.name, nullptr);
            item.setProperty ("x", target.component->getX(), nullptr);
            item.setProperty ("y", target.component->getY(), nullptr);
            item.setProperty ("w", target.component->getWidth(), nullptr);
            item.setProperty ("h", target.component->getHeight(), nullptr);
            bool colourIsEditable = false;
            const auto accent = getEditableColour (*target.component, colourIsEditable);
            if (colourIsEditable)
                item.setProperty ("accent", accent.toString(), nullptr);
            if (auto* slider = dynamic_cast<juce::Slider*> (target.component);
                slider != nullptr && slider->getSliderStyle() == juce::Slider::IncDecButtons)
            {
                const auto& properties = slider->getProperties();
                const auto defaultButtonWidth = juce::jlimit (8, 16,
                    juce::jmax (8, slider->getWidth() / 5));
                item.setProperty ("valueWidth", properties.getWithDefault (
                    incDecValueWidthProperty, slider->getTextBoxWidth()), nullptr);
                item.setProperty ("buttonWidth", properties.getWithDefault (
                    incDecButtonWidthProperty, defaultButtonWidth), nullptr);
                item.setProperty ("cornerRadius", properties.getWithDefault (
                    incDecCornerRadiusProperty, 3.0), nullptr);
                item.setProperty ("fillAlpha", properties.getWithDefault (
                    incDecFillAlphaProperty, 0.11), nullptr);
                item.setProperty ("outlineAlpha", properties.getWithDefault (
                    incDecOutlineAlphaProperty, 0.72), nullptr);
                item.setProperty ("glyphScale", properties.getWithDefault (
                    incDecGlyphScaleProperty, 0.24), nullptr);
                item.setProperty ("lineWidth", properties.getWithDefault (
                    incDecLineWidthProperty, 1.6), nullptr);
            }
            layout.appendChild (item, nullptr);
        }
        state.appendChild (layout, nullptr);
    }

    void restoreLayout (const juce::ValueTree& state)
    {
        auto layout = state.getChildWithName ("UILayout");
        if (! layout.isValid()) layout = getCompiledDefaultLayout();
        if (! layout.isValid()) return;
        for (auto& target : targets)
            for (auto item : layout)
                if (const auto id = item["id"].toString();
                    id == target.name || (! target.legacyName.isEmpty() && id == target.legacyName))
                {
                    target.component->setBounds (static_cast<int> (item["x"]), static_cast<int> (item["y"]),
                                                 static_cast<int> (item["w"]), static_cast<int> (item["h"]));
                    if (! target.attributesRestored)
                    {
                        restoreAttributes (target, item);
                        target.attributesRestored = true;
                    }
                    break;
                }
        repaint();
    }

    bool installCompiledDefaultLayout (juce::ValueTree state)
    {
        const auto compiledLayout = getCompiledDefaultLayout();
        if (! compiledLayout.isValid())
            return false;

        if (const auto previous = state.getChildWithName ("UILayout"); previous.isValid())
            state.removeChild (previous, nullptr);
        state.appendChild (compiledLayout.createCopy(), nullptr);
        for (auto& target : targets) target.attributesRestored = false;
        restoreLayout (state);
        return true;
    }

    juce::String getCompiledDefaultLayoutFingerprint() const
    {
        if (const auto xml = getCompiledDefaultLayout().createXml())
            return juce::String::toHexString (xml->toString().hashCode64());
        return {};
    }

private:
    static juce::ValueTree getCompiledDefaultLayout();

    static constexpr int gridSize = 8;
    static juce::Point<int> snapPoint (juce::Point<int> point) noexcept
    {
        const auto snap = [] (int value) { return juce::roundToInt (static_cast<float> (value) / gridSize) * gridSize; };
        return { snap (point.x), snap (point.y) };
    }

    struct Target
    {
        juce::Component* component;
        juce::String name;
        juce::String legacyName;
        int colourIndex;
        bool attributesRestored;
    };

    void changed()
    {
        repaint();
        if (onLayoutChanged != nullptr)
            onLayoutChanged();
    }

    void updateAttributePanel()
    {
        bool colourIsEditable = false;
        auto accent = juce::Colours::transparentBlack;
        if (selectedTarget != nullptr)
            accent = getEditableColour (*selectedTarget->component, colourIsEditable);
        attributePanel.setSelection (selectedTarget != nullptr ? selectedTarget->name
                                                               : juce::String {},
                                     selectedTarget != nullptr ? selectedTarget->component
                                                               : nullptr,
                                     accent, colourIsEditable);
    }

    void applyGeometryAttribute (int attribute, double value)
    {
        if (selectedTarget == nullptr)
            return;
        auto& component = *selectedTarget->component;
        const auto integerValue = juce::roundToInt (value);
        if (attribute == LayoutAttributePanel::positionX)
            component.setTopLeftPosition (integerValue, component.getY());
        else if (attribute == LayoutAttributePanel::positionY)
            component.setTopLeftPosition (component.getX(), integerValue);
        else if (attribute == LayoutAttributePanel::width)
            component.setSize (juce::jlimit (20, 4000, integerValue), component.getHeight());
        else if (attribute == LayoutAttributePanel::height)
            component.setSize (component.getWidth(), juce::jlimit (20, 4000, integerValue));
        attributePanel.refreshGeometry();
        changed();
    }

    void applyIncDecAttribute (int attribute, double value)
    {
        if (selectedTarget == nullptr)
            return;
        auto* slider = dynamic_cast<juce::Slider*> (selectedTarget->component);
        if (slider == nullptr || slider->getSliderStyle() != juce::Slider::IncDecButtons)
            return;

        auto& properties = slider->getProperties();
        if (attribute == LayoutAttributePanel::valueWidth)
        {
            const auto legalValue = juce::jlimit (12, 180, juce::roundToInt (value));
            properties.set (incDecValueWidthProperty, legalValue);
            slider->setTextBoxStyle (slider->getTextBoxPosition(), ! slider->isTextBoxEditable(),
                                     legalValue, slider->getTextBoxHeight());
        }
        else if (attribute == LayoutAttributePanel::buttonWidth)
            properties.set (incDecButtonWidthProperty,
                            juce::jlimit (6, 64, juce::roundToInt (value)));
        else if (attribute == LayoutAttributePanel::cornerRadius)
            properties.set (incDecCornerRadiusProperty,
                            juce::jlimit (0.0, 16.0, value));
        else if (attribute == LayoutAttributePanel::fillOpacity)
            properties.set (incDecFillAlphaProperty,
                            juce::jlimit (0.0, 100.0, value) / 100.0);
        else if (attribute == LayoutAttributePanel::outlineOpacity)
            properties.set (incDecOutlineAlphaProperty,
                            juce::jlimit (0.0, 100.0, value) / 100.0);
        else if (attribute == LayoutAttributePanel::glyphScale)
            properties.set (incDecGlyphScaleProperty,
                            juce::jlimit (8.0, 48.0, value) / 100.0);
        else if (attribute == LayoutAttributePanel::lineWidth)
            properties.set (incDecLineWidthProperty,
                            juce::jlimit (0.5, 5.0, value));

        slider->repaint();
        updateAttributePanel();
        changed();
    }

    static juce::Colour getEditableColour (juce::Component& component, bool& isEditable)
    {
        isEditable = true;
        if (auto* section = dynamic_cast<SectionPanel*> (&component))
            return section->getAccentColour();
        if (auto* label = dynamic_cast<juce::Label*> (&component))
            return label->findColour (juce::Label::textColourId);
        if (auto* combo = dynamic_cast<juce::ComboBox*> (&component))
            return combo->findColour (juce::ComboBox::textColourId);
        if (auto* slider = dynamic_cast<juce::Slider*> (&component))
            return slider->findColour (juce::Slider::thumbColourId);
        if (auto* button = dynamic_cast<juce::TextButton*> (&component))
            return button->findColour (juce::TextButton::buttonOnColourId);
        if (auto* toggle = dynamic_cast<juce::ToggleButton*> (&component))
            return toggle->findColour (juce::ToggleButton::tickColourId);
        isEditable = false;
        return juce::Colours::transparentBlack;
    }

    static void applyEditableColour (juce::Component& component, juce::Colour colour)
    {
        if (auto* section = dynamic_cast<SectionPanel*> (&component))
            section->setAccentColour (colour);
        if (auto* label = dynamic_cast<juce::Label*> (&component))
            label->setColour (juce::Label::textColourId, colour);
        if (auto* combo = dynamic_cast<juce::ComboBox*> (&component))
        {
            combo->setColour (juce::ComboBox::textColourId, colour);
            combo->setColour (juce::ComboBox::arrowColourId, colour);
            combo->setColour (juce::ComboBox::outlineColourId, colour.withAlpha (0.55f));
        }
        if (auto* slider = dynamic_cast<juce::Slider*> (&component))
        {
            slider->setColour (juce::Slider::thumbColourId, colour);
            slider->setColour (juce::Slider::textBoxTextColourId, colour);
        }
        if (auto* button = dynamic_cast<juce::TextButton*> (&component))
        {
            button->setColour (juce::TextButton::buttonColourId, colour.withAlpha (0.30f));
            button->setColour (juce::TextButton::buttonOnColourId, colour);
            button->setColour (juce::TextButton::textColourOffId, colour.withAlpha (0.30f));
            button->setColour (juce::TextButton::textColourOnId, background);
        }
        if (auto* toggle = dynamic_cast<juce::ToggleButton*> (&component))
        {
            toggle->setColour (juce::ToggleButton::textColourId, colour);
            toggle->setColour (juce::ToggleButton::tickColourId, colour);
            toggle->setColour (juce::ToggleButton::tickDisabledColourId,
                               colour.withAlpha (0.30f));
        }
        component.repaint();
    }

    static void restoreAttributes (Target& target, const juce::ValueTree& item)
    {
        if (item.hasProperty ("accent"))
            applyEditableColour (*target.component,
                                 juce::Colour::fromString (item["accent"].toString()));

        auto* slider = dynamic_cast<juce::Slider*> (target.component);
        if (slider == nullptr || slider->getSliderStyle() != juce::Slider::IncDecButtons)
            return;
        auto& properties = slider->getProperties();
        const auto restore = [&] (const char* itemProperty,
                                  const juce::Identifier& componentProperty)
        {
            if (item.hasProperty (itemProperty))
                properties.set (componentProperty, item.getProperty (itemProperty));
        };
        restore ("valueWidth", incDecValueWidthProperty);
        restore ("buttonWidth", incDecButtonWidthProperty);
        restore ("cornerRadius", incDecCornerRadiusProperty);
        restore ("fillAlpha", incDecFillAlphaProperty);
        restore ("outlineAlpha", incDecOutlineAlphaProperty);
        restore ("glyphScale", incDecGlyphScaleProperty);
        restore ("lineWidth", incDecLineWidthProperty);
        if (item.hasProperty ("valueWidth"))
            slider->setTextBoxStyle (slider->getTextBoxPosition(), ! slider->isTextBoxEditable(),
                                     static_cast<int> (item["valueWidth"]),
                                     slider->getTextBoxHeight());
        slider->repaint();
    }

    static void cycleColour (Target& target)
    {
        static const std::array<juce::Colour, 10> colours {
            green, cyan, orange, juce::Colours::yellow,
            juce::Colours::white, juce::Colour { 0xffa66cff },
            juce::Colour { 0xffc5cecf }, juce::Colour { 0xff7f9193 },
            juce::Colour { 0xff4d6062 }, juce::Colour { 0xff29383a }
        };
        const auto colour = colours[static_cast<size_t> (++target.colourIndex % static_cast<int> (colours.size()))];
        applyEditableColour (*target.component, colour);
    }
    std::vector<Target> targets;
    LayoutAttributePanel attributePanel;
    juce::Component* active = nullptr;
    Target* activeTarget = nullptr;
    Target* selectedTarget = nullptr;
    juce::Point<int> dragOffset;
    bool resizing = false;
    bool panelPositioned = false;
};
}
