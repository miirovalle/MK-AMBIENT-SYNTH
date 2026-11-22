#include "Theme.h"

namespace mk::ui
{

juce::Font font (float height, bool bold)
{
    auto options = juce::FontOptions().withHeight (height);
    if (bold)
        options = options.withStyle ("Bold");
    return juce::Font (options);
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, col::panelRaised);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::accentBright);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panelRaised);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
    setColour (juce::AlertWindow::outlineColourId, col::line);
    setColour (juce::ComboBox::backgroundColourId, col::panelRaised);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::line);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return font (17.0f);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (col::panelRaised);
    g.setColour (col::line);
    g.drawRect (0, 0, width, height, 1);
}

void drawPanelBackground (juce::Graphics& g, juce::Rectangle<float> area, float cornerSize, bool raised)
{
    const float radius = cornerSize;

    g.setColour (col::shadow.withAlpha (0.55f));
    g.fillRoundedRectangle (area.translated (0.0f, 2.0f), radius);

    juce::ColourGradient gradient (raised ? col::panelRaised : col::panel,
                                   area.getX(), area.getY(),
                                   (raised ? col::panel : col::panelDeep),
                                   area.getX(), area.getBottom(), false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (area, radius);

    g.setColour (col::lineSoft);
    g.drawRoundedRectangle (area.reduced (0.5f), radius, 1.0f);

    // Hairline highlight along the top edge sells the machined-metal look.
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (area.getX() + radius, area.getY() + 1.0f,
                area.getRight() - radius, area.getY() + 1.0f, 1.0f);
}

void drawScrew (juce::Graphics& g, juce::Point<float> centre)
{
    g.setColour (col::panelDeep);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (centre));
    g.setColour (col::line.withAlpha (0.8f));
    g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (centre), 0.8f);
    g.setColour (col::lineSoft);
    g.drawLine (centre.x - 2.0f, centre.y + 1.4f, centre.x + 2.0f, centre.y - 1.4f, 0.8f);
}

} // namespace mk::ui
