#include "Effects.h"

namespace mk
{

// ---------------------------------------------------------------------------
// Ensemble
// ---------------------------------------------------------------------------

void Ensemble::prepare (double sampleRate) noexcept
{
    sr = (float) sampleRate;
    const int maxDelay = (int) (sr * 0.05f) + 8;
    lineL.prepare (maxDelay);
    lineR.prepare (maxDelay);
    reset();
}

void Ensemble::reset() noexcept
{
    lineL.reset();
    lineR.reset();
    phaseA = 0.0f;
    phaseB = 0.31f;
    phaseC = 0.67f;
}

void Ensemble::process (float* left, float* right, int numSamples) noexcept
{
    if (depth <= 1.0e-4f)
        return;

    const float baseMs = 7.0f;
    const float sweepMs = 3.2f;
    const float dryGain = 1.0f - 0.35f * depth;
    const float wetGain = 0.75f * depth;
    const float phaseInc = rate / sr;
    modDepthSamples = sweepMs * 0.001f * sr;

    for (int i = 0; i < numSamples; ++i)
    {
        phaseA += phaseInc;
        phaseB += phaseInc * 1.37f;
        phaseC += phaseInc * 0.73f;
        if (phaseA >= 1.0f) phaseA -= 1.0f;
        if (phaseB >= 1.0f) phaseB -= 1.0f;
        if (phaseC >= 1.0f) phaseC -= 1.0f;

        const auto& sine = SineTable::instance();
        const float dAL = (baseMs + sweepMs * sine.lookup (phaseA)) * 0.001f * sr;
        const float dBL = (baseMs + sweepMs * sine.lookup (phaseB + 0.25f)) * 0.001f * sr;
        const float dAR = (baseMs + sweepMs * sine.lookup (phaseA + 0.5f)) * 0.001f * sr;
        const float dBR = (baseMs + sweepMs * sine.lookup (phaseC + 0.75f)) * 0.001f * sr;

        const float inL = left[i];
        const float inR = right[i];

        lineL.write (inL);
        lineR.write (inR);

        const float wetL = 0.5f * (lineL.readHermite (dAL) + lineL.readHermite (dBL));
        const float wetR = 0.5f * (lineR.readHermite (dAR) + lineR.readHermite (dBR));

        left[i] = inL * dryGain + wetL * wetGain;
        right[i] = inR * dryGain + wetR * wetGain;
    }
}

// ---------------------------------------------------------------------------
// StereoDelay
// ---------------------------------------------------------------------------

void StereoDelay::prepare (double sampleRate) noexcept
{
    sr = (float) sampleRate;
    const int maxSamples = (int) (sr * (kMaxDelayMs * 0.001f + 0.05f)) + 16;
    lineL.prepare (maxSamples);
    lineR.prepare (maxSamples);
    dampL.setCutoff (sampleRate, 6000.0f);
    dampR.setCutoff (sampleRate, 6000.0f);
    reset();
}

void StereoDelay::reset() noexcept
{
    lineL.reset();
    lineR.reset();
    dampL.reset();
    dampR.reset();
    modPhase = 0.0f;
    idle = false;
}

void StereoDelay::setParams (float timeLms, float timeRms, float feedback01, float damp01,
                             float motion01, bool pingPongMode) noexcept
{
    delayL = clampf (timeLms, 5.0f, kMaxDelayMs) * 0.001f * sr;
    delayR = clampf (timeRms, 5.0f, kMaxDelayMs) * 0.001f * sr;
    feedback = clampf (feedback01, 0.0f, 0.95f);
    motion = clampf (motion01, 0.0f, 1.0f);
    pingPong = pingPongMode;

    // Damping in the feedback path: darker repeats decay faster in the highs.
    const float cutoff = expMap (1.0f - clampf (damp01, 0.0f, 1.0f), 900.0f, 16000.0f);
    dampL.setCutoff (sr, cutoff);
    dampR.setCutoff (sr, cutoff);
}

void StereoDelay::process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept
{
    // Skip the DSP entirely once the line has gone quiet and nothing is coming
    // in: an ambient preset can sit silent for minutes at a time.
    float inputPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inputPeak = std::max (inputPeak, std::max (std::fabs (inL[i]), std::fabs (inR[i])));

    if (inputPeak < 1.0e-7f && idle)
    {
        std::fill_n (wetL, numSamples, 0.0f);
        std::fill_n (wetR, numSamples, 0.0f);
        return;
    }

    const float modDepth = 4.0f * motion * 0.001f * sr; // up to 4 ms
    const float modInc = 0.11f / sr;
    float outputPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        modPhase += modInc;
        if (modPhase >= 1.0f)
            modPhase -= 1.0f;

        const float wobble = SineTable::instance().lookup (modPhase) * modDepth;
        const float wobble2 = SineTable::instance().lookup (modPhase + 0.37f) * modDepth;

        const float readL = lineL.readHermite (delayL + wobble);
        const float readR = lineR.readHermite (delayR + wobble2);

        float writeL, writeR;
        if (pingPong)
        {
            // Cross-feedback: repeats bounce between the channels.
            writeL = inL[i] + dampR.process (readR) * feedback;
            writeR = inR[i] + dampL.process (readL) * feedback;
        }
        else
        {
            writeL = inL[i] + dampL.process (readL) * feedback;
            writeR = inR[i] + dampR.process (readR) * feedback;
        }

        lineL.write (writeL);
        lineR.write (writeR);

        wetL[i] = readL;
        wetR[i] = readR;
        outputPeak = std::max (outputPeak, std::max (std::fabs (readL), std::fabs (readR)));
    }

    idle = outputPeak < 3.0e-6f;
}

// ---------------------------------------------------------------------------
// SpaceReverb
// ---------------------------------------------------------------------------

void SpaceReverb::prepare (double sampleRate) noexcept
{
    sr = (float) sampleRate;

    // Mutually prime lengths (ms) give a dense, non-metallic tail.
    static const float baseMs[kLines] = { 29.7f, 37.1f, 41.1f, 43.7f, 47.3f, 53.1f, 59.3f, 67.9f };
    for (int i = 0; i < kLines; ++i)
        baseDelayMs[(size_t) i] = baseMs[i];

    const int maxLineSamples = (int) (sr * 0.16f) + 32;
    for (auto& line : lines)
        line.prepare (maxLineSamples);

    preL.prepare ((int) (sr * 0.25f) + 16);
    preR.prepare ((int) (sr * 0.25f) + 16);

    diffuserL[0].prepare (sampleRate, 12.6f, 0.62f);
    diffuserL[1].prepare (sampleRate, 19.3f, 0.58f);
    diffuserR[0].prepare (sampleRate, 14.9f, 0.62f);
    diffuserR[1].prepare (sampleRate, 23.1f, 0.58f);

    for (int i = 0; i < kLines; ++i)
    {
        modPhase[(size_t) i] = (float) i / (float) kLines;
        modIncrement[(size_t) i] = (0.053f + 0.041f * (float) (i % 4)) / sr;
    }

    lowCutL.setHighPass (sampleRate, 160.0f);
    lowCutR.setHighPass (sampleRate, 160.0f);

    setParams (0.6f, 0.55f, 0.45f, 30.0f, 0.9f);
    reset();
}

void SpaceReverb::reset() noexcept
{
    for (auto& line : lines)
        line.reset();
    for (auto& d : diffuserL)
        d.reset();
    for (auto& d : diffuserR)
        d.reset();
    for (auto& d : damping)
        d.reset();
    preL.reset();
    preR.reset();
    lowCutL.reset();
    lowCutR.reset();
    idle = false;
}

void SpaceReverb::setParams (float size01, float decay01, float damp01,
                             float predelayMs, float width01) noexcept
{
    size = clampf (size01, 0.0f, 1.0f);
    width = clampf (width01, 0.0f, 1.0f);
    preDelaySamples = clampf (predelayMs, 0.0f, 200.0f) * 0.001f * sr;

    const float scale = 0.45f + 1.15f * size;
    decaySeconds = expMap (clampf (decay01, 0.0f, 1.0f), 0.9f, 32.0f);

    const float dampCutoff = expMap (1.0f - clampf (damp01, 0.0f, 1.0f), 900.0f, 15000.0f);
    for (int i = 0; i < kLines; ++i)
    {
        lineDelay[(size_t) i] = baseDelayMs[(size_t) i] * scale * 0.001f * sr;
        const float lenSeconds = lineDelay[(size_t) i] / sr;
        feedbackGain[(size_t) i] = std::pow (10.0f, -3.0f * lenSeconds / decaySeconds);
        damping[(size_t) i].setCutoff (sr, dampCutoff);
    }
}

void SpaceReverb::process (const float* inL, const float* inR, float* wetL, float* wetR, int numSamples) noexcept
{
    // The FDN is the most expensive block in the engine; once its tail has
    // decayed below audibility there is nothing to compute.
    float inputPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inputPeak = std::max (inputPeak, std::max (std::fabs (inL[i]), std::fabs (inR[i])));

    if (inputPeak < 1.0e-7f && idle)
    {
        std::fill_n (wetL, numSamples, 0.0f);
        std::fill_n (wetR, numSamples, 0.0f);
        return;
    }

    const auto& sine = SineTable::instance();
    const float modDepth = 0.35f * 0.001f * sr; // subtle: 0.35 ms
    float outputPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // Pre-delay
        const float inMonoL = inL[i];
        const float inMonoR = inR[i];
        preL.write (inMonoL);
        preR.write (inMonoR);
        const float dl = preDelaySamples > 1.0f ? preL.readLinear (preDelaySamples) : inMonoL;
        const float dr = preDelaySamples > 1.0f ? preR.readLinear (preDelaySamples) : inMonoR;

        // Input diffusion
        const float diffL = diffuserL[1].process (diffuserL[0].process (dl));
        const float diffR = diffuserR[1].process (diffuserR[0].process (dr));

        // Read the network
        float v[kLines];
        for (int j = 0; j < kLines; ++j)
        {
            modPhase[(size_t) j] += modIncrement[(size_t) j];
            if (modPhase[(size_t) j] >= 1.0f)
                modPhase[(size_t) j] -= 1.0f;

            const float mod = sine.lookup (modPhase[(size_t) j]) * modDepth;
            v[j] = lines[(size_t) j].readHermite (lineDelay[(size_t) j] + mod);
        }

        // Damping + decay gain inside the loop
        for (int j = 0; j < kLines; ++j)
            v[j] = damping[(size_t) j].process (v[j]) * feedbackGain[(size_t) j];

        // Fast Walsh-Hadamard mixing (orthonormal, energy preserving)
        for (int step = 1; step < kLines; step <<= 1)
        {
            for (int j = 0; j < kLines; j += step << 1)
            {
                for (int k = j; k < j + step; ++k)
                {
                    const float a = v[k];
                    const float b = v[k + step];
                    v[k] = a + b;
                    v[k + step] = a - b;
                }
            }
        }
        constexpr float norm = 0.35355339f; // 1/sqrt(8)
        for (int j = 0; j < kLines; ++j)
            v[j] *= norm;

        // Inject new material alternating between channels
        for (int j = 0; j < kLines; ++j)
        {
            const float inject = ((j & 1) == 0 ? diffL : diffR) * ((j & 2) == 0 ? 0.42f : -0.42f);
            lines[(size_t) j].write (inject + v[j]);
        }

        float outL = (v[0] + v[2] + v[4] + v[6]) * 0.35f;
        float outR = (v[1] + v[3] + v[5] + v[7]) * 0.35f;

        const float mid = (outL + outR) * 0.5f;
        const float side = (outL - outR) * 0.5f * width;
        outL = lowCutL.process (mid + side);
        outR = lowCutR.process (mid - side);

        wetL[i] = outL;
        wetR[i] = outR;
        outputPeak = std::max (outputPeak, std::max (std::fabs (outL), std::fabs (outR)));
    }

    idle = outputPeak < 3.0e-6f;
}

// ---------------------------------------------------------------------------
// Limiter
// ---------------------------------------------------------------------------

void Limiter::prepare (double sampleRate, float lookaheadMs) noexcept
{
    sr = (float) sampleRate;
    lookaheadSamples = (int) (lookaheadMs * 0.001f * sr);
    delayL.prepare (lookaheadSamples + 8);
    delayR.prepare (lookaheadSamples + 8);
    setParams (-0.3f, 140.0f);
    reset();
}

void Limiter::reset() noexcept
{
    delayL.reset();
    delayR.reset();
    envelope = 0.0f;
    currentGain = 1.0f;
}

void Limiter::setParams (float ceilingDb, float releaseMs) noexcept
{
    ceiling = dbToGain (clampf (ceilingDb, -24.0f, 0.0f));
    releaseCoeff = 1.0f - std::exp (-1.0f / std::max (1.0f, releaseMs * 0.001f * sr));
    // The gain ramp completes in about one look-ahead window, so reduction is
    // fully in place by the time the peak reaches the output.
    attackCoeff = 1.0f - std::exp (-1.0f / std::max (1.0f, (float) lookaheadSamples * 0.33f));
}

void Limiter::process (float* left, float* right, int numSamples) noexcept
{
    const float look = (float) std::max (1, lookaheadSamples);
    float gain = currentGain;

    for (int i = 0; i < numSamples; ++i)
    {
        const float peak = std::max (std::fabs (left[i]), std::fabs (right[i]));
        if (peak > envelope)
            envelope = peak;
        else
            envelope += (peak - envelope) * releaseCoeff;

        const float targetGain = (envelope > ceiling && envelope > 1.0e-6f) ? ceiling / envelope : 1.0f;
        const float coeff = targetGain < gain ? attackCoeff : releaseCoeff;
        gain += (targetGain - gain) * coeff;

        delayL.write (left[i]);
        delayR.write (right[i]);
        const float outL = delayL.readLinear (look);
        const float outR = delayR.readLinear (look);

        left[i] = clampf (outL * gain, -1.0f, 1.0f);
        right[i] = clampf (outR * gain, -1.0f, 1.0f);
    }

    currentGain = gain;
}

} // namespace mk
