#include "PluginEditor.h"
#include <limits>

MKRattusTranceGateEditor::MKRattusTranceGateEditor (MKRattusTranceGateProcessor& p)
    : AudioProcessorEditor (&p), processor (p), gateGrid (p)
{
#if JUCE_IOS
    setSize (768, 500);
#else
    setSize (900, 500);
#endif
    title.setText ("MK RATTUS TRANCE GATE", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, juce::Colour (0xffe6edf7));
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    subtitle.setText ("GATE12 RHYTHMIC GATING EFFECT  |  DRAG CURVE POINTS",
                      juce::dontSendNotification);
    subtitle.setFont (juce::Font (juce::FontOptions (11.0f)));
    subtitle.setColour (juce::Label::textColourId, juce::Colour (0xff8fa3b8));
    addAndMakeVisible (subtitle);

    gateEnabled.setButtonText ("Gate On");
    addAndMakeVisible (gateEnabled);
    gateEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.parameters, "gateOn", gateEnabled);

    for (int i = 0; i < processor.parameters.getParameter ("gatePreset")->getNumSteps(); ++i)
        gatePreset.addItem (processor.parameters.getParameter ("gatePreset")->getText (
                                static_cast<float> (i) / static_cast<float> (
                                    juce::jmax (1, processor.parameters.getParameter ("gatePreset")->getNumSteps() - 1)),
                                64), i + 1);
    gatePreset.setSelectedItemIndex (juce::roundToInt (
        processor.parameters.getRawParameterValue ("gatePreset")->load()), juce::dontSendNotification);
    gatePreset.onChange = [this]
    {
        if (gatePreset.getSelectedItemIndex() >= 0)
        {
            processor.applyGatePreset (gatePreset.getSelectedItemIndex());
            timerCallback();
        }
    };
    gatePreset.setViewportIgnoreDragFlag (true);
    addAndMakeVisible (gatePreset);

    randomizeGate.onClick = [this] { processor.randomizeGate(); };
    addAndMakeVisible (randomizeGate);
    addAndMakeVisible (gateGrid);

    const auto populate = [this] (juce::ComboBox& box, const char* id)
    {
        auto* parameter = processor.parameters.getParameter (id);
        for (int i = 0; i < parameter->getNumSteps(); ++i)
            box.addItem (parameter->getText (parameter->convertTo0to1 (static_cast<float> (i)), 64), i + 1);
        addAndMakeVisible (box);
    };
    populate (gateCycle, "gateCycle");
    populate (gateShape, "gateShape");
    populate (gateStereo, "gateStereoMode");
    cycleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.parameters, "gateCycle", gateCycle);
    shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.parameters, "gateShape", gateShape);
    stereoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.parameters, "gateStereoMode", gateStereo);
    gateCycle.setTooltip ("Length of one gate envelope cycle; quarter note equals one beat");
    gateShape.setTooltip ("Gate12 segment type; Curve with zero tension is linear");

    addAndMakeVisible (gateRetrigger);
    retriggerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.parameters, "gateRetrigger", gateRetrigger);
    gateRetrigger.setTooltip ("Restart the gate at each MIDI note-on; off follows host song position");
    addAndMakeVisible (gateFilter);
    filterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.parameters, "gateFilterOn", gateFilter);

    styleSlider (gateSteps);
    styleSlider (gateAttack);
    styleSlider (gateRelease);
    styleSlider (gateTension);
    styleSlider (mix);
    styleSlider (outputGain);
    styleSlider (gateSwing);
    gateSwing.setSliderStyle (juce::Slider::LinearVertical);
    gateSwing.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
    gateSwing.setTextValueSuffix (" %");
    gateSwing.setName ("Gate Swing");
    gateSwing.setTooltip ("Sixteenth-note swing: 0% is straight; 100% delays the offbeat to 2:1 timing");
    swingLabel.setText ("Swing", juce::dontSendNotification);
    swingLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    swingLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb5c9dd));
    addAndMakeVisible (swingLabel);

    const juce::StringArray controlNames { "Attack", "Release", "Curve Tension", "Mix", "Output" };
    for (int i = 0; i < static_cast<int> (controlLabels.size()); ++i)
    {
        auto& label = controlLabels[static_cast<size_t> (i)];
        label.setText (controlNames[i], juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, juce::Colour (0xff8fa3b8));
        addAndMakeVisible (label);
    }
    gateSteps.setTextValueSuffix (" pts");
    gateAttack.setTextValueSuffix (" ms");
    gateRelease.setTextValueSuffix (" ms");
    outputGain.setTextValueSuffix (" dB");
    gateSteps.setTooltip ("Number of editable curve points, from 8 to 32");
    for (auto* slider : { &gateSteps, &gateAttack, &gateRelease, &gateTension, &mix, &outputGain, &gateSwing })
        addAndMakeVisible (*slider);

    gateSteps.setRange (8, 32, 1);
    gateStepsAttachment = std::make_unique<juce::ParameterAttachment> (
        *processor.parameters.getParameter ("gateSteps"),
        [this] (float value) { gateSteps.setValue (value, juce::dontSendNotification); }, nullptr);
    gateStepsAttachment->sendInitialUpdate();
    gateSteps.onValueChange = [this] { processor.setGatePointCount (juce::roundToInt (gateSteps.getValue())); };
    gateAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "gateAttack", gateAttack);
    gateReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "gateRelease", gateRelease);
    tensionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "gateTension", gateTension);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "mix", mix);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "outputGain", outputGain);
    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.parameters, "gateSwing", gateSwing);

    presetDescription.setFont (juce::Font (juce::FontOptions (11.0f)));
    presetDescription.setColour (juce::Label::textColourId, juce::Colour (0xff8fa3b8));
    addAndMakeVisible (presetDescription);
    timerCallback();
    startTimerHz (30);
}

void MKRattusTranceGateEditor::styleSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setViewportIgnoreDragFlag (true);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
    slider.setColour (juce::Slider::trackColourId, juce::Colour (0xff35c4ce));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe7f2ff));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffdce7f3));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1a2532));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff35495e));
}

void MKRattusTranceGateEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101720));
    g.setColour (juce::Colour (0xff1c2936));
    g.fillRoundedRectangle (12.0f, 64.0f, static_cast<float> (getWidth() - 24),
                            static_cast<float> (getHeight() - 94), 10.0f);
    g.setColour (juce::Colour (0xff2b3b4d));
    g.drawRoundedRectangle (12.0f, 64.0f, static_cast<float> (getWidth() - 24),
                            static_cast<float> (getHeight() - 94), 10.0f, 1.0f);
    g.setColour (juce::Colour (0xff8fa3b8));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("Gate12 curve engine by tiagolr / tilr - GPLv3", 24, getHeight() - 22,
                getWidth() - 48, 14, juce::Justification::centredRight);
}

void MKRattusTranceGateEditor::resized()
{
    const auto width = getWidth();
    const auto contentWidth = width - 48;
    title.setBounds (22, 10, 550, 30);
    subtitle.setBounds (24, 40, 560, 18);

    gateEnabled.setBounds (24, 74, 90, 32);
    gatePreset.setBounds (120, 74, contentWidth - 286, 32);
    randomizeGate.setBounds (width - 184, 74, 160, 32);

    const auto cellWidth = contentWidth / 4;
    gateCycle.setBounds (24, 114, cellWidth - 8, 32);
    gateShape.setBounds (24 + cellWidth, 114, cellWidth - 8, 32);
    gateRetrigger.setBounds (24 + cellWidth * 2, 114, cellWidth - 8, 32);
    gateSteps.setBounds (24 + cellWidth * 3, 114, cellWidth - 8, 32);

    swingLabel.setBounds (24, 156, 72, 18);
    gateSwing.setBounds (24, 176, 72, 150);
    gateGrid.setBounds (112, 156, contentWidth - 88, 200);

    const auto lowerCellWidth = (contentWidth - 88) / 5;
    const std::array<juce::Component*, 5> controls {
        &gateAttack, &gateRelease, &gateTension, &mix, &outputGain
    };
    for (size_t i = 0; i < controls.size(); ++i)
    {
        const auto x = 112 + static_cast<int> (i) * lowerCellWidth;
        controlLabels[i].setBounds (x, 366, lowerCellWidth - 8, 14);
        controls[i]->setBounds (x, 380, lowerCellWidth - 8, 32);
    }
    gateFilter.setBounds (112, 420, 130, 28);
    gateStereo.setBounds (248, 420, 150, 28);
    presetDescription.setBounds (408, 416, contentWidth - 384, 40);
}

void MKRattusTranceGateEditor::GateGrid::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff121b27));
    g.fillRoundedRectangle (area, 6.0f);
    const auto steps = processor.getGateStepCount();
    const auto plot = plotBounds();
    std::array<float, 32> levels {}, offsets {};
    for (int step = 0; step < steps; ++step)
    {
        levels[static_cast<size_t> (step)] = processor.getGateStep (step);
        offsets[static_cast<size_t> (step)] = processor.getGatePointOffset (step);
        g.setColour (juce::Colour (0xff27394b));
        const auto x = plot.getX() + plot.getWidth() * static_cast<float> (step) / (steps - 1);
        g.drawVerticalLine (static_cast<int> (x), plot.getY(), plot.getBottom());
    }
    const auto shape = juce::roundToInt (processor.parameters.getRawParameterValue ("gateShape")->load());
    const auto tension = processor.parameters.getRawParameterValue ("gateTension")->load();
    juce::Path curve;
    for (int sample = 0; sample <= 400; ++sample)
    {
        const auto phase = static_cast<float> (sample) / 400.0f;
        const auto level = MKRattusTranceGateProcessor::evaluateGateCurve (
            previewPattern, levels, offsets, steps, shape, phase, tension);
        const auto x = plot.getX() + phase * plot.getWidth();
        const auto y = plot.getBottom() - level * plot.getHeight();
        if (sample == 0) curve.startNewSubPath (x, y);
        else curve.lineTo (x, y);
    }
    g.setColour (juce::Colour (0xfff18b4c));
    g.strokePath (curve, juce::PathStrokeType (2.0f));
    for (int point = 0; point < steps; ++point)
    {
        const auto offset = point == 0 || point == steps - 1 ? 0.0f : offsets[static_cast<size_t> (point)];
        const auto x = plot.getX() + (point + offset) / (steps - 1) * plot.getWidth();
        const auto y = plot.getBottom() - levels[static_cast<size_t> (point)] * plot.getHeight();
        g.setColour (juce::Colour (point == selectedPoint ? 0xfff7faff : 0xff35c4ce));
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
    }
    g.setColour (juce::Colour (0xff35c4ce));
    const auto seek = plot.getX() + processor.getGatePlaybackPhase() * plot.getWidth();
    g.drawVerticalLine (static_cast<int> (seek), plot.getY(), plot.getBottom());
    g.setColour (juce::Colour (0xff60778d));
    g.drawRoundedRectangle (area, 6.0f, 1.0f);
}

void MKRattusTranceGateEditor::GateGrid::setStepFromPosition (juce::Point<float> position)
{
    if (selectedPoint < 0)
        return;
    const auto steps = processor.getGateStepCount();
    const auto plot = plotBounds();
    const auto phase = juce::jlimit (0.0f, 1.0f, (position.x - plot.getX()) / plot.getWidth());
    const auto value = juce::jlimit (0.0f, 1.0f, (plot.getBottom() - position.y) / plot.getHeight());
    processor.setGateStep (selectedPoint, value);
    if (selectedPoint > 0 && selectedPoint < steps - 1)
    {
        auto* parameter = processor.parameters.getParameter (
            MKRattusTranceGateProcessor::gatePositionParameterID (selectedPoint));
        const auto neighbour = [this, steps] (int point) {
            return (point + processor.getGatePointOffset (point)) / (steps - 1);
        };
        const auto lower = neighbour (selectedPoint - 1) + 0.000001f;
        const auto upper = juce::jmax (lower, neighbour (selectedPoint + 1) - 0.000001f);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (lower, upper, phase)));
    }
    repaint();
}

void MKRattusTranceGateEditor::GateGrid::mouseDown (const juce::MouseEvent& event)
{
    const auto steps = processor.getGateStepCount();
    const auto plot = plotBounds();
    auto closest = std::numeric_limits<float>::max();
    for (int point = 0; point < steps; ++point)
    {
        const auto offset = point == 0 || point == steps - 1 ? 0.0f
            : processor.getGatePointOffset (point);
        const juce::Point<float> pos {
            plot.getX() + (point + offset) / (steps - 1) * plot.getWidth(),
            plot.getBottom() - processor.getGateStep (point) * plot.getHeight()
        };
        const auto distance = pos.getDistanceSquaredFrom (event.position);
        if (distance < closest)
        {
            closest = distance;
            selectedPoint = point;
        }
    }
    processor.parameters.getParameter (
        MKRattusTranceGateProcessor::gateStepParameterID (selectedPoint))->beginChangeGesture();
    processor.parameters.getParameter (
        MKRattusTranceGateProcessor::gatePositionParameterID (selectedPoint))->beginChangeGesture();
    setStepFromPosition (event.position);
}

void MKRattusTranceGateEditor::GateGrid::mouseDrag (const juce::MouseEvent& event)
{
    setStepFromPosition (event.position);
}

void MKRattusTranceGateEditor::GateGrid::mouseUp (const juce::MouseEvent&)
{
    if (selectedPoint < 0)
        return;
    processor.parameters.getParameter (
        MKRattusTranceGateProcessor::gateStepParameterID (selectedPoint))->endChangeGesture();
    processor.parameters.getParameter (
        MKRattusTranceGateProcessor::gatePositionParameterID (selectedPoint))->endChangeGesture();
    selectedPoint = -1;
}

juce::Rectangle<float> MKRattusTranceGateEditor::GateGrid::plotBounds() const
{
    return getLocalBounds().toFloat().reduced (12.0f);
}

void MKRattusTranceGateEditor::timerCallback()
{
    gateGrid.repaint();
    const auto preset = juce::roundToInt (processor.parameters.getRawParameterValue ("gatePreset")->load());
    // A user selection is delivered asynchronously; do not overwrite it with the unchanged saved index.
    if (preset != lastSyncedPreset)
    {
        lastSyncedPreset = preset;
        gatePreset.setSelectedItemIndex (preset, juce::dontSendNotification);
        presetDescription.setText (processor.getGatePresetDescription(), juce::dontSendNotification);
    }
}
