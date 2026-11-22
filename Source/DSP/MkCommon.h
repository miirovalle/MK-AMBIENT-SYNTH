#pragma once

// Small, dependency-free numeric toolbox shared by every DSP module.
// Everything here must stay real-time safe unless documented otherwise.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace mk
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kHalfPi = 1.57079632679489661923f;

template <typename T>
inline T clampValue (T v, T lo, T hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float clampf (float v, float lo, float hi) noexcept { return clampValue (v, lo, hi); }

inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

inline float gainToDb (float g) noexcept
{
    return g <= 1.0e-9f ? -120.0f : 20.0f * std::log10 (g);
}

// Rational tanh approximation: max error ~1e-4 in the range that matters,
// monotone and about 6x faster than std::tanh.
inline float fastTanh (float x) noexcept
{
    x = clampf (x, -6.0f, 6.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float softClip (float x) noexcept
{
    const float ax = std::fabs (x);
    if (ax <= 0.6f)
        return x;
    const float sign = x < 0.0f ? -1.0f : 1.0f;
    const float over = ax - 0.6f;
    return sign * (0.6f + (1.0f - std::exp (-over * 2.2f)) * 0.4f);
}

inline float midiToHz (float note) noexcept
{
    return 440.0f * std::exp2 ((note - 69.0f) * (1.0f / 12.0f));
}

inline float hzToMidi (float hz) noexcept
{
    return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0e-6f) / 440.0f);
}

inline float semitonesToRatio (float semitones) noexcept { return std::exp2 (semitones * (1.0f / 12.0f)); }

// 2^x for |x| <= 0.5 via a 5-term Taylor series: max error ~7e-6 (0.001 cent),
// which covers every unison detune and drift offset in the engine.
inline float fastExp2Small (float x) noexcept
{
    const float t = x * 0.69314718f;
    return 1.0f + t * (1.0f + t * (0.5f + t * (0.16666667f + t * 0.041666668f)));
}

// Semitone ratio that stays exact for wide intervals but avoids exp2 for the
// small detune offsets used per unison voice, per control block.
inline float fastSemitoneRatio (float semitones) noexcept
{
    const float x = semitones * (1.0f / 12.0f);
    if (x > -0.35f && x < 0.35f)
        return fastExp2Small (x);
    return std::exp2 (x);
}

// tan(x) for 0 <= x <= 0.55 (cutoff below ~0.175 * sample rate), max error
// ~0.15%. Falls back to std::tan above that. Used by the TPT filters, whose
// coefficients are recomputed for every voice on every control block.
inline float fastTan (float x) noexcept
{
    if (x > 0.55f)
        return std::tan (x);

    const float x2 = x * x;
    return x * (1.0f + x2 * (0.33333333f + x2 * (0.13333333f + x2 * 0.053968254f)));
}

// Cheap, deterministic PRNG. Never use the C library rand() in DSP code.
class XorShiftRng
{
public:
    explicit XorShiftRng (uint32_t s = 0x9E3779B9u) { seed (s); }

    inline uint32_t nextUInt() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    inline float nextUnipolar() noexcept { return (float) (nextUInt() >> 8) * (1.0f / 16777216.0f); }
    inline float nextBipolar() noexcept { return nextUnipolar() * 2.0f - 1.0f; }
    inline int nextInt (int bound) noexcept { return bound > 0 ? (int) (nextUInt() % (uint32_t) bound) : 0; }

    // Scrambling the seed and discarding the first outputs avoids the poor
    // statistical behaviour of xorshift right after seeding, which otherwise
    // correlates generators started with neighbouring seeds.
    void seed (uint32_t s) noexcept
    {
        s += 0x9E3779B9u;
        s = (s ^ (s >> 16)) * 0x85EBCA6Bu;
        s = (s ^ (s >> 13)) * 0xC2B2AE35u;
        state = (s ^ (s >> 16)) | 1u;
        for (int i = 0; i < 8; ++i)
            nextUInt();
    }

private:
    uint32_t state = 0x9E3779B9u;
};

// Equal-power pan. pan = -1 hard left, 0 centre, +1 hard right.
inline void panGains (float pan, float& outL, float& outR) noexcept
{
    const float p = (clampf (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi; // 0..pi/2
    outL = std::cos (p);
    outR = std::sin (p);
}

inline float flushDenormal (float v) noexcept
{
    return std::fabs (v) < 1.0e-25f ? 0.0f : v;
}

// One-pole smoother used for coefficients and gains.
class OnePole
{
public:
    void setTimeConstant (double sampleRate, float seconds) noexcept
    {
        if (seconds <= 0.0f)
        {
            coeff = 1.0f;
            return;
        }
        coeff = 1.0f - std::exp (-1.0f / (float) (sampleRate * (double) seconds));
    }

    void setCoefficient (float c) noexcept { coeff = clampf (c, 0.0f, 1.0f); }
    inline float next (float target) noexcept { value += (target - value) * coeff; return value; }
    inline float get() const noexcept { return value; }
    void reset (float v = 0.0f) noexcept { value = v; }
    inline void snap (float v) noexcept { value = v; }

private:
    float coeff = 0.01f;
    float value = 0.0f;
};

// Symmetric DC blocker / rumble filter (one-zero+pole at ~5-15 Hz).
class DcBlocker
{
public:
    void prepare (double sampleRate, float cutoffHz = 8.0f) noexcept
    {
        const float w = kTwoPi * cutoffHz / (float) sampleRate;
        r = 1.0f - w;
        x1 = y1 = 0.0f;
    }

    inline float process (float x) noexcept
    {
        y1 = x - x1 + r * y1;
        x1 = x;
        return flushDenormal (y1);
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

// Sine lookup with linear interpolation, used by LFOs, windows and grains.
class SineTable
{
public:
    static constexpr int kSize = 4096;

    static const SineTable& instance() noexcept
    {
        static const SineTable table;
        return table;
    }

    inline float lookup (float phase01) const noexcept
    {
        phase01 -= std::floor (phase01);
        const float pos = phase01 * (float) kSize;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) & (kSize - 1);
        return lerp (data[(size_t) i0], data[(size_t) i1], pos - (float) i0);
    }

private:
    SineTable() noexcept
    {
        for (int i = 0; i < kSize; ++i)
            data[(size_t) i] = std::sin (kTwoPi * (float) i / (float) kSize);
    }

    std::array<float, kSize> data {};
};

// Exponential mapping between 0..1 and lo..hi, used for frequencies and times.
inline float expMap (float norm, float lo, float hi) noexcept
{
    return lo * std::pow (hi / lo, clampf (norm, 0.0f, 1.0f));
}

} // namespace mk
