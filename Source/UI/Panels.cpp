#include "Panels.h"

#include "DSP/Presets.h"

namespace mk::ui
{

juce::RangedAudioParameter& paramRef (juce::AudioProcessorValueTreeState& state, mk::P id)
{
    auto* parameter = state.getParameter (mk::paramSpec (id).key);
    jassert (parameter != nullptr);
    return *parameter;
}

// ---------------------------------------------------------------------------
// Card
// ---------------------------------------------------------------------------

Card::Card (juce::AudioProcessorValueTreeState& state, juce::String title, int knobColumns)
    : apvts (state), cardTitle (std::move (title)), columns (juce::jmax (1, knobColumns))
{
}

void Card::addKnob (mk::P id, const juce::String& label, bool bipolar)
{
    auto* knob = knobs.add (new Knob (paramRef (apvts, id), label, bipolar));
    addAndMakeVisible (knob);
}

void Card::addChoice (mk::P id, const juce::String& label)
{
    auto* choice = choices.add (new ChoiceButton (paramRef (apvts, id), label));
    addAndMakeVisible (choice);
}

void Card::paint (juce::Graphics& g)
{
    drawPanelBackground (g, getLocalBounds().toFloat());

    auto titleArea = getLocalBounds().reduced (12, 7).removeFromTop (18);
    g.setColour (col::accent);
    g.setFont (font (12.0f, true));
    g.drawText (cardTitle, titleArea, juce::Justification::centredLeft, false);

    g.setColour (col::lineSoft);
    g.drawLine ((float) titleArea.getX(), (float) titleArea.getBottom() + 2.0f,
                (float) titleArea.getRight(), (float) titleArea.getBottom() + 2.0f, 1.0f);

    drawScrew (g, { 10.0f, 10.0f });
    drawScrew (g, { (float) getWidth() - 10.0f, (float) getHeight() - 10.0f });
}

void Card::resized()
{
    auto area = getLocalBounds().reduced (11, 9);
    area.removeFromTop (20);

    for (auto* choice : choices)
    {
        choice->setBounds (area.removeFromTop (32));
        area.removeFromTop (5);
    }

    const int count = knobs.size();
    if (count == 0)
        return;

    const int rows = (count + columns - 1) / columns;
    const int cellWidth = area.getWidth() / columns;
    const int cellHeight = area.getHeight() / rows;

    // Keep knobs at a comfortable touch size and centre the whole grid inside
    // the card instead of stretching cells to the full card height.
    const int knobHeight = juce::jmin (cellHeight - 2, 150);
    const int blockHeight = rows * knobHeight;
    const int startY = area.getY() + juce::jmax (0, (area.getHeight() - blockHeight) / 2);

    for (int i = 0; i < count; ++i)
    {
        const int row = i / columns;
        const int column = i % columns;
        knobs[i]->setBounds (area.getX() + column * cellWidth + 1,
                             startY + row * knobHeight,
                             cellWidth - 2,
                             juce::jmax (48, knobHeight));
    }
}

// ---------------------------------------------------------------------------
// Pad page
// ---------------------------------------------------------------------------

PadPage::PadPage (juce::AudioProcessorValueTreeState& s)
    : oscA (s, "OSC A", 3),
      oscB (s, "OSC B", 3),
      filter (s, "FILTER", 3),
      filterEnv (s, "FILTER ENVELOPE", 2),
      ampEnv (s, "AMP ENVELOPE", 3),
      voice (s, "VOICE", 2),
      quick (s, "SPACE QUICK", 2)
{
    oscA.addChoice (mk::P::OscAWave, "WAVE");
    oscA.addKnob (mk::P::OscAPos, "SHAPE");
    oscA.addKnob (mk::P::OscALevel, "LEVEL");
    oscA.addKnob (mk::P::OscAUnison, "UNISON");
    oscA.addKnob (mk::P::OscADetune, "DETUNE");
    oscA.addKnob (mk::P::OscASpread, "SPREAD");

    oscB.addChoice (mk::P::OscBWave, "WAVE");
    oscB.addChoice (mk::P::OscBInterval, "INTERVAL");
    oscB.addKnob (mk::P::OscBPos, "SHAPE");
    oscB.addKnob (mk::P::OscBLevel, "LEVEL");
    oscB.addKnob (mk::P::OscBUnison, "UNISON");
    oscB.addKnob (mk::P::OscBDetune, "DETUNE");
    oscB.addKnob (mk::P::OscBSpread, "SPREAD");

    filter.addChoice (mk::P::FilterMode, "MODE");
    filter.addKnob (mk::P::Cutoff, "CUTOFF");
    filter.addKnob (mk::P::Resonance, "RESONANCE");
    filter.addKnob (mk::P::FiltEnvAmt, "ENV AMT", true);
    filter.addKnob (mk::P::FiltKeyTrack, "KEY TRACK");
    filter.addKnob (mk::P::FiltDrive, "DRIVE");

    filterEnv.addKnob (mk::P::FiltAttack, "ATTACK");
    filterEnv.addKnob (mk::P::FiltDecay, "DECAY");
    filterEnv.addKnob (mk::P::FiltSustain, "SUSTAIN");
    filterEnv.addKnob (mk::P::FiltRelease, "RELEASE");

    ampEnv.addKnob (mk::P::AmpAttack, "ATTACK");
    ampEnv.addKnob (mk::P::AmpDecay, "DECAY");
    ampEnv.addKnob (mk::P::AmpSustain, "SUSTAIN");
    ampEnv.addKnob (mk::P::AmpRelease, "RELEASE");
    ampEnv.addKnob (mk::P::AmpCurve, "CURVE", true);

    voice.addKnob (mk::P::PadLevel, "PAD LEVEL");
    voice.addKnob (mk::P::Drift, "DRIFT");
    voice.addKnob (mk::P::Glide, "GLIDE");
    voice.addKnob (mk::P::Polyphony, "VOICES");

    quick.addKnob (mk::P::ReverbMix, "SPACE");
    quick.addKnob (mk::P::DelayMix, "DELAY");
    quick.addKnob (mk::P::PadSpace, "PAD SEND");
    quick.addKnob (mk::P::ReverbDecay, "DECAY");

    for (auto* card : { &oscA, &oscB, &filter, &filterEnv, &ampEnv, &voice, &quick })
        addAndMakeVisible (card);
}

void PadPage::resized()
{
    auto area = getLocalBounds();
    const int gap = 8;

    auto top = area.removeFromTop ((int) (area.getHeight() * 0.50f));
    const int topWidth = (top.getWidth() - 2 * gap) / 3;
    oscA.setBounds (top.removeFromLeft (topWidth));
    top.removeFromLeft (gap);
    oscB.setBounds (top.removeFromLeft (topWidth));
    top.removeFromLeft (gap);
    filter.setBounds (top);

    area.removeFromTop (gap);

    const int secondWidth = (area.getWidth() - 3 * gap) / 4;
    filterEnv.setBounds (area.removeFromLeft (secondWidth));
    area.removeFromLeft (gap);
    ampEnv.setBounds (area.removeFromLeft (secondWidth));
    area.removeFromLeft (gap);
    voice.setBounds (area.removeFromLeft (secondWidth));
    area.removeFromLeft (gap);
    quick.setBounds (area);
}

// ---------------------------------------------------------------------------
// Texture page
// ---------------------------------------------------------------------------

TexturePage::TexturePage (juce::AudioProcessorValueTreeState& s)
    : bus (s, "TEXTURE BUS", 3)
{
    bus.addKnob (mk::P::NoiseLevel, "TEXTURE LEVEL");
    bus.addKnob (mk::P::GrainSize, "GRAIN SIZE");
    bus.addKnob (mk::P::GrainSpray, "SPRAY");
    addAndMakeVisible (bus);

    for (int i = 0; i < mk::kNoiseSlotCount; ++i)
    {
        auto* card = slots.add (new Card (s, "SLOT " + juce::String (i + 1), 3));

        card->addChoice (mk::slotParam (i, 0), "SOURCE");
        card->addChoice (mk::slotParam (i, 2), "MODE");
        card->addChoice (mk::slotParam (i, 4), "FILTER");
        card->addChoice (mk::slotParam (i, 12), "LFO SHAPE");

        card->addKnob (mk::slotParam (i, 1), "LEVEL");
        card->addKnob (mk::slotParam (i, 5), "CUTOFF");
        card->addKnob (mk::slotParam (i, 6), "RES");
        card->addKnob (mk::slotParam (i, 7), "PAN", true);
        card->addKnob (mk::slotParam (i, 8), "DRIVE");
        card->addKnob (mk::slotParam (i, 9), "DELAY");
        card->addKnob (mk::slotParam (i, 10), "SPACE");
        card->addKnob (mk::slotParam (i, 3), "TUNE", true);
        card->addKnob (mk::slotParam (i, 11), "LFO RATE");
        card->addKnob (mk::slotParam (i, 13), "LFO CUT");
        card->addKnob (mk::slotParam (i, 14), "LFO PAN");
        card->addKnob (mk::slotParam (i, 15), "LFO SPACE");
        card->addKnob (mk::slotParam (i, 16), "SWELL");

        addAndMakeVisible (card);
    }

    hint.setText ("Each slot has its own filter, LFO, pan, drive and delay/space sends.", juce::dontSendNotification);
    hint.setFont (font (12.0f));
    hint.setColour (juce::Label::textColourId, col::textFaint);
    hint.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (hint);
}

void TexturePage::resized()
{
    auto area = getLocalBounds();
    const int gap = 8;

    auto top = area.removeFromTop (104);
    bus.setBounds (top.removeFromLeft (346));
    top.removeFromLeft (gap);
    hint.setBounds (top.reduced (4, 0));

    area.removeFromTop (gap);

    const int width = (area.getWidth() - 3 * gap) / 4;
    for (int i = 0; i < slots.size(); ++i)
    {
        slots[i]->setBounds (area.getX() + i * (width + gap), area.getY(), width, area.getHeight());
    }
}

// ---------------------------------------------------------------------------
// Modulation page
// ---------------------------------------------------------------------------

ModPage::ModPage (juce::AudioProcessorValueTreeState& s)
{
    for (int i = 0; i < 3; ++i)
    {
        auto* card = lfos.add (new Card (s, "LFO " + juce::String (i + 1), 2));

        card->addChoice (mk::P ((int) mk::P::Lfo1Shape + i * 7), "SHAPE");
        card->addKnob (mk::P ((int) mk::P::Lfo1Rate + i * 7), "RATE");
        card->addKnob (mk::P ((int) mk::P::Lfo1Cutoff + i * 7), "> CUTOFF");
        card->addKnob (mk::P ((int) mk::P::Lfo1Detune + i * 7), "> DETUNE");
        card->addKnob (mk::P ((int) mk::P::Lfo1Blend + i * 7), "> BLEND");
        card->addKnob (mk::P ((int) mk::P::Lfo1Pan + i * 7), "> PAN");
        card->addKnob (mk::P ((int) mk::P::Lfo1Space + i * 7), "> SPACE");

        addAndMakeVisible (card);
    }
}

void ModPage::resized()
{
    auto area = getLocalBounds();
    const int gap = 8;
    const int width = (area.getWidth() - 2 * gap) / 3;

    for (int i = 0; i < lfos.size(); ++i)
        lfos[i]->setBounds (area.getX() + i * (width + gap), area.getY(), width, area.getHeight());
}

// ---------------------------------------------------------------------------
// Space page
// ---------------------------------------------------------------------------

SpacePage::SpacePage (juce::AudioProcessorValueTreeState& s)
    : delay (s, "STEREO DELAY", 3),
      reverb (s, "SPACE REVERB", 3),
      master (s, "WIDTH & MASTER", 3)
{
    delay.addChoice (mk::P::DelayMode, "MODE");
    delay.addKnob (mk::P::DelayTimeL, "TIME L");
    delay.addKnob (mk::P::DelayTimeR, "TIME R");
    delay.addKnob (mk::P::DelayFeedback, "FEEDBACK");
    delay.addKnob (mk::P::DelayDamp, "DAMPING");
    delay.addKnob (mk::P::DelayMix, "MIX");
    delay.addKnob (mk::P::DelayMod, "MOTION");

    reverb.addKnob (mk::P::ReverbSize, "SIZE");
    reverb.addKnob (mk::P::ReverbDecay, "DECAY");
    reverb.addKnob (mk::P::ReverbDamp, "DAMPING");
    reverb.addKnob (mk::P::ReverbPredelay, "PRE-DELAY");
    reverb.addKnob (mk::P::ReverbWidth, "WIDTH");
    reverb.addKnob (mk::P::ReverbMix, "MIX");

    master.addKnob (mk::P::EnsembleDepth, "ENSEMBLE");
    master.addKnob (mk::P::EnsembleRate, "ENS RATE");
    master.addKnob (mk::P::PadSpace, "PAD SEND");
    master.addKnob (mk::P::MasterWidth, "WIDTH");
    master.addKnob (mk::P::MasterGain, "OUTPUT");
    master.addKnob (mk::P::MasterDrive, "DRIVE");
    master.addKnob (mk::P::MasterLowCut, "LOW CUT");

    for (auto* card : { &delay, &reverb, &master })
        addAndMakeVisible (card);
}

void SpacePage::resized()
{
    auto area = getLocalBounds();
    const int gap = 8;
    const int width = (area.getWidth() - 2 * gap) / 3;

    delay.setBounds (area.removeFromLeft (width));
    area.removeFromLeft (gap);
    reverb.setBounds (area.removeFromLeft (width));
    area.removeFromLeft (gap);
    master.setBounds (area);
}

// ---------------------------------------------------------------------------
// Gate page
// ---------------------------------------------------------------------------

GatePage::GatePage (juce::AudioProcessorValueTreeState& s, GateHostContext context)
    : controls (s, "GATE", 3),
      timing (s, "TIMING & TARGET", 2),
      editor (std::make_unique<GateEditor> (s, std::move (context)))
{
    controls.addChoice (mk::P::GateTarget, "TARGET");
    controls.addKnob (mk::P::GateMix, "MIX");
    controls.addKnob (mk::P::GatePhase, "PHASE");
    controls.addKnob (mk::P::GateMin, "MIN");
    controls.addKnob (mk::P::GateMax, "MAX");
    controls.addKnob (mk::P::GateAttack, "ATTACK");
    controls.addKnob (mk::P::GateRelease, "RELEASE");

    timing.addChoice (mk::P::GateTrigger, "TRIGGER");
    timing.addChoice (mk::P::GateLength, "PATTERN LENGTH");
    timing.addKnob (mk::P::GateTension, "TENSION", true);
    timing.addKnob (mk::P::GateRate, "FREE RATE");

    addAndMakeVisible (controls);
    addAndMakeVisible (timing);
    addAndMakeVisible (*editor);
}

void GatePage::resized()
{
    auto area = getLocalBounds();
    const int gap = 8;

    auto left = area.removeFromLeft (400);
    area.removeFromLeft (gap);

    auto top = left.removeFromTop ((int) (left.getHeight() * 0.62f));
    left.removeFromTop (gap);

    controls.setBounds (top);
    timing.setBounds (left);
    editor->setBounds (area);
}

// ---------------------------------------------------------------------------
// Preset page
// ---------------------------------------------------------------------------

class PresetPage::Tile : public juce::Component{
public:
    Tile (juce::String presetName, juce::String presetCategory, juce::String presetDescription,
          juce::String tag, std::function<void()> onPress)
        : name (std::move (presetName)),
          category (std::move (presetCategory)),
          description (std::move (presetDescription)),
          labelText (std::move (tag)),
          onClick (std::move (onPress))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setSelected (bool shouldBeSelected)
    {
        if (selected != shouldBeSelected)
        {
            selected = shouldBeSelected;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        drawPanelBackground (g, bounds, 8.0f, hover);

        if (selected)
        {
            g.setColour (col::accent);
            g.drawRoundedRectangle (bounds.reduced (1.0f), 8.0f, 2.0f);
        }

        auto area = getLocalBounds().reduced (20, 16);

        // Vertically centre a fixed-height text block inside the tile.
        const int blockHeight = 16 + 28 + 16 + 6 + 52;
        const int top = area.getY() + juce::jmax (0, (area.getHeight() - blockHeight) / 2);
        area = juce::Rectangle<int> (area.getX(), top, area.getWidth(), juce::jmin (blockHeight, area.getHeight()));

        g.setColour (col::textFaint);
        g.setFont (font (11.0f));
        g.drawText (labelText, area.removeFromTop (16), juce::Justification::centredLeft, false);

        g.setColour (selected ? col::accentBright : col::text);
        g.setFont (font (21.0f, true));
        g.drawText (name, area.removeFromTop (28), juce::Justification::centredLeft, false);

        g.setColour (col::accent.withAlpha (0.8f));
        g.setFont (font (11.0f, true));
        g.drawText (category.toUpperCase(), area.removeFromTop (16), juce::Justification::centredLeft, false);

        area.removeFromTop (6);
        g.setColour (col::textDim);
        g.setFont (font (12.5f));
        g.drawFittedText (description, area, juce::Justification::topLeft, 4, 1.0f);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasDraggedSinceMouseDown() && onClick)
            onClick();
    }

private:
    juce::String name, category, description, labelText;
    std::function<void()> onClick;
    bool hover = false, selected = false;
};

PresetPage::~PresetPage() = default;

PresetPage::PresetPage (std::function<void (int)> loadPreset, std::function<int()> currentProgram)
    : load (std::move (loadPreset)), current (std::move (currentProgram))
{
    addAndMakeVisible (bankPrev);
    addAndMakeVisible (bankNext);

    bankLabel.setFont (font (12.0f, true));
    bankLabel.setColour (juce::Label::textColourId, col::textDim);
    bankLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (bankLabel);

    rebuild();
}

void PresetPage::rebuild()
{
    tiles.clear();

    const int total = juce::jmax (1, mk::factoryProgramCount());
    const int pages = juce::jmax (1, (total + kTilesPerPage - 1) / kTilesPerPage);
    bank = juce::jlimit (0, pages - 1, bank);

    const int first = bank * kTilesPerPage;
    const int count = juce::jmin (kTilesPerPage, juce::jmax (0, total - first));

    for (int i = 0; i < count; ++i)
    {
        const int programIndex = first + i;

        juce::String name, category, description;
        if (programIndex == 0)
        {
            name = "Init";
            category = "Default";
            description = "Neutral starting point: warm pad, slow swell, wide space.";
        }
        else
        {
            const auto& preset = mk::factoryPreset (programIndex - 1);
            name = preset.name;
            category = preset.category;
            description = preset.description;
        }

        auto* tile = tiles.add (new Tile (name, category, description,
                                          "PROGRAM " + juce::String (programIndex + 1),
                                          [this, programIndex] { if (load) load (programIndex); }));
        addAndMakeVisible (tile);
    }

    bankLabel.setText ("PROGRAM BANK " + juce::String (bank + 1) + " / " + juce::String (pages)
                           + "   -   " + juce::String (total) + " PROGRAMS",
                       juce::dontSendNotification);
    bankPrev.setEnabled (bank > 0);
    bankNext.setEnabled (bank < pages - 1);

    resized();
}

void PresetPage::showBank (int newBank)
{
    const int total = juce::jmax (1, mk::factoryProgramCount());
    const int pages = juce::jmax (1, (total + kTilesPerPage - 1) / kTilesPerPage);
    const int clamped = juce::jlimit (0, pages - 1, newBank);

    if (clamped == bank)
        return;

    bank = clamped;
    rebuild();
}

void PresetPage::resized()
{
    auto area = getLocalBounds();

    auto bar = area.removeFromTop (34);
    bankPrev.setBounds (bar.removeFromLeft (46).reduced (0, 2));
    bar.removeFromLeft (8);
    bankNext.setBounds (bar.removeFromLeft (46).reduced (0, 2));
    bar.removeFromLeft (12);
    bankLabel.setBounds (bar);

    area.removeFromTop (6);

    const int gap = 10;
    const int columns = 3;
    const int rows = 4;
    const int width = (area.getWidth() - (columns - 1) * gap) / columns;
    const int height = (area.getHeight() - (rows - 1) * gap) / rows;

    for (int i = 0; i < tiles.size(); ++i)
    {
        const int row = i / columns;
        const int column = i % columns;
        tiles[i]->setBounds (area.getX() + column * (width + gap),
                             area.getY() + row * (height + gap), width, height);
    }
}

void PresetPage::refresh()
{
    const int selected = current ? current() : 0;
    const int total = juce::jmax (1, mk::factoryProgramCount());
    const int pages = juce::jmax (1, (total + kTilesPerPage - 1) / kTilesPerPage);
    const int wantedBank = juce::jlimit (0, pages - 1, selected / kTilesPerPage);

    if (wantedBank != bank || tiles.isEmpty())
    {
        bank = wantedBank;
        rebuild();
    }

    const int first = bank * kTilesPerPage;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setSelected (first + i == selected);
}

} // namespace mk::ui
