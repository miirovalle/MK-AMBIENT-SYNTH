#include "Wavetables.h"

#include <chrono>

namespace mk
{

namespace
{

inline float gaussian (float x, float centre, float width) noexcept
{
    const float d = (x - centre) / width;
    return std::exp (-d * d);
}

// Spectral definition of every wave set. n is the harmonic number (1-based),
// t the morph position 0..1 across the eight frames.
float harmonicAmplitude (int setIndex, int n, float t) noexcept
{
    const float fn = (float) n;

    switch (setIndex)
    {
        case WaveAnalogSaw:
            // Classic saw that gradually opens up towards a brighter,
            // slightly scooped "analog" spectrum.
            return std::pow (fn, -(1.05f - 0.35f * t));

        case WaveWarmPad:
        {
            // Soft, rounded body with a slowly rising formant.
            const float body = std::exp (-(fn - 1.0f) / (2.5f + 8.0f * t));
            const float tail = 0.35f * std::pow (fn, -(1.7f - 0.5f * t));
            return body + tail;
        }

        case WaveHollow:
            // Odd harmonics dominate: hollow, clarinet-like, woody pads.
            return ((n & 1) ? 1.0f : (0.10f + 0.22f * t)) * std::pow (fn, -(1.25f - 0.30f * t));

        case WaveGlass:
            // Two emphatic partial clusters that travel upwards with morph.
            return 0.35f * std::pow (fn, -0.65f)
                 + 0.85f * gaussian (fn, 7.0f + 16.0f * t, 2.4f)
                 + 0.40f * gaussian (fn, 19.0f + 30.0f * t, 3.2f);

        case WaveFormant:
        {
            const float body = 0.30f / std::pow (fn, 0.55f);
            const float f1 = gaussian (fn, 2.0f + 5.0f * t, 1.7f);
            const float f2 = 0.75f * gaussian (fn, 9.0f + 12.0f * t, 2.6f);
            const float f3 = 0.45f * gaussian (fn, 20.0f + 26.0f * t, 3.6f);
            return body + f1 + f2 + f3;
        }

        case WaveOrgan:
        {
            static const int drawbars[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            static const float baseWeights[9] = { 1.0f, 0.55f, 0.72f, 0.48f, 0.38f, 0.32f, 0.18f, 0.22f, 0.15f };
            for (int i = 0; i < 9; ++i)
                if (drawbars[i] == n)
                    return baseWeights[i] * (1.0f + t * 0.35f * (float) i);
            return 0.035f / fn;
        }

        default:
            return fn > 0.0f ? 1.0f / fn : 0.0f;
    }
}

void normaliseSet (std::vector<float>& data)
{
    // One scale factor for the whole set keeps morphing and mip switching
    // level-consistent; the reference peak comes from the brightest frame.
    float peak = 0.0f;
    const size_t mip0Size = (size_t) kWaveFrames * (size_t) kWaveFrameSize;
    for (size_t i = 0; i < mip0Size; ++i)
        peak = std::max (peak, std::fabs (data[i]));

    if (peak < 1.0e-6f)
        return;

    const float gain = 0.92f / peak;
    for (float& v : data)
        v *= gain;
}

} // namespace

void WaveSet::build (int setIndex)
{
    data.assign ((size_t) kWaveMipLevels * (size_t) kWaveFrames * (size_t) kWaveFrameSize, 0.0f);

    const auto& sine = SineTable::instance();

    for (int mip = 0; mip < kWaveMipLevels; ++mip)
    {
        const int maxHarmonic = std::max (1, kWaveMaxHarmonic >> mip);

        for (int frame = 0; frame < kWaveFrames; ++frame)
        {
            const float t = (float) frame / (float) (kWaveFrames - 1);
            float* table = data.data() + ((size_t) mip * (size_t) kWaveFrames + (size_t) frame) * (size_t) kWaveFrameSize;

            for (int n = 1; n <= maxHarmonic; ++n)
            {
                const float amp = harmonicAmplitude (setIndex, n, t);
                if (amp < 1.0e-5f)
                    continue;

                float phase = 0.0f;
                const float increment = (float) n / (float) kWaveFrameSize;
                for (int k = 0; k < kWaveFrameSize; ++k)
                {
                    table[k] += amp * sine.lookup (phase);
                    phase += increment;
                    if (phase >= 1.0f)
                        phase -= 1.0f;
                }
            }
        }
    }

    normaliseSet (data);
}

WaveTableBank::WaveTableBank()
{
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kWaveSetCount; ++i)
        sets[(size_t) i].build (i);
    const auto end = std::chrono::steady_clock::now();
    buildMs = std::chrono::duration<double, std::milli> (end - start).count();
}

const WaveTableBank& WaveTableBank::instance()
{
    static const WaveTableBank bank;
    return bank;
}

size_t WaveTableBank::memoryBytes() const noexcept
{
    size_t bytes = 0;
    for (const auto& s : sets)
        bytes += s.raw().size() * sizeof (float);
    return bytes;
}

} // namespace mk
