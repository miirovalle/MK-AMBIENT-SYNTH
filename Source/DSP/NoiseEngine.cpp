#include "NoiseEngine.h"

namespace mk
{

// ---------------------------------------------------------------------------
// NoiseSlot
// ---------------------------------------------------------------------------

void NoiseSlot::prepare (double sampleRate, uint32_t seed)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    rng.seed (seed);

    noiseGen.prepare (sr, seed ^ 0xA511E9B3u);
    player.prepare (sr, seed ^ 0x63D83595u);
    filter.prepare (sr);
    lfo.prepare (seed ^ 0x1B56C4E9u);
    dc.prepare (sr, 14.0f);

    currentSource = -1;
    gateOpen = false;
    gatePhase = 0.0f;
    gateLevel = 0.0f;
    lastPeak = 0.0f;
}

void NoiseSlot::reset()
{
    player.reset();
    filter.reset();
    dc.reset();
    gatePhase = 0.0f;
    gateLevel = 0.0f;
    gateOpen = false;
    lastPeak = 0.0f;
}

void NoiseSlot::setSource (const SampleLibrary& library, int sourceIndex)
{
    if (sourceIndex == currentSource)
        return;

    currentSource = sourceIndex;

    if (sourceIndex < kNumProceduralSources)
    {
        noiseGen.setType (sourceIndex);
        player.setSample (nullptr);
    }
    else
    {
        const int sampleIndex = sourceIndex - kNumProceduralSources;
        player.setSample (library.get (sampleIndex));
        noiseGen.setType (NoiseWhite);
    }
}

void NoiseSlot::noteOn()
{
    gateOpen = true;
    // One-shot textures restart when a new phrase begins, but never cut
    // themselves off mid-tail.
    if (player.isSounding() == false)
        player.trigger();
}

void NoiseSlot::noteOff()
{
    gateOpen = false;
}

void NoiseSlot::render (float* outL, float* outR, float* delayL, float* delayR,
                        float* revL, float* revR, int numSamples, const SlotControls& c)
{
    if (c.level <= 1.0e-4f)
    {
        lastPeak = 0.0f;
        return;
    }

    const float dt = (float) numSamples / (float) sr;

    player.setMode (c.mode);
    player.setTuneSemitones (c.tuneSemitones);
    player.setGrain (c.grainSizeMs, c.grainSpray);
    player.setGate (gateOpen);

    lfo.setRate (c.lfoRate);
    lfo.setShape (c.lfoShape);
    const float lfoValue = lfo.advance (dt);

    // Swell is a smooth S-curve ramp that reaches exactly full level (or
    // exactly silence) in the programmed time, so long fades always end.
    const float swell = std::max (0.02f, c.swellSeconds);
    const float ramp = dt / swell;
    gatePhase = clampf (gatePhase + (gateOpen ? ramp : -ramp), 0.0f, 1.0f);
    gateLevel = 0.5f - 0.5f * std::cos (kPi * gatePhase);

    if (! gateOpen && gatePhase <= 0.0f)
    {
        lastPeak = 0.0f;
        return;
    }

    const float modOctaves = lfoValue * c.lfoCutoffDepth * 4.0f;
    const float cutoff = clampf (c.cutoff * std::exp2 (modOctaves), 20.0f, 18000.0f);
    filter.updateCoefficients (cutoff, c.resonance);

    const auto type = c.filterType == 1 ? SvfType::BandPass
                    : (c.filterType == 2 ? SvfType::HighPass : SvfType::LowPass);

    panGains (clampf (c.pan + lfoValue * c.lfoPanDepth, -1.0f, 1.0f), panL, panR);

    const float revSend = clampf (c.reverbSend + lfoValue * c.lfoSpaceDepth * 0.5f, 0.0f, 1.0f);
    const float driveGain = 1.0f + 4.0f * clampf (c.drive, 0.0f, 1.0f);
    const float driveComp = 1.0f / (1.0f + 1.2f * c.drive);
    const float outGain = c.level * gateLevel;

    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = currentSource >= 0 && currentSource < kNumProceduralSources
                      ? noiseGen.process()
                      : player.next();

        x = filter.processType (x, type);

        if (c.drive > 0.001f)
            x = dc.process (fastTanh (x * driveGain) * driveComp);
        else
            dc.process (x);

        x *= outGain;

        const float l = x * panL;
        const float r = x * panR;

        outL[i] += l;
        outR[i] += r;
        delayL[i] += l * c.delaySend;
        delayR[i] += r * c.delaySend;
        revL[i] += l * revSend;
        revR[i] += r * revSend;

        peak = std::max (peak, std::fabs (x));
    }

    lastPeak = std::max (lastPeak * 0.6f, peak);
}

// ---------------------------------------------------------------------------
// NoiseEngine
// ---------------------------------------------------------------------------

void NoiseEngine::prepare (double sampleRate, const SampleLibrary& library)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    libraryPtr = &library;

    for (int i = 0; i < kNoiseSlotCount; ++i)
        slots[(size_t) i].prepare (sr, 0x9E3779B9u + (uint32_t) i * 0x85EBCA6Bu);
}

void NoiseEngine::reset()
{
    for (auto& slot : slots)
        slot.reset();
}

void NoiseEngine::noteOn()
{
    for (auto& slot : slots)
        slot.noteOn();
}

void NoiseEngine::noteOff()
{
    for (auto& slot : slots)
        slot.noteOff();
}

void NoiseEngine::panic()
{
    reset();
}

float NoiseEngine::slotMeter (int slot) const noexcept
{
    return slots[(size_t) clampValue (slot, 0, kNoiseSlotCount - 1)].meter();
}

void NoiseEngine::render (float* outL, float* outR, float* delayL, float* delayR,
                          float* revL, float* revR, int numSamples, const ParamSnapshot& p)
{
    if (libraryPtr == nullptr)
        return;

    const float globalLevel = p[(size_t) P::NoiseLevel];
    const float grainSize = p[(size_t) P::GrainSize];
    const float grainSpray = p[(size_t) P::GrainSpray];

    for (int s = 0; s < kNoiseSlotCount; ++s)
    {
        SlotControls c;
        c.sourceIndex = (int) p[(size_t) slotParam (s, 0)];
        c.level = p[(size_t) slotParam (s, 1)] * globalLevel
                * p[(size_t) slotParam (s, kSlotEnableOffset)];
        c.mode = (int) p[(size_t) slotParam (s, 2)];
        c.tuneSemitones = p[(size_t) slotParam (s, 3)];
        c.filterType = (int) p[(size_t) slotParam (s, 4)];
        c.cutoff = p[(size_t) slotParam (s, 5)];
        c.resonance = p[(size_t) slotParam (s, 6)];
        c.pan = p[(size_t) slotParam (s, 7)];
        c.drive = p[(size_t) slotParam (s, 8)];
        c.delaySend = p[(size_t) slotParam (s, 9)];
        c.reverbSend = p[(size_t) slotParam (s, 10)];
        c.lfoRate = p[(size_t) slotParam (s, 11)];
        c.lfoShape = (int) p[(size_t) slotParam (s, 12)];
        c.lfoCutoffDepth = p[(size_t) slotParam (s, 13)];
        c.lfoPanDepth = p[(size_t) slotParam (s, 14)];
        c.lfoSpaceDepth = p[(size_t) slotParam (s, 15)];
        c.swellSeconds = p[(size_t) slotParam (s, 16)];
        c.grainSizeMs = grainSize;
        c.grainSpray = grainSpray;

        slots[(size_t) s].setSource (*libraryPtr, c.sourceIndex);
        slots[(size_t) s].render (outL, outR, delayL, delayR, revL, revR, numSamples, c);
    }
}

} // namespace mk
