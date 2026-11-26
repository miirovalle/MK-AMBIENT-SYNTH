#pragma once

// Page layout. Cards own their controls and lay them out in a grid, so the
// pages themselves only describe *what* belongs together.

#include "GateEditor.h"
#include "Knob.h"

#include "DSP/Params.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace mk::ui
{

juce::RangedAudioParameter& paramRef (juce::AudioProcessorValueTreeState& state, mk::P id);

class Card : public juce::Component
{
public:
    Card (juce::AudioProcessorValueTreeState& state, juce::String title, int knobColumns);

    void addKnob (mk::P id, const juce::String& label, bool bipolar = false);
    void addChoice (mk::P id, const juce::String& label);

    // Adds an on/off switch to the right of the card title. The optional
    // caption is shown left of the switch when the parameter name alone would
    // be ambiguous.
    void addToggle (mk::P id, const juce::String& caption = {});

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Rectangle<int> headerBounds() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String cardTitle;
    int columns;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<ChoiceButton> choices;
    juce::OwnedArray<Toggle> toggles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Card)
};

class Page : public juce::Component
{
public:
    virtual void refresh() {}
};

class PadPage : public Page
{
public:
    explicit PadPage (juce::AudioProcessorValueTreeState& state);
    void resized() override;

private:
    Card oscA, oscB, filter, filterEnv, ampEnv, voice, quick;
};

class TexturePage : public Page
{
public:
    explicit TexturePage (juce::AudioProcessorValueTreeState& state);
    void resized() override;

private:
    Card bus;
    juce::OwnedArray<Card> slots;
    juce::Label hint;
};

class ModPage : public Page
{
public:
    explicit ModPage (juce::AudioProcessorValueTreeState& state);
    void resized() override;

private:
    juce::OwnedArray<Card> lfos;
};

class SpacePage : public Page
{
public:
    explicit SpacePage (juce::AudioProcessorValueTreeState& state);
    void resized() override;

private:
    Card delay, reverb, master;
};

class GatePage : public Page
{
public:
    GatePage (juce::AudioProcessorValueTreeState& state, GateHostContext context);
    void resized() override;

private:
    Card controls, timing;
    std::unique_ptr<GateEditor> editor;
};

class PresetPage : public Page
{
public:
    PresetPage (std::function<void (int)> loadPreset, std::function<int()> currentProgram);
    ~PresetPage() override;
    void resized() override;
    void refresh() override;

private:
    void rebuild();
    void showBank (int newBank);

    class Tile;

    static constexpr int kTilesPerPage = 12;

    juce::OwnedArray<Tile> tiles;
    std::function<void (int)> load;
    std::function<int()> current;
    SimpleButton bankPrev { "<", [this] { showBank (bank - 1); } };
    SimpleButton bankNext { ">", [this] { showBank (bank + 1); } };
    juce::Label bankLabel;
    int bank = 0;
};

} // namespace mk::ui
