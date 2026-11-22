#pragma once

// Four independent noise / texture layers.
//
// Each slot owns its source (procedural colour or a short catalogue sample),
// its own filter, LFO, pan, drive, swell envelope and delay / reverb sends,
// so layers can move completely independently of each other and of the pad.

#include "Filters.h"
#include "Lfo.h"
#include "NoiseGen.h"
#include "Params.h"
#include "SampleLibrary.h"
#include "SamplePlayer.h"

namespace mk
{

struct SlotControls
{
    int sourceIndex = 0;
    float level = 0.0f;
    int mode = 0;
    float tuneSemitones = 0.0f;
    int filterType = 0;
    float cutoff = 2000.0f;
    float resonance = 0.3f;
    float pan = 0.0f;
    float drive = 0.1f;
    float delaySend = 0.2f;
    float reverbSend = 0.35f;
    float lfoRate = 0.07f;
    int lfoShape = LfoSine;
    float lfoCutoffDepth = 0.3f;
    float lfoPanDepth = 0.2f;
    float lfoSpaceDepth = 0.15f;
    float swellSeconds = 3.0f;
    float grainSizeMs = 90.0f;
    float grainSpray = 0.5f;
};

class NoiseSlot
{
public:
    void prepare (double sampleRate, uint32_t seed);
    void reset();

    void setSource (const SampleLibrary& library, int sourceIndex);
    void noteOn();
    void noteOff();

    void render (float* outL, float* outR, float* delayL, float* delayR,
                 float* revL, float* revR, int numSamples, const SlotControls& c);

    float meter() const noexcept { return lastPeak; }

private:
    NoiseGenerator noiseGen;
    SamplePlayer player;
    Svf filter;
    Lfo lfo;
    DcBlocker dc;
    XorShiftRng rng { 0xB5297A4Du };

    double sr = 48000.0;
    bool gateOpen = false;
    float gatePhase = 0.0f;
    float gateLevel = 0.0f;
    float lastPeak = 0.0f;
    int currentSource = -1;
    float panL = 0.70710678f, panR = 0.70710678f;
};

class NoiseEngine
{
public:
    void prepare (double sampleRate, const SampleLibrary& library);
    void reset();

    void noteOn();
    void noteOff();
    void panic();

    void render (float* outL, float* outR, float* delayL, float* delayR,
                 float* revL, float* revR, int numSamples, const ParamSnapshot& p);

    float slotMeter (int slot) const noexcept;

private:
    std::array<NoiseSlot, (size_t) kNoiseSlotCount> slots;
    const SampleLibrary* libraryPtr = nullptr;
    double sr = 48000.0;
};

} // namespace mk
