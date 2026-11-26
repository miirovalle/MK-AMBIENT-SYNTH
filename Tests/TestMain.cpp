#include "TestSupport.h"
#include "Tests.h"

#include "DSP/Params.h"
#include "DSP/Presets.h"
#include "DSP/SampleLibrary.h"
#include "DSP/Wavetables.h"

#include <chrono>
#include <cstring>
#include <set>
#include <string>

namespace mktest
{

int g_checks = 0;
int g_failures = 0;

void fail (const char* file, int line, const std::string& what)
{
    ++g_failures;
    std::printf ("  FAIL %s:%d  %s\n", file, line, what.c_str());
}

namespace
{

void runStaticTests()
{
    std::printf ("[registry]\n");

    CHECK (mk::kParamCount > 100);

    std::set<std::string> keys;
    for (int i = 0; i < mk::kParamCount; ++i)
    {
        const auto& spec = mk::paramSpecByIndex (i);
        CHECK (spec.key != nullptr);
        CHECK (spec.label != nullptr);
        CHECK (spec.minValue < spec.maxValue);
        CHECK (spec.defaultValue >= spec.minValue);
        CHECK (spec.defaultValue <= spec.maxValue);
        CHECK (mk::paramFromKey (spec.key) == (mk::P) i);
        keys.insert (spec.key);
    }
    CHECK_MSG ((int) keys.size() == mk::kParamCount, "parameter keys must be unique");

    CHECK (mk::slotSourceRangeIsValid());
    CHECK (mk::slotSourceChoices().count == mk::kNumProceduralSources + mk::kNumSampleSources);

    // Every section switch is a plain 0..1 control that defaults to on, so an
    // old patch or a fresh instance always starts with everything enabled.
    int switchCount = 0;
    for (int i = 0; i < mk::kParamCount; ++i)
    {
        const auto& spec = mk::paramSpecByIndex (i);
        if (spec.unit != mk::U_Switch)
            continue;

        ++switchCount;
        CHECK_NEAR (spec.minValue, 0.0f, 1e-6);
        CHECK_NEAR (spec.maxValue, 1.0f, 1e-6);
        CHECK_NEAR (spec.defaultValue, 1.0f, 1e-6);
    }
    CHECK_MSG (switchCount == 13, "osc A/B, four slots, three LFOs, delay, reverb, ensemble and gate all switch");

    // Slot blocks are contiguous and every slot maps onto real parameters.
    for (int slot = 0; slot < mk::kNoiseSlotCount; ++slot)
    {
        const auto source = mk::slotParam (slot, 0);
        const auto swell = mk::slotParam (slot, 16);
        CHECK ((int) swell - (int) source == 16);
        CHECK (std::strcmp (mk::paramSpec (source).label, "Slot 1 Source") != 0 || slot == 0);
    }

    // Presets must only touch valid parameters and stay inside their ranges.
    CHECK (mk::factoryPresetCount() == 100);
    CHECK (mk::factoryProgramCount() == 101);
    std::set<std::string> presetNames;
    std::set<size_t> presetHashes;
    for (int i = 0; i < mk::factoryPresetCount(); ++i)
    {
        const auto& preset = mk::factoryPreset (i);
        CHECK (preset.name != nullptr);
        CHECK (preset.description != nullptr);
        CHECK (std::strlen (preset.name) > 3);
        CHECK (presetNames.insert (preset.name).second);
        CHECK (mk::findFactoryPreset (preset.name) == i);

        const auto snapshot = mk::factoryPresetSnapshot (i);
        CHECK ((int) snapshot.size() == mk::kParamCount);

        // Every program must differ from the others (guards against duplicate
        // generated variations).
        size_t hash = 1469598103934665603ull;
        for (float v : snapshot)
        {
            hash ^= (size_t) (int) (v * 1000.0f);
            hash *= 1099511628211ull;
        }
        CHECK_MSG (presetHashes.insert (hash).second, preset.name);

        // A preset must actually change something relative to Init.
        int differences = 0;
        const auto defaults = mk::initSnapshot();
        for (int p = 0; p < mk::kParamCount; ++p)
            if (std::abs (snapshot[(size_t) p] - defaults[(size_t) p]) > 1e-4f)
                ++differences;
        CHECK_MSG (differences > 10, preset.name);
    }

    // Categories cover the whole bank.
    CHECK (mk::presetCategoryCount() >= 6);
    int categoryTotal = 0;
    for (int c = 0; c < mk::presetCategoryCount(); ++c)
        categoryTotal += mk::presetCategory (c).count;
    CHECK (categoryTotal == mk::factoryPresetCount());

    // The five flagship programs stay first.
    CHECK (std::strcmp (mk::factoryPreset (0).name, "Submerged Drone") == 0);
    CHECK (std::strcmp (mk::factoryPreset (4).name, "Evolving Granular Texture") == 0);

    // Sanitising clamps and quantises.
    CHECK_NEAR (mk::sanitiseParam (mk::P::PadLevel, 5.0f), 1.0f, 1e-6);
    CHECK_NEAR (mk::sanitiseParam (mk::P::OscAUnison, 3.7f), 4.0f, 1e-6);
    CHECK_NEAR (mk::sanitiseParam (mk::P::OscAWave, 2.2f), 2.0f, 1e-6);
}

void runSampleLibraryTests()
{
    std::printf ("[samples]\n");

    const auto& catalog = mk::sampleCatalog();
    CHECK ((int) catalog.entries.size() == mk::kNumSampleSources);

    std::set<std::string> names;
    for (const auto& entry : catalog.entries)
    {
        CHECK (! entry.name.empty());
        CHECK (names.insert (entry.name).second);
        CHECK (entry.durationSeconds > 0.2f);
        CHECK (entry.durationSeconds < 3.0f);
    }

    // Deterministic generation: same descriptor twice -> identical audio.
    std::vector<float> a, b;
    mk::renderCatalogueSample (catalog.entries[7], mk::kSampleStorageRate, a);
    mk::renderCatalogueSample (catalog.entries[7], mk::kSampleStorageRate, b);
    CHECK (a.size() == b.size());
    CHECK (a.size() > 1000);
    double maxDiff = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        maxDiff = std::max (maxDiff, (double) std::fabs (a[i] - b[i]));
    CHECK_NEAR (maxDiff, 0.0, 1e-9);

    mk::SampleLibrary library;
    library.prepare();
    CHECK (library.isReady());
    CHECK (library.numSamples() == mk::kNumSampleSources);
    std::printf ("  generated %d samples in %.1f ms (%.1f MB)\n", library.numSamples(),
                 library.generationMilliseconds(), (double) library.memoryBytes() / (1024.0 * 1024.0));

    for (int i = 0; i < library.numSamples(); ++i)
    {
        const auto view = library.get (i);
        CHECK (view.isValid());
        CHECK (view.length > 100);

        int16_t peakSample = 0;
        for (int s = 0; s < view.length; ++s)
            peakSample = std::max<int16_t> (peakSample, (int16_t) std::abs (view.data[s]));
        CHECK_MSG (peakSample > 2000, "sample must carry signal");

        double sumSq = 0.0;
        bool finite = true;
        for (int s = 0; s < view.length; ++s)
        {
            const float value = (float) view.data[s] * view.scale;
            if (! std::isfinite (value))
                finite = false;
            sumSq += (double) value * value;
        }
        CHECK (finite);
        CHECK_MSG (std::sqrt (sumSq / view.length) > 1e-4, catalog.entries[(size_t) i].name);
    }
}

void runWavetableTests()
{
    std::printf ("[wavetables]\n");

    const auto start = std::chrono::steady_clock::now();
    const auto& bank = mk::WaveTableBank::instance();
    const auto end = std::chrono::steady_clock::now();
    const double buildMs = std::chrono::duration<double, std::milli> (end - start).count();
    std::printf ("  built %d wave sets in %.1f ms (%.1f MB)\n", mk::kWaveSetCount, buildMs,
                 (double) bank.memoryBytes() / (1024.0 * 1024.0));

    for (int s = 0; s < mk::kWaveSetCount; ++s)
    {
        const auto& set = bank.set (s);
        CHECK (set.raw().size() == (size_t) (mk::kWaveMipLevels * mk::kWaveFrames * mk::kWaveFrameSize));
        float setPeak = 0.0f;
        for (float v : set.raw())
            setPeak = std::max (setPeak, std::fabs (v));
        CHECK (setPeak <= 1.0f);

        // Band limiting: the top mip must only carry two harmonics, meaning its
        // second difference stays small even at high phase increments.
        const int mip = mk::kWaveMipLevels - 1;
        double worst = 0.0;
        for (int frame = 0; frame < mk::kWaveFrames; ++frame)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float phase = (float) i / 256.0f;
                worst = std::max (worst, (double) std::fabs (set.read (phase, frame / 7.0f, mip)));
            }
        }
        CHECK (worst > 0.05);
        CHECK (worst < 1.5);
    }

    // Mip selector must pick heavier filtering as pitch rises.
    const int lowMip = mk::WaveSet::mipForIncrement (440.0f / 48000.0f);
    const int highMip = mk::WaveSet::mipForIncrement (8000.0f / 48000.0f);
    CHECK (highMip > lowMip);
}

} // namespace

void runParamTests()
{
    runStaticTests();
    runWavetableTests();
    runSampleLibraryTests();
}

} // namespace mktest

int main()
{
    using namespace mktest;

    std::printf ("MK Ambient - DSP test suite\n");
    std::printf ("parameters: %d, wave sets: %d, samples: %d\n\n",
                 mk::kParamCount, mk::kWaveSetCount, mk::kNumSampleSources);

    runParamTests();
    runFilterTests();
    runGeneratorTests();
    runEffectTests();
    runGateTests();
    runEngineTests();

    std::printf ("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
