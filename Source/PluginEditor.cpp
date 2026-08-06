#include "PluginEditor.h"
#include "Engine/RhythmUtilities.h"
#include <NdlrDefaultLayoutData.h>

namespace
{
void exportLayoutForNextBuild (const juce::ValueTree& state)
{
    const auto layout = state.getChildWithName ("UILayout");
    if (! layout.isValid())
        return;

    auto directory = juce::File::getSpecialLocation (
        juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    directory = directory.getChildFile ("Application Support");
   #endif
    directory = directory.getChildFile ("NDLR");
    if (directory.createDirectory().failed())
        return;

    if (const auto xml = layout.createXml())
        directory.getChildFile ("current-layout.xml").replaceWithText (xml->toString());
}

void colourSelectableButton (juce::TextButton& button, juce::Colour colour)
{
    button.setColour (juce::TextButton::buttonColourId, colour.withAlpha (0.30f));
    button.setColour (juce::TextButton::buttonOnColourId, colour);
    button.setColour (juce::TextButton::textColourOffId, colour.withAlpha (0.30f));
    button.setColour (juce::TextButton::textColourOnId, ndlr::ui::background);
}

void colourActionButton (juce::TextButton& button, juce::Colour colour)
{
    // LOAD/SAVE sont des actions, pas des choix radio. Leur état ON permanent
    // leur donne la même lisibilité qu'un mode sélectionné ; JUCE atténue
    // ensuite automatiquement le composant lorsqu'il est désactivé.
    button.setClickingTogglesState (false);
    button.setToggleState (true, juce::dontSendNotification);
    button.setColour (juce::TextButton::buttonColourId, colour);
    button.setColour (juce::TextButton::buttonOnColourId, colour);
    button.setColour (juce::TextButton::textColourOffId, ndlr::ui::background);
    button.setColour (juce::TextButton::textColourOnId, ndlr::ui::background);
}

void colourEditable (juce::Component& component, juce::Colour colour)
{
    if (auto* menu = dynamic_cast<juce::ComboBox*> (&component))
    {
        menu->setColour (juce::ComboBox::textColourId, colour);
        menu->setColour (juce::ComboBox::arrowColourId, colour);
        menu->setColour (juce::ComboBox::outlineColourId, colour.withAlpha (0.55f));
    }
    if (auto* slider = dynamic_cast<juce::Slider*> (&component))
    {
        slider->setColour (juce::Slider::thumbColourId, colour);
        slider->setColour (juce::Slider::textBoxTextColourId, colour);
    }
    if (auto* button = dynamic_cast<juce::TextButton*> (&component))
        colourSelectableButton (*button, colour);
    if (auto* toggle = dynamic_cast<juce::ToggleButton*> (&component))
    {
        toggle->setColour (juce::ToggleButton::textColourId, colour);
        toggle->setColour (juce::ToggleButton::tickColourId, colour);
        toggle->setColour (juce::ToggleButton::tickDisabledColourId, colour.withAlpha (0.30f));
    }
}
}

NdlrAudioProcessorEditor::NdlrAudioProcessorEditor (NdlrAudioProcessor& processorToEdit)
    : AudioProcessorEditor (&processorToEdit), owner (processorToEdit)
{
    // Le LookAndFeel est partagé par tous les contrôles natifs de cet éditeur.
    setLookAndFeel (&ndlrLookAndFeel);

    // En-tête global : nom du produit, état du transport et transport interne.
    title.setText ("NDLR", juce::dontSendNotification);
    static const auto spaceAgeTypeface = juce::Typeface::createSystemTypefaceFor (
        ndlr_resources::SpaceAge_ttf, ndlr_resources::SpaceAge_ttfSize);
    title.setFont (spaceAgeTypeface != nullptr
                       ? juce::FontOptions { spaceAgeTypeface }.withHeight (40.0f)
                       : juce::FontOptions { 40.0f, juce::Font::bold });
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, ndlr::ui::green);
    addAndMakeVisible (title);

    status.setJustificationType (juce::Justification::centred);
    status.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (status);

    transportPlay.setClickingTogglesState (true);
    addAndMakeVisible (transportPlay);
    transportTempo.setSliderStyle (juce::Slider::LinearHorizontal);
    transportTempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 22);
    transportTempo.setTextValueSuffix (" BPM");
    addAndMakeVisible (transportTempo);
    panicButton.setColour (juce::TextButton::buttonColourId, juce::Colour { 0xff8f1818 });
    panicButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    panicButton.onClick = [this] { owner.requestPanic(); };
    addAndMakeVisible (panicButton);
    layoutDebugButton.setClickingTogglesState (true);
    layoutDebugButton.onClick = [this]
    {
        layoutDebugOverlay.setVisible (layoutDebugButton.getToggleState());
        if (layoutDebugOverlay.isVisible()) layoutDebugOverlay.toFront (false);
        layoutDebugButton.toFront (false);
    };
    addAndMakeVisible (layoutDebugButton);

    for (auto* section : { &harmony, &modMatrix, &rhythmEditor })
        addAndMakeVisible (*section);

    generatorTabs.setTabBarDepth (38);
    generatorTabs.setOutline (1);
    generatorTabs.setIndent (2);
    generatorTabs.setColour (juce::TabbedComponent::backgroundColourId, ndlr::ui::background);
    generatorTabs.setColour (juce::TabbedComponent::outlineColourId, ndlr::ui::panelEdge);
    const std::array<juce::String, 4> generatorNames {
        "MOTIF 1", "MOTIF 2", "DRONE", "PAD"
    };
    const std::array<juce::Colour, 4> generatorAccents {
        ndlr::ui::cyan, ndlr::ui::orange, ndlr::ui::green, ndlr::ui::green
    };
    for (auto index = 0; index < 4; ++index)
    {
        generatorTabs.addTab (generatorNames[static_cast<size_t> (index)], ndlr::ui::panel,
                              &generatorTabPages[static_cast<size_t> (index)], false);
        if (auto* tab = generatorTabs.getTabbedButtonBar().getTabButton (index))
        {
            const auto accent = generatorAccents[static_cast<size_t> (index)];
            tab->setColour (juce::TabbedButtonBar::tabTextColourId, accent);
            tab->setColour (juce::TabbedButtonBar::frontTextColourId, accent);
        }
    }
    generatorTabs.getTabbedButtonBar().addChangeListener (this);
    generatorTabs.setCurrentTabIndex (0, false);
    addAndMakeVisible (generatorTabs);
    generatorTabPages[0].addAndMakeVisible (motif1);
    generatorTabPages[1].addAndMakeVisible (motif2);
    generatorTabPages[2].addAndMakeVisible (drone);
    generatorTabPages[3].addAndMakeVisible (pad);
    generatorTabPages[0].addAndMakeVisible (patternEditorPanel);

    motif1.onPanelClicked = [this] { selectMotif (0); };
    motif2.onPanelClicked = [this] { selectMotif (1); };
    selectMotif (0);
    layoutDebugOverlay.addTarget (harmony, "Harmony");
    layoutDebugOverlay.addTarget (motif1, "Motif 1");
    layoutDebugOverlay.addTarget (motif2, "Motif 2");
    layoutDebugOverlay.addTarget (patternEditorPanel, "Pattern Editor");
    layoutDebugOverlay.addTarget (modMatrix, "Mod Matrix 3");
    layoutDebugOverlay.addTarget (generatorTabs, "Generator Tabs");
    for (auto index = 0; index < 4; ++index)
        if (auto* tab = generatorTabs.getTabbedButtonBar().getTabButton (index))
            layoutDebugOverlay.addTarget (
                *tab, "Generator Tabs / Tab " + generatorNames[static_cast<size_t> (index)]);
    layoutDebugOverlay.addTarget (pad, "Pad");
    layoutDebugOverlay.addTarget (padVoicingMiniView, "Pad / Voicing Map");
    layoutDebugOverlay.addTarget (padOn, "Pad / On");
    for (auto index = 0; index < 6; ++index)
        layoutDebugOverlay.addTarget (
            padSpreadButtons[static_cast<size_t> (index)],
            "Pad / Spread / " + juce::String (index + 1));
    layoutDebugOverlay.addTarget (padVelocity, "Pad / Velocity");
    layoutDebugOverlay.addTarget (padChannel, "Pad / MIDI Channel");
    layoutDebugOverlay.addTarget (padStrum, "Pad / Strum");
    layoutDebugOverlay.addTarget (padStrumDivision, "Pad / Strum Division");
    layoutDebugOverlay.addTarget (padGroup, "Pad / Group");
    layoutDebugOverlay.addTarget (padPolyChain, "Pad / Poly Chain");
    layoutDebugOverlay.addTarget (padInversionMode, "Pad / Inversion Mode", "Pad / Invert");
    layoutDebugOverlay.addTarget (padQuantize, "Pad / Quantize");
    layoutDebugOverlay.addTarget (padPositionLabel, "Pad / Position Label");
    layoutDebugOverlay.addTarget (padRangeLabel, "Pad / Range Label");
    layoutDebugOverlay.addTarget (padSpreadLabel, "Pad / Spread Label");
    layoutDebugOverlay.addTarget (padVelocityLabel, "Pad / Velocity Label");
    layoutDebugOverlay.addTarget (padChannelLabel, "Pad / MIDI Label");
    layoutDebugOverlay.addTarget (padPolyLabel, "Pad / Poly Label");
    layoutDebugOverlay.addTarget (padDivisionLabel, "Pad / Division Label");
    layoutDebugOverlay.addTarget (padQuantizeLabel, "Pad / Quantize Label");
    layoutDebugOverlay.addTarget (drone, "Drone");
    layoutDebugOverlay.addTarget (droneOn, "Drone / On");
    layoutDebugOverlay.addTarget (dronePosition, "Drone / Position");
    layoutDebugOverlay.addTarget (droneType, "Drone / Type");
    layoutDebugOverlay.addTarget (droneTrigger, "Drone / Trigger");
    layoutDebugOverlay.addTarget (droneVelocity, "Drone / Velocity");
    layoutDebugOverlay.addTarget (droneChannel, "Drone / MIDI Channel");
    layoutDebugOverlay.addTarget (dronePositionLabel, "Drone / Position Label");
    layoutDebugOverlay.addTarget (droneTypeLabel, "Drone / Type Label");
    layoutDebugOverlay.addTarget (droneTriggerLabel, "Drone / Trigger Label");
    layoutDebugOverlay.addTarget (droneVelocityLabel, "Drone / Velocity Label");
    layoutDebugOverlay.addTarget (droneChannelLabel, "Drone / MIDI Label");
    layoutDebugOverlay.addTarget (rhythmEditor, "Rhythm Editor");
    layoutDebugOverlay.addTarget (harmonyWheel, "Harmony / Wheel");
    layoutDebugOverlay.addTarget (harmonyScale, "Harmony / Scales", "Harmony / Mode");
    layoutDebugOverlay.addTarget (harmonyType, "Harmony / Colors", "Harmony / Type");
    layoutDebugOverlay.addTarget (harmonyKeyMidiChannel, "Header / Key MIDI Channel",
                                  "Harmony / Key MIDI Channel");
    layoutDebugOverlay.addTarget (harmonyDegreeMidiChannel, "Header / Degree MIDI Channel",
                                  "Harmony / Degree MIDI Channel");
    layoutDebugOverlay.addTarget (motif1On, "Motif 1 / On");
    layoutDebugOverlay.addTarget (motif1PatternTypeButtons[0], "Motif 1 / Source / Chord",
                                  "Motif 1 / Source");
    layoutDebugOverlay.addTarget (motif1PatternTypeButtons[1], "Motif 1 / Source / Scale");
    layoutDebugOverlay.addTarget (motif1PatternTypeButtons[2], "Motif 1 / Source / Chromatic");
    layoutDebugOverlay.addTarget (motif1RegisterButtons[0], "Motif 1 / Register / -2",
                                  "Motif 1 / Position");
    layoutDebugOverlay.addTarget (motif1RegisterButtons[1], "Motif 1 / Register / -1");
    layoutDebugOverlay.addTarget (motif1RegisterButtons[2], "Motif 1 / Register / 0");
    layoutDebugOverlay.addTarget (motif1RegisterButtons[3], "Motif 1 / Register / 1");
    layoutDebugOverlay.addTarget (motif1RegisterButtons[4], "Motif 1 / Register / 2");
    layoutDebugOverlay.addTarget (motif1Humanize, "Motif 1 / Humanize");
    layoutDebugOverlay.addTarget (motif1Accent, "Motif 1 / Accent");
    layoutDebugOverlay.addTarget (motif1Gate, "Motif 1 / Gate");
    layoutDebugOverlay.addTarget (motif1Length, "Pattern / Length", "Pattern / M1 Length");
    layoutDebugOverlay.addTarget (motif1Variation, "Motif 1 / Variation");
    layoutDebugOverlay.addTarget (motif1Velocity, "Motif 1 / Velocity");
    layoutDebugOverlay.addTarget (motif1Channel, "Motif 1 / MIDI Channel");
    layoutDebugOverlay.addTarget (patternTypeLabel, "Motif 1 / Source Label");
    layoutDebugOverlay.addTarget (positionLabel, "Motif 1 / Register Label",
                                  "Motif 1 / Position Label");
    layoutDebugOverlay.addTarget (humanizeLabel, "Motif 1 / Humanize Label");
    layoutDebugOverlay.addTarget (accentLabel, "Motif 1 / Accent Label");
    layoutDebugOverlay.addTarget (gateLabel, "Motif 1 / Gate Label");
    layoutDebugOverlay.addTarget (lengthLabel, "Pattern / Length Label",
                                  "Pattern / M1 Length Label");
    layoutDebugOverlay.addTarget (variationLabel, "Motif 1 / Variation Label");
    layoutDebugOverlay.addTarget (velocityLabel, "Motif 1 / Velocity Label");
    layoutDebugOverlay.addTarget (channelLabel, "Motif 1 / MIDI Label");
    layoutDebugOverlay.addTarget (motif2On, "Motif 2 / On");
    layoutDebugOverlay.addTarget (motif2PatternTypeButtons[0], "Motif 2 / Source / Chord",
                                  "Motif 2 / Source");
    layoutDebugOverlay.addTarget (motif2PatternTypeButtons[1], "Motif 2 / Source / Scale");
    layoutDebugOverlay.addTarget (motif2PatternTypeButtons[2], "Motif 2 / Source / Chromatic");
    layoutDebugOverlay.addTarget (motif2RegisterButtons[0], "Motif 2 / Register / -2",
                                  "Motif 2 / Position");
    layoutDebugOverlay.addTarget (motif2RegisterButtons[1], "Motif 2 / Register / -1");
    layoutDebugOverlay.addTarget (motif2RegisterButtons[2], "Motif 2 / Register / 0");
    layoutDebugOverlay.addTarget (motif2RegisterButtons[3], "Motif 2 / Register / 1");
    layoutDebugOverlay.addTarget (motif2RegisterButtons[4], "Motif 2 / Register / 2");
    layoutDebugOverlay.addTarget (motif2Humanize, "Motif 2 / Humanize");
    layoutDebugOverlay.addTarget (motif2Accent, "Motif 2 / Accent");
    layoutDebugOverlay.addTarget (motif2Gate, "Motif 2 / Gate");
    layoutDebugOverlay.addTarget (motif2Variation, "Motif 2 / Variation");
    layoutDebugOverlay.addTarget (motif2Velocity, "Motif 2 / Velocity");
    layoutDebugOverlay.addTarget (motif2Channel, "Motif 2 / MIDI Channel");
    layoutDebugOverlay.addTarget (motif2PatternTypeLabel, "Motif 2 / Source Label");
    layoutDebugOverlay.addTarget (motif2PositionLabel, "Motif 2 / Register Label",
                                  "Motif 2 / Position Label");
    layoutDebugOverlay.addTarget (motif2HumanizeLabel, "Motif 2 / Humanize Label");
    layoutDebugOverlay.addTarget (motif2AccentLabel, "Motif 2 / Accent Label");
    layoutDebugOverlay.addTarget (motif2GateLabel, "Motif 2 / Gate Label");
    layoutDebugOverlay.addTarget (motif2VariationLabel, "Motif 2 / Variation Label");
    layoutDebugOverlay.addTarget (motif2VelocityLabel, "Motif 2 / Velocity Label");
    layoutDebugOverlay.addTarget (motif2ChannelLabel, "Motif 2 / MIDI Label");
    layoutDebugOverlay.addTarget (patternDisplay, "Pattern / Bars");
    layoutDebugOverlay.addTarget (patternSelector, "Pattern / Selector", "Pattern / User Slot");
    layoutDebugOverlay.addTarget (patternSave, "Pattern / Save");
    layoutDebugOverlay.addTarget (rhythmWheel, "Rhythm / Wheel");
    layoutDebugOverlay.addTarget (rhythmModeButtons[0], "Rhythm / Mode / Normal",
                                  "Rhythm / Mode");
    layoutDebugOverlay.addTarget (rhythmModeButtons[1], "Rhythm / Mode / Euclidean");
    layoutDebugOverlay.addTarget (motif1RhythmLength, "Rhythm / Length");
    layoutDebugOverlay.addTarget (motif1EuclideanPulses, "Rhythm / Beats");
    layoutDebugOverlay.addTarget (motif1Rotation, "Rhythm / Rotate");
    layoutDebugOverlay.addTarget (rhythmModeLabel, "Rhythm / Mode Label");
    layoutDebugOverlay.addTarget (rhythmLengthLabel, "Rhythm / Length Label");
    layoutDebugOverlay.addTarget (rhythmBeatsLabel, "Rhythm / Beats Label");
    layoutDebugOverlay.addTarget (rhythmRotateLabel, "Rhythm / Rotate Label");
    layoutDebugOverlay.addTarget (rhythmUserSlot, "Rhythm / User Slot");
    layoutDebugOverlay.addTarget (rhythmSave, "Rhythm / Save");
    layoutDebugOverlay.addTarget (rhythmLoad, "Rhythm / Load");
    layoutDebugOverlay.addTarget (harmonyScaleLabel, "Harmony / Scales Label",
                                  "Harmony / Mode Label");
    layoutDebugOverlay.addTarget (harmonyTypeLabel, "Harmony / Colors Label",
                                  "Harmony / Type Label");
    layoutDebugOverlay.addTarget (harmonyKeyMidiChannelLabel, "Header / Key MIDI Label",
                                  "Harmony / Key MIDI Label");
    layoutDebugOverlay.addTarget (harmonyDegreeMidiChannelLabel, "Header / Degree MIDI Label",
                                  "Harmony / Degree MIDI Label");
    addChildComponent (layoutDebugOverlay);
    rhythmEditor.addAndMakeVisible (rhythmWheel);
    patternEditorPanel.addAndMakeVisible (patternDisplay);
    patternEditorPanel.addAndMakeVisible (patternSelector);
    patternEditorPanel.addAndMakeVisible (patternSave);

    // Section Harmony. La roue pilote directement la tonalité et le degré ;
    // seuls les gammes et les couleurs d'accord restent sous forme de menus.
    harmonyScale.addItemList ({ "Major", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian", "Gypsy Min", "Harm. Minor", "Minor Penta", "Whole Tone", "Tonic 2nds", "Tonic 3rds", "Tonic 4ths", "Tonic 6ths", "Major Penta", "Blues", "Melodic Min", "Dorian b2", "Lydian Aug", "Lydian Dom", "Mixo b6", "Locrian #2", "Altered", "Phryg. Dom", "Byzantine", "Dim H-W", "Augmented" }, 1);
    harmonyType.addItemList ({ "Triad", "7th", "sus2", "sus4", "6th", "mu", "add9", "9th", "quartal", "power", "shell", "quintal", "11th", "13th", "cluster", "shell9", "open9", "So What" }, 1);
    for (auto channel = 1; channel <= 16; ++channel)
    {
        harmonyKeyMidiChannel.addItem (juce::String (channel), channel);
        harmonyDegreeMidiChannel.addItem (juce::String (channel), channel);
    }
    harmonyKeyMidiChannel.setTooltip ("MIDI notes on this channel select the harmony key");
    harmonyDegreeMidiChannel.setTooltip (
        "MIDI notes C, D, E, F, G, A and B on this channel select degrees I to VII");
    for (auto* menu : { &harmonyScale, &harmonyType, &harmonyKeyMidiChannel, &harmonyDegreeMidiChannel })
    {
        menu->setColour (juce::ComboBox::textColourId, ndlr::ui::green);
        menu->setColour (juce::ComboBox::arrowColourId, ndlr::ui::green);
    }

    auto configureHarmonyLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ndlr::ui::muted);
        label.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centredLeft);
        harmony.addAndMakeVisible (label);
    };
    configureHarmonyLabel (harmonyScaleLabel, "SCALES");
    configureHarmonyLabel (harmonyTypeLabel, "COLORS");
    harmonyScale.setName ("Harmony Scales");
    harmonyType.setName ("Harmony Colors");
    configureHarmonyLabel (harmonyKeyMidiChannelLabel, "KEY MIDI CH");
    configureHarmonyLabel (harmonyDegreeMidiChannelLabel, "DEG MIDI CH");
    for (auto* label : { &harmonyKeyMidiChannelLabel, &harmonyDegreeMidiChannelLabel })
    {
        label->setFont (juce::FontOptions { 10.5f, juce::Font::bold });
        label->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*label);
    }
    harmony.addAndMakeVisible (harmonyScale);
    harmony.addAndMakeVisible (harmonyType);
    addAndMakeVisible (harmonyKeyMidiChannel);
    addAndMakeVisible (harmonyDegreeMidiChannel);
    harmony.addAndMakeVisible (harmonyWheel);

    // Les sections encore non portées affichent temporairement un texte neutre.
    placeholderText.setText ("UI skeleton - engines will be connected progressively", juce::dontSendNotification);
    placeholderText.setColour (juce::Label::textColourId, ndlr::ui::muted);
    placeholderText.setJustificationType (juce::Justification::centred);
    modMatrix.addAndMakeVisible (placeholderText);
    placeholderText.setVisible (false);
    droneOn.setClickingTogglesState (true);
    dronePosition.addItemList ({ "C1-B2", "C2-B3", "C3-B4", "C4-B5" }, 1);
    droneType.addItemList ({ "Root", "Root+Oct", "Root+5", "Root+5+Oct" }, 1);
    droneTrigger.addItemList ({ "Key Sustain", "Key Beat 1", "Key 1+2", "Key 1+3", "Key 2+4", "Key Every Beat", "Key Every 3", "Key Every 5", "Chord Sustain", "Chord Beat 1", "Chord 1+2", "Chord 1+3", "Chord 2+4", "Chord Every Beat", "Chord Every 3", "Chord Every 5", "Chord Special 1", "Chord Special 2", "Chord Special 3" }, 1);
    for (auto channel = 1; channel <= 16; ++channel) droneChannel.addItem (juce::String (channel), channel);
    for (auto* menu : { &dronePosition, &droneType, &droneTrigger, &droneChannel })
    {
        menu->setColour (juce::ComboBox::textColourId, ndlr::ui::green);
        menu->setColour (juce::ComboBox::arrowColourId, ndlr::ui::green);
    }
    droneVelocity.setSliderStyle (juce::Slider::IncDecButtons);
    droneVelocity.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    for (auto* c : std::array<juce::Component*, 6> { &droneOn, &dronePosition, &droneType, &droneTrigger, &droneVelocity, &droneChannel }) drone.addAndMakeVisible (*c);
    auto configureDroneLabel = [this] (juce::Label& label, const juce::String& textToUse)
    {
        label.setText (textToUse, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ndlr::ui::muted);
        label.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centred);
        drone.addAndMakeVisible (label);
    };
    configureDroneLabel (dronePositionLabel, "POSITION");
    configureDroneLabel (droneTypeLabel, "TYPE");
    configureDroneLabel (droneTriggerLabel, "TRIGGER");
    configureDroneLabel (droneVelocityLabel, "VELOCITY");
    configureDroneLabel (droneChannelLabel, "MIDI CH");
    for (auto* toggle : { &padOn, &padStrum, &padGroup }) toggle->setClickingTogglesState (true);
    padInvert.setClickingTogglesState (true);
    for (auto* slider : { &padPosition, &padRange, &padSpread, &padVelocity, &padPolyChain })
    { slider->setSliderStyle (juce::Slider::IncDecButtons); slider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 38, 22); }
    for (auto* slider : { &padPosition, &padRange, &padSpread })
    {
        slider->setTextBoxStyle (juce::Slider::TextBoxAbove, false, 25, 16);
        slider->getProperties().set (ndlr::ui::incDecValueWidthProperty, 25);
        slider->getProperties().set (ndlr::ui::incDecButtonWidthProperty, 11);
        slider->getProperties().set (ndlr::ui::incDecGlyphScaleProperty, 0.18f);
        slider->getProperties().set (ndlr::ui::incDecLineWidthProperty, 1.2f);
    }
    padStrumDivision.addItemList ({ "1nd","1n","1nt","2nd","2n","2nt","4nd","4n","4nt","8nd","8n","8nt","16nd","16n","16nt","32nd","32n","32nt","64nd","64n","128n" }, 1);
    for (auto channel=1; channel<=16; ++channel) padChannel.addItem (juce::String (channel), channel);
    padQuantize.addItemList ({ "Off", "1/4", "1/8" }, 1);
    padInversionMode.addItemList ({ "ROOT", "1ST", "2ND", "AUTO" }, 1);
    padInversionMode.setTooltip (
        "ROOT: base voicing; 1ST/2ND: fixed inversions; AUTO: minimum tonal movement");
    for (auto* menu : { &padStrumDivision, &padChannel, &padQuantize, &padInversionMode })
    {
        menu->setColour (juce::ComboBox::textColourId, ndlr::ui::green);
        menu->setColour (juce::ComboBox::arrowColourId, ndlr::ui::green);
    }
    for (auto* c : std::array<juce::Component*, 9> { &padOn,&padStrum,&padGroup,&padInversionMode,&padVelocity,&padPolyChain,&padStrumDivision,&padChannel,&padQuantize }) pad.addAndMakeVisible (*c);
    for (auto index = 0; index < 6; ++index)
    {
        auto& button = padSpreadButtons[static_cast<size_t> (index)];
        button.onClick = [this, index]
        {
            padSpread.setValue (static_cast<double> (index + 1), juce::sendNotificationSync);
        };
        pad.addAndMakeVisible (button);
    }
    padVoicingMiniView.setEmbeddedMode (true);
    padVoicingMiniView.setInterceptsMouseClicks (true, false);
    padVoicingMiniView.onPositionChange = [this] (int value)
    {
        padPosition.setValue (value, juce::sendNotificationSync);
    };
    padVoicingMiniView.onRangeChange = [this] (int value)
    {
        padRange.setValue (value, juce::sendNotificationSync);
    };
    padVoicingMiniView.onPositionGesture = [this] (bool isStarting)
    {
        if (auto* parameter = owner.getParameters().getParameter ("pad.position"))
            isStarting ? parameter->beginChangeGesture() : parameter->endChangeGesture();
    };
    padVoicingMiniView.onRangeGesture = [this] (bool isStarting)
    {
        if (auto* parameter = owner.getParameters().getParameter ("pad.range"))
            isStarting ? parameter->beginChangeGesture() : parameter->endChangeGesture();
    };
    pad.addAndMakeVisible (padVoicingMiniView);
    padVoicingMiniView.toBack();
    auto padLabel = [this] (juce::Label& l, const juce::String& t) { l.setText(t,juce::dontSendNotification); l.setColour(juce::Label::textColourId,ndlr::ui::muted); l.setFont(juce::FontOptions{13.0f,juce::Font::bold}); l.setJustificationType(juce::Justification::centred); pad.addAndMakeVisible(l); };
    padLabel(padPositionLabel,"POSITION"); padLabel(padRangeLabel,"RANGE"); padLabel(padSpreadLabel,"SPREAD"); padLabel(padVelocityLabel,"VELOCITY"); padLabel(padChannelLabel,"MIDI CH"); padLabel(padPolyLabel,"POLY");
    padLabel (padDivisionLabel, "DIV");
    padLabel (padQuantizeLabel, "QUANTIZE");
    juce::StringArray modShapes { "Sine","Triangle","Ramp","Saw","Square","Pulse","Random",
        "Up-Down","Third Jumps","Root Pivot","Rising Zigzag","High Drone","Cascade",
        "Odds-Evens","Alberti","Chord Staircase","Bass + Chord","Double Echo","Mirror",
        "Clave","Rhythmic","Pentatonic Feel","Alternating Pairs","Spiral","Linked Steps",
        "Fourth Jumps","Rotating" };
    for (auto slot = 21; slot <= 40; ++slot) modShapes.add ("User " + juce::String (slot));
    const juce::StringArray modDivs { "16 bars","8 bars","4 bars","2 bars","1 bar","1/2 bar","1/4 bar","1/8 bar","1/16 bar" };
    const juce::StringArray modSources { "Off","LFO 1","LFO 2","LFO 3","MIDI Vel","Pitch Bend","MIDI CC","Aftertouch" };
    const juce::StringArray modDests {
        "None",
        "Key", "Scales", "Degree", "Colors",
        "Pad On", "Pad Position", "Pad Range", "Pad Spread", "Pad Strum", "Pad Velocity",
        "Drone On", "Drone Position", "Drone Type", "Drone Trigger", "Drone Velocity",
        "M1 On", "M1 Register", "M1 Pattern", "M1 Active Pattern", "M1 Length", "M1 Variation",
        "M1 Division", "M1 Velocity", "M1 Gate", "M1 Accent", "M1 Rhythm",
        "M2 On", "M2 Register", "M2 Pattern", "M2 Active Pattern", "M2 Length", "M2 Variation",
        "M2 Division", "M2 Velocity", "M2 Gate", "M2 Accent", "M2 Rhythm",
        "Perlin Zoom", "Perlin Spacing", "Perlin Roughness", "Perlin Persistence",
        "Perlin Seed", "Perlin Resolution", "Perlin Brightness",
        "MIDI CC"
    };
    const juce::StringArray slotHeaders { "SLOT","SOURCE","DESTINATION","AMOUNT","RANGE %","" };
    for(auto i=0;i<6;++i)
    {
        auto& slotLabel = modSlotLabels[static_cast<size_t>(i)];
        slotLabel.setColour (juce::Label::textColourId, ndlr::ui::muted);
        slotLabel.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        slotLabel.setJustificationType (juce::Justification::centred);
        slotLabel.setText(slotHeaders[i],juce::dontSendNotification);
        modMatrix.addAndMakeVisible(slotLabel);
        layoutDebugOverlay.addTarget (slotLabel, "Matrix 2 / Header " + slotHeaders[i]);
    }
    modSlotLabels[5].setVisible (false);
    modScopeLabel.setText ("SCOPE", juce::dontSendNotification);
    modScopeLabel.setColour (juce::Label::textColourId, ndlr::ui::muted);
    modScopeLabel.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
    modScopeLabel.setJustificationType (juce::Justification::centred);
    modMatrix.addAndMakeVisible (modScopeLabel);
    layoutDebugOverlay.addTarget (modScopeLabel, "Matrix 2 / Header Scope");
    for (auto* label : { &modCcNumberLabel, &modCcChannelLabel })
    {
        label->setColour (juce::Label::textColourId, ndlr::ui::muted);
        label->setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        label->setJustificationType (juce::Justification::centred);
        modMatrix.addChildComponent (*label);
    }
    modCcNumberLabel.setText ("MIDI CC", juce::dontSendNotification);
    modCcChannelLabel.setText ("MIDI CH", juce::dontSendNotification);
    layoutDebugOverlay.addTarget (modCcNumberLabel, "Matrix 3 / Header MIDI CC");
    layoutDebugOverlay.addTarget (modCcChannelLabel, "Matrix 3 / Header MIDI Channel");

    lfoTabs.setTabBarDepth (38);
    lfoTabs.setOutline (1);
    lfoTabs.setIndent (2);
    lfoTabs.setColour (juce::TabbedComponent::backgroundColourId, ndlr::ui::background);
    lfoTabs.setColour (juce::TabbedComponent::outlineColourId, ndlr::ui::panelEdge);
    const std::array<juce::Colour, 4> lfoTabAccents {
        ndlr::ui::green, ndlr::ui::green, ndlr::ui::green, ndlr::ui::green
    };
    lfoTabs.addTab ("LFO 1", ndlr::ui::panel, &lfoTabPages[0], false);
    lfoTabs.addTab ("LFO 2", ndlr::ui::panel, &lfoTabPages[1], false);
    lfoTabs.addTab ("LFO 3", ndlr::ui::panel, &lfoTabPages[2], false);
    lfoTabs.addTab ("Perlin Noise", ndlr::ui::panel, &perlinTabPage, false);
    for (auto tabIndex = 0; tabIndex < 4; ++tabIndex)
        if (auto* tab = lfoTabs.getTabbedButtonBar().getTabButton (tabIndex))
        {
            const auto accent = lfoTabAccents[static_cast<size_t> (tabIndex)];
            tab->setColour (juce::TabbedButtonBar::tabTextColourId, accent);
            tab->setColour (juce::TabbedButtonBar::frontTextColourId, accent);
        }
    lfoTabs.setCurrentTabIndex (0, false);
    addAndMakeVisible (lfoTabs);
    layoutDebugOverlay.addTarget (lfoTabs, "LFO Tabs");
    const std::array<juce::String, 4> lfoTabNames { "LFO 1", "LFO 2", "LFO 3", "Perlin Noise" };
    for (auto tabIndex = 0; tabIndex < 4; ++tabIndex)
        if (auto* tab = lfoTabs.getTabbedButtonBar().getTabButton (tabIndex))
            layoutDebugOverlay.addTarget (*tab,
                                          "LFO Tabs / Tab " + lfoTabNames[static_cast<size_t> (tabIndex)]);

    const std::array<juce::String, 10> lfoControlLabelText {
        "SHAPE", "RATE Hz / DIV", "MODE", "PROBABILITY %", "PULSE WIDTH %",
        "MOD SOURCE", "AMPLITUDE %", "MIDI CC", "MIDI CH", "MOD PHASE %"
    };
    for (auto lfoIndex = 0; lfoIndex < 3; ++lfoIndex)
        for (auto labelIndex = 0; labelIndex < 10; ++labelIndex)
        {
            auto& label = modLfoTabLabels[static_cast<size_t> (lfoIndex)][static_cast<size_t> (labelIndex)];
            label.setText (lfoControlLabelText[static_cast<size_t> (labelIndex)], juce::dontSendNotification);
            label.setColour (juce::Label::textColourId,
                             lfoTabAccents[static_cast<size_t> (lfoIndex)].withAlpha (0.78f));
            label.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
            label.setJustificationType (juce::Justification::centredLeft);
            lfoTabPages[static_cast<size_t> (lfoIndex)].addAndMakeVisible (label);
            const auto targetPrefix = "LFO Tabs / LFO " + juce::String (lfoIndex + 1) + " / ";
            layoutDebugOverlay.addTarget (
                label, targetPrefix
                           + lfoControlLabelText[static_cast<size_t> (labelIndex)] + " Label",
                labelIndex == 2 ? targetPrefix + "SYNC Label" : juce::String {});
        }

    for(auto i=0;i<3;++i)
    {
        const auto n = static_cast<size_t> (i);
        modLfoShape[static_cast<size_t>(i)].addItemList(modShapes,1); modLfoDivision[static_cast<size_t>(i)].addItemList(modDivs,1);
        modLfoMode[n].addItemList ({ "FREE", "RETRIGGER", "SYNC" }, 1);
        modLfoAmplitudeSource[n].addItemList (modSources, 1);
        modLfoAmplitudeSource[n].setItemEnabled (i + 2, false);
        modLfoAmplitudeSource[n].setTooltip (
            "Source controlling this LFO's amplitude; an LFO cannot modulate itself");
        for (auto cc = 0; cc <= 127; ++cc)
            modLfoAmplitudeCcNumber[n].addItem (juce::String (cc), cc + 1);
        for (auto channel = 1; channel <= 16; ++channel)
            modLfoAmplitudeCcChannel[n].addItem (juce::String (channel), channel);
        modLfoMode[n].setTooltip ("FREE: continuous; RETRIGGER: restart on Play; SYNC: follow host PPQ");
        modLfoSync[n].setClickingTogglesState (true);
        modLfoRetrigger[n].setClickingTogglesState (true);
        modLfoDivision[static_cast<size_t>(i)].setVisible (false);
        for(auto* s:{&modLfoRate[static_cast<size_t>(i)],&modLfoProbability[static_cast<size_t>(i)],&modLfoPulseWidth[static_cast<size_t>(i)],&modLfoLength[static_cast<size_t>(i)],&modLfoPhase[static_cast<size_t>(i)]}) { s->setSliderStyle(juce::Slider::IncDecButtons); s->setTextBoxStyle(juce::Slider::TextBoxLeft,false,44,20); }
        modLfoRate[n].setTextBoxStyle (juce::Slider::TextBoxLeft, false, 62, 20);
        modLfoRate[n].setTextValueSuffix (" Hz");
        modLfoPhase[n].setTextValueSuffix (" %");
        modLfoPhase[n].setTooltip (
            "Phase offset applied only to the selected LFO modulation source");
        modLfoAmplitude[n].setSliderStyle (juce::Slider::IncDecButtons);
        modLfoAmplitude[n].setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
        modLfoAmplitude[n].setTextValueSuffix (" %");
        modLfoPulseWidth[static_cast<size_t>(i)].setEnabled (false);
        modLfoPulseWidth[static_cast<size_t>(i)].setTooltip (
            "Pulse width is available when the LFO shape is Pulse");
        modLfoLength[n].setTooltip ("Number of active steps in the LFO pattern");
        for(auto* c:std::array<juce::Component*,12>{&modLfoShape[n],&modLfoDivision[n],&modLfoMode[n],&modLfoRate[n],&modLfoProbability[n],&modLfoPulseWidth[n],&modLfoLength[n],&modLfoPhase[n],&modLfoAmplitudeSource[n],&modLfoAmplitude[n],&modLfoAmplitudeCcNumber[n],&modLfoAmplitudeCcChannel[n]}) lfoTabPages[n].addAndMakeVisible(*c);
        modLfoDivision[static_cast<size_t>(i)].setVisible (false);
        modLfoLength[n].setVisible (false);
        modLfoAmplitudeCcNumber[n].setVisible (false);
        modLfoAmplitudeCcChannel[n].setVisible (false);
        modLfoTabLabels[n][7].setVisible (false);
        modLfoTabLabels[n][8].setVisible (false);
        const auto layoutPrefix = "LFO Tabs / LFO " + juce::String (i + 1) + " / ";
        layoutDebugOverlay.addTarget (modLfoShape[n], layoutPrefix + "Shape");
        layoutDebugOverlay.addTarget (modLfoRate[n], layoutPrefix + "Rate");
        layoutDebugOverlay.addTarget (modLfoDivision[n], layoutPrefix + "Division");
        layoutDebugOverlay.addTarget (modLfoMode[n], layoutPrefix + "Mode", layoutPrefix + "Sync");
        layoutDebugOverlay.addTarget (modLfoProbability[n], layoutPrefix + "Probability");
        layoutDebugOverlay.addTarget (modLfoPulseWidth[n], layoutPrefix + "Pulse Width");
        layoutDebugOverlay.addTarget (modLfoLength[n], layoutPrefix + "Length");
        layoutDebugOverlay.addTarget (modLfoPhase[n], layoutPrefix + "Phase");
        layoutDebugOverlay.addTarget (modLfoAmplitudeSource[n], layoutPrefix + "Amplitude Source");
        layoutDebugOverlay.addTarget (modLfoAmplitude[n], layoutPrefix + "Amplitude");
        layoutDebugOverlay.addTarget (modLfoAmplitudeCcNumber[n], layoutPrefix + "Amplitude CC Number");
        layoutDebugOverlay.addTarget (modLfoAmplitudeCcChannel[n], layoutPrefix + "Amplitude CC Channel");
    }
    for(auto i=0;i<8;++i)
    {
        modSlotOn[static_cast<size_t>(i)].setButtonText(juce::String(i+1)); modSlotOn[static_cast<size_t>(i)].setClickingTogglesState(true);
        modSlotSource[static_cast<size_t>(i)].addItemList(modSources,1); modSlotDestination[static_cast<size_t>(i)].addItemList(modDests,1);
        for (auto cc = 0; cc <= 127; ++cc) modSlotCcNumber[static_cast<size_t> (i)].addItem (juce::String (cc), cc + 1);
        for (auto channel = 1; channel <= 16; ++channel) modSlotCcChannel[static_cast<size_t> (i)].addItem (juce::String (channel), channel);
        modSlotCcNumber[static_cast<size_t> (i)].setTooltip ("MIDI CC number");
        modSlotCcChannel[static_cast<size_t> (i)].setTooltip ("MIDI channel");
        modSlotCcNumber[static_cast<size_t> (i)].setName ("Mod MIDI CC Number");
        modSlotCcChannel[static_cast<size_t> (i)].setName ("Mod MIDI CC Channel");
        modSlotAmount[static_cast<size_t>(i)].setSliderStyle(juce::Slider::IncDecButtons);
        modSlotAmount[static_cast<size_t>(i)].setTextBoxStyle(juce::Slider::TextBoxLeft,false,36,20);
        auto& range = modSlotRange[static_cast<size_t> (i)];
        range.setSliderStyle (juce::Slider::TwoValueHorizontal);
        range.setRange (0.0, 100.0, 1.0);
        range.setMinAndMaxValues (0.0, 100.0, juce::dontSendNotification);
        range.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        range.setTooltip ("Modulation range: drag the left and right handles");
        range.setColour (juce::Slider::backgroundColourId, ndlr::ui::panelEdge);
        range.setColour (juce::Slider::trackColourId, ndlr::ui::green.withAlpha (0.75f));
        range.onDragStart = [this, i]
        {
            const auto prefix = "mod.slot" + juce::String (i + 1) + ".";
            owner.getParameters().getParameter (prefix + "low")->beginChangeGesture();
            owner.getParameters().getParameter (prefix + "high")->beginChangeGesture();
        };
        range.onValueChange = [this, i]
        {
            const auto prefix = "mod.slot" + juce::String (i + 1) + ".";
            auto* lowParameter = owner.getParameters().getParameter (prefix + "low");
            auto* highParameter = owner.getParameters().getParameter (prefix + "high");
            const auto& slider = modSlotRange[static_cast<size_t> (i)];
            lowParameter->setValueNotifyingHost (lowParameter->convertTo0to1 (
                static_cast<float> (slider.getMinValue())));
            highParameter->setValueNotifyingHost (highParameter->convertTo0to1 (
                static_cast<float> (slider.getMaxValue())));
        };
        range.onDragEnd = [this, i]
        {
            const auto prefix = "mod.slot" + juce::String (i + 1) + ".";
            owner.getParameters().getParameter (prefix + "low")->endChangeGesture();
            owner.getParameters().getParameter (prefix + "high")->endChangeGesture();
        };
        for(auto* c:std::array<juce::Component*,5>{&modSlotOn[static_cast<size_t>(i)],&modSlotSource[static_cast<size_t>(i)],&modSlotDestination[static_cast<size_t>(i)],&modSlotAmount[static_cast<size_t>(i)],&range}) modMatrix.addAndMakeVisible(*c);
        modMatrix.addChildComponent (modSlotCcNumber[static_cast<size_t> (i)]);
        modMatrix.addChildComponent (modSlotCcChannel[static_cast<size_t> (i)]);
        modMatrix.addAndMakeVisible (modSlotScope[static_cast<size_t> (i)]);
        layoutDebugOverlay.addTarget(modSlotOn[static_cast<size_t>(i)],"Matrix 2 / Slot "+juce::String(i+1)+" On"); layoutDebugOverlay.addTarget(modSlotSource[static_cast<size_t>(i)],"Matrix 2 / Slot "+juce::String(i+1)+" Source"); layoutDebugOverlay.addTarget(modSlotDestination[static_cast<size_t>(i)],"Matrix 2 / Slot "+juce::String(i+1)+" Destination"); layoutDebugOverlay.addTarget(modSlotAmount[static_cast<size_t>(i)],"Matrix 2 / Slot "+juce::String(i+1)+" Amount"); layoutDebugOverlay.addTarget(range,"Matrix 3 / Slot "+juce::String(i+1)+" Range"); layoutDebugOverlay.addTarget(modSlotScope[static_cast<size_t>(i)],"Matrix 2 / Slot "+juce::String(i+1)+" Scope");
        layoutDebugOverlay.addTarget (modSlotCcNumber[static_cast<size_t> (i)], "Matrix 2 / Slot " + juce::String (i + 1) + " CC Number");
        layoutDebugOverlay.addTarget (modSlotCcChannel[static_cast<size_t> (i)], "Matrix 2 / Slot " + juce::String (i + 1) + " CC Channel");
    }

    const std::array<juce::String, 7> perlinLabelText { "ZOOM", "SPC", "RUFF", "PER", "SEED", "RES", "LUM" };
    const std::array<std::pair<double, double>, 7> perlinRanges {{
        { 1.0, 100.0 }, { 0.0, 100.0 }, { 1.0, 100.0 }, { 1.0, 100.0 },
        { 0.0, 999.0 }, { 10.0, 80.0 }, { 0.0, 100.0 }
    }};
    for (auto* toggle : { &perlinOn, &perlinM1, &perlinM2 })
    {
        toggle->setClickingTogglesState (true);
        perlinTabPage.addAndMakeVisible (*toggle);
    }
    perlinTabPage.addAndMakeVisible (perlinDisplay);
    perlinTabPage.addAndMakeVisible (perlinProfiles);
    for (auto i = 0; i < 7; ++i)
    {
        auto& slider = perlinControls[static_cast<size_t> (i)];
        auto& label = perlinLabels[static_cast<size_t> (i)];
        slider.setSliderStyle (juce::Slider::IncDecButtons);
        slider.setRange (perlinRanges[static_cast<size_t> (i)].first,
                         perlinRanges[static_cast<size_t> (i)].second, 1.0);
        slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 34, 18);
        label.setText (perlinLabelText[static_cast<size_t> (i)], juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ndlr::ui::muted);
        label.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centred);
        perlinTabPage.addAndMakeVisible (slider);
        perlinTabPage.addAndMakeVisible (label);
        layoutDebugOverlay.addTarget (slider, "LFO Tabs / Perlin / " + perlinLabelText[static_cast<size_t> (i)]);
        layoutDebugOverlay.addTarget (label, "LFO Tabs / Perlin / "
                                               + perlinLabelText[static_cast<size_t> (i)] + " Label");
    }
    layoutDebugOverlay.addTarget (perlinDisplay, "LFO Tabs / Perlin / Display");
    layoutDebugOverlay.addTarget (perlinProfiles, "LFO Tabs / Perlin / Profiles");
    layoutDebugOverlay.addTarget (perlinOn, "LFO Tabs / Perlin / On");
    layoutDebugOverlay.addTarget (perlinM1, "LFO Tabs / Perlin / M1");
    layoutDebugOverlay.addTarget (perlinM2, "LFO Tabs / Perlin / M2");
    perlinDisplay.onPositionChanged = [this] (int newX, int newY)
    {
        auto setParameter = [this] (const juce::String& id, float value)
        {
            if (auto* parameter = owner.getParameters().getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        setParameter ("perlin.x", static_cast<float> (newX));
        setParameter ("perlin.y", static_cast<float> (newY));
    };

    motif1Variation.addItemList ({ "Forward", "Reverse", "Ping-pong", "Ping-pong hold", "Odds / evens", "Random" }, 1);
    motif1Length.setSliderStyle (juce::Slider::IncDecButtons);
    motif1Length.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    motif1Velocity.setSliderStyle (juce::Slider::IncDecButtons);
    motif1Velocity.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    for (auto channel = 1; channel <= 16; ++channel)
        motif1Channel.addItem (juce::String (channel), channel);
    patternSelector.addItemList ({ "Up-Down", "Third Jumps", "Root Pivot", "Rising Zigzag",
                                   "High Drone", "Cascade", "Odds-Evens", "Alberti", "Chord Staircase",
                                   "Bass + Chord", "Double Echo", "Mirror", "Clave", "Rhythmic",
                                   "Pentatonic Feel", "Alternating Pairs", "Spiral", "Linked Steps",
                                   "Fourth Jumps", "Rotating", "User 21", "User 22", "User 23", "User 24",
                                   "User 25", "User 26", "User 27", "User 28", "User 29", "User 30",
                                   "User 31", "User 32", "User 33", "User 34", "User 35", "User 36",
                                   "User 37", "User 38", "User 39", "User 40" }, 1);
    patternSave.setEnabled (false);
    for (auto* menu : { &motif1Variation, &motif1Channel })
    {
        menu->setColour (juce::ComboBox::textColourId, ndlr::ui::cyan);
        menu->setColour (juce::ComboBox::arrowColourId, ndlr::ui::cyan);
    }
    motif1Humanize.setSliderStyle (juce::Slider::IncDecButtons);
    motif1Humanize.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 22);
    const auto configureHumanizeDisplay = [] (juce::Slider& slider)
    {
        slider.textFromValueFunction = [] (double value)
        {
            return juce::String (juce::roundToInt (value) * 10) + " %";
        };
        slider.valueFromTextFunction = [] (const juce::String& textToParse)
        {
            return juce::jlimit (0.0, 10.0, textToParse.getDoubleValue() / 10.0);
        };
    };
    configureHumanizeDisplay (motif1Humanize);
    const juce::StringArray accentNames {
        "Rhythm", "Humanized", "Motif fixed",
        "H-L (2)", "H-L-L-L (4)", "H-L-H-L-L-L (6)",
        "H-L-L (3)", "H-H-L-L (4)", "H-L-L-H-L-L (6)",
        "H-L-H-L-H-L-L-L (8)"
    };
    motif1Accent.addItemList (accentNames, 1);
    motif1Accent.setTooltip ("Accent velocity: H = motif velocity, L = 55% of motif velocity");
    motif1Accent.setColour (juce::ComboBox::textColourId, ndlr::ui::cyan);
    motif1Accent.setColour (juce::ComboBox::arrowColourId, ndlr::ui::cyan);
    motif1Gate.setSliderStyle (juce::Slider::IncDecButtons);
    motif1Gate.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    motif1RhythmLength.setSliderStyle (juce::Slider::IncDecButtons);
    motif1RhythmLength.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    motif1EuclideanPulses.setSliderStyle (juce::Slider::IncDecButtons);
    motif1EuclideanPulses.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    motif1Rotation.setSliderStyle (juce::Slider::IncDecButtons);
    motif1Rotation.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    rhythmEditor.addAndMakeVisible (motif1RhythmLength);
    const std::array<juce::String, 2> rhythmModeNames { "NORMAL", "EUCLIDEAN" };
    for (auto index = 0; index < 2; ++index)
    {
        auto& button = rhythmModeButtons[static_cast<size_t> (index)];
        button.setButtonText (rhythmModeNames[static_cast<size_t> (index)]);
        button.setName ("Rhythm Mode " + rhythmModeNames[static_cast<size_t> (index)]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (1301, juce::dontSendNotification);
        colourSelectableButton (button, ndlr::ui::cyan);
        rhythmEditor.addAndMakeVisible (button);
    }
    rhythmModeButtons[0].setConnectedEdges (juce::Button::ConnectedOnRight);
    rhythmModeButtons[1].setConnectedEdges (juce::Button::ConnectedOnLeft);
    rhythmEditor.addAndMakeVisible (motif1EuclideanPulses);
    rhythmEditor.addAndMakeVisible (motif1Rotation);
    for (auto slot = 1; slot <= 40; ++slot)
        rhythmUserSlot.addItem ("[----] User " + juce::String (slot), slot);
    rhythmUserSlot.setSelectedId (1, juce::dontSendNotification);
    rhythmEditor.addAndMakeVisible (rhythmUserSlot);
    rhythmEditor.addAndMakeVisible (rhythmSave);
    rhythmEditor.addAndMakeVisible (rhythmLoad);
    rhythmSave.setEnabled (true); // Les 40 emplacements de rythme sont des emplacements utilisateur.
    for (auto* button : { &patternSave, &rhythmSave, &rhythmLoad })
        colourActionButton (*button, ndlr::ui::cyan);
    auto configureRhythmLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ndlr::ui::muted);
        label.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centredLeft);
        rhythmEditor.addAndMakeVisible (label);
    };
    configureRhythmLabel (rhythmModeLabel, "MODE");
    configureRhythmLabel (rhythmLengthLabel, "LENGTH");
    configureRhythmLabel (rhythmBeatsLabel, "BEATS");
    configureRhythmLabel (rhythmRotateLabel, "ROTATE");
    motif1On.setClickingTogglesState (true);

    motif1.addAndMakeVisible (motif1On);
    motif1.addAndMakeVisible (motif1Variation);
    motif1.addAndMakeVisible (motif1Velocity);
    motif1.addAndMakeVisible (motif1Channel);
    motif1.addAndMakeVisible (motif1Humanize);
    motif1.addAndMakeVisible (motif1Accent);
    motif1.addAndMakeVisible (motif1Gate);

    auto configureLabel = [this] (juce::Label& label, const juce::String& textToUse)
    {
        label.setText (textToUse, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ndlr::ui::muted);
        label.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centred);
        motif1.addAndMakeVisible (label);
    };
    configureLabel (variationLabel, "VARIATION");
    configureLabel (velocityLabel, "VELOCITY");
    configureLabel (channelLabel, "MIDI CH");
    configureLabel (humanizeLabel, "HUMANIZE");
    configureLabel (accentLabel, "ACCENT");
    configureLabel (gateLabel, "GATE");
    configureLabel (patternTypeLabel, "SOURCE");
    configureLabel (positionLabel, "REGISTER");

    // Motif 2 reprend les mêmes commandes que Motif 1, avec un état APVTS
    // et un canal MIDI totalement indépendants.
    motif2Variation.addItemList ({ "Forward", "Reverse", "Ping-pong", "Ping-pong hold", "Odds / evens", "Random" }, 1);
    for (auto channel = 1; channel <= 16; ++channel)
        motif2Channel.addItem (juce::String (channel), channel);
    for (auto* menu : { &motif2Variation, &motif2Channel })
    {
        menu->setColour (juce::ComboBox::textColourId, ndlr::ui::orange);
        menu->setColour (juce::ComboBox::arrowColourId, ndlr::ui::orange);
    }
    motif2On.setClickingTogglesState (true);
    motif2Accent.addItemList (accentNames, 1);
    motif2Accent.setTooltip ("Accent velocity: H = motif velocity, L = 55% of motif velocity");
    motif2Accent.setColour (juce::ComboBox::textColourId, ndlr::ui::orange);
    motif2Accent.setColour (juce::ComboBox::arrowColourId, ndlr::ui::orange);
    for (auto* slider : { &motif2Velocity, &motif2Humanize, &motif2Gate })
    {
        slider->setSliderStyle (juce::Slider::IncDecButtons);
        slider->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 22);
    }
    configureHumanizeDisplay (motif2Humanize);
    for (auto* component : std::array<juce::Component*, 7> {
             &motif2On, &motif2Variation,
             &motif2Velocity, &motif2Channel,
             &motif2Humanize, &motif2Accent, &motif2Gate })
        motif2.addAndMakeVisible (*component);
    auto configureMotif2Label = [this] (juce::Label& label, const juce::String& textToUse)
    {
        label.setText (textToUse, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, ndlr::ui::muted);
        label.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centred);
        motif2.addAndMakeVisible (label);
    };
    configureMotif2Label (motif2VariationLabel, "VARIATION");
    configureMotif2Label (motif2VelocityLabel, "VELOCITY");
    configureMotif2Label (motif2ChannelLabel, "MIDI CH");
    configureMotif2Label (motif2HumanizeLabel, "HUMANIZE");
    configureMotif2Label (motif2AccentLabel, "ACCENT");
    configureMotif2Label (motif2GateLabel, "GATE");
    configureMotif2Label (motif2PatternTypeLabel, "SOURCE");
    configureMotif2Label (motif2PositionLabel, "REGISTER");

    auto configureSourceButtons = [] (ndlr::ui::SectionPanel& panel,
                                      std::array<juce::TextButton, 3>& buttons,
                                      juce::Colour accent, int radioGroup,
                                      const juce::String& namePrefix)
    {
        static const std::array<juce::String, 3> names { "CHORD", "SCALE", "CHROMATIC" };
        for (auto index = 0; index < 3; ++index)
        {
            auto& button = buttons[static_cast<size_t> (index)];
            button.setButtonText (names[static_cast<size_t> (index)]);
            button.setName ("Motif Source " + namePrefix + " " + names[static_cast<size_t> (index)]);
            button.setClickingTogglesState (true);
            button.setRadioGroupId (radioGroup, juce::dontSendNotification);
            colourSelectableButton (button, accent);
            panel.addAndMakeVisible (button);
        }
        buttons[0].setConnectedEdges (juce::Button::ConnectedOnRight);
        buttons[1].setConnectedEdges (juce::Button::ConnectedOnLeft
                                      | juce::Button::ConnectedOnRight);
        buttons[2].setConnectedEdges (juce::Button::ConnectedOnLeft);
    };
    configureSourceButtons (motif1, motif1PatternTypeButtons, ndlr::ui::cyan, 1101, "M1");
    configureSourceButtons (motif2, motif2PatternTypeButtons, ndlr::ui::orange, 1102, "M2");

    auto configureRegisterButtons = [] (ndlr::ui::SectionPanel& panel,
                                        std::array<juce::TextButton, 5>& buttons,
                                        juce::Colour accent, int radioGroup,
                                        const juce::String& namePrefix)
    {
        static const std::array<juce::String, 5> names { "-2", "-1", "0", "1", "2" };
        for (auto index = 0; index < 5; ++index)
        {
            auto& button = buttons[static_cast<size_t> (index)];
            button.setButtonText (names[static_cast<size_t> (index)]);
            button.setName ("Motif Register " + namePrefix + " "
                            + names[static_cast<size_t> (index)]);
            button.setClickingTogglesState (true);
            button.setRadioGroupId (radioGroup, juce::dontSendNotification);
            colourSelectableButton (button, accent);
            panel.addAndMakeVisible (button);
        }
        buttons[0].setConnectedEdges (juce::Button::ConnectedOnRight);
        for (auto index = 1; index < 4; ++index)
            buttons[static_cast<size_t> (index)].setConnectedEdges (
                juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        buttons[4].setConnectedEdges (juce::Button::ConnectedOnLeft);
    };
    configureRegisterButtons (motif1, motif1RegisterButtons, ndlr::ui::cyan, 1201, "M1");
    configureRegisterButtons (motif2, motif2RegisterButtons, ndlr::ui::orange, 1202, "M2");

    auto configurePatternLength = [this] (juce::Label& label, juce::Slider& slider,
                                          const juce::String& text, juce::Colour accent)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, accent);
        label.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
        label.setJustificationType (juce::Justification::centredRight);
        slider.setColour (juce::Slider::thumbColourId, accent);
        patternEditorPanel.addAndMakeVisible (label);
        patternEditorPanel.addAndMakeVisible (slider);
    };
    configurePatternLength (lengthLabel, motif1Length, "LENGTH", ndlr::ui::cyan);

    // Écrit une valeur réelle (non normalisée) dans un paramètre APVTS.
    // Les gestes begin/end permettent au DAW d'enregistrer correctement l'automation.
    auto& parameters = owner.getParameters();
    layoutDebugOverlay.onLayoutChanged = [this]
    {
        auto& state = owner.getParameters().state;
        layoutDebugOverlay.saveLayout (state);
        exportLayoutForNextBuild (state);
    };
    auto setParameter = [&parameters] (const juce::String& parameterID, float value)
    {
        if (auto* parameter = parameters.getParameter (parameterID))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    };
    padInversionMode.onChange = [this, setParameter]
    {
        setParameter ("pad.invert",
                      padInversionMode.getSelectedItemIndex() == 3 ? 1.0f : 0.0f);
    };
    for (auto i = 0; i < 3; ++i)
    {
        const auto n = static_cast<size_t> (i);
        modLfoMode[n].onChange = [this, i, setParameter]
        {
            const auto mode = juce::jlimit (0, 2,
                modLfoMode[static_cast<size_t> (i)].getSelectedItemIndex());
            const auto prefix = "mod.lfo" + juce::String (i + 1) + ".";
            setParameter (prefix + "sync", mode == 2 ? 1.0f : 0.0f);
            setParameter (prefix + "retrigger", mode == 1 ? 1.0f : 0.0f);
        };
    }
    patternSelector.onChange = [this, setParameter]
    {
        const auto selectedPattern = patternSelector.getSelectedItemIndex();
        patternSave.setEnabled (selectedPattern >= 20 && selectedPattern < 40);

        if (updatingPatternSelector)
            return;

        if (selectedPattern < 0)
            return;

        const juce::String motifPrefix = selectedMotif == 0 ? "motif1." : "motif2.";
        setParameter (motifPrefix + "pattern", static_cast<float> (selectedPattern));
        setParameter (selectedMotif == 0 ? "perlin.m1" : "perlin.m2", 0.0f);
        timerCallback();
    };
    rhythmUserSlot.onChange = [this]
    {
        const auto selectedSlot = rhythmUserSlot.getSelectedId();
        rhythmSave.setEnabled (selectedSlot >= 1 && selectedSlot <= 40);
    };
    harmonyWheel.onKeyChanged = [setParameter] (int key) { setParameter ("harmony.root", static_cast<float> (key)); };
    harmonyWheel.onDegreeChanged = [setParameter] (int degree) { setParameter ("harmony.degree", static_cast<float> (degree + 1)); };
    auto connectSourceButtons = [setParameter] (std::array<juce::TextButton, 3>& buttons,
                                                juce::String parameterID)
    {
        for (auto index = 0; index < 3; ++index)
            buttons[static_cast<size_t> (index)].onClick = [setParameter, parameterID, index]
            {
                setParameter (parameterID, static_cast<float> (index));
            };
    };
    connectSourceButtons (motif1PatternTypeButtons, "motif1.patternType");
    connectSourceButtons (motif2PatternTypeButtons, "motif2.patternType");
    auto connectRegisterButtons = [setParameter] (std::array<juce::TextButton, 5>& buttons,
                                                  juce::String parameterID)
    {
        for (auto index = 0; index < 5; ++index)
            buttons[static_cast<size_t> (index)].onClick = [setParameter, parameterID, index]
            {
                // Le parametre historique reste indexe de 0 a 4 ; les libelles
                // -2 a +2 expriment simplement le decalage de registre musical.
                setParameter (parameterID, static_cast<float> (index));
            };
    };
    connectRegisterButtons (motif1RegisterButtons, "motif1.position");
    connectRegisterButtons (motif2RegisterButtons, "motif2.position");
    auto setRhythmMode = [this, &parameters, setParameter] (int newMode)
    {
        const auto motif = static_cast<size_t> (selectedMotif);
        if (previousRhythmMode[motif] == 1 && newMode == 0)
        {
            const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
            const auto length = juce::jlimit (4, 32, juce::roundToInt (
                parameters.getRawParameterValue (prefix + "rhythmLength")->load()));
            const auto pulses = juce::jlimit (1, length, juce::roundToInt (
                parameters.getRawParameterValue (prefix + "euclideanPulses")->load()));
            const auto signedRotation = juce::jlimit (-(length - 1), length - 1, juce::roundToInt (
                parameters.getRawParameterValue (prefix + "rotation")->load()));
            const auto rotation = (((-signedRotation) % length) + length) % length;
            const auto euclidean = ndlr::rhythm::euclidean (length, pulses, rotation);

            // Le mode Normal applique encore la rotation à la lecture. On
            // écrit donc chaque pas dans sa position source afin que le motif
            // visible reste identique au moment du basculement.
            for (auto step = 0; step < length; ++step)
            {
                const auto sourceStep = (step + rotation) % length;
                setParameter (prefix + "rhythmState" + juce::String (sourceStep + 1),
                              static_cast<float> (euclidean[static_cast<size_t> (step)]));
            }
        }
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        setParameter (prefix + "rhythmMode", static_cast<float> (newMode));
        previousRhythmMode[motif] = newMode;
    };
    rhythmModeButtons[0].onClick = [setRhythmMode] { setRhythmMode (0); };
    rhythmModeButtons[1].onClick = [setRhythmMode] { setRhythmMode (1); };
    patternDisplay.onStepChanged = [this, &parameters, setParameter] (int step, int value)
    {
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        const auto selectedPattern = juce::roundToInt (parameters.getRawParameterValue (prefix + "pattern")->load());
        if (selectedPattern < 20)
        {
            const auto preset = juce::jlimit (0, 19, selectedPattern);
            const auto source = juce::roundToInt (parameters.getRawParameterValue (prefix + "customPatternSource")->load()) - 1;
            const auto editing = parameters.getRawParameterValue (prefix + "customPattern")->load() >= 0.5f && source == preset;
            if (! editing)
                for (auto i = 0; i < 16; ++i)
                    setParameter (prefix + "patternStep" + juce::String (i + 1),
                                  static_cast<float> (NdlrEngine::motifPatternValue (preset, i)));
            setParameter (prefix + "customPatternSource", static_cast<float> (preset + 1));
            setParameter (prefix + "customPattern", 1.0f);
            setParameter (prefix + "patternStep" + juce::String (step + 1), static_cast<float> (value));
            return;
        }
        const auto userSlot = juce::jlimit (0, 19, selectedPattern - 20);
        setParameter ("pattern.user" + juce::String (userSlot + 1)
                      + ".step" + juce::String (step + 1), static_cast<float> (value));
        setParameter ("pattern.user" + juce::String (userSlot + 1) + ".used", 1.0f);
    };
    const auto savePatternToUserSlot = [this, &parameters, setParameter] (int motifIndex,
                                                                          int destination)
    {
        const juce::String prefix = motifIndex == 0 ? "motif1." : "motif2.";
        destination = juce::jlimit (0, 19, destination);
        const auto selected = juce::roundToInt (parameters.getRawParameterValue (prefix + "pattern")->load());
        const auto source = juce::roundToInt (parameters.getRawParameterValue (prefix + "customPatternSource")->load()) - 1;
        const auto editBuffer = selected < 20
            && parameters.getRawParameterValue (prefix + "customPattern")->load() >= 0.5f && source == selected;
        for (auto step = 0; step < 16; ++step)
        {
            auto value = selected < 20 ? NdlrEngine::motifPatternValue (selected, step)
                                       : juce::roundToInt (parameters.getRawParameterValue (
                                           "pattern.user" + juce::String (selected - 19)
                                           + ".step" + juce::String (step + 1))->load());
            if (editBuffer) value = juce::roundToInt (parameters.getRawParameterValue (
                prefix + "patternStep" + juce::String (step + 1))->load());
            setParameter ("pattern.user" + juce::String (destination + 1)
                          + ".step" + juce::String (step + 1), static_cast<float> (value));
        }
        setParameter ("pattern.user" + juce::String (destination + 1) + ".used", 1.0f);
        setParameter (prefix + "customPattern", 0.0f);
        setParameter (prefix + "pattern", static_cast<float> (20 + destination));
        displayedUsedMotif = -1;
    };
    patternSave.onClick = [this, &parameters, savePatternToUserSlot]
    {
        const auto motifToSave = selectedMotif;
        const juce::String prefix = motifToSave == 0 ? "motif1." : "motif2.";
        const auto selected = juce::roundToInt (
            parameters.getRawParameterValue (prefix + "pattern")->load());
        if (selected >= 20 && selected < 40)
            savePatternToUserSlot (motifToSave, selected - 20);
    };
    auto displayedToBaseStep = [this, &parameters] (int step)
    {
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        const auto length = juce::jlimit (4, 32, juce::roundToInt (
            parameters.getRawParameterValue (prefix + "rhythmLength")->load()));
        const auto signedRotation = juce::jlimit (-(length - 1), length - 1, juce::roundToInt (
            parameters.getRawParameterValue (prefix + "rotation")->load()));
        // Convention UI : + = horaire, - = antihoraire.
        const auto rotation = (((-signedRotation) % length) + length) % length;
        return (step + rotation) % length;
    };
    rhythmWheel.onStateChanged = [this, setParameter, displayedToBaseStep] (int step, int state)
    {
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        setParameter (prefix + "rhythmState" + juce::String (displayedToBaseStep (step) + 1), static_cast<float> (state));
    };
    rhythmWheel.onVelocityChanged = [this, setParameter, displayedToBaseStep] (int step, int velocity)
    {
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        setParameter (prefix + "rhythmVelocity" + juce::String (displayedToBaseStep (step) + 1), static_cast<float> (velocity));
    };
    rhythmWheel.onRatchetChanged = [this, setParameter, displayedToBaseStep] (int step, int ratchet)
    {
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        setParameter (prefix + "rhythmRatchet" + juce::String (displayedToBaseStep (step) + 1), static_cast<float> (ratchet));
    };
    rhythmWheel.onDivisionStep = [this, &parameters, setParameter] (int delta)
    {
        const juce::String parameterID = selectedMotif == 0 ? "motif1.division" : "motif2.division";
        const auto current = juce::roundToInt (parameters.getRawParameterValue (parameterID)->load());
        setParameter (parameterID, static_cast<float> (juce::jlimit (0, 20, current + delta)));
    };
    rhythmWheel.onRatchetVelocityModeChanged = [this, setParameter] (int mode)
    {
        setParameter (selectedMotif == 0 ? "motif1.ratchetVelocityMode" : "motif2.ratchetVelocityMode",
                      static_cast<float> (mode));
    };
    rhythmSave.onClick = [this, &parameters, setParameter]
    {
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        const auto slotNumber = rhythmUserSlot.getSelectedId();
        if (slotNumber < 1 || slotNumber > 40)
            return;
        auto memories = parameters.state.getOrCreateChildWithName ("RhythmMemories", nullptr);
        auto memory = memories.getOrCreateChildWithName ("User" + juce::String (slotNumber), nullptr);
        memory.setProperty ("used", true, nullptr);
        for (const auto& name : { "rhythmLength", "rhythmMode", "euclideanPulses",
                                  "rotation", "division", "ratchetVelocityMode" })
            memory.setProperty (name, parameters.getRawParameterValue (prefix + name)->load(), nullptr);
        for (auto step = 0; step < 32; ++step)
        {
            const auto number = juce::String (step + 1);
            memory.setProperty ("state" + number, parameters.getRawParameterValue (
                prefix + "rhythmState" + number)->load(), nullptr);
            memory.setProperty ("velocity" + number, parameters.getRawParameterValue (
                prefix + "rhythmVelocity" + number)->load(), nullptr);
            memory.setProperty ("ratchet" + number, parameters.getRawParameterValue (
                prefix + "rhythmRatchet" + number)->load(), nullptr);
        }
        setParameter (prefix + "rhythmSlot", static_cast<float> (slotNumber));
        owner.refreshRhythmMemoryCache();
        displayedRhythmSlotsValid = false;
    };
    rhythmLoad.onClick = [this, &parameters, setParameter]
    {
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        const auto slotNumber = juce::jlimit (1, 40, rhythmUserSlot.getSelectedId());
        const auto memories = parameters.state.getChildWithName ("RhythmMemories");
        auto memory = memories.getChildWithName ("User" + juce::String (slotNumber));
        // Lecture des anciennes mémoires séparées 21–40. Si M1 et M2
        // utilisaient le même numéro, M1 est prioritaire lors de la migration.
        if ((! memory.isValid() || ! static_cast<bool> (memory.getProperty ("used", false)))
            && slotNumber >= 21)
        {
            memory = memories.getChildWithName ("M1User" + juce::String (slotNumber));
            if (! memory.isValid() || ! static_cast<bool> (memory.getProperty ("used", false)))
                memory = memories.getChildWithName ("M2User" + juce::String (slotNumber));
        }
        if (! memory.isValid() || ! static_cast<bool> (memory.getProperty ("used", false)))
        {
            // Charger un emplacement libre cree un point de depart neutre sans
            // le marquer comme utilise : SAVE reste l'action qui l'enregistre.
            setParameter (prefix + "rhythmSlot", static_cast<float> (slotNumber));
            setParameter (prefix + "rhythmMode", 0.0f);           // Normal
            setParameter (prefix + "division", 7.0f);             // 4n
            setParameter (prefix + "rhythmLength", 8.0f);
            setParameter (prefix + "euclideanPulses", 4.0f);
            setParameter (prefix + "rotation", 0.0f);
            setParameter (prefix + "ratchetVelocityMode", 0.0f);  // Equal
            for (auto step = 0; step < 32; ++step)
            {
                const auto number = juce::String (step + 1);
                setParameter (prefix + "rhythmState" + number, 1.0f);      // NOTE
                setParameter (prefix + "rhythmVelocity" + number, 100.0f);
                setParameter (prefix + "rhythmRatchet" + number, 1.0f);
            }
            previousRhythmMode[static_cast<size_t> (selectedMotif)] = 0;
            return;
        }

        setParameter (prefix + "rhythmSlot", static_cast<float> (slotNumber));
        for (const auto& name : { "rhythmLength", "rhythmMode", "euclideanPulses",
                                  "rotation", "division", "ratchetVelocityMode" })
            setParameter (prefix + name, static_cast<float> (memory.getProperty (name)));
        for (auto step = 0; step < 32; ++step)
        {
            const auto number = juce::String (step + 1);
            setParameter (prefix + "rhythmState" + number,
                          static_cast<float> (memory.getProperty ("state" + number, 1)));
            setParameter (prefix + "rhythmVelocity" + number,
                          static_cast<float> (memory.getProperty ("velocity" + number, 100)));
            setParameter (prefix + "rhythmRatchet" + number,
                          static_cast<float> (memory.getProperty ("ratchet" + number, 1)));
        }
    };

    for (auto* component : std::array<juce::Component*, 16> {
             &motif1On, &motif1Length, &motif1Variation,
             &motif1Velocity, &motif1Channel,
             &motif1PatternTypeButtons[0], &motif1PatternTypeButtons[1],
             &motif1PatternTypeButtons[2],
             &motif1RegisterButtons[0], &motif1RegisterButtons[1],
             &motif1RegisterButtons[2], &motif1RegisterButtons[3],
             &motif1RegisterButtons[4],
             &motif1Humanize, &motif1Accent, &motif1Gate })
        colourEditable (*component, ndlr::ui::cyan);
    for (auto* component : std::array<juce::Component*, 15> {
             &motif2On, &motif2Variation,
             &motif2Velocity, &motif2Channel,
             &motif2PatternTypeButtons[0], &motif2PatternTypeButtons[1],
             &motif2PatternTypeButtons[2],
             &motif2RegisterButtons[0], &motif2RegisterButtons[1],
             &motif2RegisterButtons[2], &motif2RegisterButtons[3],
             &motif2RegisterButtons[4],
             &motif2Humanize, &motif2Accent, &motif2Gate })
        colourEditable (*component, ndlr::ui::orange);
    for (auto* component : std::array<juce::Component*, 24> {
             &transportPlay, &transportTempo, &layoutDebugButton, &harmonyScale, &harmonyType,
             &harmonyKeyMidiChannel, &harmonyDegreeMidiChannel,
             &padOn, &padStrum, &padGroup, &padInversionMode, &padPosition, &padRange, &padSpread,
             &padVelocity, &padPolyChain, &padStrumDivision, &padChannel, &padQuantize,
             &droneOn, &dronePosition, &droneType, &droneTrigger, &droneVelocity })
        colourEditable (*component, ndlr::ui::green);
    colourEditable (droneChannel, ndlr::ui::green);
    for (auto i = 0; i < 3; ++i)
        for (auto* component : std::array<juce::Component*, 11> {
                 &modLfoShape[static_cast<size_t>(i)], &modLfoDivision[static_cast<size_t>(i)],
                 &modLfoMode[static_cast<size_t>(i)], &modLfoRate[static_cast<size_t>(i)],
                 &modLfoProbability[static_cast<size_t>(i)], &modLfoPulseWidth[static_cast<size_t>(i)],
                 &modLfoPhase[static_cast<size_t>(i)],
                 &modLfoAmplitudeSource[static_cast<size_t>(i)], &modLfoAmplitude[static_cast<size_t>(i)],
                 &modLfoAmplitudeCcNumber[static_cast<size_t>(i)], &modLfoAmplitudeCcChannel[static_cast<size_t>(i)] })
            colourEditable (*component, lfoTabAccents[static_cast<size_t> (i)]);
    colourEditable (perlinOn, ndlr::ui::green);
    colourEditable (perlinM1, ndlr::ui::cyan);
    colourEditable (perlinM2, ndlr::ui::orange);
    for (auto& control : perlinControls) colourEditable (control, ndlr::ui::green);
    for (auto i = 0; i < 8; ++i)
        for (auto* component : std::array<juce::Component*, 7> {
                 &modSlotOn[static_cast<size_t>(i)], &modSlotSource[static_cast<size_t>(i)],
                 &modSlotDestination[static_cast<size_t>(i)], &modSlotAmount[static_cast<size_t>(i)],
                 &modSlotRange[static_cast<size_t>(i)],
                 &modSlotCcNumber[static_cast<size_t>(i)], &modSlotCcChannel[static_cast<size_t>(i)] })
            colourEditable (*component, ndlr::ui::green);

    // Tous les contrôles +/- partagent la même géométrie et le même geste :
    // moins à gauche, valeur éditable/glissable au centre, plus à droite.
    ndlr::ui::configureIncDecSliders (*this);

    // Les Attachments assurent la synchronisation bidirectionnelle entre les
    // contrôles graphiques et les paramètres sauvegardés/automatisables.
    transportPlayAttachment = std::make_unique<ButtonAttachment> (parameters, "transport.play", transportPlay);
    transportTempoAttachment = std::make_unique<SliderAttachment> (parameters, "transport.tempo", transportTempo);
    harmonyScaleAttachment = std::make_unique<ComboAttachment> (parameters, "harmony.scale", harmonyScale);
    harmonyTypeAttachment = std::make_unique<ComboAttachment> (parameters, "harmony.type", harmonyType);
    harmonyKeyMidiChannelAttachment = std::make_unique<ComboAttachment> (
        parameters, "harmony.keyMidiChannel", harmonyKeyMidiChannel);
    harmonyDegreeMidiChannelAttachment = std::make_unique<ComboAttachment> (
        parameters, "harmony.degreeMidiChannel", harmonyDegreeMidiChannel);
    motif1OnAttachment = std::make_unique<ButtonAttachment> (parameters, "motif1.on", motif1On);
    motif1LengthAttachment = std::make_unique<SliderAttachment> (parameters, "motif1.length", motif1Length);
    motif1VariationAttachment = std::make_unique<ComboAttachment> (parameters, "motif1.variation", motif1Variation);
    motif1VelocityAttachment = std::make_unique<SliderAttachment> (parameters, "motif1.velocity", motif1Velocity);
    motif1ChannelAttachment = std::make_unique<ComboAttachment> (parameters, "motif1.channel", motif1Channel);
    updatingPatternSelector = true;
    patternSelectorAttachment = std::make_unique<ComboAttachment> (
        parameters, "motif1.pattern", patternSelector);
    updatingPatternSelector = false;
    motif1HumanizeAttachment = std::make_unique<SliderAttachment> (parameters, "motif1.humanize", motif1Humanize);
    motif1AccentAttachment = std::make_unique<ComboAttachment> (parameters, "motif1.accent", motif1Accent);
    motif1GateAttachment = std::make_unique<SliderAttachment> (parameters, "motif1.gate", motif1Gate);
    motif1RhythmLengthAttachment = std::make_unique<SliderAttachment> (parameters, "motif1.rhythmLength", motif1RhythmLength);
    motif1EuclideanPulsesAttachment = std::make_unique<SliderAttachment> (parameters, "motif1.euclideanPulses", motif1EuclideanPulses);
    motif1RotationAttachment = std::make_unique<SliderAttachment> (parameters, "motif1.rotation", motif1Rotation);
    motif2OnAttachment = std::make_unique<ButtonAttachment> (parameters, "motif2.on", motif2On);
    motif2VariationAttachment = std::make_unique<ComboAttachment> (parameters, "motif2.variation", motif2Variation);
    motif2VelocityAttachment = std::make_unique<SliderAttachment> (parameters, "motif2.velocity", motif2Velocity);
    motif2ChannelAttachment = std::make_unique<ComboAttachment> (parameters, "motif2.channel", motif2Channel);
    motif2HumanizeAttachment = std::make_unique<SliderAttachment> (parameters, "motif2.humanize", motif2Humanize);
    motif2AccentAttachment = std::make_unique<ComboAttachment> (parameters, "motif2.accent", motif2Accent);
    motif2GateAttachment = std::make_unique<SliderAttachment> (parameters, "motif2.gate", motif2Gate);
    droneOnAttachment = std::make_unique<ButtonAttachment> (parameters, "drone.on", droneOn);
    dronePositionAttachment = std::make_unique<ComboAttachment> (parameters, "drone.position", dronePosition);
    droneTypeAttachment = std::make_unique<ComboAttachment> (parameters, "drone.type", droneType);
    droneTriggerAttachment = std::make_unique<ComboAttachment> (parameters, "drone.trigger", droneTrigger);
    droneVelocityAttachment = std::make_unique<SliderAttachment> (parameters, "drone.velocity", droneVelocity);
    droneChannelAttachment = std::make_unique<ComboAttachment> (parameters, "drone.channel", droneChannel);
    padOnAttachment=std::make_unique<ButtonAttachment>(parameters,"pad.on",padOn); padStrumAttachment=std::make_unique<ButtonAttachment>(parameters,"pad.strum",padStrum); padGroupAttachment=std::make_unique<ButtonAttachment>(parameters,"pad.group",padGroup); padInvertAttachment=std::make_unique<ButtonAttachment>(parameters,"pad.invert",padInvert);
    padPositionAttachment=std::make_unique<SliderAttachment>(parameters,"pad.position",padPosition); padRangeAttachment=std::make_unique<SliderAttachment>(parameters,"pad.range",padRange); padSpreadAttachment=std::make_unique<SliderAttachment>(parameters,"pad.spread",padSpread); padVelocityAttachment=std::make_unique<SliderAttachment>(parameters,"pad.velocity",padVelocity); padPolyAttachment=std::make_unique<SliderAttachment>(parameters,"pad.polyChain",padPolyChain);
    padDivisionAttachment=std::make_unique<ComboAttachment>(parameters,"pad.strumDivision",padStrumDivision); padChannelAttachment=std::make_unique<ComboAttachment>(parameters,"pad.channel",padChannel); padQuantizeAttachment=std::make_unique<ComboAttachment>(parameters,"pad.quantize",padQuantize); padInversionModeAttachment=std::make_unique<ComboAttachment>(parameters,"pad.inversionMode",padInversionMode);
    for(auto i=0;i<3;++i){const auto p="mod.lfo"+juce::String(i+1)+"."; const auto n=static_cast<size_t>(i); modLfoShapeAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"shape",modLfoShape[n]); modLfoDivisionAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"division",modLfoDivision[n]); modLfoSyncAttachment[n]=std::make_unique<ButtonAttachment>(parameters,p+"sync",modLfoSync[n]); modLfoRetriggerAttachment[n]=std::make_unique<ButtonAttachment>(parameters,p+"retrigger",modLfoRetrigger[n]); modLfoRateAttachment[n]=std::make_unique<SliderAttachment>(parameters,p+"rate",modLfoRate[n]); modLfoProbabilityAttachment[n]=std::make_unique<SliderAttachment>(parameters,p+"probability",modLfoProbability[n]); modLfoPulseWidthAttachment[n]=std::make_unique<SliderAttachment>(parameters,p+"pulseWidth",modLfoPulseWidth[n]); modLfoLengthAttachment[n]=std::make_unique<SliderAttachment>(parameters,p+"length",modLfoLength[n]); modLfoPhaseAttachment[n]=std::make_unique<SliderAttachment>(parameters,p+"phase",modLfoPhase[n]); modLfoAmplitudeSourceAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"amplitudeSource",modLfoAmplitudeSource[n]); modLfoAmplitudeAttachment[n]=std::make_unique<SliderAttachment>(parameters,p+"amplitude",modLfoAmplitude[n]); modLfoAmplitudeCcNumberAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"amplitudeCcNumber",modLfoAmplitudeCcNumber[n]); modLfoAmplitudeCcChannelAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"amplitudeCcChannel",modLfoAmplitudeCcChannel[n]);}
    for(auto i=0;i<8;++i){const auto p="mod.slot"+juce::String(i+1)+"."; const auto n=static_cast<size_t>(i); modSlotOnAttachment[n]=std::make_unique<ButtonAttachment>(parameters,p+"on",modSlotOn[n]); modSlotSourceAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"source",modSlotSource[n]); modSlotDestinationAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"destination",modSlotDestination[n]); modSlotCcNumberAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"ccNumber",modSlotCcNumber[n]); modSlotCcChannelAttachment[n]=std::make_unique<ComboAttachment>(parameters,p+"ccChannel",modSlotCcChannel[n]); modSlotAmountAttachment[n]=std::make_unique<SliderAttachment>(parameters,p+"amount",modSlotAmount[n]);}
    perlinOnAttachment = std::make_unique<ButtonAttachment> (parameters, "perlin.on", perlinOn);
    perlinM1Attachment = std::make_unique<ButtonAttachment> (parameters, "perlin.m1", perlinM1);
    perlinM2Attachment = std::make_unique<ButtonAttachment> (parameters, "perlin.m2", perlinM2);
    const std::array<juce::String, 7> perlinParameterIds {
        "perlin.zoom", "perlin.spacing", "perlin.roughness", "perlin.persistence",
        "perlin.seed", "perlin.resolution", "perlin.brightness"
    };
    for (auto i = 0; i < 7; ++i)
        perlinControlAttachments[static_cast<size_t> (i)] = std::make_unique<SliderAttachment> (
            parameters, perlinParameterIds[static_cast<size_t> (i)], perlinControls[static_cast<size_t> (i)]);

    // Réapplique la sélection maintenant que les attachments existent : les
    // contrôles du Rhythm Editor suivent ainsi immédiatement M1 ou M2.
    selectMotif (selectedMotif);

    setResizable (true, true);
    setResizeLimits (980, 850, 1800, 1600);
    setSize (1200, 850);
    layoutDebugOverlay.restoreLayout (parameters.state);
    migrateToCompiledDefaultLayoutIfNeeded();
    migrateTabsOutOfParentPanelsIfNeeded();
    mirrorLfo2And3LayoutFromLfo1IfNeeded();
    migrateLfoStartModeControlsIfNeeded();
    mirrorMotif1LayoutFromMotif2IfNeeded();
    migrateMotifLengthsToPatternEditorIfNeeded();
    migrateHarmonyMidiChannelsToHeaderIfNeeded();
    migrateMotifSourceButtonsIfNeeded();
    migrateMotifRegisterButtonsIfNeeded();
    migrateRhythmModeButtonsIfNeeded();
    migratePadToCompactLayoutIfNeeded();
    migratePatternEditorIntoGeneratorsIfNeeded();
    ensureGeneratorContainersAreVisible();

    // LENGTH occupe la même cellule que PULSE WIDTH et devient visible
    // uniquement lorsque SHAPE sélectionne un pattern.
    for (auto lfo = 0; lfo < 3; ++lfo)
    {
        const auto n = static_cast<size_t> (lfo);
        if (modLfoLength[n].getBounds().isEmpty())
            layoutDebugOverlay.copyTargetLayout (modLfoPulseWidth[n], modLfoLength[n]);
    }

    // Layout restoration may reapply the colour and geometry saved while M2
    // was active. Synchronise once more from the actually visible tab so the
    // shared Pattern/Rhythm editors and all their attachments start on M1.
    if (const auto visibleTab = generatorTabs.getCurrentTabIndex();
        visibleTab == 0 || visibleTab == 1)
        selectMotif (visibleTab);
    layoutDebugOverlay.saveLayout (parameters.state);
    for (auto* component : std::array<juce::Component*, 4> {
             &harmonyKeyMidiChannelLabel, &harmonyDegreeMidiChannelLabel,
             &harmonyKeyMidiChannel, &harmonyDegreeMidiChannel })
        component->toFront (false);
    updateModMidiCcLayout();
    owner.refreshRhythmMemoryCache();
    generatorTabs.toBack();
    drone.toFront (false);
    startTimerHz (12);
}

NdlrAudioProcessorEditor::~NdlrAudioProcessorEditor()
{
    generatorTabs.getTabbedButtonBar().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void NdlrAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source != &generatorTabs.getTabbedButtonBar())
        return;

    const auto tab = generatorTabs.getCurrentTabIndex();
    if (tab == 0 || tab == 1)
        selectMotif (tab);
}

void NdlrAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fond général et séparation visuelle sous l'en-tête.
    g.fillAll (ndlr::ui::background);
    g.setColour (ndlr::ui::panelEdge);
    g.drawHorizontalLine (76, 24.0f, static_cast<float> (getWidth() - 24));
}

void NdlrAudioProcessorEditor::layoutHeader()
{
    auto header = getLocalBounds().reduced (22).removeFromTop (54);
    const auto statusWidth = juce::jlimit (140, 220, header.getWidth() - 796);

    title.setBounds (header.removeFromLeft (160));
    status.setBounds (header.removeFromRight (statusWidth));
    transportPlay.setBounds (header.removeFromLeft (68).reduced (3, 11));
    transportTempo.setBounds (header.removeFromLeft (180).reduced (6, 10));
    panicButton.setBounds (header.removeFromLeft (125).reduced (4, 10));
    layoutDebugButton.setBounds (header.removeFromLeft (80).reduced (4, 10));
    header.removeFromLeft (8);

    auto midiArea = header.removeFromLeft (juce::jmin (220, header.getWidth()));
    auto layoutMidiChannel = [] (juce::Rectangle<int> area, juce::Label& label,
                                 juce::ComboBox& menu)
    {
        area = area.reduced (2, 1);
        label.setBounds (area.removeFromTop (17));
        menu.setBounds (area.removeFromTop (30).reduced (2, 1));
    };
    layoutMidiChannel (midiArea.removeFromLeft (midiArea.getWidth() / 2),
                       harmonyKeyMidiChannelLabel, harmonyKeyMidiChannel);
    layoutMidiChannel (midiArea, harmonyDegreeMidiChannelLabel, harmonyDegreeMidiChannel);
}

void NdlrAudioProcessorEditor::layoutGeneratorTabs()
{
    auto motifPageBounds = generatorTabPages[0].getLocalBounds().reduced (1);
    const auto motifPanelHeight = juce::jlimit (170, 215,
        (motifPageBounds.getHeight() - 8) / 2);
    motif1.setBounds (motifPageBounds.removeFromTop (motifPanelHeight));
    motif2.setBounds (motif1.getBounds());
    motifPageBounds.removeFromTop (8);
    patternEditorPanel.setBounds (motifPageBounds);
    drone.setBounds (generatorTabPages[2].getLocalBounds().reduced (1));
    pad.setBounds (generatorTabPages[3].getLocalBounds().reduced (1));

    layoutMotifControls (0);
    layoutMotifControls (1);
    layoutPatternEditorControls();
    layoutDroneControls();
    layoutPadControls();
}

void NdlrAudioProcessorEditor::ensureGeneratorContainersAreVisible()
{
    const auto isOutsideParent = [] (const juce::Component& component,
                                     const juce::Component& parent)
    {
        const auto intersection = component.getBounds().getIntersection (parent.getLocalBounds());
        return component.getWidth() <= 0 || component.getHeight() <= 0
            || intersection.getWidth() < juce::jmax (1, component.getWidth() / 4)
            || intersection.getHeight() < juce::jmax (1, component.getHeight() / 4);
    };

    if (isOutsideParent (generatorTabs, *this))
    {
        const auto scaleX = static_cast<float> (getWidth()) / 1200.0f;
        const auto scaleY = static_cast<float> (getHeight()) / 850.0f;
        generatorTabs.setBounds (juce::roundToInt (640.0f * scaleX),
                                 juce::roundToInt (80.0f * scaleY),
                                 juce::roundToInt (544.0f * scaleX),
                                 juce::roundToInt (488.0f * scaleY));
    }

    auto motifBounds = generatorTabPages[0].getLocalBounds().reduced (1);
    const auto motifPanelHeight = juce::jlimit (
        170, 215, (motifBounds.getHeight() - 8) / 2);
    const auto motifPanelBounds = motifBounds.removeFromTop (motifPanelHeight);
    motifBounds.removeFromTop (8);

    if (isOutsideParent (motif1, generatorTabPages[0]))
        motif1.setBounds (motifPanelBounds);
    if (isOutsideParent (motif2, generatorTabPages[1]))
        motif2.setBounds (motifPanelBounds);

    auto& selectedPage = generatorTabPages[static_cast<size_t> (selectedMotif)];
    if (patternEditorPanel.getParentComponent() != &selectedPage)
        selectedPage.addAndMakeVisible (patternEditorPanel);
    if (isOutsideParent (patternEditorPanel, selectedPage))
        patternEditorPanel.setBounds (motifBounds);

    if (isOutsideParent (drone, generatorTabPages[2]))
        drone.setBounds (generatorTabPages[2].getLocalBounds().reduced (1));
    if (isOutsideParent (pad, generatorTabPages[3]))
        pad.setBounds (generatorTabPages[3].getLocalBounds().reduced (1));
}

void NdlrAudioProcessorEditor::layoutMotifControls (int motifIndex)
{
    auto layout = [] (ndlr::ui::SectionPanel& panel,
                      juce::ToggleButton& on,
                      std::array<juce::TextButton, 3>& sourceButtons,
                      std::array<juce::TextButton, 5>& registerButtons,
                      juce::Slider& humanize, juce::ComboBox& accent, juce::Slider& gate,
                      juce::ComboBox& variation, juce::Slider& velocity,
                      juce::ComboBox& channel,
                      juce::Label& sourceL, juce::Label& registerL,
                      juce::Label& humanizeL, juce::Label& accentL, juce::Label& gateL,
                      juce::Label& variationL, juce::Label& velocityL, juce::Label& channelL)
    {
        auto controls = panel.getLocalBounds().reduced (18);
        if (controls.getHeight() > 190)
            controls = controls.withSizeKeepingCentre (controls.getWidth(), 190);

        on.setBounds (controls.removeFromTop (28).removeFromLeft (76));
        controls.removeFromTop (7);
        auto patternRow = controls.removeFromTop (54);
        auto sourceArea = patternRow.removeFromLeft (patternRow.getWidth() * 3 / 5);
        auto registerArea = patternRow;
        sourceL.setBounds (sourceArea.removeFromTop (18));
        auto sourceButtonsArea = sourceArea.reduced (3, 2).withHeight (27);
        const auto sourceButtonWidth = sourceButtonsArea.getWidth() / 3;
        sourceButtons[0].setBounds (sourceButtonsArea.removeFromLeft (sourceButtonWidth));
        sourceButtons[1].setBounds (sourceButtonsArea.removeFromLeft (sourceButtonWidth));
        sourceButtons[2].setBounds (sourceButtonsArea);
        registerL.setBounds (registerArea.removeFromTop (18));
        auto registerButtonsArea = registerArea.reduced (3, 2).withHeight (27);
        const auto registerButtonWidth = registerButtonsArea.getWidth() / 5;
        for (auto index = 0; index < 4; ++index)
            registerButtons[static_cast<size_t> (index)].setBounds (
                registerButtonsArea.removeFromLeft (registerButtonWidth));
        registerButtons[4].setBounds (registerButtonsArea);

        controls.removeFromTop (5);
        auto expressionRow = controls.removeFromTop (34);
        const auto expressionCellWidth = expressionRow.getWidth() / 3;
        auto humanizeArea = expressionRow.removeFromLeft (expressionCellWidth);
        auto accentArea = expressionRow.removeFromLeft (expressionCellWidth);
        auto gateArea = expressionRow;
        humanizeL.setBounds (humanizeArea.removeFromLeft (juce::jmin (76, humanizeArea.getWidth() / 2)));
        humanize.setBounds (humanizeArea.reduced (4, 3));
        accentL.setBounds (accentArea.removeFromLeft (juce::jmin (60, accentArea.getWidth() / 2)));
        accent.setBounds (accentArea.reduced (4, 3));
        gateL.setBounds (gateArea.removeFromLeft (juce::jmin (48, gateArea.getWidth() / 2)));
        gate.setBounds (gateArea.reduced (4, 3));

        controls.removeFromTop (5);
        const auto cellWidth = controls.getWidth() / 3;
        auto variationCell = controls.removeFromLeft (cellWidth);
        auto velocityCell = controls.removeFromLeft (cellWidth);
        auto channelCell = controls;
        variationL.setBounds (variationCell.removeFromTop (18));
        variation.setBounds (variationCell.reduced (5, 2).withHeight (27));
        velocityL.setBounds (velocityCell.removeFromTop (18));
        velocity.setBounds (velocityCell.reduced (7, 2).withHeight (27));
        channelL.setBounds (channelCell.removeFromTop (18));
        channel.setBounds (channelCell.reduced (8, 2).withHeight (27));
    };

    if (motifIndex == 0)
        layout (motif1, motif1On, motif1PatternTypeButtons,
                motif1RegisterButtons, motif1Humanize, motif1Accent, motif1Gate,
                motif1Variation, motif1Velocity, motif1Channel,
                patternTypeLabel, positionLabel,
                humanizeLabel, accentLabel, gateLabel,
                variationLabel, velocityLabel, channelLabel);
    else
        layout (motif2, motif2On, motif2PatternTypeButtons,
                motif2RegisterButtons, motif2Humanize, motif2Accent, motif2Gate,
                motif2Variation, motif2Velocity, motif2Channel,
                motif2PatternTypeLabel, motif2PositionLabel,
                motif2HumanizeLabel, motif2AccentLabel, motif2GateLabel,
                motif2VariationLabel, motif2VelocityLabel, motif2ChannelLabel);
}

void NdlrAudioProcessorEditor::layoutPatternEditorControls()
{
    auto area = patternEditorPanel.getLocalBounds().reduced (10).withTrimmedTop (24);
    auto toolbar = area.removeFromTop (28);
    patternSelector.setBounds (toolbar.removeFromLeft (230));
    patternSave.setBounds (toolbar.removeFromLeft (64).reduced (3, 1));
    const auto sharedLengthWidth = juce::jmin (230, toolbar.getWidth());
    auto lengthArea = toolbar.removeFromRight (sharedLengthWidth);
    lengthLabel.setBounds (lengthArea.removeFromLeft (70));
    motif1Length.setBounds (lengthArea.reduced (3, 2));
    area.removeFromTop (5);
    patternDisplay.setBounds (area);
}

void NdlrAudioProcessorEditor::layoutDroneControls()
{
    auto area = drone.getLocalBounds().reduced (18);
    if (area.getHeight() > 72)
        area = area.withSizeKeepingCentre (area.getWidth(), 72);

    droneOn.setBounds (area.removeFromLeft (62).reduced (3, 21));
    auto layoutCell = [] (juce::Rectangle<int> cell, juce::Label& label,
                          juce::Component& control)
    {
        label.setBounds (cell.removeFromTop (19));
        control.setBounds (cell.removeFromTop (32).reduced (3, 2));
    };
    const auto remainingWidth = area.getWidth();
    layoutCell (area.removeFromLeft (remainingWidth * 17 / 100), dronePositionLabel, dronePosition);
    layoutCell (area.removeFromLeft (remainingWidth * 19 / 100), droneTypeLabel, droneType);
    layoutCell (area.removeFromLeft (remainingWidth * 27 / 100), droneTriggerLabel, droneTrigger);
    layoutCell (area.removeFromLeft (remainingWidth * 18 / 100), droneVelocityLabel, droneVelocity);
    layoutCell (area, droneChannelLabel, droneChannel);
}

void NdlrAudioProcessorEditor::layoutPadControls()
{
    auto area = pad.getLocalBounds().reduced (12);
    constexpr auto lowerControlsHeight = 59;
    constexpr auto spreadControlsHeight = 30;
    constexpr auto verticalGaps = 6;
    const auto keyboardHeight = juce::jlimit (
        70, 320, area.getHeight() - lowerControlsHeight
                     - spreadControlsHeight - verticalGaps);
    const auto keyboardArea = area.removeFromTop (keyboardHeight);
    padVoicingMiniView.setBounds (keyboardArea);

    // Les sliders restent relies aux parametres et a l'automation, mais leur
    // interface +/- est remplacee par le glissement direct dans le clavier.
    padPosition.setBounds ({});
    padRange.setBounds ({});
    padSpread.setBounds ({});
    padPositionLabel.setBounds ({});
    padRangeLabel.setBounds ({});

    area.removeFromTop (3);
    auto spreadArea = area.removeFromTop (spreadControlsHeight);
    padSpreadLabel.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    padSpreadLabel.setBounds (spreadArea.removeFromLeft (52).reduced (2, 1));
    const auto spreadButtonWidth = spreadArea.getWidth() / 6;
    for (auto index = 0; index < 6; ++index)
    {
        auto buttonArea = index == 5 ? spreadArea
                                     : spreadArea.removeFromLeft (spreadButtonWidth);
        padSpreadButtons[static_cast<size_t> (index)].setBounds (
            buttonArea.withSizeKeepingCentre (72, 24));
    }

    area.removeFromTop (3);
    auto row1 = area.removeFromTop (27);
    auto padOnArea = row1.removeFromLeft (juce::jmin (70, row1.getWidth() / 5));
    padOn.setBounds (padOnArea.reduced (2, 2));

    auto layoutInline = [] (juce::Rectangle<int> cell, juce::Label& label,
                            juce::Component& control, int labelWidth)
    {
        cell = cell.reduced (2, 1);
        label.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        label.setMinimumHorizontalScale (0.64f);
        label.setBounds (cell.removeFromLeft (juce::jmin (labelWidth, cell.getWidth() / 2)));
        control.setBounds (cell.reduced (1, 1));
    };
    const auto row1CellWidth = row1.getWidth() / 3;
    layoutInline (row1.removeFromLeft (row1CellWidth), padVelocityLabel, padVelocity, 56);
    layoutInline (row1.removeFromLeft (row1CellWidth), padChannelLabel, padChannel, 42);
    layoutInline (row1, padPolyLabel, padPolyChain, 34);

    area.removeFromTop (2);
    auto row2 = area.removeFromTop (27);
    auto takeWidth = [&row2] (int preferred)
    {
        return row2.removeFromLeft (juce::jmin (preferred, row2.getWidth()));
    };
    padStrum.setBounds (takeWidth (80).reduced (2, 2));
    layoutInline (takeWidth (115), padDivisionLabel, padStrumDivision, 27);
    padGroup.setBounds (takeWidth (78).reduced (2, 2));
    padInversionMode.setBounds (takeWidth (96).reduced (2, 2));
    layoutInline (row2, padQuantizeLabel, padQuantize, 58);

    for (auto* component : { static_cast<juce::Component*> (&padPosition),
                             static_cast<juce::Component*> (&padRange),
                             static_cast<juce::Component*> (&padOn),
                             static_cast<juce::Component*> (&padStrum),
                             static_cast<juce::Component*> (&padGroup),
                             static_cast<juce::Component*> (&padInversionMode) })
        component->toFront (false);
}

void NdlrAudioProcessorEditor::resized()
{
    // Toute la mise en page est recalculée à partir de la taille disponible.
    // removeFrom... découpe progressivement le rectangle sans coordonnées fixes globales.
    auto area = getLocalBounds().reduced (22);
    area.removeFromTop (54);
    layoutHeader();
    layoutDebugOverlay.setBounds (0, 80, getWidth(), juce::jmax (0, getHeight() - 80));
    const auto scaleX = static_cast<float> (getWidth()) / 1200.0f;
    const auto scaleY = static_cast<float> (getHeight()) / 850.0f;
    generatorTabs.setBounds (juce::roundToInt (640.0f * scaleX),
                             juce::roundToInt (80.0f * scaleY),
                             juce::roundToInt (544.0f * scaleX),
                             juce::roundToInt (488.0f * scaleY));
    layoutGeneratorTabs();
    area.removeFromTop (14);

    auto harmonyArea = area.removeFromTop (280);
    harmony.setBounds (harmonyArea);

    // La roue occupe la partie gauche du panneau Harmony ; les menus compacts
    // occupent la partie droite.
    auto harmonyContent = harmony.getLocalBounds().reduced (18).withTrimmedTop (24);
    harmonyWheel.setBounds (harmonyContent.removeFromLeft (245));
    harmonyContent.removeFromLeft (24);
    auto harmonyControls = harmonyContent.removeFromTop (70);
    auto scaleArea = harmonyControls.removeFromLeft (165).reduced (5, 4);
    harmonyScaleLabel.setBounds (scaleArea.removeFromTop (22));
    harmonyScale.setBounds (scaleArea.removeFromTop (28));
    harmonyControls.removeFromLeft (18);
    auto typeArea = harmonyControls.removeFromLeft (155).reduced (5, 4);
    harmonyTypeLabel.setBounds (typeArea.removeFromTop (22));
    harmonyType.setBounds (typeArea.removeFromTop (28));
    area.removeFromTop (12);

    area.removeFromBottom (180);
    placeholderText.setBounds (modMatrix.getLocalBounds().reduced (24).withTrimmedTop (16));
    area.removeFromBottom (12);
    auto modulationArea = area.removeFromBottom (220);
    auto lfoBounds = modulationArea.removeFromLeft (modulationArea.getWidth() * 44 / 100);
    modulationArea.removeFromLeft (12);
    lfoTabs.setBounds (lfoBounds);
    modMatrix.setBounds (modulationArea);
    layoutLfoTabs();
    auto slotsArea = modMatrix.getLocalBounds().reduced(12).withTrimmedTop(26);

    auto slotHeader=slotsArea.removeFromTop(18);
    static constexpr int modMidiCompositeWidth = 190;
    static constexpr int modMidiDestinationWidth = 88;
    static constexpr int modMidiCcWidth = 52;
    static constexpr int modMidiChannelWidth = modMidiCompositeWidth
                                             - modMidiDestinationWidth
                                             - modMidiCcWidth;
    auto anyMidiCcDestination = false;
    for (auto i = 0; i < 8; ++i)
        anyMidiCcDestination = anyMidiCcDestination
            || juce::roundToInt (owner.getParameters().getRawParameterValue (
                "mod.slot" + juce::String (i + 1) + ".destination")->load())
                    == ndlr::modulation::midiCc
            || juce::roundToInt (owner.getParameters().getRawParameterValue (
                "mod.slot" + juce::String (i + 1) + ".source")->load()) == 6;

    modSlotLabels[0].setBounds (slotHeader.removeFromLeft (38));
    modSlotLabels[1].setBounds (slotHeader.removeFromLeft (90));
    modSlotLabels[2].setBounds (slotHeader.removeFromLeft (
        anyMidiCcDestination ? modMidiDestinationWidth : modMidiCompositeWidth));
    if (anyMidiCcDestination)
    {
        modCcNumberLabel.setBounds (slotHeader.removeFromLeft (modMidiCcWidth));
        modCcChannelLabel.setBounds (slotHeader.removeFromLeft (modMidiChannelWidth));
    }
    else
    {
        const auto destinationHeader = modSlotLabels[2].getBounds();
        modCcNumberLabel.setBounds (destinationHeader.getX() + modMidiDestinationWidth,
                                    destinationHeader.getY(), modMidiCcWidth,
                                    destinationHeader.getHeight());
        modCcChannelLabel.setBounds (destinationHeader.getX() + modMidiDestinationWidth + modMidiCcWidth,
                                     destinationHeader.getY(), modMidiChannelWidth,
                                     destinationHeader.getHeight());
    }
    modCcNumberLabel.setVisible (anyMidiCcDestination);
    modCcChannelLabel.setVisible (anyMidiCcDestination);
    modSlotLabels[3].setBounds (slotHeader.removeFromLeft (75));
    modSlotLabels[4].setBounds (slotHeader.removeFromLeft (130));
    modScopeLabel.setBounds (slotHeader.removeFromLeft (80));
    for(auto i=0;i<8;++i)
    {
        auto row=slotsArea.removeFromTop(19);
        const auto n = static_cast<size_t> (i);
        const auto prefix = "mod.slot" + juce::String (i + 1) + ".";
        const auto midiCcDestination = juce::roundToInt (owner.getParameters().getRawParameterValue (
            prefix + "destination")->load()) == ndlr::modulation::midiCc
            || juce::roundToInt (owner.getParameters().getRawParameterValue (
                prefix + "source")->load()) == 6;
        modSlotOn[static_cast<size_t>(i)].setBounds(row.removeFromLeft(38));
        modSlotSource[static_cast<size_t>(i)].setBounds(row.removeFromLeft(90).reduced(2,1));
        modSlotDestination[n].setBounds(row.removeFromLeft(
            midiCcDestination ? modMidiDestinationWidth : modMidiCompositeWidth).reduced(2,1));
        if (midiCcDestination)
        {
            modSlotCcNumber[n].setBounds (row.removeFromLeft (modMidiCcWidth).reduced (2, 1));
            modSlotCcChannel[n].setBounds (row.removeFromLeft (modMidiChannelWidth).reduced (2, 1));
        }
        else
        {
            const auto destinationBounds = modSlotDestination[n].getBounds();
            modSlotCcNumber[n].setBounds (destinationBounds.getX() + modMidiDestinationWidth,
                                          destinationBounds.getY(), modMidiCcWidth - 4,
                                          destinationBounds.getHeight());
            modSlotCcChannel[n].setBounds (destinationBounds.getX() + modMidiDestinationWidth + modMidiCcWidth,
                                           destinationBounds.getY(), modMidiChannelWidth - 4,
                                           destinationBounds.getHeight());
        }
        modSlotCcNumber[n].setVisible (midiCcDestination);
        modSlotCcChannel[n].setVisible (midiCcDestination);
        modSlotAmount[static_cast<size_t>(i)].setBounds(row.removeFromLeft(75).reduced(2,1));
        modSlotRange[n].setBounds(row.removeFromLeft(130).reduced(5,1));
        modSlotScope[static_cast<size_t>(i)].setBounds(row.removeFromLeft(80).reduced(3,1));
    }
    area.removeFromBottom (12);

    const auto gap = 12;
    const auto columnWidth = (area.getWidth() - gap) / 2;
    auto left = area.removeFromLeft (columnWidth);
    area.removeFromLeft (gap);
    auto right = area;
    juce::ignoreUnused (left);

    // Le Pattern Editor vit maintenant dans les pages Motif. L'editeur de
    // rythme peut utiliser seul cette colonne dans le layout de secours.
    right.removeFromTop (24);
    rhythmEditor.setBounds (right);
    auto rhythmBounds = rhythmEditor.getLocalBounds().reduced (12).withTrimmedTop (24);
    auto rhythmToolbar = rhythmBounds.removeFromTop (30);
    rhythmUserSlot.setBounds (rhythmToolbar.removeFromLeft (145));
    rhythmSave.setBounds (rhythmToolbar.removeFromLeft (64).reduced (3, 1));
    rhythmLoad.setBounds (rhythmToolbar.removeFromLeft (64).reduced (3, 1));
    rhythmBounds.removeFromTop (4);
    const auto wheelSize = juce::jmin (rhythmBounds.getWidth(), rhythmBounds.getHeight());
    rhythmWheel.setBounds (rhythmBounds.removeFromLeft (wheelSize));
    auto layoutRhythmControl = [&rhythmBounds] (juce::Label& label, juce::Component& control, int width)
    {
        auto row = rhythmBounds.removeFromTop (34);
        label.setBounds (row.removeFromLeft (68));
        control.setBounds (row.removeFromLeft (width).reduced (3, 4));
    };
    auto rhythmModeRow = rhythmBounds.removeFromTop (34);
    rhythmModeLabel.setBounds (rhythmModeRow.removeFromLeft (68));
    auto rhythmModeArea = rhythmModeRow.removeFromLeft (140).reduced (3, 4);
    const auto rhythmModeButtonWidth = rhythmModeArea.getWidth() / 2;
    rhythmModeButtons[0].setBounds (rhythmModeArea.removeFromLeft (rhythmModeButtonWidth));
    rhythmModeButtons[1].setBounds (rhythmModeArea);
    layoutRhythmControl (rhythmLengthLabel, motif1RhythmLength, 110);
    layoutRhythmControl (rhythmBeatsLabel, motif1EuclideanPulses, 110);
    layoutRhythmControl (rhythmRotateLabel, motif1Rotation, 110);

    // Un layout personnalisé est prioritaire sur la grille responsive par défaut.
    layoutDebugOverlay.restoreLayout (owner.getParameters().state);
    // Les panneaux contenus dans les onglets utilisent des coordonnees locales.
    // Certains layouts historiques stockent encore leurs anciennes coordonnees
    // absolues : les ramener dans leur page evite une zone Generators vide.
    ensureGeneratorContainersAreVisible();
    // Les états créés avant les onglets n'ont aucune géométrie pour eux. Dans
    // ce cas seulement, recalcule leur placement après restauration du panneau.
    auto hasSavedLfoTabs = false;
    if (const auto savedLayout = owner.getParameters().state.getChildWithName ("UILayout");
        savedLayout.isValid())
        for (auto item : savedLayout)
            if (item["id"].toString() == "LFO Tabs")
            {
                hasSavedLfoTabs = true;
                break;
            }
    if (! hasSavedLfoTabs)
        layoutLfoTabs();
    for (auto* component : std::array<juce::Component*, 4> {
             &harmonyKeyMidiChannelLabel, &harmonyDegreeMidiChannelLabel,
             &harmonyKeyMidiChannel, &harmonyDegreeMidiChannel })
        component->toFront (false);
    updateModMidiCcLayout();
    generatorTabs.toBack();
    drone.toFront (false);
    if (layoutDebugOverlay.isVisible()) layoutDebugOverlay.toFront (false);
}

void NdlrAudioProcessorEditor::layoutLfoTabs()
{
    for (auto i = 0; i < 3; ++i)
    {
        const auto n = static_cast<size_t> (i);
        auto tabArea = lfoTabPages[n].getLocalBounds().reduced (14, 10);
        const auto columnGap = 14;
        const auto columnWidth = (tabArea.getWidth() - columnGap) / 2;
        auto leftColumn = tabArea.removeFromLeft (columnWidth);
        tabArea.removeFromLeft (columnGap);
        auto rightColumn = tabArea;
        const auto rowHeight = juce::jlimit (26, 34, leftColumn.getHeight() / 5);

        const auto layoutRow = [rowHeight] (juce::Rectangle<int>& column,
                                            juce::Label& label,
                                            juce::Component& control,
                                            int labelWidth)
        {
            auto row = column.removeFromTop (rowHeight);
            label.setBounds (row.removeFromLeft (labelWidth));
            control.setBounds (row.reduced (2, 3));
        };

        layoutRow (leftColumn, modLfoTabLabels[n][0], modLfoShape[n], 86);
        layoutRow (leftColumn, modLfoTabLabels[n][4], modLfoPulseWidth[n], 104);
        modLfoLength[n].setBounds (modLfoPulseWidth[n].getBounds());
        layoutRow (leftColumn, modLfoTabLabels[n][2], modLfoMode[n], 62);

        auto timingRow = leftColumn.removeFromTop (rowHeight);
        modLfoTabLabels[n][1].setBounds (timingRow.removeFromLeft (104));
        const auto rateOrDivisionBounds = timingRow.reduced (2, 3);
        modLfoRate[n].setBounds (rateOrDivisionBounds);
        modLfoDivision[n].setBounds (rateOrDivisionBounds);
        layoutRow (leftColumn, modLfoTabLabels[n][3], modLfoProbability[n], 104);

        layoutRow (rightColumn, modLfoTabLabels[n][5], modLfoAmplitudeSource[n], 92);
        layoutRow (rightColumn, modLfoTabLabels[n][6], modLfoAmplitude[n], 104);
        layoutRow (rightColumn, modLfoTabLabels[n][7], modLfoAmplitudeCcNumber[n], 72);
        layoutRow (rightColumn, modLfoTabLabels[n][8], modLfoAmplitudeCcChannel[n], 72);
        layoutRow (rightColumn, modLfoTabLabels[n][9], modLfoPhase[n], 104);
    }

    auto perlinArea = perlinTabPage.getLocalBounds().reduced (10, 8);
    const auto displayWidth = juce::jmin (145, perlinArea.getWidth() / 3);
    perlinDisplay.setBounds (perlinArea.removeFromLeft (displayWidth).reduced (2));
    perlinArea.removeFromLeft (6);
    const auto profilesWidth = juce::jmin (140, perlinArea.getWidth() * 2 / 5);
    perlinProfiles.setBounds (perlinArea.removeFromLeft (profilesWidth).reduced (2));
    perlinArea.removeFromLeft (6);

    auto perlinToggleRow = perlinArea.removeFromTop (30);
    const auto toggleWidth = juce::jmax (42, perlinToggleRow.getWidth() / 3);
    perlinOn.setBounds (perlinToggleRow.removeFromLeft (toggleWidth).reduced (2, 3));
    perlinM1.setBounds (perlinToggleRow.removeFromLeft (toggleWidth).reduced (2, 3));
    perlinM2.setBounds (perlinToggleRow.removeFromLeft (toggleWidth).reduced (2, 3));

    const auto perlinRowHeight = juce::jmax (22, perlinArea.getHeight() / 4);
    for (auto index = 0; index < 7; ++index)
    {
        const auto row = index / 2;
        const auto column = index % 2;
        auto cell = juce::Rectangle<int> (perlinArea.getX() + column * perlinArea.getWidth() / 2,
                                          perlinArea.getY() + row * perlinRowHeight,
                                          perlinArea.getWidth() / 2, perlinRowHeight).reduced (2, 1);
        perlinLabels[static_cast<size_t> (index)].setBounds (cell.removeFromLeft (38));
        perlinControls[static_cast<size_t> (index)].setBounds (cell.reduced (1, 2));
    }
}

void NdlrAudioProcessorEditor::migrateToCompiledDefaultLayoutIfNeeded()
{
    // Une instance Standalone ou un projet de DAW peut contenir un ancien
    // UILayout qui masque la ressource compilee. Cette version l'installe une
    // seule fois ; les personnalisations faites ensuite restent prioritaires.
    constexpr auto migrationProperty = "compiledDefaultLayoutVersion";
    constexpr auto fingerprintProperty = "compiledDefaultLayoutFingerprint";
    constexpr auto currentVersion = 4;
    auto& state = owner.getParameters().state;
    const auto compiledFingerprint = layoutDebugOverlay.getCompiledDefaultLayoutFingerprint();
    if (compiledFingerprint.isNotEmpty()
        && state.getProperty (fingerprintProperty).toString() == compiledFingerprint)
        return;

    if (! layoutDebugOverlay.installCompiledDefaultLayout (state))
        return;

    // Le layout embarque contient deja le resultat de toutes ces migrations.
    // Les marquer evite de recalculer et donc de deplacer ses elements.
    state.setProperty ("topLevelTabsLayoutVersion", 2, nullptr);
    state.setProperty ("generatorsTabsLayoutVersion", 2, nullptr);
    state.setProperty ("lfoTabsMirroredFromLfo1Version", 6, nullptr);
    state.setProperty ("lfoTabsInPanelHeaderVersion", 1, nullptr);
    state.setProperty ("lfoStartModeLayoutVersion", 1, nullptr);
    state.setProperty ("motif1MirroredFromMotif2Version", 1, nullptr);
    state.setProperty ("motifLengthsInPatternEditorVersion", 2, nullptr);
    state.setProperty ("harmonyMidiChannelsInHeaderVersion", 1, nullptr);
    state.setProperty ("motifSourceButtonsVersion", 1, nullptr);
    state.setProperty ("motifRegisterButtonsVersion", 1, nullptr);
    state.setProperty ("rhythmModeButtonsVersion", 1, nullptr);
    state.setProperty ("padCompactVoicingLayoutVersion", 4, nullptr);
    state.setProperty ("patternEditorInGeneratorsVersion", 1, nullptr);
    state.setProperty (migrationProperty, currentVersion, nullptr);
    state.setProperty (fingerprintProperty, compiledFingerprint, nullptr);
}

void NdlrAudioProcessorEditor::migrateTabsOutOfParentPanelsIfNeeded()
{
    // Generator Tabs et LFO Tabs sont maintenant des composants racine. Les
    // anciens layouts stockaient leurs positions relativement a deux panneaux
    // parents : reprendre le rectangle global de ces panneaux conserve
    // exactement les deux zones tout en supprimant leur cadre superflu.
    constexpr auto migrationProperty = "topLevelTabsLayoutVersion";
    constexpr auto currentVersion = 2;
    auto& state = owner.getParameters().state;
    const auto previousVersion = static_cast<int> (
        state.getProperty (migrationProperty, 0));
    if (previousVersion >= currentVersion)
        return;

    const auto savedBounds = [&state] (const juce::String& targetName)
    {
        if (const auto savedLayout = state.getChildWithName ("UILayout"); savedLayout.isValid())
            for (auto item : savedLayout)
                if (item["id"].toString() == targetName)
                    return juce::Rectangle<int> {
                        static_cast<int> (item["x"]), static_cast<int> (item["y"]),
                        static_cast<int> (item["w"]), static_cast<int> (item["h"])
                    };
        return juce::Rectangle<int> {};
    };

    auto generatorBounds = savedBounds ("Generators");
    if (generatorBounds.isEmpty() && previousVersion == 0)
    {
        for (const auto& panelName : { juce::String { "Motif 1" },
                                       juce::String { "Motif 2" },
                                       juce::String { "Drone" },
                                       juce::String { "Pad" } })
        {
            const auto panelBounds = savedBounds (panelName);
            if (! panelBounds.isEmpty())
                generatorBounds = generatorBounds.isEmpty()
                    ? panelBounds : generatorBounds.getUnion (panelBounds);
        }
    }
    if (! generatorBounds.isEmpty())
        generatorTabs.setBounds (generatorBounds);

    if (const auto lfoBounds = savedBounds ("LFO Panel"); ! lfoBounds.isEmpty())
        lfoTabs.setBounds (lfoBounds);

    // La V1 appelait layoutLfoTabs() après le redimensionnement du conteneur,
    // écrasant ainsi les positions sauvegardées. Répare uniquement les états
    // passés par cette version fautive ; une première migration conserve, elle,
    // chaque position personnalisée déjà présente dans le projet.
    if (previousVersion == 1)
    {
        for (auto lfo = 0; lfo < 3; ++lfo)
        {
            const auto n = static_cast<size_t> (lfo);
            for (auto& label : modLfoTabLabels[n])
                layoutDebugOverlay.restoreTargetFromCompiledDefault (label);
            for (auto* component : std::array<juce::Component*, 12> {
                     &modLfoShape[n], &modLfoRate[n], &modLfoDivision[n],
                     &modLfoMode[n], &modLfoProbability[n], &modLfoPulseWidth[n],
                     &modLfoLength[n], &modLfoPhase[n],
                     &modLfoAmplitudeSource[n], &modLfoAmplitude[n],
                     &modLfoAmplitudeCcNumber[n], &modLfoAmplitudeCcChannel[n] })
                layoutDebugOverlay.restoreTargetFromCompiledDefault (*component);
        }
        for (auto index = 0; index < 7; ++index)
        {
            const auto n = static_cast<size_t> (index);
            layoutDebugOverlay.restoreTargetFromCompiledDefault (perlinControls[n]);
            layoutDebugOverlay.restoreTargetFromCompiledDefault (perlinLabels[n]);
        }
        for (auto* component : std::array<juce::Component*, 5> {
                 &perlinDisplay, &perlinProfiles, &perlinOn, &perlinM1, &perlinM2 })
            layoutDebugOverlay.restoreTargetFromCompiledDefault (*component);
    }

    state.setProperty ("generatorsTabsLayoutVersion", 2, nullptr);
    state.setProperty ("lfoTabsInPanelHeaderVersion", 1, nullptr);
    state.setProperty (migrationProperty, currentVersion, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::mirrorLfo2And3LayoutFromLfo1IfNeeded()
{
    // Migration ponctuelle : recopie vers LFO 2 et 3 la geometrie, la couleur
    // et les attributs +/- sauvegardes pour LFO 1. Une fois la copie effectuee,
    // chaque onglet redevient une cible de layout independante.
    constexpr auto migrationProperty = "lfoTabsMirroredFromLfo1Version";
    constexpr auto currentVersion = 6;
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= currentVersion)
        return;

    for (auto destination = 1; destination < 3; ++destination)
    {
        const auto n = static_cast<size_t> (destination);
        for (auto label = 0; label < 10; ++label)
            layoutDebugOverlay.copyTargetLayout (
                modLfoTabLabels[0][static_cast<size_t> (label)],
                modLfoTabLabels[n][static_cast<size_t> (label)]);

        layoutDebugOverlay.copyTargetLayout (modLfoShape[0], modLfoShape[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoRate[0], modLfoRate[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoDivision[0], modLfoDivision[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoMode[0], modLfoMode[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoProbability[0], modLfoProbability[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoPulseWidth[0], modLfoPulseWidth[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoLength[0], modLfoLength[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoPhase[0], modLfoPhase[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoAmplitudeSource[0], modLfoAmplitudeSource[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoAmplitude[0], modLfoAmplitude[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoAmplitudeCcNumber[0], modLfoAmplitudeCcNumber[n]);
        layoutDebugOverlay.copyTargetLayout (modLfoAmplitudeCcChannel[0], modLfoAmplitudeCcChannel[n]);
    }

    state.setProperty (migrationProperty, currentVersion, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migrateLfoStartModeControlsIfNeeded()
{
    constexpr auto migrationProperty = "lfoStartModeLayoutVersion";
    constexpr auto currentVersion = 1;
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= currentVersion)
        return;

    for (auto i = 0; i < 3; ++i)
    {
        const auto n = static_cast<size_t> (i);
        auto bounds = modLfoMode[n].getBounds();
        if (bounds.isEmpty())
            bounds = { 48, 72, 112, 24 };

        auto availableWidth = lfoTabPages[n].getWidth() - bounds.getX() - 8;
        const auto rateX = juce::jmin (modLfoRate[n].getX(), modLfoDivision[n].getX());
        if (rateX > bounds.getX())
            availableWidth = juce::jmin (availableWidth, rateX - bounds.getX() - 4);
        bounds.setWidth (juce::jmax (bounds.getWidth(),
            juce::jmin (112, juce::jmax (72, availableWidth))));
        bounds.setHeight (juce::jmax (22, bounds.getHeight()));
        modLfoMode[n].setBounds (bounds);
    }

    state.setProperty (migrationProperty, currentVersion, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::mirrorMotif1LayoutFromMotif2IfNeeded()
{
    // M2 is the reference layout requested by the user. Copy its geometry and
    // +/- attributes once, while preserving each motif's own accent colour.
    // Afterwards both layouts remain independently editable in Layout mode.
    constexpr auto migrationProperty = "motif1MirroredFromMotif2Version";
    constexpr auto currentVersion = 1;
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= currentVersion)
        return;

    const auto copy = [this] (juce::Component& source, juce::Component& destination)
    {
        layoutDebugOverlay.copyTargetLayout (source, destination, false);
    };

    copy (motif2, motif1);
    copy (motif2On, motif1On);
    for (auto index = 0; index < 3; ++index)
        copy (motif2PatternTypeButtons[static_cast<size_t> (index)],
              motif1PatternTypeButtons[static_cast<size_t> (index)]);
    for (auto index = 0; index < 5; ++index)
        copy (motif2RegisterButtons[static_cast<size_t> (index)],
              motif1RegisterButtons[static_cast<size_t> (index)]);

    copy (motif2Humanize, motif1Humanize);
    copy (motif2Accent, motif1Accent);
    copy (motif2Gate, motif1Gate);
    copy (motif2Variation, motif1Variation);
    copy (motif2Velocity, motif1Velocity);
    copy (motif2Channel, motif1Channel);

    copy (motif2PatternTypeLabel, patternTypeLabel);
    copy (motif2PositionLabel, positionLabel);
    copy (motif2HumanizeLabel, humanizeLabel);
    copy (motif2AccentLabel, accentLabel);
    copy (motif2GateLabel, gateLabel);
    copy (motif2VariationLabel, variationLabel);
    copy (motif2VelocityLabel, velocityLabel);
    copy (motif2ChannelLabel, channelLabel);

    state.setProperty (migrationProperty, currentVersion, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migrateMotifLengthsToPatternEditorIfNeeded()
{
    constexpr auto migrationProperty = "motifLengthsInPatternEditorVersion";
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= 2)
        return;

    auto patternArea = patternEditorPanel.getLocalBounds().reduced (10).withTrimmedTop (24);
    patternArea.removeFromTop (28 + 6);
    auto toolbar = patternArea.removeFromTop (28);
    auto placeLength = [] (juce::Rectangle<int> area,
                           juce::Label& label, juce::Slider& control)
    {
        label.setBounds (area.removeFromLeft (76));
        control.setBounds (area.reduced (3, 2));
    };
    const auto sharedLengthWidth = juce::jmin (240, toolbar.getWidth());
    placeLength (toolbar.withSizeKeepingCentre (sharedLengthWidth, toolbar.getHeight()),
                 lengthLabel, motif1Length);

    state.setProperty (migrationProperty, 2, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migrateHarmonyMidiChannelsToHeaderIfNeeded()
{
    constexpr auto migrationProperty = "harmonyMidiChannelsInHeaderVersion";
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= 1)
        return;

    layoutHeader();
    state.setProperty (migrationProperty, 1, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migrateMotifSourceButtonsIfNeeded()
{
    // Les anciens layouts ne contiennent qu'un menu SOURCE. Positionne une
    // seule fois les trois nouveaux segments dans la meme zone, puis enregistre
    // leur geometrie afin qu'ils deviennent independamment editables.
    constexpr auto migrationProperty = "motifSourceButtonsVersion";
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= 1)
        return;

    auto placeButtons = [] (ndlr::ui::SectionPanel& panel,
                            std::array<juce::TextButton, 3>& buttons)
    {
        auto controls = panel.getLocalBounds().reduced (18).withTrimmedTop (30);
        controls.removeFromTop (28 + 5);
        auto patternRow = controls.removeFromTop (52);
        const auto patternRowWidth = patternRow.getWidth();
        patternRow.removeFromLeft (patternRowWidth * 29 / 100);
        auto sourceArea = patternRow.removeFromLeft (patternRowWidth * 42 / 100);
        sourceArea.removeFromTop (18);
        auto buttonArea = sourceArea.reduced (3, 2).withHeight (25);
        const auto buttonWidth = buttonArea.getWidth() / 3;
        buttons[0].setBounds (buttonArea.removeFromLeft (buttonWidth));
        buttons[1].setBounds (buttonArea.removeFromLeft (buttonWidth));
        buttons[2].setBounds (buttonArea);
    };

    placeButtons (motif1, motif1PatternTypeButtons);
    placeButtons (motif2, motif2PatternTypeButtons);
    state.setProperty (migrationProperty, 1, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migrateMotifRegisterButtonsIfNeeded()
{
    // Le premier bouton recupere, via son alias de layout, le rectangle de
    // l'ancien menu POSITION. Ce rectangle est ensuite partage entre les cinq
    // boutons afin de respecter aussi les layouts personnalises existants.
    constexpr auto migrationProperty = "motifRegisterButtonsVersion";
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= 1)
        return;

    auto splitLegacyArea = [] (std::array<juce::TextButton, 5>& buttons)
    {
        auto area = buttons[0].getBounds();
        if (area.isEmpty())
            return;

        const auto buttonWidth = area.getWidth() / 5;
        for (auto index = 0; index < 4; ++index)
            buttons[static_cast<size_t> (index)].setBounds (
                area.removeFromLeft (buttonWidth));
        buttons[4].setBounds (area);
    };

    splitLegacyArea (motif1RegisterButtons);
    splitLegacyArea (motif2RegisterButtons);
    state.setProperty (migrationProperty, 1, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migrateRhythmModeButtonsIfNeeded()
{
    // Reutilise le rectangle de l'ancien menu MODE, restaure via l'alias du
    // bouton NORMAL, puis le partage sans deplacer les autres commandes.
    constexpr auto migrationProperty = "rhythmModeButtonsVersion";
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= 1)
        return;

    auto area = rhythmModeButtons[0].getBounds();
    if (! area.isEmpty())
    {
        const auto normalWidth = area.getWidth() / 2;
        rhythmModeButtons[0].setBounds (area.removeFromLeft (normalWidth));
        rhythmModeButtons[1].setBounds (area);
    }

    state.setProperty (migrationProperty, 1, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migratePadToCompactLayoutIfNeeded()
{
    // Remplace une seule fois la grille historique du Pad, puis installe la
    // rangee des six choix Spread illustres. Les autres sections du layout
    // personnalise restent intactes.
    constexpr auto migrationProperty = "padCompactVoicingLayoutVersion";
    constexpr auto currentVersion = 4;
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= currentVersion)
        return;

    layoutPadControls();
    state.setProperty (migrationProperty, currentVersion, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::migratePatternEditorIntoGeneratorsIfNeeded()
{
    constexpr auto migrationProperty = "patternEditorInGeneratorsVersion";
    constexpr auto currentVersion = 1;
    auto& state = owner.getParameters().state;
    if (static_cast<int> (state.getProperty (migrationProperty, 0)) >= currentVersion)
        return;

    auto savedBounds = [&state] (const juce::String& targetName)
    {
        if (const auto savedLayout = state.getChildWithName ("UILayout"); savedLayout.isValid())
            for (auto item : savedLayout)
                if (item["id"].toString() == targetName)
                    return juce::Rectangle<int> {
                        static_cast<int> (item["x"]), static_cast<int> (item["y"]),
                        static_cast<int> (item["w"]), static_cast<int> (item["h"])
                    };
        return juce::Rectangle<int> {};
    };

    const auto oldPatternBounds = savedBounds ("Pattern Editor");
    const auto oldMatrixBounds = savedBounds ("Mod Matrix 3");
    if (! oldPatternBounds.isEmpty() && ! oldMatrixBounds.isEmpty())
        modMatrix.setBounds (oldPatternBounds.getUnion (oldMatrixBounds));

    auto& selectedPage = generatorTabPages[static_cast<size_t> (selectedMotif)];
    if (patternEditorPanel.getParentComponent() != &selectedPage)
        selectedPage.addAndMakeVisible (patternEditorPanel);
    layoutGeneratorTabs();

    state.setProperty (migrationProperty, currentVersion, nullptr);
    layoutDebugOverlay.saveLayout (state);
}

void NdlrAudioProcessorEditor::updateModMidiCcLayout()
{
    static constexpr int modMidiCompositeWidth = 190;
    static constexpr int modMidiDestinationWidth = 88;
    static constexpr int modMidiCcWidth = 52;
    static constexpr int modMidiChannelWidth = modMidiCompositeWidth
                                             - modMidiDestinationWidth
                                             - modMidiCcWidth;
    auto anyMidiCcDestination = false;
    for (auto i = 0; i < 8; ++i)
    {
        const auto n = static_cast<size_t> (i);
        const auto prefix = "mod.slot" + juce::String (i + 1) + ".";
        const auto midiCcDestination = juce::roundToInt (
            owner.getParameters().getRawParameterValue (prefix + "destination")->load())
                == ndlr::modulation::midiCc;
        const auto midiCcSource = juce::roundToInt (
            owner.getParameters().getRawParameterValue (prefix + "source")->load()) == 6;
        const auto showMidiCcControls = midiCcDestination || midiCcSource;
        anyMidiCcDestination = anyMidiCcDestination || showMidiCcControls;

        // Migration des layouts enregistrés avant l'ajout de ces deux menus.
        // Une géométrie déjà valide appartient à l'utilisateur et reste intacte.
        if (showMidiCcControls
            && (modSlotCcNumber[n].getWidth() < 10 || modSlotCcChannel[n].getWidth() < 10))
        {
            auto destinationBounds = modSlotDestination[n].getBounds();
            destinationBounds.setWidth (modMidiDestinationWidth - 4);
            modSlotDestination[n].setBounds (destinationBounds);
            modSlotCcNumber[n].setBounds (destinationBounds.getRight() + 4,
                                          destinationBounds.getY(), modMidiCcWidth - 4,
                                          destinationBounds.getHeight());
            modSlotCcChannel[n].setBounds (destinationBounds.getRight() + modMidiCcWidth,
                                           destinationBounds.getY(), modMidiChannelWidth - 4,
                                           destinationBounds.getHeight());
        }
        modSlotCcNumber[n].setVisible (showMidiCcControls);
        modSlotCcChannel[n].setVisible (showMidiCcControls);
    }

    if (anyMidiCcDestination
        && (modCcNumberLabel.getWidth() < 10 || modCcChannelLabel.getWidth() < 10))
    {
        auto destinationHeader = modSlotLabels[2].getBounds();
        destinationHeader.setWidth (modMidiDestinationWidth);
        modSlotLabels[2].setBounds (destinationHeader);
        modCcNumberLabel.setBounds (destinationHeader.getRight(), destinationHeader.getY(),
                                    modMidiCcWidth, destinationHeader.getHeight());
        modCcChannelLabel.setBounds (destinationHeader.getRight() + modMidiCcWidth,
                                     destinationHeader.getY(), modMidiChannelWidth,
                                     destinationHeader.getHeight());
    }
    modCcNumberLabel.setVisible (anyMidiCcDestination);
    modCcChannelLabel.setVisible (anyMidiCcDestination);
}

void NdlrAudioProcessorEditor::selectMotif (int motifIndex)
{
    selectedMotif = juce::jlimit (0, 1, motifIndex);
    if (generatorTabs.getNumTabs() >= 2
        && generatorTabs.getCurrentTabIndex() != selectedMotif)
        generatorTabs.setCurrentTabIndex (selectedMotif, false);
    auto& motifPage = generatorTabPages[static_cast<size_t> (selectedMotif)];
    if (patternEditorPanel.getParentComponent() != &motifPage)
        motifPage.addAndMakeVisible (patternEditorPanel);
    const auto selectedPrefix = selectedMotif == 0 ? juce::String { "motif1." } : juce::String { "motif2." };
    previousRhythmMode[static_cast<size_t> (selectedMotif)] = juce::roundToInt (
        owner.getParameters().getRawParameterValue (selectedPrefix + "rhythmMode")->load());
    const auto accent = selectedMotif == 0 ? ndlr::ui::cyan : ndlr::ui::orange;
    patternEditorPanel.setAccentColour (accent);
    rhythmEditor.setAccentColour (accent);
    patternDisplay.setAccentColour (accent);
    rhythmWheel.setAccentColour (accent);
    for (auto* component : std::array<juce::Component*, 8> {
             &patternSelector, &patternSave,
             &rhythmUserSlot, &rhythmSave, &rhythmLoad,
             &motif1RhythmLength, &motif1EuclideanPulses, &motif1Rotation })
        colourEditable (*component, accent);
    for (auto& button : rhythmModeButtons)
        colourSelectableButton (button, accent);
    for (auto* button : { &patternSave, &rhythmSave, &rhythmLoad })
        colourActionButton (*button, accent);
    colourEditable (motif1Length, accent);
    lengthLabel.setColour (juce::Label::textColourId, accent);

    displayedUsedMotif = -1;

    // Les contrôles du Pattern Editor et ceux placés à droite de la roue sont
    // partagés visuellement, mais leurs attachments suivent le motif sélectionné.
    if (motif1RhythmLengthAttachment != nullptr)
    {
        auto& parameters = owner.getParameters();
        const juce::String prefix = selectedMotif == 0 ? "motif1." : "motif2.";
        patternSelectorAttachment.reset();
        motif1LengthAttachment.reset();
        motif1RhythmLengthAttachment.reset();
        motif1EuclideanPulsesAttachment.reset();
        motif1RotationAttachment.reset();
        updatingPatternSelector = true;
        patternSelectorAttachment = std::make_unique<ComboAttachment> (
            parameters, prefix + "pattern", patternSelector);
        updatingPatternSelector = false;
        motif1LengthAttachment = std::make_unique<SliderAttachment> (
            parameters, prefix + "length", motif1Length);
        motif1RhythmLengthAttachment = std::make_unique<SliderAttachment> (parameters, prefix + "rhythmLength", motif1RhythmLength);
        motif1EuclideanPulsesAttachment = std::make_unique<SliderAttachment> (parameters, prefix + "euclideanPulses", motif1EuclideanPulses);
        motif1RotationAttachment = std::make_unique<SliderAttachment> (parameters, prefix + "rotation", motif1Rotation);
    }
}

void NdlrAudioProcessorEditor::timerCallback()
{
    // The tab bar sends asynchronous change notifications. Deriving the motif
    // from the visible tab here prevents a delayed/missed notification from
    // ever leaving the shared editors attached to the other motif.
    if (const auto visibleTab = generatorTabs.getCurrentTabIndex();
        (visibleTab == 0 || visibleTab == 1) && visibleTab != selectedMotif)
        selectMotif (visibleTab);

    // Le timer tourne sur le thread UI. Il lit uniquement les valeurs atomiques
    // publiées par le moteur audio et rafraîchit l'affichage sans bloquer l'audio.
    const auto& engine = owner.getEngine();
    auto& parameters = owner.getParameters();
    const juce::String editorPrefix = selectedMotif == 0 ? "motif1." : "motif2.";
    const auto modValue = [this] (int destination, int baseValue)
    {
        return owner.isDestinationModulated (destination)
            ? owner.getModulatedDestinationValue (destination) : baseValue;
    };
    const auto rawInt = [&parameters] (const juce::String& id)
    { return juce::roundToInt (parameters.getRawParameterValue (id)->load()); };
    const auto showSlider = [&] (juce::Slider& slider, int destination, const juce::String& id)
    { if (! slider.isMouseButtonDown()) slider.setValue (modValue (destination, rawInt (id)), juce::dontSendNotification); };
    const auto showCombo = [&] (juce::ComboBox& combo, int destination, const juce::String& id, int modOffset = 0, int baseOffset = 0)
    { if (! combo.isPopupActive()) combo.setSelectedItemIndex (modValue (destination, rawInt (id) + baseOffset) + modOffset, juce::dontSendNotification); };
    const auto showToggle = [&] (juce::ToggleButton& toggle, int destination, const juce::String& id)
    { toggle.setToggleState (modValue (destination, rawInt (id)) != 0, juce::dontSendNotification); };
    const auto showSourceButtons = [&rawInt] (std::array<juce::TextButton, 3>& buttons,
                                               const juce::String& id)
    {
        const auto selected = juce::jlimit (0, 2, rawInt (id));
        for (auto index = 0; index < 3; ++index)
            buttons[static_cast<size_t> (index)].setToggleState (
                index == selected, juce::dontSendNotification);
    };
    const auto showRegisterButtons = [&] (std::array<juce::TextButton, 5>& buttons,
                                           int destination, const juce::String& id)
    {
        const auto selected = juce::jlimit (0, 4,
                                            modValue (destination, rawInt (id)));
        for (auto index = 0; index < 5; ++index)
            buttons[static_cast<size_t> (index)].setToggleState (
                index == selected, juce::dontSendNotification);
    };

    showCombo (harmonyScale, 2, "harmony.scale");
    showCombo (harmonyType, 4, "harmony.type");
    showSourceButtons (motif1PatternTypeButtons, "motif1.patternType");
    showSourceButtons (motif2PatternTypeButtons, "motif2.patternType");
    showToggle (padOn, ndlr::modulation::padOn, "pad.on");
    showSlider (padPosition, ndlr::modulation::padPosition, "pad.position");
    showSlider (padRange, ndlr::modulation::padRange, "pad.range");
    showSlider (padSpread, ndlr::modulation::padSpread, "pad.spread");
    const auto visibleSpread = juce::jlimit (1, 6, juce::roundToInt (padSpread.getValue()));
    for (auto index = 0; index < 6; ++index)
        padSpreadButtons[static_cast<size_t> (index)].setToggleState (
            index + 1 == visibleSpread, juce::dontSendNotification);
    showCombo (padStrumDivision, ndlr::modulation::padStrum, "pad.strumDivision");
    showSlider (padVelocity, ndlr::modulation::padVelocity, "pad.velocity");
    showToggle (droneOn, ndlr::modulation::droneOn, "drone.on");
    showCombo (dronePosition, ndlr::modulation::dronePosition, "drone.position");
    showCombo (droneType, ndlr::modulation::droneType, "drone.type");
    showCombo (droneTrigger, ndlr::modulation::droneTrigger, "drone.trigger", -1);
    showSlider (droneVelocity, ndlr::modulation::droneVelocity, "drone.velocity");
    showToggle (motif1On, ndlr::modulation::motif1On, "motif1.on");
    showRegisterButtons (motif1RegisterButtons, ndlr::modulation::motif1Position,
                         "motif1.position");
    showCombo (patternSelector,
               selectedMotif == 0 ? ndlr::modulation::motif1Pattern
                                  : ndlr::modulation::motif2Pattern,
               editorPrefix + "pattern");
    showSlider (motif1Length,
                selectedMotif == 0 ? ndlr::modulation::motif1Length
                                   : ndlr::modulation::motif2Length,
                editorPrefix + "length");
    showCombo (motif1Variation, ndlr::modulation::motif1Variation, "motif1.variation");
    showSlider (motif1Velocity, ndlr::modulation::motif1Velocity, "motif1.velocity");
    showSlider (motif1Gate, ndlr::modulation::motif1Gate, "motif1.gate");
    showCombo (motif1Accent, ndlr::modulation::motif1Accent, "motif1.accent", -1);
    showToggle (motif2On, ndlr::modulation::motif2On, "motif2.on");
    showRegisterButtons (motif2RegisterButtons, ndlr::modulation::motif2Position,
                         "motif2.position");
    showCombo (motif2Variation, ndlr::modulation::motif2Variation, "motif2.variation");
    showSlider (motif2Velocity, ndlr::modulation::motif2Velocity, "motif2.velocity");
    showSlider (motif2Gate, ndlr::modulation::motif2Gate, "motif2.gate");
    showCombo (motif2Accent, ndlr::modulation::motif2Accent, "motif2.accent", -1);
    showSlider (perlinControls[0], ndlr::modulation::perlinZoom, "perlin.zoom");
    showSlider (perlinControls[1], ndlr::modulation::perlinSpacing, "perlin.spacing");
    showSlider (perlinControls[2], ndlr::modulation::perlinRoughness, "perlin.roughness");
    showSlider (perlinControls[3], ndlr::modulation::perlinPersistence, "perlin.persistence");
    showSlider (perlinControls[4], ndlr::modulation::perlinSeed, "perlin.seed");
    showSlider (perlinControls[5], ndlr::modulation::perlinResolution, "perlin.resolution");
    showSlider (perlinControls[6], ndlr::modulation::perlinBrightness, "perlin.brightness");
    for (auto i = 0; i < 3; ++i)
    {
        const auto prefix = "mod.lfo" + juce::String (i + 1) + ".";
        const auto synced = parameters.getRawParameterValue (prefix + "sync")->load() >= 0.5f;
        const auto retrigger = parameters.getRawParameterValue (prefix + "retrigger")->load() >= 0.5f;
        const auto shape = juce::roundToInt (
            parameters.getRawParameterValue (prefix + "shape")->load());
        const auto pulse = shape == 5;
        const auto pattern = shape >= 7;
        modLfoMode[static_cast<size_t> (i)].setSelectedItemIndex (
            synced ? 2 : retrigger ? 1 : 0, juce::dontSendNotification);
        modLfoRate[static_cast<size_t> (i)].setVisible (! synced);
        modLfoDivision[static_cast<size_t> (i)].setVisible (synced);
        modLfoPulseWidth[static_cast<size_t> (i)].setVisible (! pattern);
        modLfoPulseWidth[static_cast<size_t> (i)].setEnabled (pulse);
        modLfoLength[static_cast<size_t> (i)].setVisible (pattern);
        modLfoTabLabels[static_cast<size_t> (i)][4].setText (
            pattern ? "LENGTH" : "PULSE WIDTH %", juce::dontSendNotification);
        const auto amplitudeSource = rawInt (prefix + "amplitudeSource");
        const auto midiCcAmplitudeSource = amplitudeSource == 6;
        const auto channelledAmplitudeSource = amplitudeSource >= 5 && amplitudeSource <= 7;
        modLfoPhase[static_cast<size_t> (i)].setEnabled (
            amplitudeSource >= 1 && amplitudeSource <= 3
                && amplitudeSource != i + 1);
        modLfoAmplitude[static_cast<size_t> (i)].setEnabled (
            amplitudeSource != 0 && amplitudeSource != i + 1);
        modLfoAmplitudeCcNumber[static_cast<size_t> (i)].setVisible (midiCcAmplitudeSource);
        modLfoAmplitudeCcChannel[static_cast<size_t> (i)].setVisible (channelledAmplitudeSource);
        modLfoTabLabels[static_cast<size_t> (i)][7].setVisible (midiCcAmplitudeSource);
        modLfoTabLabels[static_cast<size_t> (i)][8].setVisible (channelledAmplitudeSource);
    }
    const auto perlinIsOn = rawInt ("perlin.on") != 0;
    const auto perlinUsesM1 = perlinIsOn && rawInt ("perlin.m1") != 0;
    const auto perlinUsesM2 = perlinIsOn && rawInt ("perlin.m2") != 0;
    perlinDisplay.setParameters (juce::roundToInt (perlinControls[4].getValue()),
                                 rawInt ("perlin.x"), rawInt ("perlin.y"),
                                 juce::roundToInt (perlinControls[0].getValue()),
                                 juce::roundToInt (perlinControls[1].getValue()),
                                 juce::roundToInt (perlinControls[2].getValue()),
                                 juce::roundToInt (perlinControls[3].getValue()),
                                 juce::roundToInt (perlinControls[5].getValue()),
                                 juce::roundToInt (perlinControls[6].getValue()));
    perlinDisplay.setProfiles (rawInt ("motif1.length"), rawInt ("motif1.patternType"), perlinUsesM1,
                               rawInt ("motif2.length"), rawInt ("motif2.patternType"), perlinUsesM2);
    std::array<int, 16> perlinProfile1 {}, perlinProfile2 {};
    for (auto step = 0; step < 16; ++step)
    {
        perlinProfile1[static_cast<size_t> (step)] = perlinDisplay.getPatternValue (0, step);
        perlinProfile2[static_cast<size_t> (step)] = perlinDisplay.getPatternValue (1, step);
    }
    const auto perlinType1 = rawInt ("motif1.patternType");
    const auto perlinType2 = rawInt ("motif2.patternType");
    perlinProfiles.setProfiles (perlinProfile1, rawInt ("motif1.length"),
                                perlinType1 == 0 ? 20 : perlinType1 == 1 ? 40 : 60, perlinUsesM1,
                                perlinProfile2, rawInt ("motif2.length"),
                                perlinType2 == 0 ? 20 : perlinType2 == 1 ? 40 : 60, perlinUsesM2);
    auto modulationLayoutNeedsRefresh = false;
    for (auto i = 0; i < 8; ++i)
    {
        const auto n = static_cast<size_t> (i);
        const auto prefix = "mod.slot" + juce::String (i + 1) + ".";
        const auto source = rawInt (prefix + "source");
        if (source >= 1 && source <= 3)
        {
            const auto restartCount = owner.getLfoRestartCount (source - 1);
            if (modScopeLastLfoSource[n] == source
                && modScopeLastRestartCount[n] != restartCount)
                modSlotScope[n].markRestart();
            modScopeLastLfoSource[n] = source;
            modScopeLastRestartCount[n] = restartCount;
        }
        else
        {
            modScopeLastLfoSource[n] = 0;
            modScopeLastRestartCount[n] = 0;
        }
        std::array<float, 16> patternValues {};
        auto patternLength = 16;
        if (owner.getModulationPatternValues (i, patternValues, patternLength))
            modSlotScope[n].setPattern (patternValues, patternLength);
        else
        {
            modSlotScope[n].clearPattern();
            const auto syncedLfoSource = source >= 1 && source <= 3
                && rawInt ("mod.lfo" + juce::String (source) + ".sync") != 0;
            if (! syncedLfoSource || owner.isModulationTransportRunning())
                modSlotScope[n].pushValue (owner.getModulationValue (i));
        }
        const auto randomLfoSource = source >= 1 && source <= 3
            && rawInt ("mod.lfo" + juce::String (source) + ".shape") == 6;
        modSlotScope[n].setStepped (randomLfoSource);
        const auto midiCcDestination = rawInt (prefix + "destination")
                                     == ndlr::modulation::midiCc;
        const auto midiCcSource = source == 6;
        const auto showMidiCcControls = midiCcDestination || midiCcSource;
        modulationLayoutNeedsRefresh = modulationLayoutNeedsRefresh
            || modSlotCcNumber[n].isVisible() != showMidiCcControls
            || modSlotCcChannel[n].isVisible() != showMidiCcControls;
        if (! modSlotRange[n].isMouseButtonDown())
        {
            const auto savedLow = rawInt (prefix + "low");
            const auto savedHigh = rawInt (prefix + "high");
            modSlotRange[n].setMinAndMaxValues (juce::jmin (savedLow, savedHigh),
                                                juce::jmax (savedLow, savedHigh),
                                                juce::dontSendNotification);
        }
    }
    if (modulationLayoutNeedsRefresh)
        resized();
    const auto key = modValue (1, rawInt ("harmony.root"));
    const auto degree = modValue (3, rawInt ("harmony.degree") - 1);
    harmonyWheel.setSelection (key, degree);
    harmonyWheel.setHarmonyNames (harmonyScale.getText(), harmonyType.getText());
    auto rhythmLength = juce::roundToInt (parameters.getRawParameterValue (editorPrefix + "rhythmLength")->load());
    const auto rawRhythmMode = rawInt (editorPrefix + "rhythmMode");
    auto rhythmMode = rawRhythmMode;
    auto pulsesValue = rawInt (editorPrefix + "euclideanPulses");
    auto signedRotationValue = rawInt (editorPrefix + "rotation");
    auto displayedDivision = modValue (selectedMotif == 0 ? ndlr::modulation::motif1Division
                                                          : ndlr::modulation::motif2Division,
                                       rawInt (editorPrefix + "division"));
    auto displayedRatchetVelocityMode = rawInt (editorPrefix + "ratchetVelocityMode");
    std::array<int, 32> rhythmStates {};
    std::array<int, 32> rhythmVelocities {};
    std::array<int, 32> rhythmRatchets {};
    for (auto step = 0; step < 32; ++step)
    {
        rhythmStates[static_cast<size_t> (step)] = juce::roundToInt (
            parameters.getRawParameterValue (editorPrefix + "rhythmState" + juce::String (step + 1))->load());
        rhythmVelocities[static_cast<size_t> (step)] = juce::roundToInt (
            parameters.getRawParameterValue (editorPrefix + "rhythmVelocity" + juce::String (step + 1))->load());
        rhythmRatchets[static_cast<size_t> (step)] = juce::roundToInt (
            parameters.getRawParameterValue (editorPrefix + "rhythmRatchet" + juce::String (step + 1))->load());
    }
    const auto rhythmDestination = selectedMotif == 0 ? ndlr::modulation::motif1Rhythm
                                                      : ndlr::modulation::motif2Rhythm;
    if (owner.isDestinationModulated (rhythmDestination))
    {
        NdlrAudioProcessor::RhythmMemory memory;
        if (owner.getRhythmMemory (owner.getModulatedDestinationValue (rhythmDestination), memory))
        {
            rhythmLength = memory.length; rhythmMode = memory.mode; pulsesValue = memory.pulses;
            signedRotationValue = memory.rotation; displayedDivision = memory.division;
            displayedRatchetVelocityMode = memory.ratchetVelocityMode;
            rhythmStates = memory.state; rhythmVelocities = memory.velocity; rhythmRatchets = memory.ratchet;
        }
    }
    rhythmMode = juce::jlimit (0, 1, rhythmMode);
    for (auto index = 0; index < 2; ++index)
        rhythmModeButtons[static_cast<size_t> (index)].setToggleState (
            index == rhythmMode, juce::dontSendNotification);
    previousRhythmMode[static_cast<size_t> (selectedMotif)] = juce::jlimit (0, 1, rawRhythmMode);
    if (! motif1RhythmLength.isMouseButtonDown()) motif1RhythmLength.setValue (rhythmLength, juce::dontSendNotification);
    if (! motif1EuclideanPulses.isMouseButtonDown()) motif1EuclideanPulses.setValue (pulsesValue, juce::dontSendNotification);
    if (! motif1Rotation.isMouseButtonDown()) motif1Rotation.setValue (signedRotationValue, juce::dontSendNotification);
    const auto pulses = juce::jlimit (1, rhythmLength, pulsesValue);
    const auto signedRotation = juce::jlimit (-(rhythmLength - 1), rhythmLength - 1, signedRotationValue);
    const auto rotation = (((-signedRotation) % rhythmLength) + rhythmLength) % rhythmLength;

    auto displayedStates = rhythmStates;
    auto displayedVelocities = rhythmVelocities;
    auto displayedRatchets = rhythmRatchets;
    if (rhythmMode == 1)
    {
        displayedStates = ndlr::rhythm::euclidean (rhythmLength, pulses, rotation);
    }
    else
    {
        for (auto step = 0; step < rhythmLength; ++step)
        {
            const auto source = (step + rotation) % rhythmLength;
            displayedStates[static_cast<size_t> (step)] = rhythmStates[static_cast<size_t> (source)];
            displayedVelocities[static_cast<size_t> (step)] = rhythmVelocities[static_cast<size_t> (source)];
            displayedRatchets[static_cast<size_t> (step)] = rhythmRatchets[static_cast<size_t> (source)];
        }
    }
    rhythmWheel.setLength (rhythmLength);
    rhythmWheel.setStates (displayedStates);
    rhythmWheel.setVelocities (displayedVelocities);
    rhythmWheel.setRatchets (displayedRatchets);
    rhythmWheel.setPlayhead (selectedMotif == 0 ? engine.getMotif1RhythmStep()
                                                : engine.getMotif2RhythmStep());
    static const juce::StringArray divisionLabels { "1nd","1n","1nt","2nd","2n","2nt","4nd","4n","4nt","8nd","8n","8nt","16nd","16n","16nt","32nd","32n","32nt","64nd","64n","128n" };
    rhythmWheel.setDivisionLabel (divisionLabels[juce::jlimit (0, 20, displayedDivision)]);
    rhythmWheel.setRatchetVelocityMode (displayedRatchetVelocityMode);

    std::array<bool, 20> usedSlots {};
    for (auto slot = 0; slot < 20; ++slot)
    {
        auto used = parameters.getRawParameterValue (
            "pattern.user" + juce::String (slot + 1) + ".used")->load() >= 0.5f;
        // Compatibilité avec les projets sauvegardés avant l'ajout du flag
        // « used » : un contenu différent du pattern initial compte aussi.
        for (auto step = 0; step < 16 && ! used; ++step)
            used = juce::roundToInt (parameters.getRawParameterValue (
                "pattern.user" + juce::String (slot + 1)
                + ".step" + juce::String (step + 1))->load())
                != NdlrEngine::motifPatternValue (0, step);
        usedSlots[static_cast<size_t> (slot)] = used;
    }
    if (displayedUsedMotif != selectedMotif || usedSlots != displayedUsedSlots)
    {
        for (auto slot = 0; slot < 20; ++slot)
            patternSelector.changeItemText (
                slot + 21,
                (usedSlots[static_cast<size_t> (slot)] ? "[USED] User " : "[----] User ")
                    + juce::String (slot + 21));
        displayedUsedSlots = usedSlots;
        displayedUsedMotif = selectedMotif;
    }

    std::array<bool, 40> rhythmUsedSlots {};
    const auto rhythmMemories = parameters.state.getChildWithName ("RhythmMemories");
    for (auto slot = 0; slot < 40; ++slot)
    {
        const auto slotNumber = slot + 1;
        auto memory = rhythmMemories.getChildWithName ("User" + juce::String (slotNumber));
        auto used = memory.isValid() && static_cast<bool> (memory.getProperty ("used", false));
        if (! used && slotNumber >= 21)
        {
            memory = rhythmMemories.getChildWithName ("M1User" + juce::String (slotNumber));
            used = memory.isValid() && static_cast<bool> (memory.getProperty ("used", false));
            if (! used)
            {
                memory = rhythmMemories.getChildWithName ("M2User" + juce::String (slotNumber));
                used = memory.isValid() && static_cast<bool> (memory.getProperty ("used", false));
            }
        }
        rhythmUsedSlots[static_cast<size_t> (slot)] = used;
    }
    if (! displayedRhythmSlotsValid || rhythmUsedSlots != displayedRhythmUsedSlots)
    {
        const auto selectedSlot = juce::jmax (1, rhythmUserSlot.getSelectedId());
        rhythmUserSlot.clear (juce::dontSendNotification);
        for (auto slot = 0; slot < 40; ++slot)
            rhythmUserSlot.addItem ((rhythmUsedSlots[static_cast<size_t> (slot)]
                                         ? "[USED] User " : "[----] User ")
                                    + juce::String (slot + 1), slot + 1);
        rhythmUserSlot.setSelectedId (selectedSlot, juce::dontSendNotification);
        displayedRhythmUsedSlots = rhythmUsedSlots;
        displayedRhythmSlotsValid = true;
    }
    const auto selectedRhythmDestination = selectedMotif == 0 ? ndlr::modulation::motif1Rhythm
                                                              : ndlr::modulation::motif2Rhythm;
    if (owner.isDestinationModulated (selectedRhythmDestination))
        rhythmUserSlot.setSelectedId (owner.getModulatedDestinationValue (selectedRhythmDestination),
                                      juce::dontSendNotification);

    std::array<int, 16> patternValues {};
    const auto patternIndex = modValue (selectedMotif == 0 ? ndlr::modulation::motif1Pattern
                                                           : ndlr::modulation::motif2Pattern,
                                        rawInt (editorPrefix + "pattern"));
    const auto patternLength = modValue (selectedMotif == 0 ? ndlr::modulation::motif1Length
                                                            : ndlr::modulation::motif2Length,
                                         rawInt (editorPrefix + "length"));
    const auto patternType = juce::roundToInt (parameters.getRawParameterValue (editorPrefix + "patternType")->load());
    const auto editSource = juce::roundToInt (
        parameters.getRawParameterValue (editorPrefix + "customPatternSource")->load()) - 1;
    const auto editBuffer = patternIndex < 20
        && parameters.getRawParameterValue (editorPrefix + "customPattern")->load() >= 0.5f
        && editSource == patternIndex;
    const auto customPattern = patternIndex >= 20;
    const auto selectedUserSlot = juce::jlimit (0, 19, patternIndex - 20);
    const auto selectedPerlinIsActive = selectedMotif == 0 ? perlinUsesM1 : perlinUsesM2;
    for (auto step = 0; step < 16; ++step)
        patternValues[static_cast<size_t> (step)] = selectedPerlinIsActive
            ? perlinDisplay.getPatternValue (selectedMotif, step)
            : editBuffer
                ? juce::roundToInt (parameters.getRawParameterValue (
                    editorPrefix + "patternStep" + juce::String (step + 1))->load())
                : customPattern
                    ? juce::roundToInt (parameters.getRawParameterValue (
                        "pattern.user" + juce::String (selectedUserSlot + 1)
                        + ".step" + juce::String (step + 1))->load())
                    : NdlrEngine::motifPatternValue (juce::jlimit (0, 19, patternIndex), step);
    const auto patternAmplitude = modValue (
        selectedMotif == 0 ? ndlr::modulation::motif1ActivePattern
                           : ndlr::modulation::motif2ActivePattern,
        50);
    const auto patternMaximum = patternType == 0 ? 20 : patternType == 1 ? 40 : 60;
    for (auto& value : patternValues)
        value = NdlrEngine::applyPatternAmplitude (value, patternAmplitude, patternMaximum);
    patternDisplay.setPattern (patternValues, patternLength,
                               patternMaximum,
                               selectedMotif == 0 ? engine.getMotif1Step()
                                                  : engine.getMotif2Step());

    NdlrEngine::HarmonySettings previewHarmony {
        modValue (ndlr::modulation::harmonyKey, rawInt ("harmony.root")),
        modValue (ndlr::modulation::harmonyMode, rawInt ("harmony.scale")),
        modValue (ndlr::modulation::harmonyDegree,
                  rawInt ("harmony.degree") - 1),
        modValue (ndlr::modulation::harmonyChordType, rawInt ("harmony.type")),
        0
    };
    NdlrEngine::PadSettings previewPad;
    previewPad.position = modValue (ndlr::modulation::padPosition,
                                    rawInt ("pad.position"));
    previewPad.range = modValue (ndlr::modulation::padRange,
                                 rawInt ("pad.range"));
    previewPad.spread = modValue (ndlr::modulation::padSpread,
                                  rawInt ("pad.spread"));
    previewPad.inversionMode = juce::jlimit (0, 3, rawInt ("pad.inversionMode"));
    auto preview = NdlrEngine::calculatePadVoicing (previewHarmony, previewPad);
    const auto previewTracksPreviousVoicing = padOn.getToggleState()
        && engine.isTransportRunning();
    if (previewPad.inversionMode == 1 || previewPad.inversionMode == 2)
        preview.notes = NdlrEngine::rotatePadVoicing (
            preview.notes, preview.noteCount, previewPad.inversionMode);
    else if (previewPad.inversionMode == 3
             && previewTracksPreviousVoicing && padPreviewNoteCount > 0)
        preview.notes = NdlrEngine::minimisePadVoiceMovement (
            preview.notes, preview.noteCount, padPreviewNotes, padPreviewNoteCount);
    if (previewTracksPreviousVoicing)
    {
        padPreviewNotes = preview.notes;
        padPreviewNoteCount = preview.noteCount;
    }
    else
        padPreviewNoteCount = 0;
    static const std::array<juce::String, 12> keyNames {
        "C", "G", "D", "A", "E", "B", "F#", "Db", "Ab", "Eb", "Bb", "F"
    };
    const auto harmonyName = keyNames[static_cast<size_t> (
        juce::jlimit (0, 11, previewHarmony.root))]
        + " / " + harmonyScale.getText()
        + " / " + harmonyType.getText()
        + " / degree " + juce::String (previewHarmony.degree + 1);
    ndlr::ui::PadVoicingControlState compactControls;
    compactControls.enabled = padOn.getToggleState();
    compactControls.strum = padStrum.getToggleState();
    compactControls.group = padGroup.getToggleState();
    compactControls.inversionMode = padInversionMode.getText();
    compactControls.velocity = modValue (ndlr::modulation::padVelocity,
                                         rawInt ("pad.velocity"));
    compactControls.midiChannel = rawInt ("pad.channel");
    compactControls.polyChain = rawInt ("pad.polyChain");
    compactControls.strumDivision = padStrumDivision.getText();
    compactControls.quantize = padQuantize.getText();
    padVoicingMiniView.setData (preview, previewPad.position,
                                previewPad.range, previewPad.spread, harmonyName,
                                std::move (compactControls));

    // Construction de la ligne BPM / transport / position courante de Motif 1.
    auto statusText = juce::String (engine.getTempo(), 1) + " BPM  |  "
                    + (engine.isTransportRunning() ? "PLAY" : "STOP");

    if (const auto step = engine.getMotif1Step(); step >= 0)
        statusText += "  |  M1 " + juce::String (step + 1).paddedLeft ('0', 2)
                    + "/" + juce::String (engine.getMotif1Length()).paddedLeft ('0', 2);

    if (const auto step = engine.getMotif2Step(); step >= 0)
        statusText += "  |  M2 " + juce::String (step + 1).paddedLeft ('0', 2)
                    + "/" + juce::String (engine.getMotif2Length()).paddedLeft ('0', 2);

    status.setText (statusText, juce::dontSendNotification);
}
