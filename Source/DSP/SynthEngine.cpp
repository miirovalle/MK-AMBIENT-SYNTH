#include "SynthEngine.h"

#include <chrono>

namespace mk
{

SynthEngine::SynthEngine()
{
    prepare (48000.0, 512);
}

void SynthEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock = clampValue (maxBlockSize, 32, 8192);

    store.prepare (sr);

    // Builds are a one-off cost: wavetables once per process, samples once.
    (void) WaveTableBank::instance();
    if (! library.isReady())
        library.prepare();

    for (int i = 0; i < kMaxVoices; ++i)
        voices[(size_t) i].prepare (sr, 0x1234567u + (uint32_t) i * 0x9E3779B9u);

    for (int i = 0; i < 3; ++i)
    {
        globalLfos[(size_t) i].prepare (0xABCDEF01u + (uint32_t) i * 7919u);
        globalLfos[(size_t) i].setRate (0.1f);
    }

    noise.prepare (sr, library);
    gate.prepare (sr);
    ensemble.prepare (sr);
    masterSaturator.prepare (sr);
    delay.prepare (sr);
    reverb.prepare (sr);
    limiter.prepare (sr, 2.0f);

    masterLowCutL.setHighPass (sr, 45.0f);
    masterLowCutR.setHighPass (sr, 45.0f);
    delayHighPassL.setHighPass (sr, 220.0f);
    delayHighPassR.setHighPass (sr, 220.0f);

    const size_t n = (size_t) maxBlock;
    padL.assign (n, 0.0f);
    padR.assign (n, 0.0f);
    padRevL.assign (n, 0.0f);
    padRevR.assign (n, 0.0f);
    noiseL.assign (n, 0.0f);
    noiseR.assign (n, 0.0f);
    delaySendL.assign (n, 0.0f);
    delaySendR.assign (n, 0.0f);
    revSendL.assign (n, 0.0f);
    revSendR.assign (n, 0.0f);
    delayWetL.assign (n, 0.0f);
    delayWetR.assign (n, 0.0f);
    revWetL.assign (n, 0.0f);
    revWetR.assign (n, 0.0f);

    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices)
        v.reset();
    noise.reset();
    gate.reset();
    ensemble.reset();
    masterSaturator.reset();
    delay.reset();
    reverb.reset();
    limiter.reset();
    masterLowCutL.reset();
    masterLowCutR.reset();
    delayHighPassL.reset();
    delayHighPassR.reset();
    adaptiveVoiceLimit = kMaxVoices;
    overloadBlocks = 0;
    calmBlocks = 0;
    meterOut.store (0.0f);
    meterPad.store (0.0f);
}

// ---------------------------------------------------------------------------
// Note handling
// ---------------------------------------------------------------------------

Voice* SynthEngine::findHeldVoice (int midiNote) noexcept
{
    for (auto& v : voices)
        if (v.isHeld() && v.note() == midiNote)
            return &v;
    return nullptr;
}

Voice* SynthEngine::allocateVoice()
{
    const int limit = clampValue (std::min ((int) store.get (P::Polyphony), adaptiveVoiceLimit),
                                  1, kMaxVoices);

    for (int i = 0; i < limit; ++i)
        if (! voices[(size_t) i].isActive())
            return &voices[(size_t) i];

    // Re-use the quietest released voice next.
    Voice* quietest = nullptr;
    float lowest = 2.0f;
    for (int i = 0; i < limit; ++i)
    {
        if (voices[(size_t) i].isHeld())
            continue;
        const float level = voices[(size_t) i].envelopeLevel();
        if (level < lowest)
        {
            lowest = level;
            quietest = &voices[(size_t) i];
        }
    }
    if (quietest != nullptr)
        return quietest;

    // Finally steal the oldest held voice.
    Voice* oldest = &voices[0];
    float maxAge = -1.0f;
    for (int i = 0; i < limit; ++i)
    {
        if (voices[(size_t) i].age() > maxAge)
        {
            maxAge = voices[(size_t) i].age();
            oldest = &voices[(size_t) i];
        }
    }
    return oldest;
}

int SynthEngine::countHeldVoices() const noexcept
{
    int count = 0;
    for (const auto& v : voices)
        if (v.isHeld())
            ++count;
    return count;
}

void SynthEngine::noteOn (int midiNote, float velocity)
{
    const bool wasSilent = countHeldVoices() == 0;

    Voice* voice = findHeldVoice (midiNote);
    if (voice == nullptr)
        voice = allocateVoice();
    if (voice == nullptr)
        return;

    const float glideMs = store.get (P::Glide);
    voice->noteOn (midiNote, velocity, glideMs > 1.0f);

    // Note-triggered gate mode restarts the pattern with every new phrase.
    gate.noteOn();

    if (wasSilent)
        noise.noteOn();
}

void SynthEngine::noteOff (int midiNote)
{
    if (Voice* voice = findHeldVoice (midiNote))
        voice->noteOff();

    if (countHeldVoices() == 0)
        noise.noteOff();
}

void SynthEngine::allNotesOff (bool immediate)
{
    for (auto& v : voices)
    {
        if (immediate)
            v.panic();
        else
            v.noteOff();
    }

    if (immediate)
        noise.panic();
    else
        noise.noteOff();
}

int SynthEngine::activeVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& v : voices)
        if (v.isActive())
            ++count;
    return count;
}

int SynthEngine::heldVoiceCount() const noexcept
{
    return countHeldVoices();
}

// ---------------------------------------------------------------------------
// Processing
// ---------------------------------------------------------------------------

GateSettings SynthEngine::makeGateSettings (const ParamSnapshot& p) const noexcept
{
    GateSettings s;
    s.mix = p[(size_t) P::GateMix];
    s.minLevel = p[(size_t) P::GateMin];
    s.maxLevel = p[(size_t) P::GateMax];
    s.tension = p[(size_t) P::GateTension];
    s.attackSeconds = p[(size_t) P::GateAttack];
    s.releaseSeconds = p[(size_t) P::GateRelease];
    s.phase01 = p[(size_t) P::GatePhase];
    s.triggerMode = (int) p[(size_t) P::GateTrigger];
    s.lengthIndex = (int) p[(size_t) P::GateLength];
    s.freeRateHz = p[(size_t) P::GateRate];
    s.bpm = hostBpm;
    s.transportPlaying = transportPlaying;

    for (int i = 0; i < kGateSteps; ++i)
        s.steps[(size_t) i] = p[(size_t) gateStepParam (i)];

    return s;
}

void SynthEngine::updateGlobalLfoRates (const ParamSnapshot& p) noexcept{
    globalLfos[0].setRate (p[(size_t) P::Lfo1Rate]);
    globalLfos[0].setShape ((int) p[(size_t) P::Lfo1Shape]);
    globalLfos[1].setRate (p[(size_t) P::Lfo2Rate]);
    globalLfos[1].setShape ((int) p[(size_t) P::Lfo2Shape]);
    globalLfos[2].setRate (p[(size_t) P::Lfo3Rate]);
    globalLfos[2].setShape ((int) p[(size_t) P::Lfo3Shape]);
}

void SynthEngine::updateLoadGuard (int numSamples) noexcept
{
    if (! adaptPolyphony || numSamples <= 0)
        return;

    const double budgetMs = 1000.0 * (double) numSamples / sr;
    if (budgetMs <= 0.0)
        return;

    // lastBlockMs covers the block just rendered.
    const double load = lastBlockMs / budgetMs;

    if (load > 0.65)
    {
        calmBlocks = 0;
        if (++overloadBlocks > 6 && adaptiveVoiceLimit > 4)
        {
            --adaptiveVoiceLimit;
            overloadBlocks = 0;
        }
    }
    else if (load < 0.35)
    {
        overloadBlocks = 0;
        if (++calmBlocks > 120 && adaptiveVoiceLimit < kMaxVoices)
        {
            ++adaptiveVoiceLimit;
            calmBlocks = 0;
        }
    }
    else
    {
        overloadBlocks = 0;
        calmBlocks = 0;
    }
}

void SynthEngine::process (float* left, float* right, int numSamples)
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const auto startTime = std::chrono::steady_clock::now();

    int offset = 0;
    while (offset < numSamples)
    {
        const int n = std::min (numSamples - offset, maxBlock);
        renderBlock (left + offset, right + offset, n);
        offset += n;
    }

    const auto endTime = std::chrono::steady_clock::now();
    lastBlockMs = std::chrono::duration<double, std::milli> (endTime - startTime).count();

    updateLoadGuard (numSamples);
}

void SynthEngine::renderBlock (float* left, float* right, int numSamples)
{
    store.advance (numSamples);
    const ParamSnapshot& p = store.smoothed();

    updateGlobalLfoRates (p);

    std::fill_n (padL.data(), numSamples, 0.0f);
    std::fill_n (padR.data(), numSamples, 0.0f);
    std::fill_n (padRevL.data(), numSamples, 0.0f);
    std::fill_n (padRevR.data(), numSamples, 0.0f);
    std::fill_n (noiseL.data(), numSamples, 0.0f);
    std::fill_n (noiseR.data(), numSamples, 0.0f);
    std::fill_n (delaySendL.data(), numSamples, 0.0f);
    std::fill_n (delaySendR.data(), numSamples, 0.0f);
    std::fill_n (revSendL.data(), numSamples, 0.0f);
    std::fill_n (revSendR.data(), numSamples, 0.0f);

    const float padLevel = p[(size_t) P::PadLevel];
    const float glideMs = p[(size_t) P::Glide];

    int done = 0;
    while (done < numSamples)
    {
        const int len = std::min (kControlBlockSize, numSamples - done);
        const float dt = (float) len / (float) sr;

        lfoValues[0] = globalLfos[0].advance (dt);
        lfoValues[1] = globalLfos[1].advance (dt);
        lfoValues[2] = globalLfos[2].advance (dt);

        const float cutoffMod = p[(size_t) P::Lfo1Cutoff] * lfoValues[0]
                              + p[(size_t) P::Lfo2Cutoff] * lfoValues[1]
                              + p[(size_t) P::Lfo3Cutoff] * lfoValues[2];
        const float detuneMod = p[(size_t) P::Lfo1Detune] * lfoValues[0]
                              + p[(size_t) P::Lfo2Detune] * lfoValues[1]
                              + p[(size_t) P::Lfo3Detune] * lfoValues[2];
        const float blendMod = p[(size_t) P::Lfo1Blend] * lfoValues[0]
                             + p[(size_t) P::Lfo2Blend] * lfoValues[1]
                             + p[(size_t) P::Lfo3Blend] * lfoValues[2];
        const float panMod = p[(size_t) P::Lfo1Pan] * lfoValues[0]
                           + p[(size_t) P::Lfo2Pan] * lfoValues[1]
                           + p[(size_t) P::Lfo3Pan] * lfoValues[2];
        const float spaceMod = p[(size_t) P::Lfo1Space] * lfoValues[0]
                             + p[(size_t) P::Lfo2Space] * lfoValues[1]
                             + p[(size_t) P::Lfo3Space] * lfoValues[2];

        VoiceControls vc;
        vc.waveA = (int) p[(size_t) P::OscAWave];
        vc.waveB = (int) p[(size_t) P::OscBWave];
        vc.posA = p[(size_t) P::OscAPos];
        vc.posB = p[(size_t) P::OscBPos];
        vc.levelA = p[(size_t) P::OscALevel] * padLevel;
        vc.levelB = p[(size_t) P::OscBLevel] * padLevel;
        vc.unisonA = (int) p[(size_t) P::OscAUnison];
        vc.unisonB = (int) p[(size_t) P::OscBUnison];
        vc.detuneA = clampf (p[(size_t) P::OscADetune] + detuneMod * 18.0f, 0.0f, 100.0f);
        vc.detuneB = clampf (p[(size_t) P::OscBDetune] + detuneMod * 18.0f, 0.0f, 100.0f);
        vc.spreadA = p[(size_t) P::OscASpread];
        vc.spreadB = p[(size_t) P::OscBSpread];
        vc.drift = p[(size_t) P::Drift];

        vc.intervalSemitones = (float) (int) std::round (p[(size_t) P::OscBInterval]);
        // Interval table: -24, -12, -7, -5, 0, +5, +7, +12
        static const float intervals[8] = { -24.0f, -12.0f, -7.0f, -5.0f, 0.0f, 5.0f, 7.0f, 12.0f };
        vc.intervalSemitones = intervals[clampValue ((int) vc.intervalSemitones, 0, 7)];

        vc.filterMode = (int) p[(size_t) P::FilterMode];
        vc.cutoffHz = p[(size_t) P::Cutoff];
        vc.resonance = p[(size_t) P::Resonance];
        vc.filterDrive = p[(size_t) P::FiltDrive];
        vc.filterEnvAmount = p[(size_t) P::FiltEnvAmt];
        vc.keyTrack = p[(size_t) P::FiltKeyTrack];
        vc.lfoCutoffSemitones = cutoffMod * 36.0f;

        vc.blendOffset = blendMod * 0.6f;
        vc.panMod = clampf (panMod * 0.85f, -1.0f, 1.0f);
        vc.spaceSend = clampf (p[(size_t) P::PadSpace] + spaceMod * 0.5f, 0.0f, 1.0f);

        const float glideSeconds = glideMs * 0.001f;
        vc.glideCoeff = glideSeconds > 0.002f ? (1.0f - std::exp (-dt / glideSeconds)) : 1.0f;
        vc.blockSeconds = dt;
        vc.blockSamples = len;

        vc.ampAttack = p[(size_t) P::AmpAttack];
        vc.ampDecay = p[(size_t) P::AmpDecay];
        vc.ampSustain = p[(size_t) P::AmpSustain];
        vc.ampRelease = p[(size_t) P::AmpRelease];
        vc.ampCurve = p[(size_t) P::AmpCurve];
        vc.filtAttack = p[(size_t) P::FiltAttack];
        vc.filtDecay = p[(size_t) P::FiltDecay];
        vc.filtSustain = p[(size_t) P::FiltSustain];
        vc.filtRelease = p[(size_t) P::FiltRelease];

        for (auto& voice : voices)
        {
            if (! voice.isActive())
                continue;
            voice.render (padL.data() + done, padR.data() + done,
                          padRevL.data() + done, padRevR.data() + done, len, vc);
        }

        noise.render (noiseL.data() + done, noiseR.data() + done,
                      delaySendL.data() + done, delaySendR.data() + done,
                      revSendL.data() + done, revSendR.data() + done, len, p);

        // ---- trance gate ---------------------------------------------------
        GateSettings gateSettings = makeGateSettings (p);
        gate.update (gateSettings, len);

        if (gateSettings.mix > 1.0e-4f)
        {
            const int target = (int) p[(size_t) P::GateTarget];
            const bool gatePad = target == 0 || target == 2;
            const bool gateTextures = target == 1 || target == 2;

            for (int i = 0; i < len; ++i)
            {
                const float gain = gate.nextGain();

                if (gatePad)
                {
                    padL[(size_t) (done + i)] *= gain;
                    padR[(size_t) (done + i)] *= gain;
                    padRevL[(size_t) (done + i)] *= gain;
                    padRevR[(size_t) (done + i)] *= gain;
                }

                if (gateTextures)
                {
                    noiseL[(size_t) (done + i)] *= gain;
                    noiseR[(size_t) (done + i)] *= gain;
                    delaySendL[(size_t) (done + i)] *= gain;
                    delaySendR[(size_t) (done + i)] *= gain;
                    revSendL[(size_t) (done + i)] *= gain;
                    revSendR[(size_t) (done + i)] *= gain;
                }
            }
        }

        done += len;
    }

    // ---- pad bus: ensemble width -------------------------------------------
    ensemble.setParams (p[(size_t) P::EnsembleDepth], p[(size_t) P::EnsembleRate]);
    ensemble.process (padL.data(), padR.data(), numSamples);

    // ---- delay return ------------------------------------------------------
    delay.setParams (p[(size_t) P::DelayTimeL], p[(size_t) P::DelayTimeR],
                     p[(size_t) P::DelayFeedback], p[(size_t) P::DelayDamp],
                     p[(size_t) P::DelayMod], p[(size_t) P::DelayMode] > 0.5f);
    delay.process (delaySendL.data(), delaySendR.data(), delayWetL.data(), delayWetR.data(), numSamples);

    const float delayMix = p[(size_t) P::DelayMix];
    const float revMix = p[(size_t) P::ReverbMix];

    for (int i = 0; i < numSamples; ++i)
    {
        const float wetL = delayHighPassL.process (delayWetL[(size_t) i]);
        const float wetR = delayHighPassR.process (delayWetR[(size_t) i]);
        delayWetL[(size_t) i] = wetL;
        delayWetR[(size_t) i] = wetR;

        // A touch of delay feeds the reverb for a deeper, more cohesive space.
        revSendL[(size_t) i] += padRevL[(size_t) i] + wetL * 0.2f * delayMix;
        revSendR[(size_t) i] += padRevR[(size_t) i] + wetR * 0.2f * delayMix;
    }

    // ---- reverb return -----------------------------------------------------
    reverb.setParams (p[(size_t) P::ReverbSize], p[(size_t) P::ReverbDecay], p[(size_t) P::ReverbDamp],
                      p[(size_t) P::ReverbPredelay], p[(size_t) P::ReverbWidth]);
    reverb.process (revSendL.data(), revSendR.data(), revWetL.data(), revWetR.data(), numSamples);

    // ---- master ------------------------------------------------------------
    masterLowCutL.setHighPass (sr, p[(size_t) P::MasterLowCut]);
    masterLowCutR.setHighPass (sr, p[(size_t) P::MasterLowCut]);

    masterSaturator.setDrive (p[(size_t) P::MasterDrive]);

    const float width = p[(size_t) P::MasterWidth];
    const float gain = dbToGain (p[(size_t) P::MasterGain]);

    limiter.setParams (-0.3f, 150.0f);

    float peakPad = 0.0f;
    float peakOut = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float l = padL[(size_t) i] + noiseL[(size_t) i]
                + delayWetL[(size_t) i] * delayMix + revWetL[(size_t) i] * revMix;
        float r = padR[(size_t) i] + noiseR[(size_t) i]
                + delayWetR[(size_t) i] * delayMix + revWetR[(size_t) i] * revMix;

        peakPad = std::max (peakPad, std::max (std::fabs (l), std::fabs (r)));

        const float mid = (l + r) * 0.5f;
        const float side = (l - r) * 0.5f * width;
        l = mid + side;
        r = mid - side;

        masterSaturator.processStereo (l, r);
        l = masterLowCutL.process (l) * gain;
        r = masterLowCutR.process (r) * gain;

        left[i] = l;
        right[i] = r;
    }

    limiter.process (left, right, numSamples);

    for (int i = 0; i < numSamples; ++i)
        peakOut = std::max (peakOut, std::max (std::fabs (left[i]), std::fabs (right[i])));

    const float smoothing = 0.35f;
    meterPad.store (meterPad.load (std::memory_order_relaxed) * (1.0f - smoothing) + peakPad * smoothing,
                    std::memory_order_relaxed);
    meterOut.store (meterOut.load (std::memory_order_relaxed) * (1.0f - smoothing) + peakOut * smoothing,
                    std::memory_order_relaxed);

    gatePhase01.store (gate.phase(), std::memory_order_relaxed);
    gateLevelValue.store (gate.isRunning() ? gate.currentLevel() : 1.0f, std::memory_order_relaxed);
}

} // namespace mk
