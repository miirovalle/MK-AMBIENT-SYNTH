#pragma once

// 12-step trance gate.
//
// A tempo-synced amplitude sequencer: each of the twelve steps holds a level,
// and transitions between steps use an attack (rising) or release (falling)
// ramp shaped by the tension curve. The same maths drives the editor preview,
// so what you draw is exactly what you hear.
//
// Timing:
//   Sync  - pattern length follows the host tempo and transport,
//   Free  - pattern runs at GateRate (Hz),
//   Note  - like Free, but restarts the pattern on every note-on.
//
// The host tempo defaults to 120 BPM so the gate is usable in the standalone
// app and before the host reports a playhead.

#include "MkCommon.h"
#include "Params.h"

namespace mk
{

struct GateSettings
{
    float mix = 0.0f;                 // 0 = bypass
    float minLevel = 0.0f;
    float maxLevel = 1.0f;
    float tension = 0.0f;             // -1 .. 1
    float attackSeconds = 0.02f;
    float releaseSeconds = 0.05f;
    float phase01 = 0.0f;
    int triggerMode = 0;              // 0 sync, 1 free, 2 note
    int lengthIndex = 1;              // index into kGateLengths
    float freeRateHz = 2.0f;
    double bpm = 120.0;
    bool transportPlaying = false;
    std::array<float, (size_t) kGateSteps> steps {};
};

// Pattern length in beats for each length choice (2 bars .. 1/16).
float gateLengthInBeats (int lengthIndex) noexcept;

// Shaped ramp position: 0 = start, 1 = end. tension 0 is linear, positive
// starts fast (concave), negative starts slow (convex).
inline float gateTensionCurve (float progress, float tension) noexcept
{
    const float t = clampf (progress, 0.0f, 1.0f);
    const float gamma = std::exp2 (-clampf (tension, -1.0f, 1.0f) * 1.6f);
    return std::pow (t, gamma);
}

// Effective step level after Min/Max scaling.
inline float gateStepLevel (float stepValue, float minLevel, float maxLevel) noexcept
{
    return clampf (minLevel + (maxLevel - minLevel) * clampf (stepValue, 0.0f, 1.0f), 0.0f, 1.0f);
}

// Pure envelope evaluation used by the editor preview (and verified against
// the audio path in the tests). stepPosition is 0 .. kGateSteps.
float gateEnvelopeAt (const GateSettings& settings, float stepPosition, float stepSamples,
                      double sampleRate) noexcept;

class TranceGate
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // Control rate (once per engine sub-block).
    void update (const GateSettings& settings, int numSamples) noexcept;
    void noteOn() noexcept { retriggerPending = true; }

    // Per sample; returns the raw gate level 0..1 (before Mix blending).
    inline float nextLevel() noexcept
    {
        if (! running)
            return 1.0f;

        stepPosition += stepIncrement;
        while (stepPosition >= (float) kGateSteps)
            stepPosition -= (float) kGateSteps;

        const int index = clampValue ((int) stepPosition, 0, kGateSteps - 1);
        if (index != currentIndex)
        {
            beginTransition (index);
            currentIndex = index;
        }

        if (ramping)
        {
            rampPosition += rampIncrement;
            if (rampPosition >= 1.0f)
            {
                ramping = false;
                level = targetLevel;
                rampPosition = 1.0f;
            }
            else
            {
                level = fromLevel + (targetLevel - fromLevel) * gateTensionCurve (rampPosition, tension);
            }
        }

        return level;
    }

    // Gain to apply to the bus, with the mixer blend already folded in.
    // Advances the sequencer, so this is the per-sample call the engine uses.
    inline float nextGain() noexcept
    {
        const float gateLevelNow = nextLevel();
        return 1.0f - mix * (1.0f - gateLevelNow);
    }

    float phase() const noexcept
    {
        return clampf (stepPosition / (float) kGateSteps, 0.0f, 0.999f);
    }

    float currentLevel() const noexcept { return level; }
    float currentMix() const noexcept { return mix; }
    bool isRunning() const noexcept { return running; }
    float stepIncrementPerSample() const noexcept { return stepIncrement; }

private:
    void beginTransition (int index) noexcept;
    void seekToPhase (float phase01) noexcept;

    double sr = 48000.0;
    float mix = 1.0f;
    float tension = 0.0f;
    float stepPosition = 0.0f;
    float stepIncrement = 0.0f;
    float stepSamples = 4000.0f;
    float attackSamples = 1.0f;
    float releaseSamples = 1.0f;

    std::array<float, (size_t) kGateSteps> levels {};

    int currentIndex = 0;
    bool running = false;
    bool retriggerPending = false;

    float level = 1.0f;
    float fromLevel = 1.0f;
    float targetLevel = 1.0f;
    float rampPosition = 1.0f;
    float rampIncrement = 1.0f;
    bool ramping = false;

    float lastPhase01 = 0.0f;
    float lastFreeRate = 2.0f;
    int lastLengthIndex = 1;
    double lastBpm = 120.0;
    bool phaseJustMoved = true;
};

} // namespace mk
