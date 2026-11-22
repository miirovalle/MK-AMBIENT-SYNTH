#include "ParamStore.h"

namespace mk
{

ParamStore::ParamStore() noexcept
{
    resetToDefaults();
    snapToTargets();
}

void ParamStore::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;

    for (int i = 0; i < kParamCount; ++i)
    {
        const auto& spec = paramSpecByIndex (i);

        // Modulation destinations and frequency-like controls want a slightly
        // longer glide than plain mix controls; stepped controls snap.
        float seconds = 0.02f;
        switch (spec.unit)
        {
            case U_Hz:  seconds = 0.04f; break;
            case U_Ms:  seconds = 0.05f; break;
            case U_Sec: seconds = 0.03f; break;
            default:    seconds = 0.02f; break;
        }

        if (spec.isStepped())
            smoothingCoeff[(size_t) i] = 1.0f;
        else
            smoothingCoeff[(size_t) i] = 1.0f - std::exp (-1.0f / (float) (sr * (double) seconds));
    }

    snapToTargets();
}

void ParamStore::setByIndex (int index, float value) noexcept
{
    if (index < 0 || index >= kParamCount)
        return;
    const float sanitised = paramSpecByIndex (index).clamp (value);
    targetValues[(size_t) index].store (sanitised, std::memory_order_relaxed);
    changeCounter.fetch_add (1, std::memory_order_relaxed);
}

float ParamStore::getByIndex (int index) const noexcept
{
    if (index < 0 || index >= kParamCount)
        return 0.0f;
    return targetValues[(size_t) index].load (std::memory_order_relaxed);
}

void ParamStore::setAll (const ParamSnapshot& snapshot) noexcept
{
    for (int i = 0; i < kParamCount; ++i)
        targetValues[(size_t) i].store (paramSpecByIndex (i).clamp (snapshot[(size_t) i]),
                                        std::memory_order_relaxed);
    changeCounter.fetch_add (1, std::memory_order_relaxed);
}

ParamSnapshot ParamStore::targets() const noexcept
{
    ParamSnapshot snapshot {};
    for (int i = 0; i < kParamCount; ++i)
        snapshot[(size_t) i] = targetValues[(size_t) i].load (std::memory_order_relaxed);
    return snapshot;
}

void ParamStore::resetToDefaults() noexcept
{
    const auto defaults = defaultSnapshot();
    for (int i = 0; i < kParamCount; ++i)
        targetValues[(size_t) i].store (defaults[(size_t) i], std::memory_order_relaxed);
    changeCounter.fetch_add (1, std::memory_order_relaxed);
}

void ParamStore::advance (int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    // exp() per parameter per block is measurable on a tablet, and the block
    // size is almost always constant, so the per-parameter step is cached.
    if (numSamples != cachedBlockSize)
    {
        cachedBlockSize = numSamples;
        for (int i = 0; i < kParamCount; ++i)
        {
            const float c = smoothingCoeff[(size_t) i];
            stepCache[(size_t) i] = c >= 1.0f ? 1.0f : 1.0f - std::pow (1.0f - c, (float) numSamples);
        }
    }

    for (int i = 0; i < kParamCount; ++i)
    {
        const float target = targetValues[(size_t) i].load (std::memory_order_relaxed);
        float& s = smoothedValues[(size_t) i];

        // Already settled: nothing to do (the common case when idle).
        const float difference = target - s;
        if (std::fabs (difference) < 1.0e-7f)
        {
            s = target;
            continue;
        }

        const float step = stepCache[(size_t) i];
        if (step >= 1.0f)
            s = target;
        else
            s += difference * step;
    }
}

void ParamStore::snapToTargets() noexcept
{
    for (int i = 0; i < kParamCount; ++i)
        smoothedValues[(size_t) i] = targetValues[(size_t) i].load (std::memory_order_relaxed);
}

bool ParamStore::hasChangedSince (int& cursor) noexcept
{
    const uint32_t current = changeCounter.load (std::memory_order_relaxed);
    if (current == lastSeenCounter)
        return false;
    lastSeenCounter = current;
    cursor = (int) (current & 0x7FFFFFFFu);
    return true;
}

} // namespace mk
