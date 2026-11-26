#pragma once

#include "PluginProcessor.h"
#include "UI/Keyboard.h"
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
    // Everything is laid out once at this size and then scaled to whatever
    // space the window or the AUv3 host gives us. A 4:3 design matches an iPad
    // screen and nothing is ever clipped at any other size.
    static constexpr int kDesignWidth = 1200;
    static constexpr int kDesignHeight = 900;
    static constexpr int kHeaderHeight = 74;
    static constexpr int kTabHeight = 46;
    static constexpr int kKeyboardHeight = 96;

    class Canvas : public juce::Component
    {
    public:
        explicit Canvas (PluginEditor& ownerEditor) : editor (ownerEditor) {}

        void paint (juce::Graphics& g) override { editor.paintCanvas (g); }

    private:
        PluginEditor& editor;
    };

    void timerCallback() override;
    void showPage (int index);
    void paintCanvas (juce::Graphics& g);
    void layoutCanvas();

    PluginProcessor& processor;
    ui::LookAndFeel lookAndFeel;
    Canvas canvas { *this };

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
    ui::Keyboard keyboard { ui::KeyboardHost {
        [this] (int note, float velocity) { processor.keyboardNoteOn (note, velocity); },
        [this] (int note) { processor.keyboardNoteOff (note); } } };

    int currentPage = 0;
    int lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};

} // namespace mk
