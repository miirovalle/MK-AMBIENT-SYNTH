#pragma once

// Touch-first controls. A Knob owns its own paint and gesture handling (no
// JUCE Slider), which keeps the drag behaviour consistent on iPad and gives
// full control over the hardware look.

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace mk::ui
{

class Knob : public juce::Component
{
public:
    Knob (juce::RangedAudioParameter& parameter, juce::String labelText, bool bipolar = false);

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void setKnobLabel (const juce::String& newLabel);

private:
    void setNormalised (float newNormalised, bool partOfGesture);

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    float normalised = 0.0f;
    float dragStartNormalised = 0.0f;
    float dragStartY = 0.0f;
    bool bipolarKnob = false;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

class ChoiceButton : public juce::Component
{
public:
    ChoiceButton (juce::RangedAudioParameter& parameter, juce::String labelText);

    void paint (juce::Graphics& g) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    void showMenu();

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    float normalised = 0.0f;
    bool hover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceButton)
};

class SimpleButton : public juce::Component
{
public:
    SimpleButton (juce::String buttonText, std::function<void()> onPress);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; pressed = false; repaint(); }

    void setActive (bool shouldBeActive) { active = shouldBeActive; repaint(); }
    void setText (const juce::String& newText) { text = newText; repaint(); }

private:
    juce::String text;
    std::function<void()> onClick;
    bool active = false, hover = false, pressed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleButton)
};

class Meter : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
    void setLevel (float newLevel); // 0..1

private:
    float level = 0.0f;
};

} // namespace mk::ui
