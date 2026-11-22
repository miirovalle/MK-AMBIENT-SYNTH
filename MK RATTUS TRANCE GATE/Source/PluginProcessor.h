#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <Pattern.h>

class MKRattusTranceGateProcessor final : public juce::AudioProcessor
{
public:
    MKRattusTranceGateProcessor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MK RATTUS TRANCE GATE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setGateStep (int step, float value);
    float getGateStep (int step) const;
    int getGateStepCount() const;
    void setGatePointCount (int count);
    void applyGatePreset (int presetIndex);
    void randomizeGate();
    static juce::StringArray getGatePresetNames();
    juce::String getGatePresetDescription() const;
    float getGatePointOffset (int point) const;
    static juce::String gateStepParameterID (int step);
    static juce::String gatePositionParameterID (int point);
    static float evaluateGateCurve (Pattern& evaluator, const std::array<float, 32>& levels,
                                    const std::array<float, 32>& offsets,
                                    int count, int shape, float phase, float tension = 0.0f);
    static double swingGatePosition (double beats, float amount) noexcept;
    float getGatePlaybackPhase() const { return gatePlaybackPhase.load(); }

    juce::AudioProcessorValueTreeState parameters;

private:
    struct Controls
    {
        std::array<float, 32> gate {};
        std::array<float, 32> gateOffsets {};
        int gateSteps = 16;
        int gateShape = 0;
        bool gateRetrigger = false;
        double gateCycleBeats = 1.0;
        bool gateOn = true;
        bool gateFilterOn = false;
        int gateStereoMode = 0;
        float gateAttack = 2.0f, gateRelease = 15.0f;
        float gateTension = 0.0f;
        float outputGain = 1.0f;
    };

    void updateControls (Controls&);

    double currentSampleRate = 44100.0;
    double gatePhase = 0.0;
    float smoothedGate = 0.0f, smoothedGateRight = 0.0f;
    std::array<std::array<float, 2>, 2> gateFilterState {};
    juce::SmoothedValue<float> gateSwingSmoothing, mixSmoothing;
    std::atomic<float> gatePlaybackPhase { 0.0f };
    Pattern gatePattern { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MKRattusTranceGateProcessor)
};
