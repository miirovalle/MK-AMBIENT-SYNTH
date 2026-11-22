#include "TestSupport.h"
#include "Tests.h"

#include "DSP/TranceGate.h"

#include <vector>

namespace mktest
{
namespace
{

constexpr double kSr = 48000.0;

// 120 BPM, 1 bar = 2 s, 12 steps -> 8000 samples per step, 96000 per pattern.
constexpr int kStep = 8000;
constexpr int kPattern = kStep * mk::kGateSteps;

mk::GateSettings makeSettings (int lengthIndex = 1)
{
    mk::GateSettings s;
    s.mix = 1.0f;
    s.minLevel = 0.0f;
    s.maxLevel = 1.0f;
    s.attackSeconds = 0.0005f;   // near-instant, so steps are easy to measure
    s.releaseSeconds = 0.0005f;
    s.tension = 0.0f;
    s.triggerMode = 0;           // sync
    s.lengthIndex = lengthIndex;
    s.bpm = 120.0;

    for (int i = 0; i < mk::kGateSteps; ++i)
        s.steps[(size_t) i] = (i % 2 == 0) ? 1.0f : 0.0f;

    return s;
}

std::vector<float> runGate (const mk::GateSettings& settings, int numSamples, double sr = kSr)
{
    mk::TranceGate gate;
    gate.prepare (sr);
    gate.update (settings, 32);

    std::vector<float> out ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        out[(size_t) i] = gate.nextLevel();

    return out;
}

void runTiming()
{
    CHECK_NEAR (mk::gateLengthInBeats (0), 8.0f, 1e-6);
    CHECK_NEAR (mk::gateLengthInBeats (1), 4.0f, 1e-6);
    CHECK_NEAR (mk::gateLengthInBeats (5), 0.25f, 1e-6);

    auto settings = makeSettings();
    auto levels = runGate (settings, kPattern * 2);

    CHECK_MSG (levels[100] > 0.99f, "step 1 starts high");
    CHECK_MSG (levels[kStep + 100] < 0.01f, "step 2 is low");
    CHECK_MSG (levels[2 * kStep + 100] > 0.99f, "step 3 is high");
    CHECK_MSG (levels[3 * kStep + 100] < 0.01f, "step 4 is low");
    CHECK_MSG (levels[11 * kStep + 100] < 0.01f, "step 12 is low");

    // The pattern repeats every 96000 samples (2 s at 120 BPM).
    CHECK_NEAR (levels[100], levels[100 + kPattern], 0.01);
    CHECK_NEAR (levels[kStep + 100], levels[kStep + 100 + kPattern], 0.01);

    // Doubling the tempo halves the pattern.
    settings.bpm = 240.0;
    auto fast = runGate (settings, kPattern);
    CHECK_NEAR (fast[100], fast[100 + kPattern / 2], 0.02);

    // 1/16 pattern is eight times shorter than a bar.
    settings.bpm = 120.0;
    settings.lengthIndex = 5;
    auto sixteenth = runGate (settings, 12000);
    CHECK_NEAR (sixteenth[100], sixteenth[100 + 6000], 0.02);

    // Free mode at 2 Hz gives a 0.5 s pattern (24000 samples).
    settings.lengthIndex = 1;
    settings.triggerMode = 1;
    settings.freeRateHz = 2.0f;
    auto free = runGate (settings, 48000);
    CHECK_NEAR (free[100], free[100 + 24000], 0.02);
}

void runLevelMapping()
{
    auto settings = makeSettings();
    settings.minLevel = 0.25f;
    settings.maxLevel = 0.75f;

    auto levels = runGate (settings, 20000);
    CHECK_NEAR (levels[200], 0.75, 0.02);
    CHECK_NEAR (levels[kStep + 200], 0.25, 0.02);

    settings.minLevel = 0.4f;
    settings.maxLevel = 0.6f;
    auto tight = runGate (settings, 20000);
    CHECK_NEAR (tight[200], 0.6, 0.02);
    CHECK_NEAR (tight[kStep + 200], 0.4, 0.02);
}

void runMixAndBypass()
{
    auto settings = makeSettings();
    settings.mix = 0.0f;

    mk::TranceGate bypass;
    bypass.prepare (kSr);
    bypass.update (settings, 32);

    float lowest = 1.0f;
    for (int i = 0; i < 20000; ++i)
        lowest = std::min (lowest, bypass.nextGain());

    CHECK_MSG (lowest > 0.9999f, "mix = 0 must bypass completely");
    CHECK (! bypass.isRunning());

    // Mix 0.5 blends towards unity: the low step sits at 0.5.
    settings.mix = 0.5f;

    mk::TranceGate gate;
    gate.prepare (kSr);
    gate.update (settings, 32);

    float minGain = 1.0f, maxGain = 0.0f;
    for (int i = 0; i < 20000; ++i)
    {
        const float gain = gate.nextGain();
        minGain = std::min (minGain, gain);
        maxGain = std::max (maxGain, gain);
    }

    CHECK_NEAR (minGain, 0.5, 0.03);
    CHECK_NEAR (maxGain, 1.0, 0.02);
}

void runTensionCurve()
{
    CHECK_NEAR (mk::gateTensionCurve (0.0f, 0.0f), 0.0f, 1e-6);
    CHECK_NEAR (mk::gateTensionCurve (1.0f, 0.0f), 1.0f, 1e-6);
    CHECK_NEAR (mk::gateTensionCurve (0.5f, 0.0f), 0.5, 1e-6);

    for (float tension : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
    {
        float previous = -1.0f;
        bool monotone = true;
        for (int i = 0; i <= 100; ++i)
        {
            const float v = mk::gateTensionCurve ((float) i / 100.0f, tension);
            if (v < previous - 1.0e-5f || ! std::isfinite (v))
                monotone = false;
            previous = v;
        }
        CHECK_MSG (monotone, "tension curve must be monotone");
    }

    CHECK (mk::gateTensionCurve (0.25f, 1.0f) > 0.25f);
    CHECK (mk::gateTensionCurve (0.25f, -1.0f) < 0.25f);
}

void runAttackReleaseRamps()
{
    auto settings = makeSettings();
    settings.attackSeconds = 0.05f;   // 2400 samples
    settings.releaseSeconds = 0.10f;  // 4800 samples

    auto levels = runGate (settings, 24000);

    // Falling into step 2 at sample 8000.
    CHECK_MSG (levels[kStep + 60] > 0.9f, "fall starts near full level");
    CHECK_MSG (levels[kStep + 2400] < 0.7f && levels[kStep + 2400] > 0.1f, "fall is in progress");
    CHECK_MSG (levels[kStep + 4900] < 0.02f, "fall completes after the release time");

    // Rising into step 3 at sample 16000.
    CHECK_MSG (levels[2 * kStep + 60] < 0.1f, "rise starts near zero");
    CHECK_MSG (levels[2 * kStep + 1200] > 0.05f && levels[2 * kStep + 1200] < 0.8f, "rise is in progress");
    CHECK_MSG (levels[2 * kStep + 2500] > 0.98f, "rise completes after the attack time");
}

void runPreviewMatchesAudio()
{
    auto settings = makeSettings();
    settings.attackSeconds = 0.012f;
    settings.releaseSeconds = 0.03f;
    settings.tension = 0.35f;
    settings.minLevel = 0.1f;
    settings.maxLevel = 0.9f;
    settings.steps[4] = 0.4f;
    settings.steps[9] = 0.65f;

    const int total = 24000;
    auto audio = runGate (settings, total);

    mk::TranceGate gate;
    gate.prepare (kSr);
    gate.update (settings, 32);
    const float stepIncrement = gate.stepIncrementPerSample();
    const float stepSamples = 1.0f / stepIncrement;

    double worst = 0.0;
    float position = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        // Skip the first step: the audio path seeds directly at step 1's level
        // while the preview evaluates the pattern as a seamless loop.
        if (i >= kStep)
        {
            const float preview = mk::gateEnvelopeAt (settings, position, stepSamples, kSr);
            worst = std::max (worst, (double) std::fabs (preview - audio[(size_t) i]));
        }

        position += stepIncrement;
        if (position >= (float) mk::kGateSteps)
            position -= (float) mk::kGateSteps;
    }

    std::printf ("  preview vs audio max error %.5f\n", worst);
    CHECK_MSG (worst < 0.02, "editor preview must match the audio path");
}

void runClickFree()
{
    auto settings = makeSettings();
    settings.attackSeconds = 0.002f;
    settings.releaseSeconds = 0.004f;

    auto levels = runGate (settings, kPattern);

    float worstStep = 0.0f;
    for (size_t i = 1; i < levels.size(); ++i)
        worstStep = std::max (worstStep, std::fabs (levels[i] - levels[i - 1]));

    // The steepest legitimate slope is a full-scale 2 ms ramp: 1/96 per sample.
    CHECK_MSG (worstStep < 0.02f, "gate transitions must stay smooth");
    CHECK_MSG (allFinite (levels.data(), (int) levels.size()), "gate output must be finite");
}

void runNoteTrigger()
{
    auto settings = makeSettings();
    settings.triggerMode = 2;
    settings.freeRateHz = 1.0f; // 1 s pattern

    mk::TranceGate gate;
    gate.prepare (kSr);
    gate.update (settings, 32);

    for (int i = 0; i < 20000; ++i)
        gate.nextLevel();

    const float before = gate.phase();
    CHECK (before > 0.2f);

    gate.noteOn();
    gate.update (settings, 32);
    CHECK_MSG (gate.phase() < 0.02f, "note trigger restarts the pattern");
}

void runPhaseOffset()
{
    auto settings = makeSettings();

    // A descending ramp makes the starting point unambiguous.
    for (int i = 0; i < mk::kGateSteps; ++i)
        settings.steps[(size_t) i] = 1.0f - (float) i / (float) mk::kGateSteps;

    auto atZero = runGate (settings, 2000);
    settings.phase01 = 0.5f;
    auto atHalf = runGate (settings, 2000);

    CHECK_MSG (atZero[100] > 0.9f, "phase 0 starts on step 1");
    CHECK_MSG (std::abs (atHalf[100] - 0.5f) < 0.06f, "phase 0.5 starts halfway through the pattern");
}

} // namespace

void runGateTests()
{
    std::printf ("[gate]\n");
    runTiming();
    runLevelMapping();
    runMixAndBypass();
    runTensionCurve();
    runAttackReleaseRamps();
    runPreviewMatchesAudio();
    runClickFree();
    runNoteTrigger();
    runPhaseOffset();
}

} // namespace mktest
