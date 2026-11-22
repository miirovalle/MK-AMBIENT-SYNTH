#include "TestSupport.h"
#include "Tests.h"

#include "DSP/Filters.h"

#include <cmath>

namespace mktest
{
namespace
{

constexpr double kSr = 48000.0;

struct Response
{
    double lp = 0.0, bp = 0.0, hp = 0.0;
};

Response measureSvf (double frequency, double cutoff, double resonance)
{
    mk::Svf filter;
    filter.prepare (kSr);
    filter.updateCoefficients ((float) cutoff, (float) resonance);

    double sumLp = 0.0, sumBp = 0.0, sumHp = 0.0;
    int count = 0;

    const int total = (int) kSr;
    for (int i = 0; i < total; ++i)
    {
        const float input = (float) std::sin (kTwoPiD * frequency * (double) i / kSr);
        float lp, bp, hp;
        filter.process (input, lp, bp, hp);
        if (i > total / 4)
        {
            sumLp += (double) lp * lp;
            sumBp += (double) bp * bp;
            sumHp += (double) hp * hp;
            ++count;
        }
    }

    const double norm = 2.0 / std::max (1, count);
    return { std::sqrt (sumLp * norm), std::sqrt (sumBp * norm), std::sqrt (sumHp * norm) };
}

void runSvfResponse()
{
    const double cutoff = 1000.0;

    const auto low = measureSvf (100.0, cutoff, 0.0);
    const auto mid = measureSvf (cutoff, cutoff, 0.0);
    const auto high = measureSvf (8000.0, cutoff, 0.0);

    CHECK_MSG (low.lp > 0.9, "low pass must pass sub-cutoff content");
    CHECK_MSG (mid.lp > 0.6 && mid.lp < 0.9, "low pass must be ~-3 dB at cutoff");
    CHECK_MSG (high.lp < 0.08, "low pass must reject well above cutoff");

    CHECK_MSG (mid.bp > 0.4, "band pass must peak at cutoff");
    CHECK_MSG (low.bp < 0.2, "band pass must reject below cutoff");
    CHECK_MSG (high.bp < 0.2, "band pass must reject above cutoff");

    CHECK_MSG (high.hp > 0.85, "high pass must pass above cutoff");
    CHECK_MSG (low.hp < 0.08, "high pass must reject below cutoff");

    // Resonance lifts the cutoff region.
    const auto flat = measureSvf (cutoff, cutoff, 0.0);
    const auto resonant = measureSvf (cutoff, cutoff, 0.85);
    CHECK_MSG (resonant.lp > flat.lp * 2.5, "resonance must lift the cutoff region");
    CHECK (std::isfinite (resonant.lp));
}

void runCascadeAndStability()
{
    // 24 dB must be clearly steeper than 12 dB one octave above cutoff.
    auto measureMultimode = [] (int mode, double frequency) {
        mk::MultimodeFilter filter;
        filter.prepare (kSr);
        filter.prepareDc (kSr);
        filter.setParams (mode, 800.0f, 0.1f, 0.0f);

        double sum = 0.0;
        int count = 0;
        for (int i = 0; i < (int) kSr; ++i)
        {
            const float input = (float) std::sin (kTwoPiD * frequency * (double) i / kSr);
            const float out = filter.process (input);
            if (i > (int) kSr / 4)
            {
                sum += (double) out * out;
                ++count;
            }
        }
        return std::sqrt (sum / std::max (1, count)) / 0.70710678;
    };

    const double lp12 = measureMultimode (mk::MultimodeFilter::LowPass12, 3200.0);
    const double lp24 = measureMultimode (mk::MultimodeFilter::LowPass24, 3200.0);
    CHECK_MSG (lp24 < lp12 * 0.7, "24 dB low pass must be steeper than 12 dB");

    // Fast modulation of an extreme cutoff must stay finite and bounded.
    mk::MultimodeFilter filter;
    filter.prepare (kSr);
    double peak = 0.0;
    for (int i = 0; i < (int) kSr * 2; ++i)
    {
        if ((i % 64) == 0)
        {
            const float cutoff = 20.0f + 17900.0f * (0.5f + 0.5f * std::sin ((float) i * 0.0013f));
            filter.setParams (mk::MultimodeFilter::LowPass24, cutoff, 0.95f, 0.8f);
        }
        const float out = filter.process ((float) std::sin (kTwoPiD * 220.0 * i / kSr));
        CHECK_MSG (std::isfinite (out), "filter output must stay finite under modulation");
        peak = std::max (peak, (double) std::fabs (out));
        if (! std::isfinite (out))
            break;
    }
    CHECK (peak < 60.0);
}

void runBiquad()
{
    mk::Biquad hp;
    hp.setHighPass (kSr, 200.0f);

    auto gain = [&] (double frequency) {
        hp.reset();
        double sum = 0.0;
        int count = 0;
        for (int i = 0; i < (int) kSr; ++i)
        {
            const float out = hp.process ((float) std::sin (kTwoPiD * frequency * (double) i / kSr));
            if (i > (int) kSr / 2)
            {
                sum += (double) out * out;
                ++count;
            }
        }
        return std::sqrt (sum / std::max (1, count)) / 0.70710678;
    };

    CHECK_MSG (gain (40.0) < 0.1, "high pass must reject rumble");
    CHECK_MSG (gain (2000.0) > 0.95, "high pass must pass the mid range");
}

void runDelayLine()
{
    mk::DelayLine line;
    line.prepare (2048);
    CHECK (line.isPrepared());

    line.write (1.0f);
    for (int i = 0; i < 100; ++i)
        line.write (0.0f);

    CHECK_NEAR (line.readHermite (100.0f), 1.0f, 1e-4);
    CHECK_NEAR (line.readLinear (100.0f), 1.0f, 1e-4);

    line.reset();
    CHECK_NEAR (line.readHermite (100.0f), 0.0f, 1e-9);

    // Fractional reads interpolate between neighbours.
    line.write (0.0f);
    line.write (1.0f);
    for (int i = 0; i < 10; ++i)
        line.write (0.0f);
    const float half = line.readLinear (10.5f);
    CHECK (half > 0.3f && half < 0.7f);
}

void runDcBlocker()
{
    mk::DcBlocker dc;
    dc.prepare (kSr, 8.0f);

    float last = 0.0f;
    for (int i = 0; i < (int) kSr; ++i)
        last = dc.process (1.0f);

    CHECK_MSG (std::fabs (last) < 0.01f, "DC blocker must remove a constant offset");
}

} // namespace

void runFilterTests()
{
    std::printf ("[filters]\n");
    runSvfResponse();
    runCascadeAndStability();
    runBiquad();
    runDelayLine();
    runDcBlocker();
}

} // namespace mktest
