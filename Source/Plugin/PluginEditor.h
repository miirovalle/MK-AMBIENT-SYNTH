#pragma once

#include "PluginProcessor.h"
#include "UI/Panels.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace mk
{

class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (int index);

    PluginProcessor& processor;
    ui::LookAndFeel lookAndFeel;

    ui::SimpleButton tabPad { "PAD", [this] { showPage (0); } };
    ui::SimpleButton tabTextures { "TEXTURES", [this] { showPage (1); } };
    ui::SimpleButton tabMod { "MOTION", [this] { showPage (2); } };
    ui::SimpleButton tabSpace { "SPACE", [this] { showPage (3); } };
    ui::SimpleButton tabGate { "GATE", [this] { showPage (4); } };
    ui::SimpleButton tabPresets { "PRESETS", [this] { showPage (5); } };

    ui::SimpleButton previousButton { "<", [this] { processor.loadProgram (processor.currentProgramIndex() - 1); } };
    ui::SimpleButton nextButton { ">", [this] { processor.loadProgram (processor.currentProgramIndex() + 1); } };

    ui::PadPage padPage;
    ui::TexturePage texturePage;
    ui::ModPage modPage;
    ui::SpacePage spacePage;
    ui::GatePage gatePage;
    ui::PresetPage presetPage;

    ui::Meter outputMeter;
    int currentPage = 0;
    int lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

} // namespace mk
