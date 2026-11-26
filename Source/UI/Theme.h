#pragma once

// Monochrome olive-gray "vintage hardware" visual language.
// Every control is deliberately large and high contrast for touch.

#include <juce_gui_basics/juce_gui_basics.h>

namespace mk::ui
{

namespace col
{
    inline const juce::Colour background   { 0xff262720 };
    inline const juce::Colour backgroundHi { 0xff2e2f27 };
    inline const juce::Colour panel        { 0xff33342b };
    inline const juce::Colour panelRaised  { 0xff3b3c32 };
    inline const juce::Colour panelDeep    { 0xff22231d };
    inline const juce::Colour line         { 0xff54554a };
    inline const juce::Colour lineSoft     { 0xff454639 };
    inline const juce::Colour text         { 0xffcecbb6 };
    inline const juce::Colour textDim      { 0xff8d8b79 };
    inline const juce::Colour textFaint    { 0xff6b6959 };
    inline const juce::Colour accent       { 0xffa9a37a };
    inline const juce::Colour accentBright { 0xffcdc48c };
    inline const juce::Colour led          { 0xffd6cc86 };
    inline const juce::Colour shadow       { 0xff191a15 };
    inline const juce::Colour warn         { 0xffc09a5a };
    // Ivory keys for the on-screen keyboard, kept warm to match the panel.
    inline const juce::Colour ivory        { 0xffd3cfba };
    inline const juce::Colour ivoryShade   { 0xff9d9a86 };
}

juce::Font font (float height, bool bold = false);

// Shared look-and-feel for popup menus and any stock widgets we use.
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;
};

void drawPanelBackground (juce::Graphics& g, juce::Rectangle<float> area, float cornerSize = 7.0f,
                          bool raised = false);
void drawScrew (juce::Graphics& g, juce::Point<float> centre);

} // namespace mk::ui
