#pragma once

#include "DSP/SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <set>
#include <vector>

namespace mk
{

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MK Ambient"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Program helpers used by the editor.
    juce::String getProgramCategory (int index) const;
    void loadProgram (int index);
    int currentProgramIndex() const noexcept { return currentProgram; }

    juce::AudioProcessorValueTreeState apvts;
    SynthEngine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void renderChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void handleMidiMessage (const juce::MidiMessage& message);

    std::array<std::atomic<float>*, (size_t) kParamCount> rawParams {};
    std::vector<float> scratchL, scratchR;
    std::set<int> sustainedNotes;
    bool sustainPedal = false;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};

} // namespace mk
