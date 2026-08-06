#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NdlrLookAndFeel.h"
#include "UI/HarmonyWheel.h"
#include "UI/RhythmWheel.h"
#include "UI/PatternDisplay.h"
#include "UI/PadVoicingPreview.h"
#include "UI/LayoutDebugOverlay.h"
#include "UI/ModulationScope.h"
#include "UI/PerlinDisplay.h"

class NdlrAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer,
                                       private juce::ChangeListener
{
public:
    explicit NdlrAudioProcessorEditor (NdlrAudioProcessor& processorToEdit);
    ~NdlrAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void layoutHeader();
    void layoutGeneratorTabs();
    void ensureGeneratorContainersAreVisible();
    void layoutMotifControls (int motifIndex);
    void layoutPatternEditorControls();
    void layoutDroneControls();
    void layoutPadControls();
    void updateModMidiCcLayout();
    void layoutLfoTabs();
    void migrateToCompiledDefaultLayoutIfNeeded();
    void migrateTabsOutOfParentPanelsIfNeeded();
    void mirrorLfo2And3LayoutFromLfo1IfNeeded();
    void migrateLfoStartModeControlsIfNeeded();
    void mirrorMotif1LayoutFromMotif2IfNeeded();
    void migrateMotifLengthsToPatternEditorIfNeeded();
    void migrateHarmonyMidiChannelsToHeaderIfNeeded();
    void migrateMotifSourceButtonsIfNeeded();
    void migrateMotifRegisterButtonsIfNeeded();
    void migrateRhythmModeButtonsIfNeeded();
    void migratePadToCompactLayoutIfNeeded();
    void migratePatternEditorIntoGeneratorsIfNeeded();

    NdlrAudioProcessor& owner;
    ndlr::ui::LookAndFeel ndlrLookAndFeel;
    juce::Label title;
    juce::Label status;
    juce::ToggleButton transportPlay { "PLAY" };
    juce::Slider transportTempo;
    juce::TextButton panicButton { "ALL NOTES OFF" };
    juce::ToggleButton layoutDebugButton { "LAYOUT" };
    ndlr::ui::LayoutDebugOverlay layoutDebugOverlay;
    juce::Label harmonyText;
    ndlr::ui::HarmonyWheel harmonyWheel;
    juce::Label placeholderText;
    juce::ToggleButton droneOn { "ON" };
    juce::ComboBox dronePosition, droneType, droneTrigger, droneChannel;
    juce::Slider droneVelocity;
    juce::Label dronePositionLabel, droneTypeLabel, droneTriggerLabel;
    juce::Label droneVelocityLabel, droneChannelLabel;
    juce::ToggleButton padOn { "ON" }, padStrum { "STRUM" }, padGroup { "GROUP" };
    juce::ToggleButton padInvert;
    juce::Slider padPosition, padRange, padSpread, padVelocity, padPolyChain;
    juce::ComboBox padStrumDivision, padChannel, padQuantize, padInversionMode;
    juce::Label padPositionLabel, padRangeLabel, padSpreadLabel, padVelocityLabel;
    juce::Label padChannelLabel, padPolyLabel, padDivisionLabel, padQuantizeLabel;
    ndlr::ui::PadVoicingMiniView padVoicingMiniView;
    std::array<int, 22> padPreviewNotes {};
    int padPreviewNoteCount = 0;
    std::array<ndlr::ui::PadSpreadButton, 6> padSpreadButtons {
        ndlr::ui::PadSpreadButton { 1 }, ndlr::ui::PadSpreadButton { 2 },
        ndlr::ui::PadSpreadButton { 3 }, ndlr::ui::PadSpreadButton { 4 },
        ndlr::ui::PadSpreadButton { 5 }, ndlr::ui::PadSpreadButton { 6 }
    };
    std::array<juce::ComboBox,3> modLfoShape, modLfoDivision;
    std::array<juce::ComboBox,3> modLfoMode, modLfoAmplitudeSource;
    std::array<juce::ComboBox,3> modLfoAmplitudeCcNumber, modLfoAmplitudeCcChannel;
    std::array<juce::Slider,3> modLfoRate, modLfoProbability, modLfoPulseWidth,
                               modLfoLength, modLfoPhase, modLfoAmplitude;
    std::array<juce::ToggleButton,3> modLfoSync, modLfoRetrigger;
    std::array<juce::ToggleButton,8> modSlotOn;
    std::array<juce::ComboBox,8> modSlotSource, modSlotDestination;
    std::array<juce::ComboBox,8> modSlotCcNumber, modSlotCcChannel;
    std::array<juce::Slider,8> modSlotAmount, modSlotRange;
    std::array<ndlr::ui::ModulationScope,8> modSlotScope;
    std::array<uint64_t, 8> modScopeLastRestartCount {};
    std::array<int, 8> modScopeLastLfoSource {};
    std::array<std::array<juce::Label, 10>, 3> modLfoTabLabels;
    std::array<juce::Label,6> modSlotLabels;
    juce::Label modScopeLabel, modCcNumberLabel, modCcChannelLabel;
    ndlr::ui::PerlinDisplay perlinDisplay;
    ndlr::ui::PerlinProfiles perlinProfiles;
    juce::ToggleButton perlinOn { "ON" }, perlinM1 { "M1" }, perlinM2 { "M2" };
    std::array<juce::Slider, 7> perlinControls;
    std::array<juce::Label, 7> perlinLabels;
    std::array<juce::Component, 3> lfoTabPages;
    juce::Component perlinTabPage;
    juce::TabbedComponent lfoTabs { juce::TabbedButtonBar::TabsAtTop };
    std::array<juce::Component, 4> generatorTabPages;
    juce::TabbedComponent generatorTabs { juce::TabbedButtonBar::TabsAtTop };

    ndlr::ui::SectionPanel harmony { "Harmony", ndlr::ui::green };
    ndlr::ui::SectionPanel motif1 { {}, ndlr::ui::cyan };
    ndlr::ui::SectionPanel motif2 { {}, ndlr::ui::orange };
    ndlr::ui::SectionPanel patternEditorPanel { "Pattern Editor", ndlr::ui::green };
    ndlr::ui::SectionPanel pad { {}, ndlr::ui::green };
    ndlr::ui::SectionPanel drone { {}, ndlr::ui::green };
    ndlr::ui::SectionPanel modMatrix { "Mod Matrix", ndlr::ui::green };
    ndlr::ui::SectionPanel rhythmEditor { "Rhythm Editor", ndlr::ui::green };
    ndlr::ui::RhythmWheel rhythmWheel;
    ndlr::ui::PatternDisplay patternDisplay;
    juce::ComboBox patternSelector;
    juce::TextButton patternSave { "SAVE" };

    juce::ToggleButton motif1On { "ON" };
    juce::Slider motif1Length;
    juce::ComboBox motif1Variation;
    juce::Slider motif1Velocity;
    juce::Label lengthLabel;
    juce::Label variationLabel;
    juce::Label velocityLabel;
    juce::Label channelLabel;
    juce::ComboBox harmonyScale;
    juce::ComboBox harmonyType;
    juce::ComboBox harmonyKeyMidiChannel, harmonyDegreeMidiChannel;
    juce::Label harmonyScaleLabel;
    juce::Label harmonyTypeLabel;
    juce::Label harmonyKeyMidiChannelLabel, harmonyDegreeMidiChannelLabel;
    juce::ComboBox motif1Channel;
    std::array<juce::TextButton, 3> motif1PatternTypeButtons;
    std::array<juce::TextButton, 5> motif1RegisterButtons;
    juce::Slider motif1RhythmLength;
    std::array<juce::TextButton, 2> rhythmModeButtons;
    juce::Slider motif1EuclideanPulses;
    juce::Slider motif1Rotation;
    juce::Label rhythmModeLabel;
    juce::Label rhythmLengthLabel;
    juce::Label rhythmBeatsLabel;
    juce::Label rhythmRotateLabel;
    juce::ComboBox rhythmUserSlot;
    juce::TextButton rhythmSave { "SAVE" };
    juce::TextButton rhythmLoad { "LOAD" };
    juce::Slider motif1Humanize;
    juce::ComboBox motif1Accent;
    juce::Slider motif1Gate;
    juce::Label humanizeLabel;
    juce::Label accentLabel;
    juce::Label gateLabel;
    juce::Label patternTypeLabel;
    juce::Label positionLabel;

    juce::ToggleButton motif2On { "ON" };
    juce::ComboBox motif2Variation;
    juce::Slider motif2Velocity;
    juce::ComboBox motif2Channel;
    std::array<juce::TextButton, 3> motif2PatternTypeButtons;
    std::array<juce::TextButton, 5> motif2RegisterButtons;
    juce::Slider motif2Humanize;
    juce::ComboBox motif2Accent;
    juce::Slider motif2Gate;
    juce::Label motif2VariationLabel;
    juce::Label motif2VelocityLabel, motif2ChannelLabel;
    juce::Label motif2PatternTypeLabel, motif2PositionLabel;
    juce::Label motif2HumanizeLabel, motif2AccentLabel, motif2GateLabel;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ButtonAttachment> motif1OnAttachment;
    std::unique_ptr<ButtonAttachment> transportPlayAttachment;
    std::unique_ptr<SliderAttachment> transportTempoAttachment;
    std::unique_ptr<ComboAttachment> harmonyScaleAttachment;
    std::unique_ptr<ComboAttachment> harmonyTypeAttachment;
    std::unique_ptr<ComboAttachment> harmonyKeyMidiChannelAttachment, harmonyDegreeMidiChannelAttachment;
    std::unique_ptr<SliderAttachment> motif1LengthAttachment;
    std::unique_ptr<ComboAttachment> motif1VariationAttachment;
    std::unique_ptr<SliderAttachment> motif1VelocityAttachment;
    std::unique_ptr<ComboAttachment> motif1ChannelAttachment;
    std::unique_ptr<ComboAttachment> patternSelectorAttachment;
    std::unique_ptr<SliderAttachment> motif1RhythmLengthAttachment;
    std::unique_ptr<SliderAttachment> motif1EuclideanPulsesAttachment;
    std::unique_ptr<SliderAttachment> motif1RotationAttachment;
    std::unique_ptr<ButtonAttachment> motif2OnAttachment;
    std::unique_ptr<ComboAttachment> motif2VariationAttachment;
    std::unique_ptr<SliderAttachment> motif2VelocityAttachment;
    std::unique_ptr<ComboAttachment> motif2ChannelAttachment;
    std::unique_ptr<SliderAttachment> motif2HumanizeAttachment;
    std::unique_ptr<ComboAttachment> motif2AccentAttachment;
    std::unique_ptr<SliderAttachment> motif2GateAttachment;

    int selectedMotif = 0;
    bool updatingPatternSelector = false;
    void selectMotif (int motifIndex);
    std::unique_ptr<SliderAttachment> motif1HumanizeAttachment;
    std::unique_ptr<ComboAttachment> motif1AccentAttachment;
    std::unique_ptr<SliderAttachment> motif1GateAttachment;
    std::unique_ptr<ButtonAttachment> droneOnAttachment;
    std::unique_ptr<ComboAttachment> dronePositionAttachment, droneTypeAttachment;
    std::unique_ptr<ComboAttachment> droneTriggerAttachment, droneChannelAttachment;
    std::unique_ptr<SliderAttachment> droneVelocityAttachment;
    std::unique_ptr<ButtonAttachment> padOnAttachment, padStrumAttachment, padGroupAttachment, padInvertAttachment;
    std::unique_ptr<SliderAttachment> padPositionAttachment, padRangeAttachment, padSpreadAttachment, padVelocityAttachment, padPolyAttachment;
    std::unique_ptr<ComboAttachment> padDivisionAttachment, padChannelAttachment, padQuantizeAttachment, padInversionModeAttachment;
    std::array<std::unique_ptr<ComboAttachment>,3> modLfoShapeAttachment, modLfoDivisionAttachment;
    std::array<std::unique_ptr<ComboAttachment>,3> modLfoAmplitudeSourceAttachment;
    std::array<std::unique_ptr<ComboAttachment>,3> modLfoAmplitudeCcNumberAttachment, modLfoAmplitudeCcChannelAttachment;
    std::array<std::unique_ptr<SliderAttachment>,3> modLfoRateAttachment,
        modLfoProbabilityAttachment, modLfoPulseWidthAttachment,
        modLfoLengthAttachment, modLfoPhaseAttachment, modLfoAmplitudeAttachment;
    std::array<std::unique_ptr<ButtonAttachment>,3> modLfoSyncAttachment, modLfoRetriggerAttachment;
    std::array<std::unique_ptr<ButtonAttachment>,8> modSlotOnAttachment;
    std::array<std::unique_ptr<ComboAttachment>,8> modSlotSourceAttachment, modSlotDestinationAttachment;
    std::array<std::unique_ptr<ComboAttachment>,8> modSlotCcNumberAttachment, modSlotCcChannelAttachment;
    std::array<std::unique_ptr<SliderAttachment>,8> modSlotAmountAttachment;
    std::unique_ptr<ButtonAttachment> perlinOnAttachment, perlinM1Attachment, perlinM2Attachment;
    std::array<std::unique_ptr<SliderAttachment>, 7> perlinControlAttachments;
    std::array<bool, 20> displayedUsedSlots {};
    int displayedUsedMotif = -1;
    std::array<bool, 40> displayedRhythmUsedSlots {};
    bool displayedRhythmSlotsValid = false;
    std::array<int, 2> previousRhythmMode { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NdlrAudioProcessorEditor)
};
