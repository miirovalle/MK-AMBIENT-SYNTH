#pragma once

// Space and tone-shaping effects.
//
// * Saturator    - gentle analog warmth, used on the pad bus and inside slots
// * Ensemble     - wide modulated chorus built from short stereo delay taps
// * StereoDelay  - free-time (ms) delay with damping, motion and ping-pong
// * SpaceReverb  - 8-line feedback delay network with diffusion, pre-delay,
//                  damping, width and a wet high pass that protects the lows
// * Limiter      - look-ahead peak limiter for the master output

#include "Filters.h"

namespace mk
{

class Saturator
{
public:
    void prepare (double sampleRate) noexcept
    {
        dcL.prepare (sampleRate, 10.0f);
        dcR.prepare (sampleRate, 10.0f);
    }

    void reset() noexcept
    {
        dcL.reset();
        dcR.reset();
    }

    void setDrive (float drive01) noexcept
    {
        drive = clampf (drive01, 0.0f, 1.0f);
        gain = 1.0f + 5.0f * drive;
        makeup = 1.0f + 0.35f * drive;
    }

    inline float process (float x) noexcept { return dcL.process (shape (x)); }

    // Stereo-safe: each channel keeps its own DC blocker state so the two
    // channels never bleed into each other.
    inline void processStereo (float& left, float& right) noexcept
    {
        left = dcL.process (shape (left));
        right = dcR.process (shape (right));
    }

private:
    inline float shape (float x) const noexcept { return fastTanh (x * gain) * makeup / gain; }

    DcBlocker dcL, dcR;
    float drive = 0.0f, gain = 1.0f, makeup = 1.0f;
};

class Ensemble
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setParams (float depth01, float rateHz) noexcept
    {
        depth = clampf (depth01, 0.0f, 1.0f);
        rate = clampf (rateHz, 0.01f, 8.0f);
    }

    // In-place stereo width / movement.
    void process (float* left, float* right, int numSamples) noexcept;

private:
    DelayLine lineL, lineR;
    float sr = 48000.0f;
    float depth = 0.3f, rate = 0.35f;
    float phaseA = 0.0f, phaseB = 0.31f, phaseC = 0.67f;
    float modDepthSamples = 0.0f;
};

class StereoDelay
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setParams (float timeLms, float timeRms, float feedback01, float damp01,
                    float motion01, bool pingPong) noexcept;

    // Produces only the wet return; the dry signal is mixed by the engine.
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept;

    // True when the line has been silent long enough to skip processing.
    bool isIdle() const noexcept { return idle; }

private:
    DelayLine lineL, lineR;
    OnePoleLowPass dampL, dampR;
    float sr = 48000.0f;
    float delayL = 20000.0f, delayR = 25000.0f;
    float feedback = 0.4f;
    float motion = 0.2f;
    bool pingPong = false;
    float modPhase = 0.0f;
    bool idle = false;
    static constexpr float kMaxDelayMs = 2000.0f;
};

class SpaceReverb
{
public:
    static constexpr int kLines = 8;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setParams (float size01, float decay01, float damp01, float predelayMs, float width01) noexcept;

    // Wet return only, low-cut internally so the pad's low end stays tight.
    void process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept;

    // True when the network carries no audible energy and can be skipped.
    bool isIdle() const noexcept { return idle; }

private:
    std::array<DelayLine, (size_t) kLines> lines;
    std::array<OnePoleLowPass, (size_t) kLines> damping;
    std::array<float, (size_t) kLines> lineDelay {};
    std::array<float, (size_t) kLines> feedbackGain {};
    std::array<float, (size_t) kLines> modPhase {};
    std::array<float, (size_t) kLines> modIncrement {};
    std::array<float, (size_t) kLines> baseDelayMs {};

    std::array<AllPassDiffuser, 2> diffuserL;
    std::array<AllPassDiffuser, 2> diffuserR;

    DelayLine preL, preR;
    Biquad lowCutL, lowCutR;

    float sr = 48000.0f;
    float preDelaySamples = 0.0f;
    float width = 0.9f;
    float size = 0.6f;
    float decaySeconds = 6.0f;
    bool idle = false;
};

class Limiter
{
public:
    void prepare (double sampleRate, float lookaheadMs = 2.0f) noexcept;
    void reset() noexcept;

    void setParams (float ceilingDb, float releaseMs) noexcept;

    void process (float* left, float* right, int numSamples) noexcept;

private:
    DelayLine delayL, delayR;
    float sr = 48000.0f;
    float ceiling = 0.99f;
    float attackCoeff = 0.5f, releaseCoeff = 0.0001f;
    float envelope = 0.0f;
    float currentGain = 1.0f;
    int lookaheadSamples = 0;
};

} // namespace mk
