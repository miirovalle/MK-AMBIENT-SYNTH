#pragma once

// Top-level synth: voice allocation, the four-slot texture engine, the two
// effect returns, the master chain and metering. This is the only class the
// plugin wrapper / AUv3 host needs to talk to.

#include "Effects.h"
#include "Lfo.h"
#include "NoiseEngine.h"
#include "ParamStore.h"
#include "Params.h"
#include "SampleLibrary.h"
#include "TranceGate.h"
#include "Voice.h"

#include <atomic>

namespace mk
{

class SynthEngine
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kControlBlockSize = 32;

    SynthEngine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    ParamStore& params() noexcept { return store; }
    const ParamStore& params() const noexcept { return store; }

    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);
    void allNotesOff (bool immediate = false);

    // Host tempo for the trance gate. Safe to call every block.
    void setTempo (double bpm) noexcept { hostBpm = bpm > 1.0 ? bpm : 120.0; }
    void setTransportPlaying (bool isPlaying) noexcept { transportPlaying = isPlaying; }
    double tempo() const noexcept { return hostBpm; }

    float gatePhase() const noexcept { return gatePhase01.load (std::memory_order_relaxed); }
    float gateLevel() const noexcept { return gateLevelValue.load (std::memory_order_relaxed); }

    // Interleaved-free stereo processing; buffers may be the same pointer.
    void process (float* left, float* right, int numSamples);

    int activeVoiceCount() const noexcept;
    int heldVoiceCount() const noexcept;

    // Voice budget after the adaptive load guard (never above the Polyphony
    // parameter). On slower hardware the guard lowers it as load rises, so the
    // audio thread cannot be overrun by a 16-voice chord plus four textures.
    int effectiveVoiceLimit() const noexcept { return adaptiveVoiceLimit; }
    void setAdaptivePolyphony (bool shouldAdapt) noexcept { adaptPolyphony = shouldAdapt; }

    float outputMeter() const noexcept { return meterOut.load (std::memory_order_relaxed); }
    float padMeter() const noexcept { return meterPad.load (std::memory_order_relaxed); }
    float textureMeter (int slot) const noexcept { return noise.slotMeter (slot); }

    double sampleRate() const noexcept { return sr; }
    int maximumBlockSize() const noexcept { return maxBlock; }
    const SampleLibrary& sampleLibrary() const noexcept { return library; }

    double lastBlockMilliseconds() const noexcept { return lastBlockMs; }

private:
    void renderBlock (float* left, float* right, int numSamples);
    Voice* allocateVoice();
    Voice* findHeldVoice (int midiNote) noexcept;
    int countHeldVoices() const noexcept;
    void updateGlobalLfoRates (const ParamSnapshot& p) noexcept;
    GateSettings makeGateSettings (const ParamSnapshot& p) const noexcept;
    void updateLoadGuard (int numSamples) noexcept;

    std::array<Voice, (size_t) kMaxVoices> voices;
    std::array<Lfo, 3> globalLfos;

    ParamStore store;
    SampleLibrary library;
    NoiseEngine noise;
    TranceGate gate;

    Ensemble ensemble;
    Saturator masterSaturator;
    StereoDelay delay;
    SpaceReverb reverb;
    Limiter limiter;
    Biquad masterLowCutL, masterLowCutR;
    Biquad delayHighPassL, delayHighPassR;

    std::vector<float> padL, padR, padRevL, padRevR;
    std::vector<float> noiseL, noiseR, delaySendL, delaySendR, revSendL, revSendR;
    std::vector<float> delayWetL, delayWetR, revWetL, revWetR;

    double sr = 48000.0;
    int maxBlock = 512;
    float lfoValues[3] = { 0.0f, 0.0f, 0.0f };
    double lastBlockMs = 0.0;
    double hostBpm = 120.0;
    bool transportPlaying = false;

    int adaptiveVoiceLimit = kMaxVoices;
    int overloadBlocks = 0;
    int calmBlocks = 0;
    bool adaptPolyphony = true;

    std::atomic<float> meterOut { 0.0f };
    std::atomic<float> meterPad { 0.0f };
    std::atomic<float> gatePhase01 { 0.0f };
    std::atomic<float> gateLevelValue { 1.0f };
};

} // namespace mk
