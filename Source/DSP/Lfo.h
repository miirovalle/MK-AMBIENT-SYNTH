#pragma once

// Free-running LFO with the shapes that matter for slow ambient movement.
// Runs at control rate; the engine advances it once per control block.

#include "MkCommon.h"

namespace mk
{

enum LfoShape
{
    LfoSine = 0,
    LfoTriangle,
    LfoRampUp,
    LfoRampDown,
    LfoSquare,
    LfoSampleHold,
    LfoSmoothRandom
};

class Lfo
{
public:
    void prepare (uint32_t seed) noexcept
    {
        rng.seed (seed);
        phase = rng.nextUnipolar();
        heldValue = rng.nextBipolar();
        previousRandom = heldValue;
        nextRandom = heldValue;
        value = heldValue;
    }

    void setRate (float hz) noexcept { rate = clampf (hz, 0.001f, 40.0f); }
    void setShape (int shapeIndex) noexcept { shape = clampValue (shapeIndex, 0, 6); }
    void retrigger() noexcept { phase = 0.0f; }

    // Advances by dtSeconds and returns the new value in -1..1.
    inline float advance (float dtSeconds) noexcept
    {
        const float previousPhase = phase;
        phase += rate * dtSeconds;

        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            previousRandom = nextRandom;
            nextRandom = rng.nextBipolar();
        }

        switch (shape)
        {
            case LfoSine:      value = SineTable::instance().lookup (phase); break;
            case LfoTriangle:  value = 1.0f - 4.0f * std::fabs (phase - 0.5f); break;
            case LfoRampUp:    value = 2.0f * phase - 1.0f; break;
            case LfoRampDown:  value = 1.0f - 2.0f * phase; break;
            case LfoSquare:    value = phase < 0.5f ? 1.0f : -1.0f; break;
            case LfoSampleHold:
                if (phase < previousPhase) // wrapped: latch a new value
                    heldValue = nextRandom;
                value = heldValue;
                break;
            case LfoSmoothRandom:
            default:
            {
                if (phase < previousPhase)
                    heldValue = previousRandom;
                const float w = 0.5f - 0.5f * std::cos (kPi * phase);
                value = lerp (heldValue, nextRandom, w);
                break;
            }
        }

        return value;
    }

    float getValue() const noexcept { return value; }

private:
    XorShiftRng rng { 0x2545F491u };
    float rate = 0.1f;
    float phase = 0.0f;
    float value = 0.0f;
    float heldValue = 0.0f;
    float previousRandom = 0.0f;
    float nextRandom = 0.0f;
    int shape = LfoSine;
};

} // namespace mk
