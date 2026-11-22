#pragma once

// A polyphonic pad voice: two morphing wavetable oscillators (each with up to
// seven unison voices), a stereo multimode filter driven by its own envelope,
// a curved amplitude envelope and an independent space send.

#include "Envelope.h"
#include "Filters.h"
#include "Oscillator.h"
#include "Wavetables.h"

namespace mk
{

// Everything a voice needs, already resolved from parameters and modulation.
struct VoiceControls
{
    int waveA = WaveWarmPad;
    int waveB = WaveHollow;
    float posA = 0.35f, posB = 0.5f;
    float levelA = 0.75f, levelB = 0.4f;
    int unisonA = 5, unisonB = 3;
    float detuneA = 12.0f, detuneB = 9.0f;
    float spreadA = 0.7f, spreadB = 0.6f;
    float intervalSemitones = -12.0f;
    float drift = 0.35f;

    int filterMode = MultimodeFilter::LowPass24;
    float cutoffHz = 1400.0f;
    float resonance = 0.22f;
    float filterDrive = 0.1f;
    float filterEnvAmount = 0.3f;
    float keyTrack = 0.3f;
    float lfoCutoffSemitones = 0.0f;

    float blendOffset = 0.0f;
    float panMod = 0.0f;
    float spaceSend = 0.35f;
    float glideCoeff = 1.0f;
    float blockSeconds = 0.000666f;
    int blockSamples = 32;

    float ampAttack = 2.5f, ampDecay = 3.0f, ampSustain = 0.8f, ampRelease = 6.0f, ampCurve = 0.2f;
    float filtAttack = 1.6f, filtDecay = 2.5f, filtSustain = 0.4f, filtRelease = 5.5f;
};

class Voice
{
public:
    void prepare (double sampleRate, uint32_t seed);
    void reset();

    void noteOn (int midiNote, float velocity, bool glide);
    void noteOff();
    void panic();

    bool isActive() const noexcept { return ! ampEnv.hasFinished(); }
    bool isHeld() const noexcept { return held; }
    int note() const noexcept { return targetNote; }
    float envelopeLevel() const noexcept { return ampEnv.getValue(); }
    float age() const noexcept { return ageCounter; }

    // n must not exceed the engine's control block size (32 samples).
    void render (float* outL, float* outR, float* sendL, float* sendR, int numSamples, const VoiceControls& c);

private:
    void updateControlBlock (const VoiceControls& c);

    UnisonOscillator oscA, oscB;
    MultimodeFilter filterL, filterR;
    AdsrEnvelope ampEnv, filterEnv;

    int targetNote = 60;
    float currentNote = 60.0f;
    float velocityGain = 0.0f;
    float ageCounter = 0.0f;
    bool held = false;

    float morphA = 0.35f, morphB = 0.5f;
    float midLGain = 1.0f, midRGain = 1.0f;
    float sideGain = 1.0f;
    float spaceSend = 0.35f;
};

} // namespace mk
