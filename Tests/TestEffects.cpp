#include "TestSupport.h"
#include "Tests.h"

#include "DSP/Effects.h"

#include <vector>

namespace mktest
{
namespace
{

constexpr double kSr = 48000.0;

std::vector<float> sine (double frequency, double seconds, double amplitude = 0.5)
{
    const int n = (int) (seconds * kSr);
    std::vector<float> data ((size_t) n);
    for (int i = 0; i < n; ++i)
        data[(size_t) i] = (float) (amplitude * std::sin (kTwoPiD * frequency * (double) i / kSr));
    return data;
}

void runStereoDelay()
{
    // Impulse response: echoes at the programmed times with the feedback gain.
    mk::StereoDelay delay;
    delay.prepare (kSr);
    delay.setParams (100.0f, 100.0f, 0.5f, 0.0f, 0.0f, false);

    const int n = (int) (kSr * 1.0);
    std::vector<float> inL ((size_t) n, 0.0f), inR ((size_t) n, 0.0f);
    std::vector<float> wetL ((size_t) n, 0.0f), wetR ((size_t) n, 0.0f);
    inL[0] = 1.0f;

    delay.process (inL.data(), inR.data(), wetL.data(), wetR.data(), n);

    CHECK (allFinite (wetL.data(), n) && allFinite (wetR.data(), n));

    const int echo1 = (int) (0.1 * kSr);
    const int echo2 = 2 * echo1;
    const int echo3 = 3 * echo1;

    CHECK_MSG (peak (wetL.data() + echo1 - 20, 40) > 0.8f, "first echo must be present");
    CHECK_MSG (peak (wetL.data() + echo2 - 20, 40) > 0.35f, "second echo must be present");
    CHECK_MSG (peak (wetL.data() + echo2 - 20, 40) < 0.65f, "feedback must decay the second echo");
    CHECK_MSG (peak (wetL.data() + echo3 - 20, 40) < 0.3f, "third echo must decay further");
}

void runPingPong()
{
    mk::StereoDelay delay;
    delay.prepare (kSr);
    delay.setParams (100.0f, 100.0f, 0.5f, 0.0f, 0.0f, true);

    const int n = (int) (kSr * 1.0);
    std::vector<float> inL ((size_t) n, 0.0f), inR ((size_t) n, 0.0f);
    std::vector<float> wetL ((size_t) n, 0.0f), wetR ((size_t) n, 0.0f);
    inL[0] = 1.0f;

    delay.process (inL.data(), inR.data(), wetL.data(), wetR.data(), n);

    const int e1 = (int) (0.1 * kSr);
    const int e2 = 2 * e1;

    CHECK_MSG (peak (wetL.data() + e1 - 20, 40) > 0.8f, "ping-pong: first repeat stays on the input side");
    CHECK_MSG (peak (wetR.data() + e2 - 20, 40) > 0.3f, "ping-pong: repeat must cross to the other channel");
    CHECK_MSG (peak (wetL.data() + e2 - 20, 40) < 0.1f, "ping-pong: second repeat must leave the first channel");
}

void runReverb()
{
    mk::SpaceReverb reverb;
    reverb.prepare (kSr);
    reverb.setParams (0.7f, 0.5f, 0.3f, 20.0f, 1.0f);

    const int n = (int) (kSr * 8.0);
    std::vector<float> inL ((size_t) n, 0.0f), inR ((size_t) n, 0.0f);
    std::vector<float> wetL ((size_t) n, 0.0f), wetR ((size_t) n, 0.0f);
    inL[0] = 1.0f;
    inR[0] = 1.0f;

    reverb.process (inL.data(), inR.data(), wetL.data(), wetR.data(), n);

    CHECK (allFinite (wetL.data(), n) && allFinite (wetR.data(), n));

    const double early = rms (wetL.data() + 4800, 9600);
    const double middle = rms (wetL.data() + (int) (2 * kSr), 9600);
    const double late = rms (wetL.data() + (int) (5 * kSr), 9600);

    CHECK_MSG (early > 1e-4, "reverb must produce a tail");
    CHECK_MSG (middle < early, "reverb tail must decay");
    CHECK_MSG (late < middle, "reverb tail must keep decaying");
    CHECK_MSG (peak (wetL.data(), n) < 2.0f, "reverb must stay bounded");

    const double corr = correlation (wetL.data(), wetR.data(), n);
    CHECK_MSG (corr < 0.95, "reverb must be stereo");
    CHECK_MSG (corr > -0.95, "reverb channels must not cancel");
}

void runEnsemble()
{
    mk::Ensemble ensemble;
    ensemble.prepare (kSr);
    ensemble.setParams (1.0f, 0.4f);

    auto left = sine (220.0, 2.0);
    auto right = left;
    ensemble.process (left.data(), right.data(), (int) left.size());

    CHECK (allFinite (left.data(), (int) left.size()));
    CHECK_MSG (rms (left.data(), (int) left.size()) > 0.1, "ensemble must not kill the signal");
    const double corr = correlation (left.data(), right.data(), (int) left.size());
    CHECK_MSG (corr < 0.95, "ensemble must widen a mono source");

    // Zero depth is a true bypass.
    mk::Ensemble bypass;
    bypass.prepare (kSr);
    bypass.setParams (0.0f, 0.4f);
    auto a = sine (220.0, 1.0);
    auto b = a;
    bypass.process (a.data(), b.data(), (int) a.size());
    CHECK_NEAR (correlation (a.data(), b.data(), (int) a.size()), 1.0, 1e-6);
}

void runLimiter()
{
    mk::Limiter limiter;
    limiter.prepare (kSr, 2.0f);
    limiter.setParams (-0.3f, 120.0f);

    auto left = sine (110.0, 2.0, 3.0); // 3x over full scale
    auto right = left;
    limiter.process (left.data(), right.data(), (int) left.size());

    CHECK (allFinite (left.data(), (int) left.size()));
    CHECK_MSG (peak (left.data(), (int) left.size()) <= 1.0f, "limiter must not exceed full scale");
    CHECK_MSG (peak (left.data(), (int) left.size()) > 0.5f, "limiter must not crush the signal");

    // Rumble and other gentle content must pass through unchanged.
    mk::Limiter clean;
    clean.prepare (kSr, 2.0f);
    clean.setParams (-0.3f, 120.0f);
    auto quiet = sine (220.0, 1.0, 0.25);
    auto quietR = quiet;
    clean.process (quiet.data(), quietR.data(), (int) quiet.size());
    const double outRms = rms (quiet.data() + 4800, (int) quiet.size() - 4800);
    CHECK_NEAR (outRms, 0.25 / std::sqrt (2.0), 0.01);

    // Silence stays silent (after the look-ahead delay has flushed).
    std::vector<float> silence ((size_t) 8192, 0.0f);
    clean.process (silence.data(), silence.data(), (int) silence.size());
    CHECK_MSG (peak (silence.data() + 4096, 4096) < 1e-6f, "limiter must not generate noise");
}

void runSaturator()
{
    mk::Saturator sat;
    sat.prepare (kSr);

    for (float drive : { 0.0f, 0.5f, 1.0f })
    {
        sat.setDrive (drive);
        sat.reset();
        auto signal = sine (220.0, 1.0, 0.5);
        for (auto& v : signal)
            v = sat.process (v);
        CHECK (allFinite (signal.data(), (int) signal.size()));
        CHECK_MSG (peak (signal.data(), (int) signal.size()) < 1.2f, "saturation must stay bounded");
        CHECK_MSG (rms (signal.data(), (int) signal.size()) > 0.05, "saturation must not mute");
    }

    // Channel independence: the two stereo channels must not leak into each other.
    mk::Saturator stereo;
    stereo.prepare (kSr);
    stereo.setDrive (0.6f);
    float l = 0.5f, r = 0.0f;
    for (int i = 0; i < 64; ++i)
        stereo.processStereo (l, r);
    CHECK_MSG (std::fabs (r) < 1e-6f, "stereo saturation must keep silent channels silent");
}

} // namespace

void runEffectTests()
{
    std::printf ("[effects]\n");
    runStereoDelay();
    runPingPong();
    runReverb();
    runEnsemble();
    runLimiter();
    runSaturator();
}

} // namespace mktest
