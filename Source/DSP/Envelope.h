#pragma once

// Curved ADSR envelope tuned for very long pads: multi-second attacks and
// releases, click-free retriggering, and a shape control that morphs between
// "fast start / long tail" and "slow swell / quick ending".

#include "MkCommon.h"

namespace mk
{

class AdsrEnvelope
{
public:
    enum Stage
    {
        Idle = 0,
        Attack,
        Decay,
        Sustain,
        Release
    };

    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        rebuildCurves (curveAmount);
        reset();
    }

    void reset() noexcept
    {
        stage = Idle;
        value = 0.0f;
        position = 0.0f;
        startValue = 0.0f;
    }

    // Parameters are in seconds / 0..1 / -1..1 and are expected to be set at
    // control rate. Rebuilding the 257-point shape table is cheap and only
    // happens when the curve actually changes.
    void setParams (float attackSeconds, float decaySeconds, float sustainLevel,
                    float releaseSeconds, float curve) noexcept
    {
        attackInc = 1.0f / std::max (0.0005f, attackSeconds * (float) sr);
        decayInc = 1.0f / std::max (0.0005f, decaySeconds * (float) sr);
        releaseInc = 1.0f / std::max (0.0005f, releaseSeconds * (float) sr);
        sustain = clampf (sustainLevel, 0.0f, 1.0f);

        if (std::fabs (curve - curveAmount) > 0.002f)
            rebuildCurves (curve);
    }

    void noteOn() noexcept
    {
        startValue = value;
        position = 0.0f;
        stage = Attack;
    }

    void noteOff() noexcept
    {
        if (stage != Idle)
        {
            startValue = value;
            position = 0.0f;
            stage = Release;
        }
    }

    inline float process() noexcept
    {
        switch (stage)
        {
            case Attack:
            {
                position += attackInc;
                if (position >= 1.0f)
                {
                    value = 1.0f;
                    position = 0.0f;
                    stage = Decay;
                }
                else
                {
                    value = startValue + (1.0f - startValue) * lookupCurve (position, true);
                }
                break;
            }

            case Decay:
            {
                position += decayInc;
                if (position >= 1.0f)
                {
                    value = sustain;
                    stage = Sustain;
                }
                else
                {
                    value = sustain + (1.0f - sustain) * (1.0f - lookupCurve (position, true));
                }
                break;
            }

            case Sustain:
                value = sustain;
                break;

            case Release:
            {
                position += releaseInc;
                if (position >= 1.0f)
                {
                    value = 0.0f;
                    stage = Idle;
                }
                else
                {
                    value = startValue * lookupCurve (position, false);
                }
                break;
            }

            case Idle:
            default:
                value = 0.0f;
                break;
        }

        return value;
    }

    // Control-rate advance. Reaches the same end state as calling process()
    // numSamples times; only used for modulation, where block granularity is
    // already fine.
    inline float advanceSamples (int numSamples) noexcept
    {
        const float n = (float) numSamples;

        switch (stage)
        {
            case Attack:
            {
                position += attackInc * n;
                if (position >= 1.0f)
                {
                    value = 1.0f;
                    position = 0.0f;
                    stage = Decay;
                }
                else
                {
                    value = startValue + (1.0f - startValue) * lookupCurve (position, true);
                }
                break;
            }

            case Decay:
            {
                position += decayInc * n;
                if (position >= 1.0f)
                {
                    value = sustain;
                    stage = Sustain;
                }
                else
                {
                    value = sustain + (1.0f - sustain) * (1.0f - lookupCurve (position, true));
                }
                break;
            }

            case Sustain:
                value = sustain;
                break;

            case Release:
            {
                position += releaseInc * n;
                if (position >= 1.0f)
                {
                    value = 0.0f;
                    stage = Idle;
                }
                else
                {
                    value = startValue * lookupCurve (position, false);
                }
                break;
            }

            case Idle:
            default:
                value = 0.0f;
                break;
        }

        return value;
    }

    // True while the envelope can still produce sound.
    bool isActive() const noexcept { return stage != Idle; }
    bool isReleasing() const noexcept { return stage == Release; }
    float getValue() const noexcept { return value; }
    Stage getStage() const noexcept { return stage; }

    // Shortest audible tail, used to retire voices.
    bool hasFinished (float threshold = 1.0e-5f) const noexcept
    {
        return stage == Idle || (stage == Release && value < threshold);
    }

private:
    static constexpr int kCurvePoints = 256;

    void rebuildCurves (float curve) noexcept
    {
        curveAmount = clampf (curve, -1.0f, 1.0f);
        const float exponent = std::pow (2.0f, curveAmount * 1.2f);
        const float inverse = 1.0f / exponent;

        for (int i = 0; i <= kCurvePoints; ++i)
        {
            const float x = (float) i / (float) kCurvePoints;
            riseCurve[(size_t) i] = std::pow (x, exponent);
            fallCurve[(size_t) i] = 1.0f - std::pow (x, inverse);
        }
    }

    inline float lookupCurve (float position, bool rising) const noexcept
    {
        const float p = clampf (position, 0.0f, 1.0f) * (float) kCurvePoints;
        const int i = (int) p;
        const int j = std::min (i + 1, kCurvePoints);
        const float frac = p - (float) i;
        const auto& table = rising ? riseCurve : fallCurve;
        return lerp (table[(size_t) i], table[(size_t) j], frac);
    }

    double sr = 48000.0;
    float attackInc = 0.001f, decayInc = 0.001f, releaseInc = 0.001f;
    float sustain = 0.8f;
    float curveAmount = 0.2f;
    float startValue = 0.0f;
    float position = 0.0f;
    float value = 0.0f;
    Stage stage = Idle;

    std::array<float, kCurvePoints + 1> riseCurve {};
    std::array<float, kCurvePoints + 1> fallCurve {};
};

} // namespace mk
