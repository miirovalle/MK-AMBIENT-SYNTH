#pragma once

// One oscillator = up to seven detuned unison voices with per-voice stereo
// placement and slow analog drift. Tuning and mip selection happen at control
// rate; the sample loop is a table read plus two multiply-accumulates.

#include "Wavetables.h"

namespace mk
{

class UnisonOscillator
{
public:
    static constexpr int kMaxUnison = 7;

    void prepare (double sampleRate, uint32_t seed) noexcept
    {
        sr = (float) sampleRate;
        rng.seed (seed);
        for (auto& v : voices)
            v = UnisonVoice {};
    }

    void reset() noexcept
    {
        for (auto& v : voices)
        {
            v.phase = 0;
            v.driftOffset = 0.0f;
            v.driftTarget = 0.0f;
        }
    }

    // Randomises start phases so stacked unison never sums into one loud click.
    void noteOn() noexcept
    {
        for (auto& v : voices)
        {
            v.phase = rng.nextUInt();
            v.driftTarget = rng.nextBipolar();
            v.driftTimer = 0.15f + 1.1f * rng.nextUnipolar();
            v.driftOffset = v.driftTarget * 0.5f;
        }
    }

    // Control rate. baseHz is the pitch before unison detune; lfoCents and
    // driftCents are musical offsets applied to every unison voice. The wave
    // set is needed so each voice can cache its morph frames for the block.
    void setPitch (float baseHz, int unisonCount, float detuneCents, float spread01,
                   float drift01, float dtSeconds, const WaveSet& waveSet, float morph) noexcept
    {
        activeVoices = clampValue (unisonCount, 1, kMaxUnison);
        const float detune = clampf (detuneCents, 0.0f, 100.0f);
        const float spread = clampf (spread01, 0.0f, 1.0f);

        for (int i = 0; i < activeVoices; ++i)
        {
            auto& v = voices[(size_t) i];

            // Slow random drift per unison voice.
            v.driftTimer -= dtSeconds;
            if (v.driftTimer <= 0.0f)
            {
                v.driftTarget = rng.nextBipolar();
                v.driftTimer = 0.25f + 1.4f * rng.nextUnipolar();
            }
            const float driftSmooth = 1.0f - std::exp (-dtSeconds / 0.35f);
            v.driftOffset += (v.driftTarget - v.driftOffset) * driftSmooth;
            const float driftCents = v.driftOffset * 7.0f * clampf (drift01, 0.0f, 1.0f);

            const float position = (activeVoices == 1)
                                       ? 0.0f
                                       : ((float) i / (float) (activeVoices - 1)) * 2.0f - 1.0f;

            const float cents = position * detune + driftCents;
            const float hz = clampf (baseHz * fastSemitoneRatio (cents * 0.01f), 8.0f, sr * 0.45f);
            v.increment = hz / sr;
            v.phaseIncrement = (uint32_t) ((double) v.increment * 4294967296.0);
            v.mip = WaveSet::mipForIncrement (v.increment);
            v.cursor = waveSet.makeCursor (morph, v.mip);

            const float pan = spread * position;
            panGains (pan, v.panL, v.panR);
        }

        // Normalise the stack so unison count does not change loudness.
        voiceGain = 1.0f / std::sqrt ((float) activeVoices);
    }

    int activeVoiceCount() const noexcept { return activeVoices; }

    inline void addTo (float* outL, float* outR, const WaveSet& waveSet, float level) noexcept
    {
        if (level <= 1.0e-5f)
            return;

        const float gain = level * voiceGain;
        for (int i = 0; i < activeVoices; ++i)
        {
            auto& v = voices[(size_t) i];
            const float s = waveSet.readCursorFixed (v.phase, v.cursor) * gain;
            *outL += s * v.panL;
            *outR += s * v.panR;

            v.phase += v.phaseIncrement; // uint32 wrap is the phase wrap
        }
    }

private:
    struct UnisonVoice
    {
        uint32_t phase = 0;
        uint32_t phaseIncrement = 0;
        float increment = 0.01f;
        float panL = 0.70710678f;
        float panR = 0.70710678f;
        float driftOffset = 0.0f;
        float driftTarget = 0.0f;
        float driftTimer = 0.0f;
        int mip = 0;
        WaveSet::Cursor cursor {};
    };

    std::array<UnisonVoice, (size_t) kMaxUnison> voices {};
    XorShiftRng rng { 0x1D2C3B4Au };
    float sr = 48000.0f;
    float voiceGain = 1.0f;
    int activeVoices = 1;
};

} // namespace mk
