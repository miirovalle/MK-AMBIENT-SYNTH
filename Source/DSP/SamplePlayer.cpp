#include "SamplePlayer.h"

#include "SampleLibrary.h"

namespace mk
{

void SamplePlayer::prepare (double hostSampleRate, uint32_t seed) noexcept
{
    hostRate = hostSampleRate > 0.0 ? hostSampleRate : 48000.0;
    rng.seed (seed);
    reset();
}

void SamplePlayer::reset() noexcept
{
    position = 0.0f;
    driftPosition = 0.0f;
    finished = false;
    grainCursor = 0;
    for (auto& g : grains)
        g = Grain {};

    if (sampleView.isValid())
    {
        const float size = (float) sampleView.length;
        // 30 ms crossfade keeps the loop seam inaudible even on noise.
        fadeSamples = std::min (0.03f * (float) kSampleStorageRate, size * 0.2f);
        loopLength = size - fadeSamples;
        if (loopLength < size * 0.25f)
        {
            fadeSamples = 0.0f;
            loopLength = size;
        }
    }
    else
    {
        loopLength = 0.0f;
        fadeSamples = 0.0f;
    }
}

float SamplePlayer::readSample (float p) const noexcept
{
    const float maxIndex = (float) (sampleView.length - 1);
    const float clamped = clampf (p, 0.0f, maxIndex);
    const int i0 = (int) clamped;
    const int i1 = std::min (i0 + 1, sampleView.length - 1);
    const float a = (float) sampleView.data[i0] * sampleView.scale;
    const float b = (float) sampleView.data[i1] * sampleView.scale;
    return a + (b - a) * (clamped - (float) i0);
}

float SamplePlayer::increment() const noexcept
{
    return semitonesToRatio (tune) * (float) (kSampleStorageRate / hostRate);
}

void SamplePlayer::trigger() noexcept
{
    position = 0.0f;
    driftPosition = 0.0f;
    finished = false;
    for (auto& g : grains)
        g = Grain {};
    grainCursor = 0;
}

bool SamplePlayer::isSounding() const noexcept
{
    if (! sampleView.isValid() || finished)
        return false;
    if (mode == 2)
    {
        for (const auto& g : grains)
            if (g.active)
                return true;
        return gate;
    }
    return true;
}

float SamplePlayer::nextLoop() noexcept
{
    if (loopLength < 4.0f)
        return 0.0f;

    position += increment();

    // Play through the crossfade tail, then jump back to the crossfade end so
    // the waveform is continuous (no seam, no click).
    if (position >= loopLength + fadeSamples)
        position -= loopLength;

    if (fadeSamples > 1.0f && position >= loopLength)
    {
        const float w = (position - loopLength) / fadeSamples;
        return readSample (position) * (1.0f - w) + readSample (position - loopLength) * w;
    }

    return readSample (position);
}

float SamplePlayer::nextOneShot() noexcept
{
    if (finished)
        return 0.0f;

    const float size = (float) sampleView.length;
    position += increment();
    if (position >= size)
    {
        finished = true;
        return 0.0f;
    }
    return readSample (position);
}

float SamplePlayer::nextGrain() noexcept
{
    const float size = (float) sampleView.length;
    if (size < 8.0f)
        return 0.0f;

    // Slow scan through the sample so repeated grains travel through it.
    driftPosition += 0.0009f * (float) kSampleStorageRate;
    if (driftPosition > size * 0.8f)
        driftPosition = 0.0f;

    float out = 0.0f;
    for (int i = 0; i < kMaxGrains; ++i)
    {
        auto& g = grains[(size_t) i];
        if (! g.active)
        {
            if (gate)
                spawnGrain (i);
            continue;
        }

        // Hann window keeps grains click-free.
        const float window = 0.5f - 0.5f * std::cos (kTwoPi * g.phase);
        out += readSample (g.position) * window * 0.5f;

        g.position += g.increment;
        if (g.position >= size)
            g.position -= size;
        g.phase += g.phaseIncrement;
        if (g.phase >= 1.0f)
            g.active = false;
    }

    return out;
}

void SamplePlayer::spawnGrain (int index) noexcept
{
    auto& g = grains[(size_t) index];
    const float size = (float) sampleView.length;

    const float jitter = 0.02f * rng.nextBipolar();
    g.increment = increment() * (1.0f + jitter);
    g.phase = 0.0f;
    g.phaseIncrement = 1.0f / std::max (16.0f, grainSizeSamples);
    g.position = driftPosition + spray * size * 0.35f * rng.nextUnipolar();
    if (g.position >= size)
        g.position -= size;
    g.active = true;
}

} // namespace mk
