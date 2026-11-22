#include "TestSupport.h"
#include "Tests.h"

#include "DSP/Presets.h"
#include "DSP/SynthEngine.h"

#include <chrono>
#include <vector>

namespace mktest
{
namespace
{

constexpr double kHostRate = 48000.0;

mk::SynthEngine& engine()
{
    static mk::SynthEngine instance; // prepared at 48 kHz / 512 samples
    return instance;
}

struct StereoBuffer
{
    std::vector<float> left, right;

    int size() const { return (int) left.size(); }
    const float* l() const { return left.data(); }
    const float* r() const { return right.data(); }
};

StereoBuffer render (mk::SynthEngine& e, double seconds, int blockSize = 256)
{
    const int total = (int) std::llround (seconds * e.sampleRate());
    StereoBuffer buffer;
    buffer.left.assign ((size_t) total, 0.0f);
    buffer.right.assign ((size_t) total, 0.0f);

    int position = 0;
    while (position < total)
    {
        const int n = std::min (blockSize, total - position);
        e.process (buffer.left.data() + position, buffer.right.data() + position, n);
        position += n;
    }
    return buffer;
}

void resetToDefaults (mk::SynthEngine& e)
{
    e.allNotesOff (true);
    e.params().resetToDefaults();
    e.params().snapToTargets();
    e.reset();
}

void noteOnChord (mk::SynthEngine& e, float velocity = 0.8f)
{
    e.noteOn (48, velocity);
    e.noteOn (55, velocity * 0.9f);
    e.noteOn (60, velocity * 0.85f);
}

void checkStereoOutput (const StereoBuffer& buffer, double minRms, double maxPeak, const char* label)
{
    CHECK_MSG (allFinite (buffer.l(), buffer.size()), std::string (label) + ": left must be finite");
    CHECK_MSG (allFinite (buffer.r(), buffer.size()), std::string (label) + ": right must be finite");
    CHECK_MSG (rms (buffer.l(), buffer.size()) > minRms, std::string (label) + ": must produce audio");
    CHECK_MSG (peak (buffer.l(), buffer.size()) <= maxPeak, std::string (label) + ": left must not clip");
    CHECK_MSG (peak (buffer.r(), buffer.size()) <= maxPeak, std::string (label) + ": right must not clip");
    CHECK_MSG (std::fabs (mean (buffer.l(), buffer.size())) < 0.01, std::string (label) + ": no DC offset");

    const double corr = correlation (buffer.l(), buffer.r(), buffer.size());
    CHECK_MSG (corr < 0.995, std::string (label) + ": must have stereo width");
    CHECK_MSG (corr > -0.995, std::string (label) + ": channels must not be out of phase");
}

void runPadAndTail()
{
    auto& e = engine();
    resetToDefaults (e);
    noteOnChord (e);

    const auto sustained = render (e, 6.0);
    checkStereoOutput (sustained, 0.002, 1.0, "default pad");

    e.allNotesOff (false);
    const auto tail = render (e, 30.0);

    const int window = (int) kHostRate;
    const double early = rms (tail.l() + (int) (1.5 * kHostRate), window);
    const double tailEnd = rms (tail.l() + tail.size() - window, window);
    const double tailMid = rms (tail.l() + (int) (4 * kHostRate), window);

    CHECK_MSG (tailEnd < 1e-3, "release + reverb tail must decay to silence");
    CHECK_MSG (tailMid < early, "tail must keep decaying");
    CHECK_MSG (tailMid > 1e-6, "tail must still resonate after release");
}

void runCutoffAutomation()
{
    auto& e = engine();
    resetToDefaults (e);

    auto configure = [&e] (float cutoff) {
        auto& p = e.params();
        p.set (mk::P::Cutoff, cutoff);
        p.set (mk::P::Resonance, 0.05f);
        p.set (mk::P::FiltEnvAmt, 0.0f);
        p.set (mk::P::FiltKeyTrack, 0.0f);
        p.set (mk::P::NoiseLevel, 0.0f);
        p.set (mk::P::ReverbMix, 0.0f);
        p.set (mk::P::DelayMix, 0.0f);
        p.set (mk::P::EnsembleDepth, 0.0f);
        p.set (mk::P::PadSpace, 0.0f);
        p.set (mk::P::Lfo1Cutoff, 0.0f);
        p.set (mk::P::Lfo2Cutoff, 0.0f);
        p.set (mk::P::Lfo3Cutoff, 0.0f);
        p.snapToTargets();
    };

    configure (300.0f);
    e.noteOn (57, 0.9f);
    render (e, 1.0);
    const auto dark = render (e, 2.0);
    e.allNotesOff (true);

    configure (7000.0f);
    e.noteOn (57, 0.9f);
    render (e, 1.0);
    const auto bright = render (e, 2.0);
    e.allNotesOff (true);

    const auto darkBands = measureBands (dark.l(), dark.size(), kHostRate);
    const auto brightBands = measureBands (bright.l(), bright.size(), kHostRate);

    CHECK_MSG (brightBands.highOverLow() > darkBands.highOverLow() * 2.0,
               "opening the filter must add high-frequency energy");
}

void runVoiceLimiting()
{
    auto& e = engine();
    resetToDefaults (e);
    e.params().set (mk::P::Polyphony, 4.0f);
    e.params().snapToTargets();

    for (int i = 0; i < 12; ++i)
        e.noteOn (48 + i * 2, 0.6f);

    render (e, 0.5);
    CHECK_MSG (e.activeVoiceCount() <= 4, "polyphony limit must be honoured");
    CHECK_MSG (e.heldVoiceCount() <= 4, "held voice count must respect the limit");

    e.allNotesOff (true);
}

void runPresetRenders()
{
    auto& e = engine();

    for (int i = 0; i < mk::factoryPresetCount(); ++i)
    {
        resetToDefaults (e);
        mk::applyFactoryPreset (e.params(), i);
        noteOnChord (e, 0.75f);

        const auto buffer = render (e, 3.0);
        checkStereoOutput (buffer, 1e-4, 1.0, mk::factoryPreset (i).name);

        const auto bands = measureBands (buffer.l(), buffer.size(), kHostRate);
        CHECK_MSG (bands.low > 1e-6 && bands.high > 1e-6,
                   std::string (mk::factoryPreset (i).name) + ": must contain both low and high content");

        e.allNotesOff (true);
        render (e, 1.0);
    }
}

void runTextureLayers()
{
    auto& e = engine();
    resetToDefaults (e);

    auto& p = e.params();
    p.set (mk::P::NoiseLevel, 0.9f);
    p.set (mk::P::PadLevel, 0.0f);
    p.set (mk::P::ReverbMix, 0.3f);

    // All four slots at once, each with a different source family.
    p.set (mk::P::Slot1Source, 2.0f);    // brown
    p.set (mk::P::Slot1Level, 0.5f);
    p.set (mk::P::Slot2Source, 50.0f);   // tape sample
    p.set (mk::P::Slot2Level, 0.4f);
    p.set (mk::P::Slot3Source, 100.0f);  // granular sample
    p.set (mk::P::Slot3Level, 0.4f);
    p.set (mk::P::Slot3Mode, 2.0f);
    p.set (mk::P::Slot4Source, 125.0f);  // city sample
    p.set (mk::P::Slot4Level, 0.4f);
    p.snapToTargets();

    e.noteOn (60, 0.8f);
    const auto buffer = render (e, 4.0);
    checkStereoOutput (buffer, 0.001, 1.0, "four texture layers");

    for (int slot = 0; slot < 4; ++slot)
        CHECK_MSG (e.textureMeter (slot) >= 0.0f, "slot meters must be valid");

    e.allNotesOff (false);
    const auto afterRelease = render (e, 20.0);
    CHECK_MSG (rms (afterRelease.l() + afterRelease.size() - (int) kHostRate, (int) kHostRate) < 1e-3,
               "textures must fade out with their swell after release");
}

void runBlockSizeRobustness()
{
    auto& e = engine();
    resetToDefaults (e);
    noteOnChord (e);

    const int sizes[] = { 1, 3, 37, 511, 64 };
    float l[512], r[512];

    for (int size : sizes)
    {
        for (int block = 0; block < 8; ++block)
        {
            std::fill (l, l + size, 0.0f);
            std::fill (r, r + size, 0.0f);
            e.process (l, r, size);
            CHECK_MSG (allFinite (l, size) && allFinite (r, size), "odd block sizes must stay finite");
        }
    }

    e.allNotesOff (true);
}

void runSilenceAfterPanic()
{
    auto& e = engine();
    resetToDefaults (e);
    noteOnChord (e);
    render (e, 3.0);

    e.allNotesOff (true);
    const auto buffer = render (e, 12.0);
    const int window = (int) kHostRate;
    CHECK_MSG (rms (buffer.l() + buffer.size() - window, window) < 1e-4,
               "panic must silence voices and effects");
}

void runLongTailDecay()
{
    // The batch analysis only renders a 12 s window; verify here that the
    // longest programs really do reach silence, however slow their swell.
    auto& e = engine();
    const char* names[] = { "Cathedral Nave", "Distant Rain / Wind Pad", "Granular Understory" };

    for (const char* name : names)
    {
        const int index = mk::findFactoryPreset (name);
        CHECK_MSG (index >= 0, std::string ("missing preset ") + name);
        if (index < 0)
            continue;

        resetToDefaults (e);
        mk::applyFactoryPreset (e.params(), index);
        noteOnChord (e, 0.7f);
        render (e, 3.0);
        e.allNotesOff (false);

        const auto tail = render (e, 60.0);
        const int window = (int) kHostRate;
        const double end = rms (tail.l() + tail.size() - window, window);
        CHECK_MSG (end < 1.0e-4, std::string (name) + ": long tail must reach silence within 60 s");
    }
}

void runGateInEngine()
{
    auto& e = engine();
    resetToDefaults (e);

    auto& p = e.params();
    p.set (mk::P::GateMix, 1.0f);
    p.set (mk::P::GateMin, 0.0f);
    p.set (mk::P::GateMax, 1.0f);
    p.set (mk::P::GateTrigger, 0.0f);      // sync to tempo
    p.set (mk::P::GateLength, 3.0f);       // 1/4 = 1 beat -> 2 Hz at 120 BPM
    p.set (mk::P::GateAttack, 0.004f);
    p.set (mk::P::GateRelease, 0.006f);
    p.set (mk::P::GateTarget, 0.0f);       // pad only
    p.set (mk::P::NoiseLevel, 0.0f);
    p.set (mk::P::ReverbMix, 0.0f);
    p.set (mk::P::DelayMix, 0.0f);
    p.set (mk::P::EnsembleDepth, 0.0f);
    p.set (mk::P::PadSpace, 0.0f);
    p.set (mk::P::AmpAttack, 0.01f);
    p.set (mk::P::AmpSustain, 1.0f);
    p.set (mk::P::AmpRelease, 0.05f);

    // One pulse per pattern so the measured rate is the pattern rate itself.
    for (int i = 0; i < mk::kGateSteps; ++i)
        p.set (mk::gateStepParam (i), i == 0 ? 1.0f : 0.0f);

    p.snapToTargets();
    e.setTempo (120.0);

    e.noteOn (57, 0.9f);
    const auto buffer = render (e, 4.0, 128);

    checkStereoOutput (buffer, 0.001, 1.0, "gated pad");

    // Amplitude envelope: 10 ms RMS windows smoothed over 50 ms, then count
    // how often it crosses its own mean (two crossings per gate cycle).
    const int window = (int) (0.010 * kHostRate);
    const int count = buffer.size() / window;
    std::vector<double> envelope ((size_t) count);
    for (int i = 0; i < count; ++i)
        envelope[(size_t) i] = rms (buffer.l() + i * window, window);

    std::vector<double> smooth ((size_t) count, 0.0);
    for (int i = 0; i < count; ++i)
    {
        double sum = 0.0;
        int n = 0;
        for (int k = std::max (0, i - 1); k <= std::min (count - 1, i + 1); ++k)
        {
            sum += envelope[(size_t) k];
            ++n;
        }
        smooth[(size_t) i] = sum / n;
    }

    const double peakEnvelope = *std::max_element (smooth.begin(), smooth.end());
    const double floorEnvelope = *std::min_element (smooth.begin() + 20, smooth.end() - 20);
    CHECK_MSG (peakEnvelope > 0.005, "gated pad must still be audible");
    CHECK_MSG (peakEnvelope > floorEnvelope * 3.0, "gate must create clear level contrast");

    const double level = 0.5 * (peakEnvelope + floorEnvelope);
    const double hysteresis = 0.15 * (peakEnvelope - floorEnvelope);

    int crossings = 0;
    int state = 0;
    for (double v : smooth)
    {
        if (state <= 0 && v > level + hysteresis)
        {
            ++crossings;
            state = 1;
        }
        else if (state >= 0 && v < level - hysteresis)
        {
            ++crossings;
            state = -1;
        }
    }

    // 2 Hz over 4 s -> about 8 cycles -> 16 crossings.
    std::printf ("  gated pad: %d envelope cycles in 4 s (expect ~8)\n", crossings / 2);
    CHECK_MSG (crossings / 2 >= 6 && crossings / 2 <= 10, "gate must pulse at the programmed rate");

    e.allNotesOff (true);
}

void runGateBypassIsNeutral()
{
    auto& e = engine();
    resetToDefaults (e);
    e.params().set (mk::P::GateMix, 0.0f);
    e.params().snapToTargets();

    e.noteOn (57, 0.8f);
    const auto buffer = render (e, 2.0);
    checkStereoOutput (buffer, 0.002, 1.0, "gate bypassed");
    e.allNotesOff (true);
}

void runPerformance()
{
    auto& e = engine();
    resetToDefaults (e);
    mk::applyFactoryPreset (e.params(), 4);
    noteOnChord (e, 0.8f);

    const auto start = std::chrono::steady_clock::now();
    render (e, 10.0, 128);
    const auto end = std::chrono::steady_clock::now();

    const double seconds = std::chrono::duration<double> (end - start).count();
    const double realtimeFactor = 10.0 / std::max (1.0e-6, seconds);
    std::printf ("  rendered 10 s of the busiest preset in %.2f s (%.1fx realtime)\n", seconds, realtimeFactor);

    CHECK_MSG (realtimeFactor > 5.0, "engine must render much faster than realtime");

    e.allNotesOff (true);
}

} // namespace

void runEngineTests()
{
    std::printf ("[engine]\n");
    runPadAndTail();
    runCutoffAutomation();
    runVoiceLimiting();
    runPresetRenders();
    runTextureLayers();
    runBlockSizeRobustness();
    runSilenceAfterPanic();
    runLongTailDecay();
    runGateInEngine();
    runGateBypassIsNeutral();
    runPerformance();
}

} // namespace mktest
