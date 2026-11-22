#pragma once

#include "PluginProcessor.h"
#include <array>
#include <memory>

class MKRattusTranceGateEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MKRattusTranceGateEditor (MKRattusTranceGateProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class GateGrid final : public juce::Component
    {
    public:
        explicit GateGrid (MKRattusTranceGateProcessor& p) : processor (p) { setViewportIgnoreDragFlag (true); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
    private:
        void setStepFromPosition (juce::Point<float>);
        juce::Rectangle<float> plotBounds() const;
        Pattern previewPattern { 1 };
        int selectedPoint = -1;
        MKRattusTranceGateProcessor& processor;
    };

    void styleSlider (juce::Slider&);
    void timerCallback() override;
    MKRattusTranceGateProcessor& processor;
    juce::Label title, subtitle, presetDescription, swingLabel;
    std::array<juce::Label, 5> controlLabels;
    juce::ToggleButton gateEnabled, gateFilter { "Filter Gate" }, gateRetrigger { "MIDI Retrigger" };
    juce::ComboBox gatePreset, gateCycle, gateShape, gateStereo;
    int lastSyncedPreset = -1;
    juce::Slider gateSteps, gateAttack, gateRelease, gateTension, mix, outputGain, gateSwing;
    juce::TextButton randomizeGate { "Randomize Gate" };
    GateGrid gateGrid;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> gateEnabledAttachment,
        retriggerAttachment, filterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cycleAttachment,
        shapeAttachment, stereoAttachment;
    std::unique_ptr<juce::ParameterAttachment> gateStepsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttackAttachment,
        gateReleaseAttachment, tensionAttachment, mixAttachment, outputGainAttachment, swingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MKRattusTranceGateEditor)
};
