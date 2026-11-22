#include "PluginEditor.h"

namespace mk
{

PluginEditor::PluginEditor (PluginProcessor& owner)
    : AudioProcessorEditor (&owner),
      processor (owner),
      padPage (owner.apvts),
      texturePage (owner.apvts),
      modPage (owner.apvts),
      spacePage (owner.apvts),
      gatePage (owner.apvts, ui::GateHostContext {
                                [this] { return processor.engine.gatePhase(); },
                                [this] { return processor.engine.gateLevel(); },
                                [this] { return processor.engine.tempo(); },
                                [this] { return processor.engine.sampleRate(); } }),
      presetPage ([this] (int index) { processor.loadProgram (index); },
                  [this] { return processor.currentProgramIndex(); })
{
    setLookAndFeel (&lookAndFeel);

    for (auto* tab : { &tabPad, &tabTextures, &tabMod, &tabSpace, &tabGate, &tabPresets })
        addAndMakeVisible (tab);

    addAndMakeVisible (previousButton);
    addAndMakeVisible (nextButton);
    addAndMakeVisible (outputMeter);

    addChildComponent (padPage);
    addChildComponent (texturePage);
    addChildComponent (modPage);
    addChildComponent (spacePage);
    addChildComponent (gatePage);
    addChildComponent (presetPage);

    setResizable (true, true);
    setResizeLimits (960, 660, 1800, 1250);
    getConstrainer()->setFixedAspectRatio (1200.0 / 820.0);
    setSize (1200, 820);

    showPage (0);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::showPage (int index)
{
    currentPage = juce::jlimit (0, 5, index);

    padPage.setVisible (currentPage == 0);
    texturePage.setVisible (currentPage == 1);
    modPage.setVisible (currentPage == 2);
    spacePage.setVisible (currentPage == 3);
    gatePage.setVisible (currentPage == 4);
    presetPage.setVisible (currentPage == 5);

    tabPad.setActive (currentPage == 0);
    tabTextures.setActive (currentPage == 1);
    tabMod.setActive (currentPage == 2);
    tabSpace.setActive (currentPage == 3);
    tabGate.setActive (currentPage == 4);
    tabPresets.setActive (currentPage == 5);

    if (currentPage == 5)
        presetPage.refresh();
}

void PluginEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient backgroundGradient (ui::col::backgroundHi, 0.0f, 0.0f,
                                             ui::col::background, 0.0f, bounds.getBottom(), false);
    g.setGradientFill (backgroundGradient);
    g.fillAll();

    // Vignette for depth.
    juce::ColourGradient vignette (juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
                                   ui::col::shadow.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillRect (bounds);

    const int headerHeight = 74;
    const int tabHeight = 46;

    // Title block
    g.setColour (ui::col::text);
    g.setFont (ui::font (23.0f, true));
    g.drawText ("MK AMBIENT", 22, 12, 320, 26, juce::Justification::centredLeft, false);

    g.setColour (ui::col::textFaint);
    g.setFont (ui::font (9.5f));
    g.drawText ("EVOLVING PAD & TEXTURE SYNTHESIZER", 23, 38, 340, 14,
                juce::Justification::centredLeft, false);

    // Preset display
    auto presetArea = juce::Rectangle<int> (getWidth() / 2 - 190, 12, 380, 50);
    g.setColour (ui::col::panelDeep);
    g.fillRoundedRectangle (presetArea.toFloat(), 6.0f);
    g.setColour (ui::col::lineSoft);
    g.drawRoundedRectangle (presetArea.toFloat().reduced (0.5f), 6.0f, 1.0f);

    g.setColour (ui::col::accent);
    g.setFont (ui::font (15.5f, true));
    g.drawText (processor.getProgramName (processor.currentProgramIndex()),
                presetArea.reduced (14, 6).removeFromTop (24), juce::Justification::centredLeft, false);

    g.setColour (ui::col::textFaint);
    g.setFont (ui::font (9.5f, true));
    g.drawText (processor.getProgramCategory (processor.currentProgramIndex()).toUpperCase(),
                presetArea.reduced (14, 6).removeFromBottom (16), juce::Justification::centredLeft, false);

    // Header meter label
    g.setColour (ui::col::textFaint);
    g.setFont (ui::font (9.0f));
    g.drawText ("OUT", getWidth() - 158, 20, 40, 12, juce::Justification::centredLeft, false);

    // Separators
    g.setColour (ui::col::lineSoft.withAlpha (0.7f));
    g.drawLine (0.0f, (float) headerHeight, (float) getWidth(), (float) headerHeight, 1.0f);
    g.drawLine (0.0f, (float) (headerHeight + tabHeight), (float) getWidth(),
                (float) (headerHeight + tabHeight), 1.0f);

    // Page caption on the right of the tab strip
    static const char* pageNames[] = { "VOICE & FILTER", "FOUR TEXTURE LAYERS", "MODULATION MATRIX",
                                       "DELAY / REVERB / MASTER", "12-STEP TRANCE GATE",
                                       "FACTORY PROGRAMS" };
    g.setColour (ui::col::textFaint);
    g.setFont (ui::font (10.0f));
    g.drawText (pageNames[juce::jlimit (0, 5, currentPage)], getWidth() - 360, headerHeight + 14, 340, 16,
                juce::Justification::centredRight, false);

    // Screws in the outer corners
    ui::drawScrew (g, { 10.0f, (float) getHeight() - 10.0f });
    ui::drawScrew (g, { (float) getWidth() - 10.0f, 10.0f });
}

void PluginEditor::resized()
{
    const int headerHeight = 74;
    const int tabHeight = 46;

    previousButton.setBounds (getWidth() - 330, 22, 44, 34);
    nextButton.setBounds (getWidth() - 280, 22, 44, 34);
    outputMeter.setBounds (getWidth() - 200, 32, 150, 16);

    const int tabWidth = 132;
    int x = 18;
    for (auto* tab : { &tabPad, &tabTextures, &tabMod, &tabSpace, &tabGate, &tabPresets })
    {
        tab->setBounds (x, headerHeight + 7, tabWidth, tabHeight - 14);
        x += tabWidth + 6;
    }

    const int margin = 18;
    auto content = juce::Rectangle<int> (margin, headerHeight + tabHeight + 10,
                                         getWidth() - 2 * margin,
                                         getHeight() - headerHeight - tabHeight - 26);

    padPage.setBounds (content);
    texturePage.setBounds (content);
    modPage.setBounds (content);
    spacePage.setBounds (content);
    gatePage.setBounds (content);
    presetPage.setBounds (content);
}

void PluginEditor::timerCallback()
{
    outputMeter.setLevel (processor.engine.outputMeter());

    if (processor.currentProgramIndex() != lastProgram)
    {
        lastProgram = processor.currentProgramIndex();
        presetPage.refresh();
        repaint();
    }
}

} // namespace mk
