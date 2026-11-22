#include "NoiseGen.h"

namespace mk
{

void NoiseGenerator::prepare (double sampleRate, uint32_t seed) noexcept
{
    rng.seed (seed);
    dc.prepare (sampleRate, 12.0f);
    lpLow.setCutoff (sampleRate, 500.0f);
    lpHigher.setCutoff (sampleRate, 3500.0f);
    reset();
}

void NoiseGenerator::reset() noexcept
{
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    brown = prevWhite = prevPink = 0.0f;
    dc.reset();
    lpLow.reset();
    lpHigher.reset();
}

} // namespace mk
