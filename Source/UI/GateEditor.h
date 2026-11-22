#pragma once

// Touch-first 12-step gate editor.
//
// * Drag any node vertically to set that step (Point mode).
// * Drag across columns to paint several steps at once (Paint mode).
// * Double-tap a node to jump it to Max.
// * Pattern strip loads one of eight shapes; Flip mirrors; Random scatters.
// * Snap quantises dragging to a 1/16 grid.
// * Undo/redo keeps the last edits.
//
// The curve drawn here is produced by the same function the DSP uses, so the
// preview is sample-accurate rather than an approximation.

#include "Theme.h"

#include "DSP/Params.h"
#include "DSP/TranceGate.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <functional>
#include <vector>

namespace mk::ui
{

struct GateHostContext
{
    std::function<float()> phase;     // 0..1 pattern position from the engine
    std::function<float()> level;     // current gate gain 0..1
    std::function<double()> tempo;    // host BPM
    std::function<double()> sampleRate;
};

class GateEditor : public juce::Component, private juce::Timer
{
public:
    GateEditor (juce::AudioProcessorValueTreeState& state, GateHostContext context);
    ~GateEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    struct Controls
    {
        juce::RangedAudioParameter* steps[mk::kGateSteps] {};
        juce::RangedAudioParameter* minLevel = nullptr;
        juce::RangedAudioParameter* maxLevel = nullptr;
        juce::RangedAudioParameter* attack = nullptr;
        juce::RangedAudioParameter* release = nullptr;
        juce::RangedAudioParameter* tension = nullptr;
        juce::RangedAudioParameter* phase = nullptr;
        juce::RangedAudioParameter* mix = nullptr;
        juce::RangedAudioParameter* trigger = nullptr;
        juce::RangedAudioParameter* length = nullptr;
        juce::RangedAudioParameter* rate = nullptr;
    };

    void timerCallback() override;

    mk::GateSettings currentSettings() const;
    float stepSamplesFor (const mk::GateSettings& settings, double sampleRate) const;

    juce::Rectangle<float> plotArea() const;
    float valueToY (float value, juce::Rectangle<float> area) const;
    float yToValue (float y, juce::Rectangle<float> area) const;
    int columnAt (float x) const;
    float snapValue (float value) const;

    void readSteps (std::array<float, (size_t) mk::kGateSteps>& out) const;
    void writeStep (int index, float value);
    void writeSteps (const std::array<float, (size_t) mk::kGateSteps>& values);
    void pushUndo();
    void undo();
    void redo();
    void loadPattern (int index);
    void flipX();
    void randomise();
    void setAll (float value);
    void updateUndoButtons();

    class PatternStrip;
    class ToolButton;

    juce::AudioProcessorValueTreeState& apvts;
    GateHostContext host;
    Controls controls;

    std::unique_ptr<PatternStrip> patterns;
    std::vector<std::unique_ptr<ToolButton>> tools;

    std::vector<std::array<float, (size_t) mk::kGateSteps>> undoStack;
    int undoIndex = -1;

    bool paintMode = false;
    bool snapToGrid = true;
    int dragColumn = -1;
    float dragStartValue = 1.0f;
    int lastPaintedColumn = -1;
    bool gestureActive = false;

    float displayPhase = 0.0f;
    float displayLevel = 1.0f;
    bool engineRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GateEditor)
};

} // namespace mk::ui
