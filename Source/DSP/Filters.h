#pragma once

// Filters and delay primitives. All coefficient maths runs at control rate;
// the per-sample paths are branch-free and denormal-safe.

#include "MkCommon.h"

namespace mk
{

enum class SvfType : int
{
    LowPass = 0,
    BandPass = 1,
    HighPass = 2
};

// Topology-preserving-transform state variable filter (Zavalishin).
// Stable under fast modulation, cheap, and gives all three responses at once.
class Svf
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = (float) sampleRate;
        reset();
        updateCoefficients (1000.0f, 0.0f);
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    // res01: 0 = no resonance, 1 = screaming. Q spans 0.707 .. ~20.
    // The tangent is approximated below ~8 kHz and the resonance uses exp2
    // instead of pow: this runs per voice, twice per control block.
    void updateCoefficients (float cutoffHz, float res01) noexcept
    {
        k = resonanceToK (res01);
        updateCutoff (cutoffHz, k);
    }

    static float resonanceToK (float res01) noexcept
    {
        return 1.0f / (0.707f * std::exp2 (4.8074f * clampf (res01, 0.0f, 1.0f)));
    }

    // Reuses an already computed k (= 1/Q), typically shared by both stages.
    void updateCutoff (float cutoffHz, float kValue) noexcept
    {
        const float fc = clampf (cutoffHz, 12.0f, sr * 0.47f);
        k = kValue;
        g = fastTan (kPi * fc / sr);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        lastCutoff = fc;
    }

    inline void process (float in, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = in - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = in - k * v1 - v2;
    }

    inline float processType (float in, SvfType type) noexcept
    {
        float lp, bp, hp;
        process (in, lp, bp, hp);
        switch (type)
        {
            case SvfType::LowPass:  return lp;
            case SvfType::BandPass: return bp;
            case SvfType::HighPass: return hp;
        }
        return lp;
    }

    float cutoff() const noexcept { return lastCutoff; }

private:
    float sr = 48000.0f;
    float g = 0.1f, k = 1.0f, a1 = 0.1f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
    float lastCutoff = 1000.0f;
};

// Voice filter: 12/24 dB low pass, band pass or high pass, with soft drive.
class MultimodeFilter
{
public:
    enum Mode : int
    {
        LowPass12 = 0,
        LowPass24 = 1,
        BandPass = 2,
        HighPass = 3
    };

    void prepare (double sampleRate) noexcept
    {
        stage1.prepare (sampleRate);
        stage2.prepare (sampleRate);
        driveGain = 1.0f;
        postGain = 1.0f;
    }

    void reset() noexcept
    {
        stage1.reset();
        stage2.reset();
        dc.reset();
    }

    void setParams (int mode, float cutoffHz, float res01, float drive01) noexcept
    {
        this->mode = mode;
        const float k1 = Svf::resonanceToK (res01);
        // Slightly lower resonance on the second pole pair keeps 24 dB stable
        // and avoids a double resonant peak.
        const float k2 = k1 * 1.1764706f; // 1 / 0.85

        stage1.updateCutoff (cutoffHz, k1);
        stage2.updateCutoff (cutoffHz, k2);

        driveGain = 1.0f + 3.0f * clampf (drive01, 0.0f, 1.0f);
        postGain = 1.0f / std::sqrt (driveGain);
    }

    inline float process (float in) noexcept
    {
        float x = in * driveGain;
        if (driveGain > 1.001f)
            x = fastTanh (x);

        float out = 0.0f;
        switch (mode)
        {
            case LowPass12: out = stage1.processType (x, SvfType::LowPass); break;
            case LowPass24:
                out = stage1.processType (x, SvfType::LowPass);
                out = stage2.processType (out, SvfType::LowPass);
                break;
            case BandPass:  out = stage1.processType (x, SvfType::BandPass); break;
            case HighPass:  out = stage1.processType (x, SvfType::HighPass); break;
        }
        return dc.process (out * postGain);
    }

    void prepareDc (double sampleRate) noexcept { dc.prepare (sampleRate, 8.0f); }

private:
    Svf stage1, stage2;
    DcBlocker dc;
    int mode = LowPass24;
    float driveGain = 1.0f, postGain = 1.0f;
};

// Simple one-pole low pass, used for damping inside delays and reverbs.
class OnePoleLowPass
{
public:
    void setCutoff (double sampleRate, float cutoffHz) noexcept
    {
        const float x = std::exp (-kTwoPi * clampf (cutoffHz, 20.0f, (float) sampleRate * 0.45f) / (float) sampleRate);
        coeff = 1.0f - x;
    }

    inline float process (float in) noexcept
    {
        state += coeff * (in - state);
        state = flushDenormal (state);
        return state;
    }

    void reset() noexcept { state = 0.0f; }

private:
    float coeff = 0.5f, state = 0.0f;
};

// RBJ biquad, recreated at control rate. Used for the master low cut and the
// effect-return high pass that keeps the low end clean.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    void setHighPass (double sampleRate, float cutoffHz, float q = 0.70710678f) noexcept
    {
        const float w0 = kTwoPi * clampf (cutoffHz, 10.0f, (float) sampleRate * 0.45f) / (float) sampleRate;
        const float cosw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f + cosw) * 0.5f / a0;
        b1 = -(1.0f + cosw) / a0;
        b2 = b0;
        a1 = -2.0f * cosw / a0;
        a2 = (1.0f - alpha) / a0;
    }

    void setLowPass (double sampleRate, float cutoffHz, float q = 0.70710678f) noexcept
    {
        const float w0 = kTwoPi * clampf (cutoffHz, 10.0f, (float) sampleRate * 0.45f) / (float) sampleRate;
        const float cosw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f - cosw) * 0.5f / a0;
        b1 = (1.0f - cosw) / a0;
        b2 = b0;
        a1 = -2.0f * cosw / a0;
        a2 = (1.0f - alpha) / a0;
    }

    inline float process (float in) noexcept
    {
        const float out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        z1 = flushDenormal (z1);
        z2 = flushDenormal (z2);
        return out;
    }
};

// Power-of-two circular buffer with linear / Catmull-Rom fractional reads.
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 8;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    inline void write (float x) noexcept
    {
        buffer[(size_t) writePos] = flushDenormal (x);
        writePos = (writePos + 1) & mask;
    }

    inline float readLinear (float delaySamples) const noexcept
    {
        const float maxDelay = (float) mask - 2.0f;
        const float d = clampf (delaySamples, 1.0f, maxDelay);
        const int i = (int) d;
        const float frac = d - (float) i;
        const float a = buffer[(size_t) ((writePos - 1 - i) & mask)];
        const float b = buffer[(size_t) ((writePos - 2 - i) & mask)];
        return a + (b - a) * frac;
    }

    inline float readHermite (float delaySamples) const noexcept
    {
        const float maxDelay = (float) mask - 3.0f;
        const float d = clampf (delaySamples, 1.0f, maxDelay);
        const int i = (int) d;
        const float frac = d - (float) i;
        const int base = (writePos - 1 - i) & mask;
        const float ym1 = buffer[(size_t) ((base - 1) & mask)];
        const float y0 = buffer[(size_t) base];
        const float y1 = buffer[(size_t) ((base + 1) & mask)];
        const float y2 = buffer[(size_t) ((base + 2) & mask)];

        // Catmull-Rom
        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    int size() const noexcept { return (int) buffer.size(); }
    bool isPrepared() const noexcept { return ! buffer.empty(); }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};

// Schroeder all-pass used for diffusion in the reverb and for the ensemble.
class AllPassDiffuser
{
public:
    void prepare (double sampleRate, float delayMs, float g)
    {
        delaySamples = delayMs * 0.001f * (float) sampleRate;
        line.prepare ((int) std::ceil (delaySamples) + 4);
        coeff = g;
        line.reset();
    }

    inline float process (float in) noexcept
    {
        const float delayed = line.readLinear (delaySamples);
        const float v = in + coeff * delayed;
        line.write (v);
        return delayed - coeff * v;
    }

    void reset() noexcept { line.reset(); }

private:
    DelayLine line;
    float delaySamples = 100.0f;
    float coeff = 0.5f;
};

} // namespace mk
