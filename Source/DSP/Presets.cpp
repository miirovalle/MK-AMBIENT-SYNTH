#include "Presets.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace mk
{

namespace
{

// Slot-source indices: six procedural colours, then the 132-entry catalogue.
constexpr int kSrcWhite = 0;
constexpr int kSrcPink = 1;
constexpr int kSrcBrown = 2;
constexpr int kSrcBlue = 3;
constexpr int kSrcViolet = 4;
constexpr int kSrcGrey = 5;

constexpr int kCrackle = 6;    // + 0..21
constexpr int kVinyl = 28;     // + 0..21
constexpr int kTape = 50;      // + 0..21
constexpr int kFoley = 72;     // + 0..21
constexpr int kGranular = 94;  // + 0..21
constexpr int kCity = 116;     // + 0..21

// ---------------------------------------------------------------------------
// Patch builder
// ---------------------------------------------------------------------------

struct Patch
{
    ParamSnapshot s = defaultSnapshot();

    Patch& set (P id, float value)
    {
        s[(size_t) id] = sanitiseParam (id, value);
        return *this;
    }

    Patch& osc (int waveA, float posA, float levelA, int unisonA, float detuneA, float spreadA,
                int waveB, float posB, float levelB, int unisonB, float detuneB, float spreadB,
                int intervalIndex)
    {
        set (P::OscAWave, (float) waveA);
        set (P::OscAPos, posA);
        set (P::OscALevel, levelA);
        set (P::OscAUnison, (float) unisonA);
        set (P::OscADetune, detuneA);
        set (P::OscASpread, spreadA);
        set (P::OscBWave, (float) waveB);
        set (P::OscBPos, posB);
        set (P::OscBLevel, levelB);
        set (P::OscBUnison, (float) unisonB);
        set (P::OscBDetune, detuneB);
        set (P::OscBSpread, spreadB);
        set (P::OscBInterval, (float) intervalIndex);
        return *this;
    }

    Patch& filter (int mode, float cutoff, float resonance, float envAmount, float keyTrack,
                   float drive, float attack, float decay, float sustain, float release)
    {
        set (P::FilterMode, (float) mode);
        set (P::Cutoff, cutoff);
        set (P::Resonance, resonance);
        set (P::FiltEnvAmt, envAmount);
        set (P::FiltKeyTrack, keyTrack);
        set (P::FiltDrive, drive);
        set (P::FiltAttack, attack);
        set (P::FiltDecay, decay);
        set (P::FiltSustain, sustain);
        set (P::FiltRelease, release);
        return *this;
    }

    Patch& ampEnv (float attack, float decay, float sustain, float release, float curve)
    {
        set (P::AmpAttack, attack);
        set (P::AmpDecay, decay);
        set (P::AmpSustain, sustain);
        set (P::AmpRelease, release);
        set (P::AmpCurve, curve);
        return *this;
    }

    Patch& lfo (int index, float rate, int shape, float toCutoff, float toDetune,
                float toBlend, float toPan, float toSpace)
    {
        const int base = (int) P::Lfo1Rate + index * 7;
        set ((P) base, rate);
        set ((P) (base + 1), (float) shape);
        set ((P) (base + 2), toCutoff);
        set ((P) (base + 3), toDetune);
        set ((P) (base + 4), toBlend);
        set ((P) (base + 5), toPan);
        set ((P) (base + 6), toSpace);
        return *this;
    }

    Patch& slot (int index, int source, float level, int filterType, float cutoff, float resonance,
                 float pan, float drive, float delaySend, float reverbSend,
                 float lfoRate, int lfoShape, float lfoCutoff, float lfoPan, float lfoSpace,
                 float swell, int mode = 0)
    {
        set (slotParam (index, 0), (float) source);
        set (slotParam (index, 1), level);
        set (slotParam (index, 2), (float) mode);
        set (slotParam (index, 4), (float) filterType);
        set (slotParam (index, 5), cutoff);
        set (slotParam (index, 6), resonance);
        set (slotParam (index, 7), pan);
        set (slotParam (index, 8), drive);
        set (slotParam (index, 9), delaySend);
        set (slotParam (index, 10), reverbSend);
        set (slotParam (index, 11), lfoRate);
        set (slotParam (index, 12), (float) lfoShape);
        set (slotParam (index, 13), lfoCutoff);
        set (slotParam (index, 14), lfoPan);
        set (slotParam (index, 15), lfoSpace);
        set (slotParam (index, 16), swell);
        return *this;
    }

    Patch& space (float delayL, float delayR, float feedback, float delayDamp, float delayMix,
                  float delayMotion, int delayMode,
                  float revSize, float revDecay, float revDamp, float predelay, float revWidth,
                  float revMix, float ensembleDepth, float ensembleRate, float padSpace)
    {
        set (P::DelayTimeL, delayL);
        set (P::DelayTimeR, delayR);
        set (P::DelayFeedback, feedback);
        set (P::DelayDamp, delayDamp);
        set (P::DelayMix, delayMix);
        set (P::DelayMod, delayMotion);
        set (P::DelayMode, (float) delayMode);
        set (P::ReverbSize, revSize);
        set (P::ReverbDecay, revDecay);
        set (P::ReverbDamp, revDamp);
        set (P::ReverbPredelay, predelay);
        set (P::ReverbWidth, revWidth);
        set (P::ReverbMix, revMix);
        set (P::EnsembleDepth, ensembleDepth);
        set (P::EnsembleRate, ensembleRate);
        set (P::PadSpace, padSpace);
        return *this;
    }

    Patch& master (float gain, float drive, float lowCut, float width)
    {
        set (P::MasterGain, gain);
        set (P::MasterDrive, drive);
        set (P::MasterLowCut, lowCut);
        set (P::MasterWidth, width);
        return *this;
    }
};

// ---------------------------------------------------------------------------
// Families
// ---------------------------------------------------------------------------

void buildSubmerged (Patch& p)
{
    p.set (P::PadLevel, 0.85f).set (P::Drift, 0.55f).set (P::Polyphony, 7)
     .osc (1, 0.10f, 0.55f, 3, 6, 0.45f, 0, 0.15f, 0.30f, 2, 4, 0.40f, 1)
     .filter (1, 420, 0.18f, 0.12f, 0.15f, 0.20f, 3.0f, 4.0f, 0.60f, 6.0f)
     .ampEnv (1.20f, 4.0f, 0.85f, 9.0f, 0.35f)
     .lfo (0, 0.035f, 6, 0.25f, 0.08f, 0.08f, 0.15f, 0.30f)
     .lfo (1, 0.020f, 0, 0.18f, 0.05f, 0.0f, 0.0f, 0.22f)
     .lfo (2, 0.060f, 6, 0.10f, 0.0f, 0.0f, 0.20f, 0.0f)
     .set (P::NoiseLevel, 0.55f).set (P::GrainSize, 120).set (P::GrainSpray, 0.60f)
     .slot (0, kSrcBrown, 0.55f, 0, 180, 0.15f, 0.0f, 0.20f, 0.12f, 0.40f, 0.045f, 0, 0.25f, 0.15f, 0.25f, 6.0f)
     .slot (1, kCity + 2, 0.20f, 0, 420, 0.20f, -0.20f, 0.15f, 0.20f, 0.50f, 0.030f, 6, 0.20f, 0.20f, 0.30f, 9.0f)
     .space (620, 780, 0.50f, 0.60f, 0.15f, 0.30f, 0,
             0.85f, 0.75f, 0.60f, 40, 0.95f, 0.60f, 0.30f, 0.20f, 0.45f)
     .master (-3.0f, 0.25f, 35, 1.10f);
}

void buildAbyssal (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.65f).set (P::Polyphony, 6).set (P::Glide, 90)
     .osc (1, 0.05f, 0.50f, 2, 4, 0.30f, 0, 0.10f, 0.26f, 2, 3, 0.30f, 0)
     .filter (1, 300, 0.22f, 0.10f, 0.10f, 0.28f, 3.5f, 4.5f, 0.60f, 8.0f)
     .ampEnv (2.60f, 5.0f, 0.90f, 12.0f, 0.42f)
     .lfo (0, 0.025f, 6, 0.20f, 0.06f, 0.10f, 0.12f, 0.30f)
     .lfo (1, 0.050f, 0, 0.12f, 0.0f, 0.0f, 0.0f, 0.20f)
     .lfo (2, 0.080f, 5, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f)
     .set (P::NoiseLevel, 0.62f).set (P::GrainSize, 150).set (P::GrainSpray, 0.70f)
     .slot (0, kSrcBrown, 0.50f, 0, 140, 0.20f, 0.0f, 0.25f, 0.10f, 0.50f, 0.035f, 0, 0.20f, 0.10f, 0.30f, 7.0f)
     .slot (1, kCity + 0, 0.18f, 0, 300, 0.20f, 0.25f, 0.20f, 0.15f, 0.60f, 0.025f, 6, 0.15f, 0.20f, 0.30f, 11.0f)
     .slot (2, kSrcGrey, 0.12f, 1, 500, 0.40f, -0.30f, 0.15f, 0.10f, 0.55f, 0.040f, 6, 0.20f, 0.20f, 0.25f, 9.0f)
     .space (700, 900, 0.55f, 0.70f, 0.18f, 0.30f, 0,
             0.95f, 0.80f, 0.65f, 55, 0.95f, 0.65f, 0.25f, 0.15f, 0.50f)
     .master (-3.0f, 0.30f, 30, 1.15f);
}

void buildGlacier (Patch& p)
{
    p.set (P::PadLevel, 0.78f).set (P::Drift, 0.35f).set (P::Polyphony, 8)
     .osc (2, 0.35f, 0.62f, 4, 10, 0.80f, 3, 0.40f, 0.25f, 3, 6, 0.80f, 7)
     .filter (0, 2000, 0.25f, 0.20f, 0.40f, 0.05f, 4.0f, 5.0f, 0.50f, 9.0f)
     .ampEnv (5.0f, 6.0f, 0.85f, 12.0f, 0.30f)
     .lfo (0, 0.040f, 0, 0.30f, 0.10f, 0.10f, 0.35f, 0.35f)
     .lfo (1, 0.090f, 6, 0.25f, 0.05f, 0.20f, 0.20f, 0.30f)
     .lfo (2, 0.060f, 0, 0.10f, 0.0f, 0.0f, 0.15f, 0.15f)
     .set (P::NoiseLevel, 0.60f).set (P::GrainSize, 110).set (P::GrainSpray, 0.65f)
     .slot (0, kSrcBlue, 0.26f, 1, 3200, 0.30f, -0.30f, 0.08f, 0.30f, 0.60f, 0.060f, 0, 0.35f, 0.30f, 0.30f, 6.0f)
     .slot (1, kSrcWhite, 0.13f, 1, 5500, 0.40f, 0.35f, 0.05f, 0.25f, 0.55f, 0.080f, 6, 0.30f, 0.30f, 0.25f, 8.0f)
     .slot (2, kGranular + 2, 0.12f, 0, 5000, 0.20f, 0.05f, 0.05f, 0.30f, 0.60f, 0.045f, 5, 0.25f, 0.20f, 0.20f, 9.0f)
     // Permanent sub bed: keeps every Glacier variation anchored in the low end.
     .slot (3, kSrcBrown, 0.38f, 0, 120, 0.15f, 0.0f, 0.20f, 0.0f, 0.25f, 0.020f, 0, 0.15f, 0.10f, 0.15f, 7.0f)
     .space (380, 520, 0.50f, 0.35f, 0.30f, 0.25f, 1,
             0.90f, 0.75f, 0.30f, 60, 1.0f, 0.70f, 0.40f, 0.30f, 0.40f)
     .master (-3.5f, 0.10f, 70, 1.20f);
}

void buildCathedral (Patch& p)
{
    p.set (P::PadLevel, 0.82f).set (P::Drift, 0.40f).set (P::Polyphony, 10)
     .osc (1, 0.50f, 0.60f, 6, 14, 0.85f, 5, 0.30f, 0.35f, 2, 5, 0.60f, 6)
     .filter (1, 3000, 0.18f, 0.35f, 0.30f, 0.08f, 5.0f, 6.0f, 0.55f, 10.0f)
     .ampEnv (6.0f, 7.0f, 0.90f, 14.0f, 0.35f)
     .lfo (0, 0.030f, 0, 0.35f, 0.05f, 0.10f, 0.30f, 0.40f)
     .lfo (1, 0.055f, 6, 0.20f, 0.08f, 0.10f, 0.15f, 0.35f)
     .lfo (2, 0.020f, 0, 0.10f, 0.0f, 0.0f, 0.10f, 0.15f)
     .set (P::NoiseLevel, 0.50f).set (P::GrainSize, 180).set (P::GrainSpray, 0.50f)
     .slot (0, kVinyl + 6, 0.20f, 0, 2500, 0.15f, 0.10f, 0.10f, 0.10f, 0.50f, 0.030f, 0, 0.15f, 0.10f, 0.20f, 8.0f)
     .slot (1, kSrcPink, 0.25f, 1, 1500, 0.35f, -0.25f, 0.05f, 0.15f, 0.60f, 0.045f, 6, 0.25f, 0.25f, 0.30f, 10.0f)
     .space (560, 760, 0.45f, 0.55f, 0.18f, 0.25f, 0,
             1.0f, 0.90f, 0.25f, 80, 1.0f, 0.80f, 0.45f, 0.22f, 0.55f)
     .master (-4.0f, 0.12f, 45, 1.25f);
}

void buildHorizon (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.30f).set (P::Polyphony, 10)
     .osc (1, 0.75f, 0.60f, 7, 18, 1.0f, 0, 0.40f, 0.40f, 4, 10, 0.90f, 7)
     .filter (0, 4200, 0.22f, 0.50f, 0.35f, 0.10f, 7.0f, 6.0f, 0.60f, 10.0f)
     .ampEnv (7.0f, 8.0f, 0.85f, 12.0f, 0.30f)
     .lfo (0, 0.040f, 0, 0.45f, 0.05f, 0.20f, 0.40f, 0.40f)
     .lfo (1, 0.070f, 6, 0.20f, 0.15f, 0.25f, 0.20f, 0.30f)
     .lfo (2, 0.030f, 0, 0.15f, 0.0f, 0.0f, 0.15f, 0.20f)
     .set (P::NoiseLevel, 0.55f).set (P::GrainSize, 160).set (P::GrainSpray, 0.70f)
     .slot (0, kSrcPink, 0.30f, 0, 800, 0.20f, -0.25f, 0.05f, 0.20f, 0.50f, 0.035f, 0, 0.30f, 0.25f, 0.30f, 7.0f)
     .slot (1, kSrcWhite, 0.18f, 1, 2500, 0.30f, 0.30f, 0.05f, 0.25f, 0.60f, 0.060f, 6, 0.30f, 0.30f, 0.30f, 8.0f)
     .slot (2, kFoley + 4, 0.10f, 0, 3000, 0.20f, 0.05f, 0.05f, 0.20f, 0.55f, 0.050f, 5, 0.20f, 0.20f, 0.20f, 11.0f)
     .space (480, 660, 0.40f, 0.45f, 0.20f, 0.30f, 0,
             0.95f, 0.80f, 0.35f, 70, 1.0f, 0.70f, 0.50f, 0.30f, 0.50f)
     .master (-4.0f, 0.12f, 55, 1.25f);
}

void buildAurora (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.45f).set (P::Polyphony, 9)
     .osc (3, 0.30f, 0.50f, 5, 12, 0.90f, 1, 0.60f, 0.40f, 4, 9, 0.90f, 6)
     .filter (1, 5200, 0.20f, 0.40f, 0.40f, 0.05f, 3.0f, 4.0f, 0.50f, 8.0f)
     .ampEnv (3.50f, 5.0f, 0.80f, 10.0f, 0.20f)
     .lfo (0, 0.060f, 0, 0.40f, 0.08f, 0.30f, 0.45f, 0.40f)
     .lfo (1, 0.030f, 5, 0.25f, 0.05f, 0.15f, 0.30f, 0.30f)
     .lfo (2, 0.085f, 6, 0.15f, 0.10f, 0.10f, 0.20f, 0.20f)
     .set (P::NoiseLevel, 0.50f).set (P::GrainSize, 100).set (P::GrainSpray, 0.75f)
     .slot (0, kSrcViolet, 0.18f, 2, 3000, 0.30f, -0.35f, 0.05f, 0.30f, 0.60f, 0.070f, 0, 0.30f, 0.35f, 0.30f, 5.0f)
     .slot (1, kGranular + 7, 0.20f, 0, 7000, 0.20f, 0.35f, 0.05f, 0.30f, 0.65f, 0.050f, 6, 0.25f, 0.30f, 0.30f, 6.0f, 2)
     .space (300, 450, 0.45f, 0.40f, 0.25f, 0.30f, 1,
             0.90f, 0.72f, 0.30f, 40, 1.0f, 0.68f, 0.45f, 0.45f, 0.45f)
     .master (-4.0f, 0.10f, 65, 1.20f);
}

void buildVelvet (Patch& p)
{
    p.set (P::PadLevel, 0.82f).set (P::Drift, 0.50f).set (P::Polyphony, 10)
     .osc (0, 0.25f, 0.60f, 5, 13, 0.60f, 1, 0.40f, 0.35f, 3, 8, 0.50f, 3)
     .filter (1, 1800, 0.30f, 0.30f, 0.30f, 0.20f, 1.5f, 2.5f, 0.45f, 5.0f)
     .ampEnv (1.20f, 3.0f, 0.80f, 5.0f, 0.25f)
     .lfo (0, 0.045f, 0, 0.25f, 0.05f, 0.10f, 0.30f, 0.25f)
     .lfo (1, 0.080f, 6, 0.10f, 0.20f, 0.05f, 0.15f, 0.15f)
     .lfo (2, 0.030f, 0, 0.10f, 0.0f, 0.0f, 0.10f, 0.10f)
     .set (P::NoiseLevel, 0.45f).set (P::GrainSize, 90).set (P::GrainSpray, 0.45f)
     .slot (0, kVinyl + 8, 0.20f, 0, 2600, 0.20f, -0.10f, 0.15f, 0.10f, 0.25f, 0.040f, 0, 0.15f, 0.10f, 0.15f, 3.0f)
     .slot (1, kTape + 2, 0.15f, 0, 5000, 0.15f, 0.15f, 0.15f, 0.10f, 0.20f, 0.060f, 6, 0.15f, 0.15f, 0.10f, 2.5f)
     .space (340, 430, 0.35f, 0.50f, 0.18f, 0.20f, 0,
             0.60f, 0.50f, 0.50f, 25, 0.95f, 0.40f, 0.35f, 0.35f, 0.30f)
     .master (-4.0f, 0.25f, 50, 1.05f);
}

void buildStrings (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.30f).set (P::Polyphony, 12)
     .osc (0, 0.35f, 0.70f, 7, 16, 0.90f, 0, 0.15f, 0.30f, 5, 12, 0.80f, 6)
     .filter (1, 2600, 0.20f, 0.30f, 0.35f, 0.12f, 2.0f, 2.5f, 0.50f, 6.0f)
     .ampEnv (1.80f, 2.5f, 0.85f, 6.0f, 0.25f)
     .lfo (0, 0.050f, 0, 0.20f, 0.08f, 0.10f, 0.25f, 0.25f)
     .lfo (1, 0.090f, 6, 0.10f, 0.22f, 0.05f, 0.15f, 0.15f)
     .lfo (2, 0.035f, 0, 0.10f, 0.0f, 0.0f, 0.10f, 0.10f)
     .set (P::NoiseLevel, 0.35f).set (P::GrainSize, 80).set (P::GrainSpray, 0.40f)
     .slot (0, kTape + 0, 0.18f, 0, 6000, 0.15f, 0.0f, 0.15f, 0.08f, 0.20f, 0.035f, 0, 0.15f, 0.10f, 0.10f, 2.0f)
     .slot (1, kSrcPink, 0.10f, 1, 3000, 0.30f, -0.20f, 0.05f, 0.10f, 0.25f, 0.050f, 6, 0.20f, 0.20f, 0.15f, 3.0f)
     .space (420, 540, 0.40f, 0.45f, 0.20f, 0.25f, 0,
             0.65f, 0.50f, 0.45f, 25, 1.0f, 0.45f, 0.45f, 0.40f, 0.35f)
     .master (-4.0f, 0.18f, 50, 1.10f);
}

void buildChoir (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.40f).set (P::Polyphony, 9)
     .osc (4, 0.35f, 0.65f, 5, 10, 0.70f, 4, 0.60f, 0.30f, 3, 6, 0.70f, 4)
     .filter (1, 1600, 0.35f, 0.25f, 0.30f, 0.10f, 2.5f, 3.0f, 0.50f, 7.0f)
     .ampEnv (2.80f, 4.0f, 0.90f, 9.0f, 0.30f)
     .lfo (0, 0.035f, 0, 0.20f, 0.05f, 0.25f, 0.30f, 0.30f)
     .lfo (1, 0.060f, 6, 0.10f, 0.10f, 0.20f, 0.20f, 0.25f)
     .lfo (2, 0.025f, 0, 0.10f, 0.0f, 0.0f, 0.10f, 0.15f)
     .set (P::NoiseLevel, 0.40f).set (P::GrainSize, 130).set (P::GrainSpray, 0.50f)
     .slot (0, kSrcPink, 0.28f, 1, 900, 0.35f, -0.20f, 0.05f, 0.10f, 0.50f, 0.040f, 0, 0.25f, 0.20f, 0.25f, 6.0f)
     .slot (1, kFoley + 0, 0.10f, 0, 1200, 0.20f, 0.25f, 0.05f, 0.10f, 0.45f, 0.050f, 6, 0.20f, 0.20f, 0.20f, 8.0f)
     .space (420, 560, 0.30f, 0.50f, 0.15f, 0.20f, 0,
             0.85f, 0.70f, 0.40f, 45, 1.0f, 0.60f, 0.40f, 0.25f, 0.45f)
     .master (-4.0f, 0.15f, 55, 1.15f);
}

void buildWornTape (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.75f).set (P::Polyphony, 8)
     .osc (0, 0.30f, 0.65f, 5, 14, 0.55f, 1, 0.45f, 0.30f, 3, 6, 0.50f, 6)
     .filter (1, 1600, 0.35f, 0.25f, 0.30f, 0.25f, 1.20f, 2.0f, 0.40f, 4.0f)
     .ampEnv (1.10f, 2.50f, 0.75f, 4.50f, 0.35f)
     .lfo (0, 0.040f, 6, 0.20f, 0.25f, 0.10f, 0.25f, 0.20f)
     .lfo (1, 0.070f, 6, 0.10f, 0.30f, 0.0f, 0.0f, 0.0f)
     .lfo (2, 0.030f, 0, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f)
     .set (P::NoiseLevel, 0.70f).set (P::GrainSize, 80).set (P::GrainSpray, 0.40f)
     .slot (0, kTape + 1, 0.30f, 0, 6500, 0.15f, 0.05f, 0.20f, 0.10f, 0.25f, 0.030f, 0, 0.15f, 0.10f, 0.10f, 2.0f)
     .slot (1, kVinyl + 3, 0.30f, 1, 3000, 0.20f, -0.15f, 0.15f, 0.12f, 0.20f, 0.050f, 6, 0.20f, 0.15f, 0.15f, 1.5f)
     .slot (2, kCrackle + 4, 0.22f, 2, 1200, 0.10f, 0.20f, 0.10f, 0.10f, 0.20f, 0.080f, 5, 0.20f, 0.20f, 0.10f, 1.0f)
     .space (320, 410, 0.35f, 0.60f, 0.18f, 0.20f, 0,
             0.50f, 0.40f, 0.60f, 20, 0.90f, 0.35f, 0.30f, 0.40f, 0.30f)
     .master (-4.5f, 0.35f, 50, 1.05f);
}

void buildEmber (Patch& p)
{
    p.set (P::PadLevel, 0.78f).set (P::Drift, 0.60f).set (P::Polyphony, 8)
     .osc (1, 0.20f, 0.55f, 4, 9, 0.50f, 0, 0.20f, 0.30f, 3, 7, 0.45f, 1)
     .filter (1, 1300, 0.32f, 0.20f, 0.25f, 0.30f, 1.80f, 3.0f, 0.40f, 6.0f)
     .ampEnv (1.60f, 3.50f, 0.75f, 6.50f, 0.35f)
     .lfo (0, 0.050f, 6, 0.20f, 0.10f, 0.10f, 0.20f, 0.20f)
     .lfo (1, 0.110f, 5, 0.15f, 0.0f, 0.0f, 0.15f, 0.10f)
     .lfo (2, 0.035f, 0, 0.0f, 0.0f, 0.0f, 0.10f, 0.0f)
     .set (P::NoiseLevel, 0.60f).set (P::GrainSize, 70).set (P::GrainSpray, 0.55f)
     .slot (0, kCrackle + 3, 0.30f, 2, 900, 0.10f, 0.10f, 0.10f, 0.10f, 0.20f, 0.070f, 5, 0.20f, 0.20f, 0.10f, 1.5f)
     .slot (1, kVinyl + 5, 0.28f, 1, 2800, 0.20f, -0.20f, 0.15f, 0.10f, 0.20f, 0.045f, 6, 0.20f, 0.15f, 0.15f, 2.5f)
     .slot (2, kSrcBrown, 0.20f, 0, 150, 0.20f, 0.0f, 0.20f, 0.05f, 0.25f, 0.030f, 0, 0.15f, 0.10f, 0.15f, 5.0f)
     .space (300, 400, 0.30f, 0.65f, 0.15f, 0.20f, 0,
             0.55f, 0.45f, 0.60f, 20, 0.90f, 0.35f, 0.30f, 0.35f, 0.25f)
     .master (-4.0f, 0.40f, 55, 1.0f);
}

void buildRainfield (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.40f).set (P::Polyphony, 9)
     .osc (1, 0.60f, 0.60f, 7, 16, 0.90f, 2, 0.40f, 0.35f, 3, 8, 0.80f, 6)
     .filter (1, 2200, 0.30f, 0.40f, 0.35f, 0.08f, 6.0f, 5.0f, 0.50f, 8.0f)
     .ampEnv (4.50f, 6.0f, 0.80f, 9.0f, 0.30f)
     .lfo (0, 0.050f, 0, 0.50f, 0.05f, 0.15f, 0.40f, 0.35f)
     .lfo (1, 0.110f, 6, 0.35f, 0.15f, 0.20f, 0.0f, 0.30f)
     .lfo (2, 0.070f, 0, 0.0f, 0.0f, 0.0f, 0.25f, 0.20f)
     .set (P::NoiseLevel, 0.80f).set (P::GrainSize, 160).set (P::GrainSpray, 0.70f)
     .slot (0, kSrcPink, 0.60f, 1, 1200, 0.35f, -0.25f, 0.08f, 0.30f, 0.70f, 0.060f, 0, 0.45f, 0.35f, 0.30f, 5.0f)
     .slot (1, kSrcWhite, 0.35f, 1, 3000, 0.50f, 0.30f, 0.10f, 0.25f, 0.75f, 0.090f, 6, 0.40f, 0.30f, 0.30f, 7.0f)
     .slot (2, kCity + 5, 0.30f, 0, 3800, 0.20f, 0.10f, 0.05f, 0.20f, 0.65f, 0.040f, 6, 0.25f, 0.25f, 0.30f, 8.0f)
     .space (480, 640, 0.45f, 0.50f, 0.30f, 0.35f, 0,
             0.90f, 0.70f, 0.40f, 45, 1.0f, 0.70f, 0.50f, 0.25f, 0.50f)
     .master (-4.0f, 0.15f, 55, 1.20f);
}

void buildWindTunnel (Patch& p)
{
    p.set (P::PadLevel, 0.62f).set (P::Drift, 0.45f).set (P::Polyphony, 8)
     .osc (2, 0.50f, 0.35f, 4, 12, 0.80f, 1, 0.15f, 0.20f, 2, 5, 0.50f, 1)
     .filter (2, 900, 0.45f, 0.20f, 0.20f, 0.10f, 4.0f, 5.0f, 0.60f, 8.0f)
     .ampEnv (4.50f, 6.0f, 0.80f, 10.0f, 0.30f)
     .lfo (0, 0.050f, 0, 0.55f, 0.05f, 0.10f, 0.40f, 0.30f)
     .lfo (1, 0.090f, 6, 0.40f, 0.05f, 0.10f, 0.30f, 0.25f)
     .lfo (2, 0.030f, 0, 0.30f, 0.0f, 0.0f, 0.15f, 0.15f)
     .set (P::NoiseLevel, 0.85f).set (P::GrainSize, 140).set (P::GrainSpray, 0.80f)
     .slot (0, kSrcPink, 0.45f, 1, 700, 0.50f, -0.30f, 0.10f, 0.20f, 0.55f, 0.050f, 0, 0.50f, 0.35f, 0.30f, 5.0f)
     .slot (1, kSrcWhite, 0.30f, 1, 2200, 0.45f, 0.35f, 0.10f, 0.20f, 0.50f, 0.080f, 6, 0.45f, 0.30f, 0.25f, 7.0f)
     .slot (2, kSrcGrey, 0.20f, 1, 1200, 0.40f, 0.0f, 0.10f, 0.15f, 0.50f, 0.035f, 6, 0.35f, 0.20f, 0.25f, 9.0f)
     .space (600, 800, 0.50f, 0.45f, 0.22f, 0.30f, 0,
             0.90f, 0.70f, 0.45f, 50, 1.0f, 0.65f, 0.35f, 0.20f, 0.40f)
     .master (-4.0f, 0.12f, 60, 1.15f);
}

void buildOcean (Patch& p)
{
    p.set (P::PadLevel, 0.78f).set (P::Drift, 0.55f).set (P::Polyphony, 7)
     .osc (1, 0.05f, 0.45f, 3, 7, 0.60f, 1, 0.10f, 0.30f, 2, 4, 0.50f, 0)
     .filter (1, 700, 0.25f, 0.20f, 0.20f, 0.15f, 3.0f, 4.0f, 0.60f, 7.0f)
     .ampEnv (3.0f, 5.0f, 0.85f, 9.0f, 0.30f)
     .lfo (0, 0.030f, 0, 0.45f, 0.05f, 0.10f, 0.25f, 0.30f)
     .lfo (1, 0.045f, 6, 0.30f, 0.05f, 0.10f, 0.20f, 0.35f)
     .lfo (2, 0.020f, 0, 0.15f, 0.0f, 0.0f, 0.10f, 0.15f)
     .set (P::NoiseLevel, 0.82f).set (P::GrainSize, 200).set (P::GrainSpray, 0.75f)
     .slot (0, kSrcBrown, 0.50f, 0, 220, 0.20f, 0.0f, 0.20f, 0.10f, 0.40f, 0.030f, 0, 0.30f, 0.15f, 0.30f, 6.0f)
     .slot (1, kSrcPink, 0.35f, 1, 900, 0.30f, -0.20f, 0.10f, 0.15f, 0.45f, 0.045f, 6, 0.35f, 0.25f, 0.30f, 8.0f)
     .slot (2, kSrcWhite, 0.15f, 1, 3000, 0.30f, 0.30f, 0.05f, 0.10f, 0.45f, 0.060f, 0, 0.30f, 0.25f, 0.25f, 10.0f)
     .space (700, 950, 0.45f, 0.60f, 0.20f, 0.30f, 0,
             0.85f, 0.65f, 0.50f, 40, 0.95f, 0.55f, 0.30f, 0.18f, 0.45f)
     .master (-3.5f, 0.15f, 40, 1.15f);
}

void buildNightCity (Patch& p)
{
    p.set (P::PadLevel, 0.72f).set (P::Drift, 0.50f).set (P::Polyphony, 8)
     .osc (1, 0.30f, 0.40f, 5, 11, 0.70f, 2, 0.25f, 0.25f, 2, 5, 0.50f, 1)
     .filter (1, 1100, 0.30f, 0.20f, 0.25f, 0.20f, 2.0f, 3.0f, 0.50f, 6.0f)
     .ampEnv (2.40f, 4.0f, 0.80f, 8.0f, 0.30f)
     .lfo (0, 0.040f, 6, 0.25f, 0.05f, 0.10f, 0.30f, 0.25f)
     .lfo (1, 0.070f, 5, 0.15f, 0.0f, 0.0f, 0.25f, 0.15f)
     .lfo (2, 0.025f, 0, 0.10f, 0.0f, 0.0f, 0.10f, 0.10f)
     .set (P::NoiseLevel, 0.75f).set (P::GrainSize, 170).set (P::GrainSpray, 0.65f)
     .slot (0, kCity + 6, 0.35f, 0, 1200, 0.20f, -0.10f, 0.10f, 0.20f, 0.55f, 0.035f, 0, 0.20f, 0.20f, 0.25f, 6.0f)
     .slot (1, kCity + 12, 0.25f, 2, 400, 0.20f, 0.30f, 0.10f, 0.15f, 0.50f, 0.050f, 6, 0.25f, 0.25f, 0.20f, 8.0f)
     .slot (2, kVinyl + 4, 0.15f, 1, 2500, 0.20f, -0.25f, 0.10f, 0.10f, 0.25f, 0.040f, 6, 0.20f, 0.15f, 0.15f, 4.0f)
     .space (500, 700, 0.40f, 0.55f, 0.20f, 0.25f, 0,
             0.80f, 0.60f, 0.50f, 35, 0.95f, 0.55f, 0.30f, 0.20f, 0.40f)
     .master (-4.0f, 0.18f, 50, 1.10f);
}

void buildFogBank (Patch& p)
{
    p.set (P::PadLevel, 0.55f).set (P::Drift, 0.60f).set (P::Polyphony, 8)
     .osc (1, 0.70f, 0.35f, 6, 20, 1.0f, 4, 0.40f, 0.20f, 3, 9, 0.90f, 5)
     .filter (1, 800, 0.40f, 0.15f, 0.15f, 0.10f, 5.0f, 6.0f, 0.60f, 9.0f)
     .ampEnv (6.0f, 8.0f, 0.90f, 12.0f, 0.35f)
     .lfo (0, 0.020f, 5, 0.35f, 0.10f, 0.30f, 0.40f, 0.35f)
     .lfo (1, 0.050f, 6, 0.30f, 0.10f, 0.30f, 0.30f, 0.30f)
     .lfo (2, 0.030f, 0, 0.15f, 0.0f, 0.0f, 0.15f, 0.15f)
     .set (P::NoiseLevel, 0.90f).set (P::GrainSize, 220).set (P::GrainSpray, 0.85f)
     .slot (0, kGranular + 11, 0.45f, 0, 6000, 0.30f, -0.40f, 0.10f, 0.35f, 0.70f, 0.035f, 5, 0.45f, 0.40f, 0.35f, 8.0f, 2)
     .slot (1, kSrcGrey, 0.30f, 1, 1800, 0.50f, 0.35f, 0.10f, 0.25f, 0.60f, 0.045f, 6, 0.40f, 0.35f, 0.30f, 10.0f)
     .slot (2, kFoley + 6, 0.20f, 0, 2500, 0.20f, 0.10f, 0.10f, 0.30f, 0.70f, 0.060f, 5, 0.30f, 0.25f, 0.25f, 12.0f, 2)
     .space (620, 860, 0.50f, 0.55f, 0.28f, 0.30f, 0,
             1.0f, 0.85f, 0.40f, 70, 1.0f, 0.70f, 0.40f, 0.15f, 0.50f)
     .master (-4.0f, 0.12f, 60, 1.25f);
}

void buildMachineRoom (Patch& p)
{
    p.set (P::PadLevel, 0.45f).set (P::Drift, 0.35f).set (P::Polyphony, 8)
     .osc (2, 0.60f, 0.30f, 3, 6, 0.50f, 0, 0.60f, 0.20f, 2, 4, 0.40f, 2)
     .filter (2, 1400, 0.50f, 0.35f, 0.20f, 0.30f, 0.60f, 1.20f, 0.30f, 3.0f)
     .ampEnv (0.80f, 1.50f, 0.60f, 4.0f, 0.10f)
     .lfo (0, 0.180f, 5, 0.40f, 0.05f, 0.10f, 0.35f, 0.20f)
     .lfo (1, 0.070f, 5, 0.20f, 0.0f, 0.30f, 0.20f, 0.15f)
     .lfo (2, 0.320f, 5, 0.25f, 0.0f, 0.0f, 0.25f, 0.10f)
     .set (P::NoiseLevel, 0.80f).set (P::GrainSize, 60).set (P::GrainSpray, 0.35f)
     .slot (0, kFoley + 8, 0.35f, 2, 900, 0.30f, -0.25f, 0.20f, 0.30f, 0.40f, 0.120f, 5, 0.30f, 0.30f, 0.20f, 1.0f, 1)
     .slot (1, kTape + 6, 0.25f, 1, 4000, 0.30f, 0.25f, 0.15f, 0.20f, 0.30f, 0.080f, 5, 0.25f, 0.25f, 0.20f, 2.0f)
     .slot (2, kSrcGrey, 0.20f, 1, 900, 0.50f, 0.0f, 0.10f, 0.20f, 0.40f, 0.150f, 5, 0.30f, 0.20f, 0.20f, 3.0f)
     .space (260, 380, 0.45f, 0.60f, 0.30f, 0.30f, 0,
             0.70f, 0.55f, 0.50f, 25, 0.90f, 0.50f, 0.25f, 0.40f, 0.30f)
     .master (-4.0f, 0.35f, 60, 1.05f);
}

void buildBellGarden (Patch& p)
{
    p.set (P::PadLevel, 0.80f).set (P::Drift, 0.15f).set (P::Polyphony, 10)
     .osc (3, 0.55f, 0.72f, 4, 7, 0.80f, 3, 0.90f, 0.38f, 2, 5, 0.90f, 7)
     .filter (0, 5500, 0.15f, 0.60f, 0.40f, 0.05f, 0.01f, 1.60f, 0.10f, 3.0f)
     .ampEnv (0.02f, 2.80f, 0.16f, 4.50f, -0.20f)
     .lfo (0, 0.090f, 0, 0.15f, 0.0f, 0.0f, 0.35f, 0.30f)
     .lfo (1, 0.050f, 6, 0.0f, 0.10f, 0.25f, 0.0f, 0.30f)
     .lfo (2, 0.210f, 5, 0.0f, 0.0f, 0.0f, 0.15f, 0.0f)
     .set (P::NoiseLevel, 0.40f).set (P::GrainSize, 70).set (P::GrainSpray, 0.60f)
     .slot (0, kSrcWhite, 0.18f, 1, 9000, 0.60f, -0.30f, 0.05f, 0.30f, 0.60f, 0.150f, 0, 0.30f, 0.30f, 0.30f, 0.30f)
     .slot (1, kSrcBlue, 0.12f, 2, 6000, 0.20f, 0.35f, 0.0f, 0.30f, 0.55f, 0.120f, 6, 0.25f, 0.35f, 0.30f, 0.40f)
     .slot (2, kGranular + 6, 0.15f, 0, 8000, 0.20f, 0.10f, 0.0f, 0.25f, 0.60f, 0.080f, 6, 0.20f, 0.20f, 0.20f, 0.50f, 2)
     .space (240, 360, 0.40f, 0.40f, 0.28f, 0.20f, 1,
             0.80f, 0.65f, 0.30f, 20, 1.0f, 0.65f, 0.40f, 0.50f, 0.40f)
     .master (-3.0f, 0.10f, 80, 1.15f);
}

void buildCrystal (Patch& p)
{
    p.set (P::PadLevel, 0.78f).set (P::Drift, 0.10f).set (P::Polyphony, 10)
     .osc (3, 0.85f, 0.60f, 3, 5, 0.90f, 3, 0.40f, 0.30f, 2, 3, 0.80f, 7)
     .filter (0, 7000, 0.12f, 0.60f, 0.45f, 0.03f, 0.02f, 1.20f, 0.08f, 3.5f)
     .ampEnv (0.03f, 2.50f, 0.10f, 5.0f, -0.15f)
     .lfo (0, 0.120f, 0, 0.20f, 0.0f, 0.0f, 0.40f, 0.30f)
     .lfo (1, 0.060f, 6, 0.10f, 0.05f, 0.30f, 0.20f, 0.35f)
     .lfo (2, 0.180f, 5, 0.0f, 0.0f, 0.0f, 0.20f, 0.0f)
     .set (P::NoiseLevel, 0.35f).set (P::GrainSize, 55).set (P::GrainSpray, 0.65f)
     .slot (0, kSrcBlue, 0.15f, 2, 6000, 0.20f, -0.30f, 0.05f, 0.30f, 0.60f, 0.140f, 0, 0.25f, 0.30f, 0.30f, 0.25f)
     .slot (1, kGranular + 6, 0.18f, 0, 9000, 0.20f, 0.30f, 0.05f, 0.30f, 0.65f, 0.080f, 6, 0.20f, 0.30f, 0.25f, 0.35f, 2)
     .slot (2, kSrcWhite, 0.10f, 1, 9000, 0.40f, 0.0f, 0.05f, 0.25f, 0.55f, 0.110f, 5, 0.25f, 0.20f, 0.20f, 0.30f)
     .space (220, 330, 0.45f, 0.35f, 0.30f, 0.20f, 1,
             0.85f, 0.65f, 0.25f, 25, 1.0f, 0.60f, 0.40f, 0.60f, 0.40f)
     .master (-3.5f, 0.10f, 90, 1.20f);
}

void buildGranular (Patch& p)
{
    p.set (P::PadLevel, 0.60f).set (P::Drift, 0.60f).set (P::Polyphony, 8)
     .osc (1, 0.80f, 0.25f, 7, 20, 1.0f, 4, 0.30f, 0.15f, 4, 12, 0.90f, 6)
     .filter (1, 900, 0.40f, 0.20f, 0.20f, 0.15f, 5.0f, 6.0f, 0.50f, 8.0f)
     .ampEnv (6.0f, 8.0f, 0.90f, 10.0f, 0.40f)
     .lfo (0, 0.030f, 5, 0.40f, 0.15f, 0.20f, 0.50f, 0.40f)
     .lfo (1, 0.070f, 6, 0.20f, 0.0f, 0.30f, 0.30f, 0.30f)
     .lfo (2, 0.130f, 5, 0.25f, 0.0f, 0.0f, 0.40f, 0.20f)
     .set (P::NoiseLevel, 0.85f).set (P::GrainSize, 140).set (P::GrainSpray, 0.80f)
     .slot (0, kGranular + 11, 0.50f, 0, 6000, 0.30f, -0.50f, 0.15f, 0.45f, 0.70f, 0.050f, 5, 0.50f, 0.50f, 0.40f, 8.0f, 2)
     .slot (1, kCity + 9, 0.35f, 0, 1800, 0.25f, 0.50f, 0.10f, 0.30f, 0.75f, 0.040f, 6, 0.30f, 0.40f, 0.35f, 10.0f)
     .slot (2, kFoley + 3, 0.30f, 2, 400, 0.20f, 0.15f, 0.10f, 0.35f, 0.80f, 0.110f, 5, 0.40f, 0.35f, 0.30f, 12.0f, 2)
     .slot (3, kSrcGrey, 0.20f, 1, 2500, 0.50f, -0.15f, 0.08f, 0.25f, 0.60f, 0.080f, 6, 0.35f, 0.30f, 0.25f, 6.0f)
     .space (500, 730, 0.55f, 0.55f, 0.35f, 0.40f, 0,
             1.0f, 0.85f, 0.35f, 60, 1.0f, 0.75f, 0.45f, 0.30f, 0.55f)
     .master (-4.0f, 0.12f, 60, 1.25f);
}

// ---------------------------------------------------------------------------
// Variation shaping
// ---------------------------------------------------------------------------

struct Mood
{
    float cutoff;
    float motion;
    float space;
    float attack;
    float detune;
    const char* tag;
};

const Mood kMoods[5] = {
    { 0.55f, 0.60f, 1.15f, 1.70f, 0.80f, "Dark, slow and deep." },
    { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, "Balanced and wide." },
    { 1.75f, 1.05f, 0.92f, 0.80f, 1.15f, "Open, bright and airy." },
    { 1.15f, 2.30f, 0.98f, 0.70f, 1.30f, "Fast, restless modulation." },
    { 0.80f, 0.45f, 1.25f, 1.90f, 0.60f, "Sparse, intimate and slow." },
};

// Interval table index chosen per variation for the layer oscillator:
// -12, 0, +12, +7, -5.
const int kIntervalByTier[5] = { 1, 4, 7, 6, 3 };

struct Family
{
    const char* name;
    const char* category;
    const char* description;
    const char* suffixes[5];
    int pool[6];
    void (*build) (Patch&);
    bool varyInterval;
    const char* classic;
};

const Family kFamilies[] = {
    { "Submerged", "Deep Drone",
      "Sine/triangle foundation over low-passed brown noise: heavy, breathing, subterranean.",
      { "Trench", "Drone", "Shaft", "Pressure", "Silt" },
      { kSrcBrown, kCity + 2, kSrcGrey, kCity + 5, kVinyl + 1, kSrcPink },
      buildSubmerged, false, "Submerged Drone" },

    { "Abyssal", "Deep Drone",
      "Sub-heavy drone with a slow, dark current and distant underwater pressure.",
      { "Void", "Current", "Hadal", "Sunken Bell", "Black Water" },
      { kSrcBrown, kCity + 0, kSrcGrey, kCity + 3, kSrcPink, kVinyl + 9 },
      buildAbyssal, false, nullptr },

    { "Glacier", "Deep Drone",
      "Cold, hollow body with blue-white air moving slowly through ice.",
      { "Crevasse", "Melt", "White Silence", "Ice Field", "Blue Ice" },
      { kSrcBlue, kSrcWhite, kGranular + 2, kSrcGrey, kSrcViolet, kCity + 1 },
      buildGlacier, false, nullptr },

    { "Cathedral", "Cinematic Pad",
      "Enormous stone space: warm organ-like pad with an unhurried, weightless swell.",
      { "Nave", "Vespers", "Stone Light", "Reliquary", "Ascension" },
      { kVinyl + 6, kSrcPink, kSrcGrey, kSrcWhite, kFoley + 1, kCity + 4 },
      buildCathedral, true, nullptr },

    { "Horizon", "Cinematic Pad",
      "Wide octave-layered swell that opens like a landscape at dawn.",
      { "Dawn", "Meridian", "Afterglow", "Far Shore", "Skyline" },
      { kSrcPink, kSrcWhite, kFoley + 4, kSrcGrey, kGranular + 4, kCity + 7 },
      buildHorizon, true, nullptr },

    { "Aurora", "Cinematic Pad",
      "Shimmering upper air over a warm bed, with slow random movement.",
      { "Solar Wind", "Green Veil", "Magnetic", "Polar Night", "Corona" },
      { kSrcViolet, kGranular + 7, kSrcBlue, kSrcWhite, kSrcPink, kFoley + 9 },
      buildAurora, true, nullptr },

    { "Velvet", "Analog Pad",
      "Detuned saws and soft filtering: warm, close and unhurried.",
      { "Room", "Suede", "Amber", "Slow Bloom", "Hush" },
      { kVinyl + 8, kTape + 2, kSrcPink, kCrackle + 6, kSrcBrown, kVinyl + 0 },
      buildVelvet, true, nullptr },

    { "Strings", "Analog Pad",
      "Ensemble string machine: wide unison, gentle bow noise, long release.",
      { "Ensemble", "Sable", "Rosin", "Chamber", "Long Bow" },
      { kTape + 0, kSrcPink, kSrcGrey, kVinyl + 2, kSrcWhite, kFoley + 2 },
      buildStrings, true, nullptr },

    { "Choir", "Formant Pad",
      "Vowel-shaped formants drifting like a distant choir in a large room.",
      { "Vowel", "Sanctus", "Whisper", "Aether", "Low Mass" },
      { kSrcPink, kFoley + 0, kSrcWhite, kGranular + 1, kSrcGrey, kVinyl + 7 },
      buildChoir, true, nullptr },

    { "Worn Tape", "Lo-Fi Pad",
      "Warm, drifting analog pad with tape hiss and light crackle for vintage cassette character.",
      { "Hiss", "Reel", "Cassette", "Dropout", "Oxide" },
      { kTape + 1, kVinyl + 3, kCrackle + 4, kTape + 8, kVinyl + 12, kSrcBrown },
      buildWornTape, false, "Worn Tape Chord" },

    { "Ember", "Lo-Fi Pad",
      "Smouldering vinyl crackle under a soft, dark chord.",
      { "Vinyl", "Cinder", "Warm Static", "Dust", "Faded" },
      { kCrackle + 3, kVinyl + 5, kSrcBrown, kTape + 4, kCrackle + 11, kSrcGrey },
      buildEmber, false, nullptr },

    { "Rainfield", "Atmosphere",
      "Pink and filtered white noise swept by slow LFOs through band-pass filters, long reverb.",
      { "Drizzle", "Downpour", "Window", "Monsoon", "Distant" },
      { kSrcPink, kSrcWhite, kCity + 5, kSrcGrey, kVinyl + 2, kFoley + 7 },
      buildRainfield, true, "Distant Rain / Wind Pad" },

    { "Wind Tunnel", "Atmosphere",
      "Resonant air moving through long pipes, never resolving.",
      { "Gust", "Howl", "Draft", "Chimney", "Gale" },
      { kSrcPink, kSrcWhite, kSrcGrey, kCity + 8, kSrcBrown, kFoley + 5 },
      buildWindTunnel, false, nullptr },

    { "Ocean", "Atmosphere",
      "Deep water breathing: brown-noise swells with foam-like air on top.",
      { "Swell", "Tide", "Surge", "Undertow", "Shoreline" },
      { kSrcBrown, kSrcPink, kSrcWhite, kSrcGrey, kCity + 1, kVinyl + 10 },
      buildOcean, false, nullptr },

    { "Night City", "Atmosphere",
      "Rooftop air, distant traffic and rain on concrete, under a low pad.",
      { "Dusk", "Neon", "Subway", "Rooftop", "Rain" },
      { kCity + 6, kCity + 12, kVinyl + 4, kCity + 0, kFoley + 8, kSrcBrown },
      buildNightCity, false, nullptr },

    { "Fog Bank", "Texture",
      "Granular clouds and grey noise drifting into each other.",
      { "Grey", "Drift", "Low Cloud", "Murk", "Vapour" },
      { kGranular + 11, kSrcGrey, kFoley + 6, kGranular + 3, kCity + 9, kSrcPink },
      buildFogBank, false, nullptr },

    { "Machine Room", "Texture",
      "Mechanical foley and tape loops, half-remembered machinery.",
      { "Servo", "Conveyor", "Relay", "Turbine", "Idle" },
      { kFoley + 8, kTape + 6, kSrcGrey, kFoley + 14, kVinyl + 16, kSrcBrown },
      buildMachineRoom, false, nullptr },

    { "Bell Garden", "Shimmer",
      "Delicate bell/pluck with a short envelope, accented by airy blue and white noise.",
      { "Frost", "Petal", "Chime", "Glasshouse", "Dusk Bell" },
      { kSrcWhite, kSrcBlue, kGranular + 6, kSrcViolet, kGranular + 14, kFoley + 2 },
      buildBellGarden, false, "Glass / Shimmer Pluck" },

    { "Crystal", "Shimmer",
      "Hard, bright partials in a frozen hall, with slow ping-pong echoes.",
      { "Prism", "Quartz", "Splinter", "Halide", "Ice Nine" },
      { kSrcBlue, kGranular + 6, kSrcWhite, kSrcViolet, kGranular + 1, kFoley + 10 },
      buildCrystal, false, nullptr },

    { "Granular", "Texture",
      "Four independent texture layers, grey noise and granular clouds, all slowly drifting.",
      { "Fog", "Moss", "Splinters", "Static Field", "Understory" },
      { kGranular + 11, kCity + 9, kFoley + 3, kSrcGrey, kGranular + 2, kVinyl + 14 },
      buildGranular, false, "Evolving Granular Texture" },
};

void applyMood (ParamSnapshot& s, const Mood& m)
{
    auto scale = [&s] (P id, float factor)
    {
        s[(size_t) id] = sanitiseParam (id, s[(size_t) id] * factor);
    };

    scale (P::Cutoff, m.cutoff);
    scale (P::AmpAttack, m.attack);
    scale (P::AmpRelease, m.attack);
    scale (P::FiltAttack, m.attack);
    scale (P::FiltRelease, m.attack);
    scale (P::OscADetune, m.detune);
    scale (P::OscBDetune, m.detune);

    for (int i = 0; i < 3; ++i)
        scale ((P) ((int) P::Lfo1Rate + i * 7), m.motion);

    scale (P::ReverbSize, 0.75f + 0.25f * m.space);
    scale (P::ReverbDecay, m.space);
    scale (P::ReverbPredelay, m.space);

    for (int slot = 0; slot < kNoiseSlotCount; ++slot)
    {
        const P rate = slotParam (slot, 11);
        const P swell = slotParam (slot, 16);
        s[(size_t) rate] = sanitiseParam (rate, s[(size_t) rate] * m.motion);
        s[(size_t) swell] = sanitiseParam (swell, s[(size_t) swell] * m.attack);
    }
}

// Each variation re-uses the family's designed slot levels, filters and sends
// but swaps in different sources from its texture pool. Bright procedural
// colours are attenuated so a swap never turns a soft layer into hiss.
void applyTextures (Patch& p, const int pool[6], int tier)
{
    auto brightnessScale = [] (int source)
    {
        switch (source)
        {
            case kSrcWhite:  return 0.55f;
            case kSrcBlue:   return 0.65f;
            case kSrcViolet: return 0.55f;
            case kSrcGrey:   return 0.85f;
            default:         return 1.0f;
        }
    };

    const int sources[3] = { pool[tier % 6], pool[(tier + 2) % 6], pool[(tier + 4) % 6] };

    for (int i = 0; i < 3; ++i)
    {
        const P levelParam = slotParam (i, 1);
        const float level = p.s[(size_t) levelParam] * brightnessScale (sources[i]);
        p.set (slotParam (i, 0), (float) sources[i]);
        p.set (levelParam, level);
    }

    p.set (slotParam (1, 7), (tier & 1) ? 0.30f : -0.30f);
}

// ---------------------------------------------------------------------------
// Program storage
// ---------------------------------------------------------------------------

struct Program
{
    std::string name;
    std::string category;
    std::string description;
    ParamSnapshot values;
};

struct ProgramStore
{
    std::vector<Program> programs;
    std::vector<FactoryPreset> infos; // parallel to programs[1..]
};

const char* const kClassicOrder[5] = {
    "Submerged Drone",
    "Distant Rain / Wind Pad",
    "Worn Tape Chord",
    "Glass / Shimmer Pluck",
    "Evolving Granular Texture"
};

void orderClassicsFirst (std::vector<Program>& list)
{
    std::vector<Program> ordered;
    ordered.reserve (list.size());

    for (const char* classic : kClassicOrder)
    {
        for (auto it = list.begin(); it != list.end(); ++it)
        {
            if (it->name == classic)
            {
                ordered.push_back (std::move (*it));
                list.erase (it);
                break;
            }
        }
    }

    for (auto& program : list)
        ordered.push_back (std::move (program));

    list = std::move (ordered);
}

ProgramStore buildProgramStore()
{
    std::vector<Program> factory;
    const int familyCount = (int) (sizeof (kFamilies) / sizeof (kFamilies[0]));
    factory.reserve ((size_t) familyCount * 5);

    for (const auto& family : kFamilies)
    {
        for (int tier = 0; tier < 5; ++tier)
        {
            Patch patch;
            family.build (patch);

            const bool balanced = (tier == 1);
            if (! balanced)
            {
                applyTextures (patch, family.pool, tier);
                if (family.varyInterval)
                    patch.set (P::OscBInterval, (float) kIntervalByTier[tier]);
                applyMood (patch.s, kMoods[tier]);
            }

            Program program;
            program.name = (balanced && family.classic != nullptr)
                               ? std::string (family.classic)
                               : std::string (family.name) + " " + family.suffixes[tier];
            program.category = family.category;
            program.description = std::string (family.description) + " " + kMoods[tier].tag;
            program.values = patch.s;
            factory.push_back (std::move (program));
        }
    }

    orderClassicsFirst (factory);

    ProgramStore result;
    result.programs.reserve (factory.size() + 1);
    result.programs.push_back ({ "Init", "Default",
                                 "Neutral starting point: warm pad, slow swell, wide space.",
                                 defaultSnapshot() });

    for (auto& program : factory)
        result.programs.push_back (std::move (program));

    result.infos.reserve (result.programs.size());
    for (size_t i = 1; i < result.programs.size(); ++i)
    {
        const auto& p = result.programs[i];
        result.infos.push_back ({ p.name.c_str(), p.category.c_str(), p.description.c_str() });
    }

    return result;
}

const ProgramStore& store()
{
    static const ProgramStore instance = buildProgramStore();
    return instance;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

int factoryPresetCount() noexcept
{
    return (int) store().infos.size();
}

int factoryProgramCount() noexcept
{
    return (int) store().programs.size();
}

const FactoryPreset& factoryPreset (int index) noexcept
{
    const auto& infos = store().infos;
    const int clamped = clampValue (index, 0, (int) infos.size() - 1);
    return infos[(size_t) clamped];
}

int findFactoryPreset (const char* name) noexcept
{
    if (name == nullptr)
        return -1;

    const auto& infos = store().infos;
    for (size_t i = 0; i < infos.size(); ++i)
        if (std::strcmp (infos[i].name, name) == 0)
            return (int) i;

    return -1;
}

void applyFactoryPreset (ParamStore& target, int index)
{
    target.setAll (factoryPresetSnapshot (index));
    target.snapToTargets();
}

ParamSnapshot factoryPresetSnapshot (int index)
{
    const auto& programs = store().programs;
    const int clamped = clampValue (index + 1, 1, (int) programs.size() - 1);
    return programs[(size_t) clamped].values;
}

ParamSnapshot initSnapshot()
{
    return defaultSnapshot();
}

int presetCategoryCount() noexcept
{
    static const auto count = []
    {
        std::vector<std::string> names;
        for (const auto& info : store().infos)
            if (std::find (names.begin(), names.end(), info.category) == names.end())
                names.emplace_back (info.category);
        return (int) names.size();
    }();
    return count;
}

PresetCategory presetCategory (int index) noexcept
{
    static const auto categories = []
    {
        std::vector<std::string> names;
        for (const auto& info : store().infos)
            if (std::find (names.begin(), names.end(), info.category) == names.end())
                names.emplace_back (info.category);

        std::vector<PresetCategory> list;
        list.reserve (names.size());

        const auto& infos = store().infos;
        for (const auto& name : names)
        {
            int first = -1;
            int count = 0;
            for (size_t i = 0; i < infos.size(); ++i)
            {
                if (name == infos[i].category)
                {
                    if (first < 0)
                        first = (int) i;
                    ++count;
                }
            }
            list.push_back ({ name.c_str(), first, count });
        }

        return list;
    }();

    const int clamped = clampValue (index, 0, (int) categories.size() - 1);
    return categories[(size_t) clamped];
}

} // namespace mk
