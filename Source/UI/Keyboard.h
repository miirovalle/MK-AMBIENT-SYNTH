#pragma once

// Small piano keyboard that sits at the bottom of the editor, so the synth can
// be played straight from the plug-in without a host or a MIDI controller.
//
// * Multi-touch: every finger plays its own note and glides between keys.
// * Seven octaves (C1..C7) at a small key width, so a scroll bar appears
//   whenever the window is narrower than the whole range.
// * Notes are reported through callbacks; the processor turns them into MIDI so
//   the audio thread never sees UI state.

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace mk::ui
{

struct KeyboardHost
{
    std::function<void (int note, float velocity)> noteOn;
    std::function<void (int note)> noteOff;
};

class Keyboard : public juce::Component
{
public:
    explicit Keyboard (KeyboardHost host);
    ~Keyboard() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Releases everything the keyboard is holding (used when the editor closes).
    void releaseAllNotes();

private:
    static constexpr int kLowestNote = 24;   // C1
    static constexpr int kHighestNote = 96;  // C7
    static constexpr int kWhiteKeyWidth = 34;
    static constexpr int kScrollBarHeight = 12;
    static constexpr int kMaxPointers = 10;

    struct Pointer
    {
        int source = -1;
        int note = -1;
        bool scrolling = false;
        float dragStartX = 0.0f;
        float scrollStart = 0.0f;
    };

    static bool isBlackKey (int note) noexcept;
    static juce::String nameForNote (int note);

    int whiteKeyIndex (int note) const;
    int noteForWhiteIndex (int index) const;
    int whiteKeyCount() const;
    float contentWidth() const;
    float maximumScroll() const;

    juce::Rectangle<float> keysArea() const;
    juce::Rectangle<float> thumbArea() const;
    int noteAt (juce::Point<float> position) const;
    bool scrollBarVisible() const { return contentWidth() > keysArea().getWidth() + 0.5f; }

    Pointer& pointerFor (int sourceIndex);
    void playNote (Pointer& pointer, int note);
    void stopPointer (Pointer& pointer);
    void scrollTo (float newScroll);

    KeyboardHost host;
    std::array<Pointer, (size_t) kMaxPointers> pointers;
    std::array<bool, 128> active {};

    float scroll = 0.0f;
    bool thumbDragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Keyboard)
};

} // namespace mk::ui
