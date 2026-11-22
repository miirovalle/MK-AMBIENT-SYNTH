#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/GatePresets.h"
#include <cmath>
#include <iostream>

namespace
{
double energyIn (const juce::AudioBuffer<float>& buffer, int start, int end)
{
    double energy = 0.0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = start; sample < end; ++sample)
        {
            const auto value = buffer.getSample (channel, sample);
            if (! std::isfinite (value))
                return -1.0;
            energy += static_cast<double> (value) * value;
        }
    return energy;
}

bool check (bool passed, const char* description)
{
    if (! passed)
        std::cerr << description << '\n';
    return passed;
}

void setParameter (MKRattusTranceGateProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter (id);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void fillNoise (juce::AudioBuffer<float>& buffer)
{
    uint32_t random = 0x2545f491u;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            buffer.setSample (channel, sample,
                              static_cast<float> (random >> 8) * (1.4f / 16777215.0f) - 0.7f);
        }
}

class TestPlayHead final : public juce::AudioPlayHead
{
public:
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo position;
        position.setPpqPosition (ppq);
        position.setBpm (bpm);
        position.setIsPlaying (true);
        return position;
    }
};

bool testGateTiming()
{
    MKRattusTranceGateProcessor processor;
    TestPlayHead host;
    processor.setPlayHead (&host);
    setParameter (processor, "gateAttack", 0.1f);
    setParameter (processor, "gateRelease", 0.1f);
    processor.applyGatePreset (36); // 1/4 Classic Gate
    processor.prepareToPlay (8000.0, 4000);
    juce::AudioBuffer<float> buffer (2, 4000);
    juce::MidiBuffer midi;
    fillNoise (buffer);
    processor.processBlock (buffer, midi);
    if (! check (energyIn (buffer, 500, 1800) > 0.001
                 && energyIn (buffer, 2500, 3800) == 0.0,
                 "Quarter-note gate must open and close within one beat at 120 BPM"))
        return false;
    host.ppq = 0.7;
    fillNoise (buffer);
    processor.processBlock (buffer, midi);
    if (! check (energyIn (buffer, 100, 900) == 0.0,
                 "Host seek must position the gate in its closed section"))
        return false;

    processor.applyGatePreset (45); // 1/8 Classic Gate
    host.ppq = 0.0;
    fillNoise (buffer);
    processor.processBlock (buffer, midi);
    if (! check (energyIn (buffer, 200, 900) > 0.001
                 && energyIn (buffer, 1400, 1800) == 0.0
                 && energyIn (buffer, 2300, 2900) > 0.001,
                 "Eighth-note preset must run twice as fast as quarter-note preset"))
        return false;

    processor.applyGatePreset (36);
    setParameter (processor, "gateRetrigger", 1.0f);
    host.ppq = 0.7;
    midi.addEvent (juce::MidiMessage::noteOn (1, 62, 1.0f), 1000);
    fillNoise (buffer);
    processor.processBlock (buffer, midi);
    if (! check (energyIn (buffer, 1200, 2000) > 0.001,
                 "MIDI retrigger must restart the envelope at the event offset"))
        return false;

    setParameter (processor, "gateRetrigger", 0.0f);
    host.ppq = 0.0;
    host.bpm = 240.0;
    fillNoise (buffer);
    processor.processBlock (buffer, midi);
    if (! check (energyIn (buffer, 200, 900) > 0.001
                 && energyIn (buffer, 1400, 1800) == 0.0,
                 "Host tempo doubling must double gate speed"))
        return false;

    for (int point = 0; point < 32; ++point)
        processor.setGateStep (point, 0.0f);
    fillNoise (buffer);
    processor.processBlock (buffer, midi);
    return check (energyIn (buffer, 100, 4000) == 0.0, "Zero curve must completely close the gate");
}

bool testBypassAndMix()
{
    MKRattusTranceGateProcessor gated, open, dryMix;
    for (auto* processor : { &gated, &open, &dryMix })
        processor->applyGatePreset (36);
    setParameter (open, "gateOn", 0.0f);
    setParameter (dryMix, "mix", 0.0f);
    juce::AudioBuffer<float> reference (2, 8000), a (2, 8000), b (2, 8000), c (2, 8000);
    fillNoise (reference);
    for (auto* buffer : { &a, &b, &c })
        buffer->makeCopyOf (reference);
    juce::MidiBuffer midi;
    for (auto* processor : { &gated, &open, &dryMix })
        processor->prepareToPlay (8000.0, 8000);
    gated.processBlock (a, midi);
    open.processBlock (b, midi);
    dryMix.processBlock (c, midi);
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 8000; ++sample)
        {
            if (! check (c.getSample (channel, sample) == reference.getSample (channel, sample),
                         "Zero mix must pass the dry input through exactly"))
                return false;
            if (sample > 1000 && ! check (std::abs (b.getSample (channel, sample)
                                                    - reference.getSample (channel, sample)) < 0.01f,
                                          "Gate Off must pass audio through at unity gain"))
                return false;
        }
    return check (energyIn (a, 2500, 3800) == 0.0 && energyIn (a, 500, 1800) > 0.001,
                  "The enabled gate must chop the same input the bypass checks passed through");
}

bool testSwing()
{
    using Processor = MKRattusTranceGateProcessor;
    for (int pair = -8; pair <= 8; ++pair)
        for (float amount : { 0.0f, 50.0f, 100.0f })
        {
            const auto start = pair * 0.5;
            const auto delayedOffbeat = start + 0.25 + amount / 1200.0;
            if (! check (Processor::swingGatePosition (start, amount) == start
                         && std::abs (Processor::swingGatePosition (delayedOffbeat, amount)
                                      - (start + 0.25)) < 0.000001,
                         "Swing must delay offbeat sixteenths without moving eighth-note boundaries"))
                return false;
        }
    Processor straight, swung;
    TestPlayHead host;
    const auto preset = Processor::getGatePresetNames().indexOf ("1/16 Classic Gate");
    for (auto* processor : { &straight, &swung })
    {
        processor->applyGatePreset (preset);
        processor->setPlayHead (&host);
    }
    setParameter (swung, "gateSwing", 100.0f);
    for (auto* processor : { &straight, &swung })
        processor->prepareToPlay (8000.0, 8000);
    juce::AudioBuffer<float> a (2, 8000), b (2, 8000);
    fillNoise (a);
    b.makeCopyOf (a);
    juce::MidiBuffer midi;
    straight.processBlock (a, midi);
    swung.processBlock (b, midi);
    return check (energyIn (a, 1100, 1200) > 0 && energyIn (b, 1100, 1200) == 0
                  && energyIn (a, 1560, 1640) == 0 && energyIn (b, 1560, 1640) > 0
                  && energyIn (a, 2200, 2300) > 0 && energyIn (b, 2200, 2300) > 0,
                  "100% swing must move the second sixteenth-note onset from 1000 to 1333 samples");
}

bool testStereoAndFilter()
{
    MKRattusTranceGateProcessor processor;
    const auto names = processor.getGatePresetNames();
    processor.applyGatePreset (names.indexOf ("Stereo Alternating Gate"));
    processor.prepareToPlay (8000.0, 8000);
    juce::AudioBuffer<float> stereo (2, 8000);
    fillNoise (stereo);
    juce::MidiBuffer midi;
    processor.processBlock (stereo, midi);
    double leftFirst = 0, rightFirst = 0, leftSecond = 0, rightSecond = 0;
    for (int i = 200; i < 3800; ++i)
    {
        leftFirst += std::pow (stereo.getSample (0, i), 2);
        rightFirst += std::pow (stereo.getSample (1, i), 2);
        leftSecond += std::pow (stereo.getSample (0, i + 4000), 2);
        rightSecond += std::pow (stereo.getSample (1, i + 4000), 2);
    }
    if (! check (leftFirst > 0.01 && rightFirst == 0 && leftSecond == 0 && rightSecond > 0.01,
                 "Stereo alternating preset must actually switch audible channels"))
        return false;

    MKRattusTranceGateProcessor filtered, amplitude;
    for (auto* p : { &filtered, &amplitude })
    {
        p->applyGatePreset (names.indexOf ("1/8 Triplet Filter Gate"));
        p->prepareToPlay (8000.0, 8000);
    }
    setParameter (amplitude, "gateFilterOn", 0.0f);
    juce::AudioBuffer<float> a (2, 8000), b (2, 8000);
    fillNoise (a);
    b.makeCopyOf (a);
    filtered.processBlock (a, midi);
    amplitude.processBlock (b, midi);
    if (! check (energyIn (a, 100, 8000) > 0 && energyIn (a, 100, 8000) < energyIn (b, 100, 8000) * 0.9,
                 "Filter Gate presets must apply real lowpass filtering"))
        return false;
    processor.applyGatePreset (36);
    return check (processor.parameters.getRawParameterValue ("gateFilterOn")->load() == 0
                  && processor.parameters.getRawParameterValue ("gateStereoMode")->load() == 0,
                  "Ordinary presets must reset stereo and filter gating");
}

bool testPresets()
{
    MKRattusTranceGateProcessor processor;
    processor.prepareToPlay (8000.0, 2000);
    const auto names = processor.getGatePresetNames();
    juce::AudioBuffer<float> buffer (2, 2000);
    juce::MidiBuffer midi;
    for (int index = 0; index < names.size(); ++index)
    {
        const auto preset = makeGatePreset (names[index]);
        processor.applyGatePreset (index);
        if (! check (processor.getGateStepCount() == static_cast<int> (preset.points.size())
                     && preset.description.isNotEmpty(),
                     "Every named preset must apply its authored point count and description"))
            return false;
        for (size_t i = 0; i < preset.points.size(); ++i)
        {
            const auto actualX = (static_cast<float> (i) + processor.getGatePointOffset (static_cast<int> (i)))
                               / static_cast<float> (preset.points.size() - 1);
            if (! check (std::abs (actualX - preset.points[i].x) < 0.00002f
                         && std::abs (processor.getGateStep (static_cast<int> (i)) - preset.points[i].y) < 0.0011f,
                         "Applying presets must preserve their actual curve times and levels"))
            {
                std::cerr << names[index] << ", point " << i << '\n';
                return false;
            }
        }
        fillNoise (buffer);
        processor.processBlock (buffer, midi);
        if (! check (energyIn (buffer, 0, 2000) >= 0.0, "Every preset must render finite audio"))
        {
            std::cerr << names[index] << '\n';
            return false;
        }
    }
    return true;
}

bool testState()
{
    MKRattusTranceGateProcessor source, restored;
    source.applyGatePreset (45);
    source.setGateStep (4, 0.63f);
    setParameter (source, "gateX05", 0.3f);
    setParameter (source, "gateShape", 8.0f);
    setParameter (source, "gateTension", -0.3f);
    setParameter (source, "gateSwing", 37.5f);
    setParameter (source, "gateFilterOn", 1.0f);
    setParameter (source, "gateStereoMode", 2.0f);
    setParameter (source, "mix", 0.8f);
    juce::MemoryBlock state;
    source.getStateInformation (state);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    for (const auto* id : { "gateStep05", "gateX05", "gateShape", "gateTension", "gateCycle",
                            "gateSwing", "gateFilterOn", "gateStereoMode", "mix" })
        if (! check (std::abs (source.parameters.getRawParameterValue (id)->load()
                               - restored.parameters.getRawParameterValue (id)->load()) < 0.0001f,
                     "Edited gate curves and controls must survive session restore"))
            return false;
    return true;
}

bool testEditor()
{
    MKRattusTranceGateProcessor processor;
    processor.prepareToPlay (8000.0, 2000);
    MKRattusTranceGateEditor editor (processor);
    juce::Slider* swing = nullptr;
    juce::ComboBox* menu = nullptr;
    const auto names = processor.getGatePresetNames();
    for (auto* child : editor.getChildren())
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (child); slider != nullptr && slider->getName() == "Gate Swing")
            swing = slider;
        if (auto* box = dynamic_cast<juce::ComboBox*> (child); box != nullptr && box->getNumItems() == names.size())
            menu = box;
    }
    if (! check (swing != nullptr && swing->isVertical() && swing->getHeight() == 150 && swing->getX() == 24,
                 "Swing must be a 150 px vertical slider at the left of the grid"))
        return false;
    for (auto* child : editor.getChildren())
        if (child != swing && ! check (! child->getBounds().intersects (swing->getBounds()),
                                       "The swing slider must not overlap any other control"))
            return false;
    swing->setValue (50, juce::sendNotificationSync);
    if (! check (processor.parameters.getRawParameterValue ("gateSwing")->load() == 50,
                 "The swing slider must control its automatable DSP parameter"))
        return false;
    if (! check (menu != nullptr, "The preset menu must expose every supplied preset"))
        return false;
    juce::AudioBuffer<float> buffer (2, 2000);
    juce::MidiBuffer midi;
    for (int index = 0; index < names.size(); ++index)
    {
        menu->setSelectedItemIndex (index, juce::sendNotificationSync);
        const auto expected = makeGatePreset (names[index]);
        if (! check (juce::roundToInt (processor.parameters.getRawParameterValue ("gatePreset")->load()) == index
                     && processor.getGateStepCount() == static_cast<int> (expected.points.size())
                     && processor.parameters.getRawParameterValue ("gateCycle")->load() == expected.cycle,
                     "Every menu selection must apply its preset, not just change the label"))
        {
            std::cerr << names[index] << '\n';
            return false;
        }
        fillNoise (buffer);
        processor.processBlock (buffer, midi);
        if (! check (energyIn (buffer, 0, 2000) >= 0.0, "Menu-selected presets must render finite audio"))
            return false;
    }
    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (! testGateTiming() || ! testBypassAndMix() || ! testSwing()
        || ! testStereoAndFilter() || ! testPresets() || ! testState() || ! testEditor())
        return 1;
    return 0;
}
