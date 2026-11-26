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

    addAndMakeVisible (canvas);

    for (auto* tab : { &tabPad, &tabTextures, &tabMod, &tabSpace, &tabGate, &tabPresets })
        canvas.addAndMakeVisible (tab);

    canvas.addAndMakeVisible (previousButton);
    canvas.addAndMakeVisible (nextButton);
    canvas.addAndMakeVisible (outputMeter);
    canvas.addAndMakeVisible (keyboard);

    canvas.addChildComponent (padPage);
    canvas.addChildComponent (texturePage);
    canvas.addChildComponent (modPage);
    canvas.addChildComponent (spacePage);
    canvas.addChildComponent (gatePage);
    canvas.addChildComponent (presetPage);

    // The interface is designed once and scaled to fit, so the window may be
    // any size from a small laptop window to a full iPad screen. On iPad the
    // AUv3 host decides the size, so the editor does not constrain it.
   #if JUCE_IOS
    setResizable (false, false);
   #else
    setResizable (true, true);
    setResizeLimits (600, 450, 2400, 1800);
    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((double) kDesignWidth / (double) kDesignHeight);
   #endif

    // Open at the design size, or smaller if the screen is smaller, so the
    // window always fits.
    int initialWidth = kDesignWidth;
    int initialHeight = kDesignHeight;

    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto userArea = display->userArea;
        const float fit = juce::jmin (1.0f,
                                      juce::jmin ((float) userArea.getWidth() * 0.92f / (float) kDesignWidth,
                                                  (float) userArea.getHeight() * 0.92f / (float) kDesignHeight));
        initialWidth = juce::jmax (600, juce::roundToInt ((float) kDesignWidth * fit));
        initialHeight = juce::jmax (450, juce::roundToInt ((float) kDesignHeight * fit));
    }

    setSize (initialWidth, initialHeight);

    showPage (0);
    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    keyboard.releaseAllNotes();
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
    // Letterbox behind the scaled canvas.
    g.fillAll (ui::col::shadow);
}

void PluginEditor::paintCanvas (juce::Graphics& g)
{
    auto bounds = juce::Rectangle<float> ((float) kDesignWidth, (float) kDesignHeight);

    juce::ColourGradient backgroundGradient (ui::col::backgroundHi, 0.0f, 0.0f,
                                             ui::col::background, 0.0f, bounds.getBottom(), false);
    g.setGradientFill (backgroundGradient);
    g.fillRect (bounds);

    // Vignette for depth.
    juce::ColourGradient vignette (juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
                                   ui::col::shadow.withAlpha (0.55f), 0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillRect (bounds);

    // Title block
    g.setColour (ui::col::text);
    g.setFont (ui::font (23.0f, true));
    g.drawText ("MK AMBIENT", 22, 12, 320, 26, juce::Justification::centredLeft, false);

    g.setColour (ui::col::textFaint);
    g.setFont (ui::font (9.5f));
    g.drawText ("EVOLVING PAD & TEXTURE SYNTHESIZER", 23, 38, 340, 14,
                juce::Justification::centredLeft, false);

    // Preset display
    auto presetArea = juce::Rectangle<int> (kDesignWidth / 2 - 190, 12, 380, 50);
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
    g.drawText ("OUT", kDesignWidth - 158, 20, 40, 12, juce::Justification::centredLeft, false);

    // Separators
    g.setColour (ui::col::lineSoft.withAlpha (0.7f));
    g.drawLine (0.0f, (float) kHeaderHeight, (float) kDesignWidth, (float) kHeaderHeight, 1.0f);
    g.drawLine (0.0f, (float) (kHeaderHeight + kTabHeight), (float) kDesignWidth,
                (float) (kHeaderHeight + kTabHeight), 1.0f);

    // Page caption on the right of the tab strip
    static const char* pageNames[] = { "VOICE & FILTER", "FOUR TEXTURE LAYERS", "MODULATION MATRIX",
                                       "DELAY / REVERB / MASTER", "12-STEP TRANCE GATE",
                                       "FACTORY PROGRAMS" };
    g.setColour (ui::col::textFaint);
    g.setFont (ui::font (10.0f));
    g.drawText (pageNames[juce::jlimit (0, 5, currentPage)], kDesignWidth - 360, kHeaderHeight + 14, 340, 16,
                juce::Justification::centredRight, false);

    // Screws in the outer corners
    ui::drawScrew (g, { 10.0f, (float) kDesignHeight - 10.0f });
    ui::drawScrew (g, { (float) kDesignWidth - 10.0f, 10.0f });
}

void PluginEditor::resized()
{
    // Scale the fixed design to the space we have been given, keeping the
    // aspect ratio so every control stays visible and correctly proportioned.
    const auto area = getLocalBounds().toFloat();
    const float scale = juce::jmin (area.getWidth() / (float) kDesignWidth,
                                    area.getHeight() / (float) kDesignHeight);
    const float offsetX = (area.getWidth() - (float) kDesignWidth * scale) * 0.5f;
    const float offsetY = (area.getHeight() - (float) kDesignHeight * scale) * 0.5f;

    canvas.setBounds (0, 0, kDesignWidth, kDesignHeight);
    canvas.setTransform (juce::AffineTransform::scale (juce::jmax (0.05f, scale))
                             .translated (offsetX, offsetY));

    layoutCanvas();
}

void PluginEditor::layoutCanvas()
{
    const int width = kDesignWidth;

    previousButton.setBounds (width - 330, 22, 44, 34);
    nextButton.setBounds (width - 280, 22, 44, 34);
    outputMeter.setBounds (width - 200, 32, 150, 16);

    const int tabWidth = 132;
    int x = 18;
    for (auto* tab : { &tabPad, &tabTextures, &tabMod, &tabSpace, &tabGate, &tabPresets })
    {
        tab->setBounds (x, kHeaderHeight + 7, tabWidth, kTabHeight - 14);
        x += tabWidth + 6;
    }

    const int margin = 18;

    keyboard.setBounds (margin, kDesignHeight - 10 - kKeyboardHeight,
                        width - 2 * margin, kKeyboardHeight);

    const int pageTop = kHeaderHeight + kTabHeight + 10;
    auto content = juce::Rectangle<int> (margin, pageTop, width - 2 * margin,
                                         keyboard.getY() - 10 - pageTop);

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
