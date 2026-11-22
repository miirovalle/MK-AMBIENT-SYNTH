#include "TestSupport.h"
#include "Tests.h"

#include "DSP/Envelope.h"
#include "DSP/Lfo.h"
#include "DSP/NoiseGen.h"
#include "DSP/Oscillator.h"
#include "DSP/SamplePlayer.h"
#include "DSP/Wavetables.h"

#include <cmath>
#include <vector>

namespace mktest
{
namespace
{

constexpr double kSr = 48000.0;

void runEnvelope()
{
    mk::AdsrEnvelope env;
    env.prepare (kSr);
    env.setParams (0.5f, 0.4f, 0.6f, 1.0f, 0.0f);

    CHECK (! env.isActive());

    env.noteOn();
    float value = 0.0f;
    for (int i = 0; i < (int) (0.5 * kSr); ++i)
        value = env.process();
    CHECK_MSG (value > 0.95f, "attack must reach full level after the attack time");

    for (int i = 0; i < (int) (0.4 * kSr); ++i)
        value = env.process();
    CHECK_NEAR (value, 0.6, 0.02);

    env.noteOff();
    float half = 0.0f;
    for (int i = 0; i < (int) (0.5 * kSr); ++i)
        half = env.process();
    CHECK_MSG (half < 0.6f, "release must fall");
    CHECK_MSG (half > 0.0f, "release must still be audible halfway through");

    for (int i = 0; i < (int) (0.6 * kSr); ++i)
        value = env.process();
    CHECK_MSG (! env.isActive(), "release must finish");

    // Retrigger must not click: level continues from where it was.
    env.setParams (2.0f, 1.0f, 0.5f, 2.0f, 0.2f);
    env.noteOn();
    for (int i = 0; i < (int) (0.1 * kSr); ++i)
        value = env.process();
    const float before = value;
    env.noteOn();
    const float after = env.process();
    CHECK_NEAR (after, before, 0.01);

    // Very long releases stay finite and monotone.
    env.setParams (0.01f, 0.01f, 1.0f, 30.0f, 0.5f);
    env.noteOn();
    for (int i = 0; i < (int) (0.05 * kSr); ++i)
        env.process();
    env.noteOff();
    float previous = 1.0f;
    bool monotone = true;
    for (int i = 0; i < (int) (5.0 * kSr); ++i)
    {
        const float v = env.process();
        if (v > previous + 1e-4f || ! std::isfinite (v))
            monotone = false;
        previous = v;
    }
    CHECK_MSG (monotone, "long release must decay monotonically");
}

void runLfo()
{
    for (int shape = 0; shape <= 6; ++shape)
    {
        mk::Lfo lfo;
        lfo.prepare (0x1234u + (uint32_t) shape);
        lfo.setRate (2.0f);
        lfo.setShape (shape);

        float minValue = 2.0f, maxValue = -2.0f;
        for (int i = 0; i < (int) (kSr * 2); ++i)
        {
            const float v = lfo.advance (1.0f / (float) kSr);
            CHECK_MSG (std::isfinite (v), "LFO must stay finite");
            minValue = std::min (minValue, v);
            maxValue = std::max (maxValue, v);
        }
        CHECK_MSG (minValue >= -1.0001f && maxValue <= 1.0001f, "LFO must stay within -1..1");
        const bool randomShape = shape == mk::LfoSampleHold || shape == mk::LfoSmoothRandom;
        CHECK_MSG (maxValue - minValue > 0.2f, "LFO must move");
        if (! randomShape)
            CHECK_MSG (maxValue > 0.5f, "deterministic LFO shapes must reach their peak");
    }

    // Sample & hold holds its value between wraps.
    mk::Lfo lfo;
    lfo.prepare (7u);
    lfo.setShape (mk::LfoSampleHold);
    lfo.setRate (1.0f);
    const float a = lfo.advance (0.1f);
    const float b = lfo.advance (0.1f);
    CHECK_NEAR (a, b, 1e-6);
}

void runOscillator()
{
    const auto& bank = mk::WaveTableBank::instance();
    const auto& set = bank.set (mk::WaveWarmPad);

    mk::UnisonOscillator osc;
    osc.prepare (kSr, 99u);
    osc.noteOn();
    osc.setPitch (220.0f, 5, 12.0f, 0.8f, 0.5f, 1.0f / 750.0f, set, 0.4f);

    double energyL = 0.0, energyR = 0.0, diff = 0.0;
    for (int i = 0; i < (int) kSr; ++i)
    {
        float l = 0.0f, r = 0.0f;
        osc.addTo (&l, &r, set, 0.5f);
        CHECK_MSG (std::isfinite (l) && std::isfinite (r), "oscillator must stay finite");
        energyL += (double) l * l;
        energyR += (double) r * r;
        diff += (double) (l - r) * (l - r);
    }

    CHECK_MSG (energyL > 1.0 && energyR > 1.0, "oscillator must produce signal");
    CHECK_MSG (diff > energyL * 0.05, "stereo spread must decorrelate the channels");

    // Unison normalisation: one voice vs seven must be within ~3 dB.
    auto voiceEnergy = [&set] (int unison) {
        mk::UnisonOscillator o;
        o.prepare (kSr, 5u);
        o.noteOn();
        o.setPitch (220.0f, unison, 10.0f, 0.0f, 0.0f, 1.0f / 750.0f, set, 0.2f);
        double sum = 0.0;
        for (int i = 0; i < (int) (kSr / 2); ++i)
        {
            float l = 0.0f, r = 0.0f;
            o.addTo (&l, &r, set, 1.0f);
            sum += (double) l * l + (double) r * r;
        }
        return sum;
    };

    const double one = voiceEnergy (1);
    const double seven = voiceEnergy (7);
    const double ratio = seven / std::max (1.0e-9, one);
    CHECK_MSG (ratio > 0.25 && ratio < 4.0, "unison loudness must stay normalised");

    // Mip selection must prevent aliasing: at a very high pitch the rendered
    // signal must remain band-limited enough that successive samples are smooth.
    mk::UnisonOscillator high;
    high.prepare (kSr, 11u);
    high.noteOn();
    high.setPitch (12000.0f, 1, 0.0f, 0.0f, 0.0f, 1.0f / 750.0f, set, 0.3f);
    float previous = 0.0f, maxDelta = 0.0f, maxValue = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        float l = 0.0f, r = 0.0f;
        high.addTo (&l, &r, set, 1.0f);
        maxDelta = std::max (maxDelta, std::fabs (l - previous));
        maxValue = std::max (maxValue, std::fabs (l));
        previous = l;
    }
    CHECK_MSG (maxValue > 0.01f, "high pitch must still produce signal");
    CHECK_MSG (maxDelta < maxValue * 3.0f, "band-limited tables must avoid hard aliasing steps");
}

void runNoiseColours()
{
    double tilt[6] = {};

    for (int type = 0; type < 6; ++type)
    {
        mk::NoiseGenerator gen;
        gen.prepare (kSr, 0xBEEF0000u + (uint32_t) type);
        gen.setType (type);

        std::vector<float> data ((size_t) (kSr * 2));
        double sum = 0.0;
        for (auto& v : data)
        {
            v = gen.process();
            sum += (double) v * v;
        }
        const double level = std::sqrt (sum / (double) data.size());

        CHECK_MSG (allFinite (data.data(), (int) data.size()), "noise must be finite");
        CHECK_MSG (level > 0.02 && level < 0.6, "noise levels must be roughly matched");

        const auto bands = measureBands (data.data(), (int) data.size(), kSr);
        tilt[type] = bands.highOverLow();
    }

    // Expected spectral tilt: brown < pink < white < blue < violet.
    CHECK_MSG (tilt[mk::NoiseBrown] < tilt[mk::NoisePink], "brown must be darker than pink");
    CHECK_MSG (tilt[mk::NoisePink] < tilt[mk::NoiseWhite], "pink must be darker than white");
    CHECK_MSG (tilt[mk::NoiseWhite] < tilt[mk::NoiseBlue], "blue must be brighter than white");
    CHECK_MSG (tilt[mk::NoiseBlue] < tilt[mk::NoiseViolet], "violet must be brightest");
    CHECK_MSG (tilt[mk::NoiseGrey] > tilt[mk::NoiseBrown], "grey must not be dark");
}

void runSamplePlayer()
{
    // Build a smooth test tone with a slow amplitude contour.
    std::vector<float> source ((size_t) (mk::kSampleStorageRate * 1.0));
    for (size_t i = 0; i < source.size(); ++i)
    {
        const double t = (double) i / mk::kSampleStorageRate;
        source[(size_t) i] = (float) (0.6 * std::sin (2.0 * kPiD * 220.0 * t) * (0.7 + 0.3 * std::sin (2.0 * kPiD * 0.7 * t)));
    }

    std::vector<int16_t> quantised (source.size());
    for (size_t i = 0; i < source.size(); ++i)
        quantised[i] = (int16_t) std::lround (source[i] * 32767.0f);

    mk::SampleView view { quantised.data(), (int) quantised.size(), 1.0f / 32767.0f };

    mk::SamplePlayer player;
    player.prepare (48000.0, 3u);
    player.setSample (view);
    player.setMode (0);
    player.setTuneSemitones (0.0f);
    player.setGate (true);

    std::vector<float> output ((size_t) (48000 * 3));
    for (auto& v : output)
        v = player.next();

    CHECK (allFinite (output.data(), (int) output.size()));
    CHECK_MSG (rms (output.data(), (int) output.size()) > 0.05, "loop playback must produce signal");

    // The loop seam must not introduce a click: the largest sample-to-sample
    // step in the second pass should be close to the natural slope.
    double maxStepEarly = 0.0, maxStepLate = 0.0;
    for (int i = 1; i < 48000; ++i)
        maxStepEarly = std::max (maxStepEarly, (double) std::fabs (output[(size_t) i] - output[(size_t) (i - 1)]));
    for (int i = 48000 + 1; i < 48000 * 2; ++i)
        maxStepLate = std::max (maxStepLate, (double) std::fabs (output[(size_t) i] - output[(size_t) (i - 1)]));
    CHECK_MSG (maxStepLate <= maxStepEarly * 1.6 + 1e-4, "loop crossfade must stay seamless");

    // Granular mode needs an open gate and produces a dense cloud.
    player.setMode (2);
    player.reset();
    player.setGate (true);
    player.setGrain (120.0f, 0.7f);

    std::vector<float> grains ((size_t) 24000);
    for (auto& v : grains)
        v = player.next();
    CHECK_MSG (rms (grains.data(), (int) grains.size()) > 0.002, "grain mode must produce a texture");

    player.setGate (false);
    for (int i = 0; i < 48000; ++i)
        player.next();
    std::vector<float> silent ((size_t) 4096);
    for (auto& v : silent)
        v = player.next();
    CHECK_MSG (peak (silent.data(), (int) silent.size()) < 1e-5f, "grain mode must stop when the gate closes");
}

} // namespace

void runGeneratorTests()
{
    std::printf ("[generators]\n");
    runEnvelope();
    runLfo();
    runOscillator();
    runNoiseColours();
    runSamplePlayer();
}

} // namespace mktest
