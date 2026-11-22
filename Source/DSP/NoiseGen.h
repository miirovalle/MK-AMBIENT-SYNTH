#pragma once

// Procedural noise colours for the texture engine.
// All generators are normalised to roughly the same RMS so switching colour
// does not jump the layer level.

#include "Filters.h"
#include "MkCommon.h"

namespace mk
{

enum NoiseType
{
    NoiseWhite = 0,
    NoisePink,
    NoiseBrown,
    NoiseBlue,
    NoiseViolet,
    NoiseGrey
};

class NoiseGenerator
{
public:
    void prepare (double sampleRate, uint32_t seed) noexcept;
    void reset() noexcept;

    void setType (int typeIndex) noexcept
    {
        type = clampValue (typeIndex, 0, 5);
    }

    int getType() const noexcept { return type; }

    inline float process() noexcept
    {
        const float white = rng.nextBipolar();

        switch (type)
        {
            case NoiseWhite:
                return white * 0.50f;

            case NoisePink:
                return pinkSample (white);

            case NoiseBrown:
            {
                brown = (brown + white * 0.02f) / 1.02f;
                return dc.process (brown * 3.4f);
            }

            case NoiseBlue:
            {
                // Blue = differentiated pink: +3 dB/octave.
                const float p = pinkSample (white);
                const float d = p - prevPink;
                prevPink = p;
                return d * 3.0f;
            }

            case NoiseViolet:
            {
                // Violet = differentiated white: +6 dB/octave.
                const float d = (white - prevWhite) * 0.36f;
                prevWhite = white;
                return d;
            }

            case NoiseGrey:
            {
                // Inverse-A-weighting style balance: full lows, dipped mids,
                // lifted air. Reads as "even" rather than harsh.
                const float low = lpLow.process (white);
                const float high = white - lpHigher.process (white);
                return (low * 0.95f + high * 0.70f + white * 0.12f) * 0.55f;
            }

            default:
                return white * 0.5f;
        }
    }

private:
    // Paul Kellet's pink filter, shared by the pink and blue generators.
    inline float pinkSample (float white) noexcept
    {
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;
        const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;
        return pink * 0.11f;
    }

    XorShiftRng rng { 0x51A3u };
    int type = NoiseWhite;

    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f, b4 = 0.0f, b5 = 0.0f, b6 = 0.0f;
    float brown = 0.0f, prevWhite = 0.0f, prevPink = 0.0f;
    DcBlocker dc;
    OnePoleLowPass lpLow, lpHigher;
};

} // namespace mk
