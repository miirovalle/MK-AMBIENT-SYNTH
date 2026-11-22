#pragma once

// Band-limited, mip-mapped, morphing wavetable bank.
//
// Every wave set is a 8-frame morph sequence rendered additively so each mip
// level contains only harmonics that stay below Nyquist for its octave band.
// Six sets cover the analog-pad / cinematic / glass / formant territory.

#include "MkCommon.h"

namespace mk
{

constexpr int kWaveSetCount = 6;
constexpr int kWaveFrames = 8;
constexpr int kWaveFrameSize = 1024;
constexpr int kWaveMipLevels = 8;     // 256, 128, 64 ... 2 harmonics
constexpr int kWaveMaxHarmonic = 256;

enum WaveSetId
{
    WaveAnalogSaw = 0,
    WaveWarmPad,
    WaveHollow,
    WaveGlass,
    WaveFormant,
    WaveOrgan
};

class WaveSet
{
public:
    // Per-block sampling state: the two morph frames and their blend weight,
    // resolved once per control block instead of once per unison voice/sample.
    struct Cursor
    {
        const float* a = nullptr;
        const float* b = nullptr;
        float weight = 0.0f;
    };

    void build (int setIndex);

    Cursor makeCursor (float morph, int mip) const noexcept
    {
        const float fm = clampf (morph, 0.0f, 1.0f) * (float) (kWaveFrames - 1);
        const int f0 = (int) fm;
        const int f1 = std::min (f0 + 1, kWaveFrames - 1);
        const float* base = data.data() + (size_t) mip * (size_t) kWaveFrames * (size_t) kWaveFrameSize;
        return { base + (size_t) f0 * (size_t) kWaveFrameSize,
                 base + (size_t) f1 * (size_t) kWaveFrameSize,
                 fm - (float) f0 };
    }

    // phase in [0,1)
    inline float readCursor (float phase01, const Cursor& c) const noexcept
    {
        const float pos = phase01 * (float) kWaveFrameSize;
        const int i0 = (int) pos;
        const float frac = pos - (float) i0;
        const int i1 = (i0 + 1) & (kWaveFrameSize - 1);

        const float va = c.a[i0] + (c.a[i1] - c.a[i0]) * frac;
        const float vb = c.b[i0] + (c.b[i1] - c.b[i0]) * frac;
        return va + (vb - va) * c.weight;
    }

    // Fixed-point variant used by the voices: no float->int conversion and no
    // wrap branch in the per-sample loop. 16-bit fraction is ~-96 dB.
    inline float readCursorFixed (uint32_t phase32, const Cursor& c) const noexcept
    {
        const int i0 = (int) (phase32 >> 22);
        const float frac = (float) ((phase32 >> 6) & 0xFFFFu) * (1.0f / 65536.0f);
        const int i1 = (i0 + 1) & (kWaveFrameSize - 1);

        const float va = c.a[i0] + (c.a[i1] - c.a[i0]) * frac;
        const float vb = c.b[i0] + (c.b[i1] - c.b[i0]) * frac;
        return va + (vb - va) * c.weight;
    }

    // Convenience for non-realtime callers (tests, tools).
    inline float read (float phase01, float morph, int mip) const noexcept
    {
        return readCursor (phase01, makeCursor (morph, mip));
    }

    static int mipForIncrement (float increment) noexcept
    {
        const float maxHarmonic = 0.45f / std::max (increment, 1.0e-7f);
        int mip = 0;
        while (mip < kWaveMipLevels - 1 && (float) (kWaveMaxHarmonic >> mip) > maxHarmonic)
            ++mip;
        return mip;
    }

    const std::vector<float>& raw() const noexcept { return data; }

private:
    std::vector<float> data;
};

class WaveTableBank
{
public:
    // Built once, cached for the lifetime of the process.
    static const WaveTableBank& instance();

    const WaveSet& set (int index) const noexcept
    {
        return sets[(size_t) clampValue (index, 0, kWaveSetCount - 1)];
    }

    double buildMilliseconds() const noexcept { return buildMs; }
    size_t memoryBytes() const noexcept;

private:
    WaveTableBank();

    std::array<WaveSet, (size_t) kWaveSetCount> sets;
    double buildMs = 0.0;
};

} // namespace mk
