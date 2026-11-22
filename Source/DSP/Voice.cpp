#include "Voice.h"

namespace mk
{

void Voice::prepare (double sampleRate, uint32_t seed)
{
    oscA.prepare (sampleRate, seed ^ 0x9E3779B9u);
    oscB.prepare (sampleRate, seed ^ 0x85EBCA6Bu);
    filterL.prepare (sampleRate);
    filterL.prepareDc (sampleRate);
    filterR.prepare (sampleRate);
    filterR.prepareDc (sampleRate);
    ampEnv.prepare (sampleRate);
    filterEnv.prepare (sampleRate);
    reset();
}

void Voice::reset()
{
    oscA.reset();
    oscB.reset();
    filterL.reset();
    filterR.reset();
    ampEnv.reset();
    filterEnv.reset();
    held = false;
    velocityGain = 0.0f;
    currentNote = (float) targetNote;
}

void Voice::noteOn (int midiNote, float velocity, bool glide)
{
    if (! isActive())
        currentNote = (float) midiNote;
    else if (! glide)
        currentNote = (float) midiNote;

    targetNote = midiNote;
    held = true;
    velocityGain = 0.90f * std::pow (clampf (velocity, 0.0f, 1.0f), 1.35f);
    ageCounter = 0.0f;

    ampEnv.noteOn();
    filterEnv.noteOn();
    oscA.noteOn();
    oscB.noteOn();
}

void Voice::noteOff()
{
    if (! held)
        return;
    held = false;
    ampEnv.noteOff();
    filterEnv.noteOff();
}

void Voice::panic()
{
    held = false;
    ampEnv.reset();
    filterEnv.reset();
    filterL.reset();
    filterR.reset();
    velocityGain = 0.0f;
}

void Voice::updateControlBlock (const VoiceControls& c)
{
    ageCounter += c.blockSeconds;

    currentNote += ((float) targetNote - currentNote) * c.glideCoeff;

    const auto& bank = WaveTableBank::instance();
    const WaveSet& setA = bank.set (c.waveA);
    const WaveSet& setB = bank.set (c.waveB);

    const float noteHz = midiToHz (currentNote);
    morphA = clampf (c.posA + c.blendOffset, 0.0f, 1.0f);
    morphB = clampf (c.posB + c.blendOffset, 0.0f, 1.0f);

    oscA.setPitch (noteHz, c.unisonA, c.detuneA, c.spreadA, c.drift, c.blockSeconds, setA, morphA);
    oscB.setPitch (noteHz * semitonesToRatio (c.intervalSemitones), c.unisonB, c.detuneB,
                   c.spreadB, c.drift, c.blockSeconds, setB, morphB);

    ampEnv.setParams (c.ampAttack, c.ampDecay, c.ampSustain, c.ampRelease, c.ampCurve);
    filterEnv.setParams (c.filtAttack, c.filtDecay, c.filtSustain, c.filtRelease, c.ampCurve);

    // Filter modulation only needs block resolution, so the envelope is
    // advanced once per control block instead of once per sample.
    filterEnv.advanceSamples (c.blockSamples);

    const float keySemitones = (currentNote - 60.0f) * c.keyTrack;
    const float envSemitones = filterEnv.getValue() * c.filterEnvAmount * 48.0f;
    const float cutoff = clampf (c.cutoffHz * std::exp2 ((keySemitones + envSemitones + c.lfoCutoffSemitones) / 12.0f),
                                 20.0f, 18000.0f);

    filterL.setParams (c.filterMode, cutoff, c.resonance, c.filterDrive);
    filterR.setParams (c.filterMode, cutoff, c.resonance, c.filterDrive);

    panGains (c.panMod, midLGain, midRGain);
    midLGain *= 1.41421356f;
    midRGain *= 1.41421356f;
    sideGain = 1.0f - 0.55f * std::fabs (clampf (c.panMod, -1.0f, 1.0f));
    spaceSend = c.spaceSend;
}

void Voice::render (float* outL, float* outR, float* sendL, float* sendR, int numSamples, const VoiceControls& c)
{
    if (! isActive())
        return;

    updateControlBlock (c);

    const auto& bank = WaveTableBank::instance();
    const WaveSet& setA = bank.set (c.waveA);
    const WaveSet& setB = bank.set (c.waveB);

    const float levelA = c.levelA;
    const float levelB = c.levelB;

    for (int i = 0; i < numSamples; ++i)
    {
        const float amp = ampEnv.process();

        float l = 0.0f, r = 0.0f;
        oscA.addTo (&l, &r, setA, levelA);
        oscB.addTo (&l, &r, setB, levelB);

        float fl = filterL.process (l);
        float fr = filterR.process (r);

        const float gain = amp * velocityGain;
        fl *= gain;
        fr *= gain;

        // Pan is applied in mid/side so the stereo image moves without
        // collapsing (a centred pan is bit-exact identity).
        const float mid = (fl + fr) * 0.5f;
        const float side = (fl - fr) * 0.5f * sideGain;
        const float ol = mid * midLGain + side;
        const float orr = mid * midRGain - side;

        outL[i] += ol;
        outR[i] += orr;
        sendL[i] += ol * spaceSend;
        sendR[i] += orr * spaceSend;
    }
}

} // namespace mk
