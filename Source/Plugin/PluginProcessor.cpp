#include "PluginProcessor.h"

#include "DSP/Presets.h"
#include "PluginEditor.h"

namespace mk
{

namespace
{

juce::String formatValue (const ParamSpec& spec, float value)
{
    switch (spec.unit)
    {
        case U_Pct:
        {
            const int percent = juce::roundToInt (value * 100.0f);
            return juce::String (percent) + "%";
        }
        case U_Hz:
            if (value >= 1000.0f)
                return juce::String (value / 1000.0f, 2) + " kHz";
            if (value >= 100.0f)
                return juce::String (juce::roundToInt (value)) + " Hz";
            if (value >= 10.0f)
                return juce::String (value, 1) + " Hz";
            return juce::String (value, 2) + " Hz";
        case U_Sec:
            return value < 1.0f ? juce::String (juce::roundToInt (value * 1000.0f)) + " ms"
                                : juce::String (value, 2) + " s";
        case U_Ms:
            return juce::String (juce::roundToInt (value)) + " ms";
        case U_Db:
            return juce::String (value, 1) + " dB";
        case U_Semi:
            return (value > 0.0f ? "+" : "") + juce::String (juce::roundToInt (value)) + " st";
        case U_Cent:
            return juce::String (juce::roundToInt (value)) + " ct";
        case U_Switch:
            return value > 0.5f ? "On" : "Off";
        case U_None:
        default:
            return juce::String (juce::roundToInt (value));
    }
}

juce::NormalisableRange<float> makeRange (const ParamSpec& spec)
{
    juce::NormalisableRange<float> range (spec.minValue, spec.maxValue);

    if (spec.isStepped() && ! spec.isChoice())
        range.interval = 1.0f;

    // Musical ranges feel wrong when linear: put the useful middle of the
    // range near the centre of the knob travel.
    switch (spec.unit)
    {
        case U_Hz:
            range.setSkewForCentre (std::sqrt (spec.minValue * spec.maxValue));
            break;
        case U_Ms:
            range.setSkewForCentre (std::sqrt (spec.minValue * spec.maxValue));
            break;
        case U_Sec:
            range.setSkewForCentre (juce::jlimit (spec.minValue, spec.maxValue, 0.35f));
            break;
        default:
            break;
    }
    return range;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kParamCount; ++i)
    {
        const auto& spec = paramSpecByIndex (i);

        if (spec.isChoice())
        {
            juce::StringArray items;
            for (int c = 0; c < spec.choices->count; ++c)
                items.add (spec.choices->names[c]);

            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { spec.key, 1 }, spec.label, items,
                juce::jlimit (0, items.size() - 1, (int) spec.defaultValue)));
        }
        else
        {
            auto attributes = juce::AudioParameterFloatAttributes()
                                  .withStringFromValueFunction ([spec] (float value, int)
                                                                { return formatValue (spec, value); });

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { spec.key, 1 }, spec.label, makeRange (spec), spec.defaultValue,
                attributes));
        }
    }

    return layout;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MKAMBIENT", createLayout())
{
    for (int i = 0; i < kParamCount; ++i)
        rawParams[(size_t) i] = apvts.getRawParameterValue (paramSpecByIndex (i).key);

    // Start on the Init program.
    loadProgram (0);
}

PluginProcessor::~PluginProcessor() = default;

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jlimit (32, 8192, samplesPerBlock));
    scratchL.assign ((size_t) juce::jmax (32, samplesPerBlock), 0.0f);
    scratchR.assign ((size_t) juce::jmax (32, samplesPerBlock), 0.0f);
    sustainedNotes.clear();
    sustainPedal = false;

    // The master limiter has a 2 ms look-ahead; tell the host so it can
    // compensate.
    setLatencySamples (juce::roundToInt (0.002 * sampleRate));
}

void PluginProcessor::releaseResources()
{
    engine.reset();
    engine.allNotesOff (true);
}

void PluginProcessor::renderChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    float* left = scratchL.data();
    float* right = scratchR.data();
    engine.process (left, right, numSamples);

    const int channels = buffer.getNumChannels();
    if (channels >= 2)
    {
        buffer.copyFrom (0, startSample, left, numSamples);
        buffer.copyFrom (1, startSample, right, numSamples);
        for (int ch = 2; ch < channels; ++ch)
            buffer.clear (ch, startSample, numSamples);
    }
    else if (channels == 1)
    {
        auto* destination = buffer.getWritePointer (0, startSample);
        for (int i = 0; i < numSamples; ++i)
            destination[i] = 0.5f * (left[i] + right[i]);
    }
}

void PluginProcessor::keyboardNoteOn (int note, float velocity)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    keyboardFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 + size2 <= 0)
        return; // UI pushed faster than the audio thread drained: drop the event.

    auto& event = keyboardEvents[(size_t) (size1 > 0 ? start1 : start2)];
    event.note = juce::jlimit (0, 127, note);
    event.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    event.on = true;
    keyboardFifo.finishedWrite (1);
}

void PluginProcessor::keyboardNoteOff (int note)
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    keyboardFifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 + size2 <= 0)
        return;

    auto& event = keyboardEvents[(size_t) (size1 > 0 ? start1 : start2)];
    event.note = juce::jlimit (0, 127, note);
    event.velocity = 0.0f;
    event.on = false;
    keyboardFifo.finishedWrite (1);
}

void PluginProcessor::mergeKeyboardNotes (juce::MidiBuffer& midiMessages)
{
    const int ready = keyboardFifo.getNumReady();
    if (ready <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    keyboardFifo.prepareToRead (ready, start1, size1, start2, size2);

    for (int i = 0; i < size1 + size2; ++i)
    {
        const auto& event = keyboardEvents[(size_t) (i < size1 ? start1 + i : start2 + (i - size1))];
        midiMessages.addEvent (event.on ? juce::MidiMessage::noteOn (1, event.note, event.velocity)
                                        : juce::MidiMessage::noteOff (1, event.note),
                               0);
    }

    keyboardFifo.finishedRead (size1 + size2);
}

void PluginProcessor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        const int note = message.getNoteNumber();
        sustainedNotes.erase (note);
        engine.noteOn (note, message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        const int note = message.getNoteNumber();
        if (sustainPedal)
            sustainedNotes.insert (note);
        else
            engine.noteOff (note);
    }
    else if (message.isAllNotesOff())
    {
        sustainedNotes.clear();
        engine.allNotesOff (false);
    }
    else if (message.isAllSoundOff())
    {
        sustainedNotes.clear();
        engine.allNotesOff (true);
    }
    else if (message.isSustainPedalOn())
    {
        sustainPedal = true;
    }
    else if (message.isSustainPedalOff())
    {
        sustainPedal = false;
        for (int note : sustainedNotes)
            engine.noteOff (note);
        sustainedNotes.clear();
    }
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    mergeKeyboardNotes (midiMessages);

    // Push host automation into the engine's lock-free parameter store.
    for (int i = 0; i < kParamCount; ++i)
    {
        if (auto* raw = rawParams[(size_t) i])
            engine.params().setByIndex (i, raw->load (std::memory_order_relaxed));
    }

    // Transport information for the tempo-synced trance gate.
    if (auto* playHead = getPlayHead())
    {
        if (const auto info = playHead->getPosition())
        {
            if (const auto bpm = info->getBpm())
                engine.setTempo (*bpm);
            engine.setTransportPlaying (info->getIsPlaying());
        }
    }

    int position = 0;
    for (const auto metadata : midiMessages)
    {
        const int eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        while (position < eventPosition)
        {
            const int chunk = juce::jmin (eventPosition - position, (int) scratchL.size());
            renderChunk (buffer, position, chunk);
            position += chunk;
        }
        handleMidiMessage (metadata.getMessage());
    }

    while (position < numSamples)
    {
        const int chunk = juce::jmin (numSamples - position, (int) scratchL.size());
        renderChunk (buffer, position, chunk);
        position += chunk;
    }
}

// ---------------------------------------------------------------------------
// Programs
// ---------------------------------------------------------------------------

int PluginProcessor::getNumPrograms()
{
    return factoryProgramCount(); // Init + 100 factory presets
}

int PluginProcessor::getCurrentProgram()
{
    return currentProgram;
}

const juce::String PluginProcessor::getProgramName (int index)
{
    if (index <= 0)
        return "Init";
    return factoryPreset (index - 1).name;
}

juce::String PluginProcessor::getProgramCategory (int index) const
{
    if (index <= 0)
        return "Default";
    return factoryPreset (index - 1).category;
}

void PluginProcessor::changeProgramName (int, const juce::String&) {}

void PluginProcessor::loadProgram (int index)
{
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, index);

    const auto snapshot = currentProgram == 0 ? initSnapshot() : factoryPresetSnapshot (currentProgram - 1);

    for (int i = 0; i < kParamCount; ++i)
    {
        if (auto* parameter = apvts.getParameter (paramSpecByIndex (i).key))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (snapshot[(size_t) i]));
    }

    // Targets only: the smoothers glide to the new patch on the audio thread,
    // which is click-free and avoids touching audio-thread state from the UI.
    engine.params().setAll (snapshot);
}

void PluginProcessor::setCurrentProgram (int index)
{
    loadProgram (index);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("programName", getProgramName (currentProgram), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            apvts.replaceState (tree);
            // The engine copies the restored parameter values on the next block.
        }
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

} // namespace mk

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new mk::PluginProcessor();
}
