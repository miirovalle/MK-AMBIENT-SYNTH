#include "TranceGate.h"

namespace mk
{

float gateLengthInBeats (int lengthIndex) noexcept
{
    static const float beats[] = { 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };
    return beats[clampValue (lengthIndex, 0, 5)];
}

float gateEnvelopeAt (const GateSettings& settings, float stepPosition, float stepSamples,
                      double sampleRate) noexcept
{
    const float steps = (float) kGateSteps;
    float pos = stepPosition;
    while (pos >= steps)
        pos -= steps;
    while (pos < 0.0f)
        pos += steps;

    const int index = clampValue ((int) pos, 0, kGateSteps - 1);
    const int previous = (index + kGateSteps - 1) % kGateSteps;

    const float from = gateStepLevel (settings.steps[(size_t) previous], settings.minLevel, settings.maxLevel);
    const float to = gateStepLevel (settings.steps[(size_t) index], settings.minLevel, settings.maxLevel);

    const bool rising = to >= from;
    const float seconds = rising ? settings.attackSeconds : settings.releaseSeconds;
    const float rampLength = clampf (seconds * (float) sampleRate, 1.0f, stepSamples);

    const float rampProgress = (pos - (float) index) * stepSamples / rampLength;
    if (rampProgress < 1.0f)
        return from + (to - from) * gateTensionCurve (rampProgress, settings.tension);

    return to;
}

// ---------------------------------------------------------------------------

void TranceGate::prepare (double sampleRate) noexcept
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void TranceGate::reset() noexcept
{
    stepPosition = 0.0f;
    currentIndex = 0;
    level = 1.0f;
    fromLevel = 1.0f;
    targetLevel = 1.0f;
    rampPosition = 1.0f;
    rampIncrement = 1.0f;
    ramping = false;
    retriggerPending = false;
    running = false;
    phaseJustMoved = true;
}

void TranceGate::beginTransition (int index) noexcept
{
    const float next = levels[(size_t) index];
    fromLevel = level;
    targetLevel = next;

    const bool rising = next >= fromLevel;
    const float samples = clampf (rising ? attackSamples : releaseSamples, 1.0f, stepSamples);

    rampIncrement = 1.0f / samples;
    rampPosition = 0.0f;
    ramping = true;

    if (std::fabs (targetLevel - fromLevel) < 1.0e-5f)
    {
        ramping = false;
        level = targetLevel;
        rampPosition = 1.0f;
    }
}

void TranceGate::seekToPhase (float phase01) noexcept
{
    stepPosition = clampf (phase01, 0.0f, 0.999999f) * (float) kGateSteps;
    currentIndex = clampValue ((int) stepPosition, 0, kGateSteps - 1);
    level = levels[(size_t) currentIndex];
    fromLevel = level;
    targetLevel = level;
    ramping = false;
    rampPosition = 1.0f;
    phaseJustMoved = false;
}

void TranceGate::update (const GateSettings& settings, int numSamples) noexcept
{
    (void) numSamples;

    mix = clampf (settings.mix, 0.0f, 1.0f);
    tension = clampf (settings.tension, -1.0f, 1.0f);
    attackSamples = clampf (settings.attackSeconds, 0.0005f, 4.0f) * (float) sr;
    releaseSamples = clampf (settings.releaseSeconds, 0.0005f, 4.0f) * (float) sr;

    for (int i = 0; i < kGateSteps; ++i)
        levels[(size_t) i] = gateStepLevel (settings.steps[(size_t) i], settings.minLevel, settings.maxLevel);

    if (settings.triggerMode == 0)
    {
        const double bpm = settings.bpm > 1.0 ? settings.bpm : 120.0;
        const double beats = (double) gateLengthInBeats (settings.lengthIndex);
        const double patternSeconds = beats * 60.0 / bpm;
        stepSamples = (float) (patternSeconds * sr / (double) kGateSteps);
    }
    else
    {
        const float rate = clampf (settings.freeRateHz, 0.02f, 40.0f);
        stepSamples = (float) sr / (rate * (float) kGateSteps);
    }

    stepSamples = std::max (2.0f, stepSamples);
    // stepPosition is measured in steps, so one step lasts stepSamples samples.
    stepIncrement = 1.0f / stepSamples;

    const bool wasRunning = running;
    running = mix > 1.0e-4f;

    const bool timingChanged = settings.lengthIndex != lastLengthIndex
                            || std::fabs (settings.freeRateHz - lastFreeRate) > 1.0e-4f
                            || std::fabs ((float) settings.bpm - (float) lastBpm) > 1.0e-3f;

    const bool restart = retriggerPending
                      || (running && ! wasRunning)
                      || (settings.triggerMode == 2 && timingChanged)
                      || phaseJustMoved;

    if (restart)
    {
        retriggerPending = false;
        seekToPhase (settings.phase01);
    }
    else if (std::fabs (settings.phase01 - lastPhase01) > 1.0e-5f)
    {
        seekToPhase (settings.phase01);
    }

    if (! running)
    {
        level = 1.0f;
        fromLevel = 1.0f;
        targetLevel = 1.0f;
        ramping = false;
        rampPosition = 1.0f;
    }

    lastPhase01 = settings.phase01;
    lastLengthIndex = settings.lengthIndex;
    lastFreeRate = settings.freeRateHz;
    lastBpm = settings.bpm;
}

} // namespace mk
