#pragma once

// Parameter storage shared between the UI/host thread and the audio thread.
//
// * Targets are atomics: any thread may call set()/get() at any time.
// * Smoothed values live on the audio thread and are advanced once per
//   control block, which removes zipper noise without per-sample cost.
// * Preset loading snaps the smoothers to avoid transition artefacts.

#include "Params.h"

#include <atomic>

namespace mk
{

class ParamStore
{
public:
    ParamStore() noexcept;

    void prepare (double sampleRate) noexcept;

    void set (P p, float value) noexcept { setByIndex ((int) p, value); }
    void setByIndex (int index, float value) noexcept;

    float get (P p) const noexcept { return getByIndex ((int) p); }
    float getByIndex (int index) const noexcept;

    void setAll (const ParamSnapshot& snapshot) noexcept;
    ParamSnapshot targets() const noexcept;

    void resetToDefaults() noexcept;

    // Moves every smoothed value towards its target by numSamples of audio.
    void advance (int numSamples) noexcept;

    // Instantly align smoothed values with the targets (preset load, reset).
    void snapToTargets() noexcept;

    const ParamSnapshot& smoothed() const noexcept { return smoothedValues; }

    double sampleRate() const noexcept { return sr; }

    bool hasChangedSince (int& cursor) noexcept;

private:
    std::array<std::atomic<float>, (size_t) kParamCount> targetValues;
    ParamSnapshot smoothedValues {};
    std::array<float, (size_t) kParamCount> smoothingCoeff {};
    std::array<float, (size_t) kParamCount> stepCache {};
    int cachedBlockSize = 0;
    double sr = 48000.0;
    std::atomic<uint32_t> changeCounter { 0 };
    uint32_t lastSeenCounter = 0;
};

} // namespace mk
