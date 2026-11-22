#pragma once

// Playback of the short texture catalogue: seamless crossfaded loops,
// one-shots and a small granular cloud, all lock-free once prepared.

#include "MkCommon.h"
#include "SampleLibrary.h"

namespace mk
{

class SamplePlayer
{
public:
    void prepare (double hostSampleRate, uint32_t seed) noexcept;
    void reset() noexcept;

    // The library owns the data; this only records the view.
    void setSample (const SampleView& sample) noexcept
    {
        if (sample.data == sampleView.data)
            return;
        sampleView = sample;
        reset();
    }

    // Clears the source (used when a slot switches back to a procedural colour).
    void setSample (std::nullptr_t) noexcept
    {
        sampleView = {};
        reset();
    }

    void setMode (int newMode) noexcept
    {
        if (newMode != mode)
        {
            mode = clampValue (newMode, 0, 2);
            reset();
        }
    }

    void setTuneSemitones (float semitones) noexcept { tune = clampf (semitones, -36.0f, 36.0f); }
    void setGrain (float sizeMs, float spray01) noexcept
    {
        grainSizeSamples = clampf (sizeMs, 5.0f, 400.0f) * 0.001f * (float) kSampleStorageRate;
        spray = clampf (spray01, 0.0f, 1.0f);
    }

    void trigger() noexcept;
    void setGate (bool open) noexcept { gate = open; }

    inline float next() noexcept
    {
        if (! sampleView.isValid() || sampleView.length < 8)
            return 0.0f;

        switch (mode)
        {
            case 0:  return nextLoop();
            case 1:  return nextOneShot();
            default: return nextGrain();
        }
    }

    bool isSounding() const noexcept;

private:
    float readSample (float position) const noexcept;
    float nextLoop() noexcept;
    float nextOneShot() noexcept;
    float nextGrain() noexcept;
    void spawnGrain (int index) noexcept;
    float increment() const noexcept;

    struct Grain
    {
        float position = 0.0f;
        float increment = 1.0f;
        float phase = 1.0f;      // 1 = finished
        float phaseIncrement = 0.001f;
        bool active = false;
    };

    static constexpr int kMaxGrains = 4;

    SampleView sampleView {};
    double hostRate = 48000.0;
    XorShiftRng rng { 0x5EED1234u };

    int mode = 0;
    float tune = 0.0f;
    float position = 0.0f;
    float loopLength = 0.0f;
    float fadeSamples = 0.0f;
    float driftPosition = 0.0f;

    float grainSizeSamples = 2160.0f;
    float spray = 0.5f;
    std::array<Grain, (size_t) kMaxGrains> grains {};
    int grainCursor = 0;

    bool gate = false;
    bool finished = false;
};

} // namespace mk
