#pragma once

// Single source of truth for every automatable parameter.
//
// The table below drives, in this order:
//   * enum P (compile-time indices),
//   * the parameter registry (ranges, defaults, units, choice lists),
//   * the JUCE AudioProcessorValueTreeState layout,
//   * preset snapshots and generic test rendering.
//
// Adding a parameter is a one-line change here; nothing else needs to know
// about the ordering.

#include "MkCommon.h"

namespace mk
{

enum Group
{
    Pad,
    Filter,
    Envelope,
    Mod,
    Noise,
    Space,
    Gate,
    Master
};

enum Unit
{
    U_None,
    U_Pct,
    U_Sec,
    U_Hz,
    U_Db,
    U_Semi,
    U_Cent,
    U_Ms
};

struct ChoiceList
{
    const char* const* names;
    int count;
};

const char* groupName (Group group) noexcept;
const char* unitSuffix (Unit unit) noexcept;

extern const ChoiceList kWaveChoices;
extern const ChoiceList kIntervalChoices;
extern const ChoiceList kFilterModeChoices;
extern const ChoiceList kLfoShapeChoices;
extern const ChoiceList kSlotModeChoices;
extern const ChoiceList kSlotFilterChoices;
extern const ChoiceList kDelayModeChoices;
extern const ChoiceList kGateTriggerChoices;
extern const ChoiceList kGateLengthChoices;
extern const ChoiceList kGateTargetChoices;

// Number of steps in the trance gate pattern.
constexpr int kGateSteps = 12;

// Six procedural noise types followed by the short-texture sample catalogue.
// Catalogue names are static; only the sample data is generated at prepare time.
const ChoiceList& slotSourceChoices() noexcept;

struct ParamSpec
{
    const char* key;
    const char* label;
    Group group;
    float minValue;
    float maxValue;
    float defaultValue;
    Unit unit;
    const ChoiceList* choices;

    bool isChoice() const noexcept { return choices != nullptr && choices->count > 0; }
    bool isStepped() const noexcept { return isChoice() || (unit == U_None && maxValue <= 32.0f); }
    float range() const noexcept { return maxValue - minValue; }
    float clamp (float v) const noexcept
    {
        v = clampf (v, minValue, maxValue);
        return isStepped() ? std::round (v) : v;
    }
};

// ---------------------------------------------------------------------------
// The parameter table:  X(enum, key, label, group, min, max, default, unit, choices)
// ---------------------------------------------------------------------------
#define MK_PARAM_TABLE(X)                                                                                    \
    X (PadLevel,       "padLevel",      "Pad Level",       Pad,      0,      1,       0.80f,  U_Pct,  nullptr) \
    X (OscAWave,       "oscAWave",      "Osc A Wave",      Pad,      0,      5,       1,      U_None, &kWaveChoices) \
    X (OscAPos,        "oscAPos",       "Osc A Shape",     Pad,      0,      1,       0.35f,  U_Pct,  nullptr) \
    X (OscALevel,      "oscALevel",     "Osc A Level",     Pad,      0,      1,       0.75f,  U_Pct,  nullptr) \
    X (OscAUnison,     "oscAUnison",    "Osc A Unison",    Pad,      1,      7,       5,      U_None, nullptr) \
    X (OscADetune,     "oscADetune",    "Osc A Detune",    Pad,      0,      40,      12,     U_Cent, nullptr) \
    X (OscASpread,     "oscASpread",    "Osc A Spread",    Pad,      0,      1,       0.70f,  U_Pct,  nullptr) \
    X (OscBWave,       "oscBWave",      "Osc B Wave",      Pad,      0,      5,       2,      U_None, &kWaveChoices) \
    X (OscBPos,        "oscBPos",       "Osc B Shape",     Pad,      0,      1,       0.50f,  U_Pct,  nullptr) \
    X (OscBLevel,      "oscBLevel",     "Osc B Level",     Pad,      0,      1,       0.40f,  U_Pct,  nullptr) \
    X (OscBUnison,     "oscBUnison",    "Osc B Unison",    Pad,      1,      7,       3,      U_None, nullptr) \
    X (OscBDetune,     "oscBDetune",    "Osc B Detune",    Pad,      0,      40,      9,      U_Cent, nullptr) \
    X (OscBSpread,     "oscBSpread",    "Osc B Spread",    Pad,      0,      1,       0.60f,  U_Pct,  nullptr) \
    X (OscBInterval,   "oscBInterval",  "Osc B Interval",  Pad,      0,      7,       1,      U_None, &kIntervalChoices) \
    X (Drift,          "drift",         "Analog Drift",    Pad,      0,      1,       0.35f,  U_Pct,  nullptr) \
    X (Glide,          "glide",         "Glide",          Pad,      0,      2000,    0,      U_Ms,   nullptr) \
    X (Polyphony,      "polyphony",     "Voices",         Pad,      1,      16,      8,      U_None, nullptr) \
                                                                                                              \
    X (FilterMode,     "filterMode",    "Filter Mode",    Filter,   0,      3,       1,      U_None, &kFilterModeChoices) \
    X (Cutoff,         "cutoff",        "Cutoff",         Filter,   20,     18000,   1400,   U_Hz,   nullptr) \
    X (Resonance,      "resonance",     "Resonance",      Filter,   0,      1,       0.22f,  U_Pct,  nullptr) \
    X (FiltEnvAmt,     "filtEnvAmt",    "Env Amount",     Filter,  -1,      1,       0.30f,  U_Pct,  nullptr) \
    X (FiltKeyTrack,   "filtKeyTrack",  "Key Track",      Filter,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (FiltDrive,      "filtDrive",     "Filter Drive",   Filter,   0,      1,       0.10f,  U_Pct,  nullptr) \
    X (FiltAttack,     "filtAttack",    "Filter Attack",  Filter,   0.001f, 20,      1.60f,  U_Sec,  nullptr) \
    X (FiltDecay,      "filtDecay",     "Filter Decay",   Filter,   0.01f,  20,      2.50f,  U_Sec,  nullptr) \
    X (FiltSustain,    "filtSustain",   "Filter Sustain", Filter,   0,      1,       0.40f,  U_Pct,  nullptr) \
    X (FiltRelease,    "filtRelease",   "Filter Release", Filter,   0.01f,  30,      5.50f,  U_Sec,  nullptr) \
                                                                                                              \
    X (AmpAttack,      "ampAttack",     "Attack",         Envelope, 0.001f, 20,      2.50f,  U_Sec,  nullptr) \
    X (AmpDecay,       "ampDecay",      "Decay",          Envelope, 0.01f,  20,      3.00f,  U_Sec,  nullptr) \
    X (AmpSustain,     "ampSustain",    "Sustain",        Envelope, 0,      1,       0.80f,  U_Pct,  nullptr) \
    X (AmpRelease,     "ampRelease",    "Release",        Envelope, 0.01f,  30,      6.00f,  U_Sec,  nullptr) \
    X (AmpCurve,       "ampCurve",      "Curve",          Envelope, -1,     1,       0.20f,  U_Pct,  nullptr) \
                                                                                                              \
    X (Lfo1Rate,       "lfo1Rate",      "LFO 1 Rate",     Mod,     0.005f, 10,      0.08f,  U_Hz,   nullptr) \
    X (Lfo1Shape,      "lfo1Shape",     "LFO 1 Shape",    Mod,     0,      6,       0,      U_None, &kLfoShapeChoices) \
    X (Lfo1Cutoff,     "lfo1Cutoff",    "LFO 1 > Cutoff", Mod,     0,      1,       0.35f,  U_Pct,  nullptr) \
    X (Lfo1Detune,     "lfo1Detune",    "LFO 1 > Detune", Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo1Blend,      "lfo1Blend",     "LFO 1 > Blend",  Mod,     0,      1,       0.15f,  U_Pct,  nullptr) \
    X (Lfo1Pan,        "lfo1Pan",       "LFO 1 > Pan",    Mod,     0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Lfo1Space,      "lfo1Space",     "LFO 1 > Space",  Mod,     0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Lfo2Rate,       "lfo2Rate",      "LFO 2 Rate",     Mod,     0.005f, 10,      0.05f,  U_Hz,   nullptr) \
    X (Lfo2Shape,      "lfo2Shape",     "LFO 2 Shape",    Mod,     0,      6,       6,      U_None, &kLfoShapeChoices) \
    X (Lfo2Cutoff,     "lfo2Cutoff",    "LFO 2 > Cutoff", Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo2Detune,     "lfo2Detune",    "LFO 2 > Detune", Mod,     0,      1,       0.12f,  U_Pct,  nullptr) \
    X (Lfo2Blend,      "lfo2Blend",     "LFO 2 > Blend",  Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo2Pan,        "lfo2Pan",       "LFO 2 > Pan",    Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo2Space,      "lfo2Space",     "LFO 2 > Space",  Mod,     0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Lfo3Rate,       "lfo3Rate",      "LFO 3 Rate",     Mod,     0.005f, 10,      0.22f,  U_Hz,   nullptr) \
    X (Lfo3Shape,      "lfo3Shape",     "LFO 3 Shape",    Mod,     0,      6,       5,      U_None, &kLfoShapeChoices) \
    X (Lfo3Cutoff,     "lfo3Cutoff",    "LFO 3 > Cutoff", Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo3Detune,     "lfo3Detune",    "LFO 3 > Detune", Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo3Blend,      "lfo3Blend",     "LFO 3 > Blend",  Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo3Pan,        "lfo3Pan",       "LFO 3 > Pan",    Mod,     0,      1,       0,      U_Pct,  nullptr) \
    X (Lfo3Space,      "lfo3Space",     "LFO 3 > Space",  Mod,     0,      1,       0,      U_Pct,  nullptr) \
                                                                                                              \
    X (NoiseLevel,     "noiseLevel",    "Texture Level",  Noise,   0,      1,       0.60f,  U_Pct,  nullptr) \
    X (GrainSize,      "grainSize",     "Grain Size",     Noise,   20,     300,     90,     U_Ms,   nullptr) \
    X (GrainSpray,     "grainSpray",    "Grain Spray",    Noise,   0,      1,       0.50f,  U_Pct,  nullptr) \
    X (Slot1Source,    "slot1Source",   "Slot 1 Source",  Noise,   0,      137,     0,      U_None, &slotSourceChoices()) \
    X (Slot1Level,     "slot1Level",    "Slot 1 Level",   Noise,   0,      1,       0,      U_Pct,  nullptr) \
    X (Slot1Mode,      "slot1Mode",     "Slot 1 Mode",    Noise,   0,      2,       0,      U_None, &kSlotModeChoices) \
    X (Slot1Rate,      "slot1Rate",     "Slot 1 Tune",    Noise,  -24,     24,      0,      U_Semi, nullptr) \
    X (Slot1FiltType,  "slot1FiltType", "Slot 1 Filter",  Noise,   0,      2,       0,      U_None, &kSlotFilterChoices) \
    X (Slot1Cutoff,    "slot1Cutoff",   "Slot 1 Cutoff",  Noise,   30,     18000,   2000,   U_Hz,   nullptr) \
    X (Slot1Res,       "slot1Res",      "Slot 1 Res",     Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot1Pan,       "slot1Pan",      "Slot 1 Pan",     Noise,  -1,      1,       0,      U_Pct,  nullptr) \
    X (Slot1Drive,     "slot1Drive",    "Slot 1 Drive",   Noise,   0,      1,       0.10f,  U_Pct,  nullptr) \
    X (Slot1DelaySend, "slot1DelaySend","Slot 1 Delay",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot1RevSend,   "slot1RevSend",  "Slot 1 Space",   Noise,   0,      1,       0.35f,  U_Pct,  nullptr) \
    X (Slot1LfoRate,   "slot1LfoRate",  "Slot 1 LFO Rate",Noise,   0.01f,  8,       0.07f,  U_Hz,   nullptr) \
    X (Slot1LfoShape,  "slot1LfoShape", "Slot 1 LFO",     Noise,   0,      6,       0,      U_None, &kLfoShapeChoices) \
    X (Slot1LfoCut,    "slot1LfoCut",   "S1 LFO > Cut",   Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot1LfoPan,    "slot1LfoPan",   "S1 LFO > Pan",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot1LfoSpace,  "slot1LfoSpace", "S1 LFO > Space", Noise,   0,      1,       0.15f,  U_Pct,  nullptr) \
    X (Slot1Swell,     "slot1Swell",    "Slot 1 Swell",   Noise,   0,      20,      3,      U_Sec,  nullptr) \
    X (Slot2Source,    "slot2Source",   "Slot 2 Source",  Noise,   0,      137,     0,      U_None, &slotSourceChoices()) \
    X (Slot2Level,     "slot2Level",    "Slot 2 Level",   Noise,   0,      1,       0,      U_Pct,  nullptr) \
    X (Slot2Mode,      "slot2Mode",     "Slot 2 Mode",    Noise,   0,      2,       0,      U_None, &kSlotModeChoices) \
    X (Slot2Rate,      "slot2Rate",     "Slot 2 Tune",    Noise,  -24,     24,      0,      U_Semi, nullptr) \
    X (Slot2FiltType,  "slot2FiltType", "Slot 2 Filter",  Noise,   0,      2,       0,      U_None, &kSlotFilterChoices) \
    X (Slot2Cutoff,    "slot2Cutoff",   "Slot 2 Cutoff",  Noise,   30,     18000,   2000,   U_Hz,   nullptr) \
    X (Slot2Res,       "slot2Res",      "Slot 2 Res",     Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot2Pan,       "slot2Pan",      "Slot 2 Pan",     Noise,  -1,      1,      -0.40f,  U_Pct,  nullptr) \
    X (Slot2Drive,     "slot2Drive",    "Slot 2 Drive",   Noise,   0,      1,       0.10f,  U_Pct,  nullptr) \
    X (Slot2DelaySend, "slot2DelaySend","Slot 2 Delay",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot2RevSend,   "slot2RevSend",  "Slot 2 Space",   Noise,   0,      1,       0.35f,  U_Pct,  nullptr) \
    X (Slot2LfoRate,   "slot2LfoRate",  "Slot 2 LFO Rate",Noise,   0.01f,  8,       0.05f,  U_Hz,   nullptr) \
    X (Slot2LfoShape,  "slot2LfoShape", "Slot 2 LFO",     Noise,   0,      6,       6,      U_None, &kLfoShapeChoices) \
    X (Slot2LfoCut,    "slot2LfoCut",   "S2 LFO > Cut",   Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot2LfoPan,    "slot2LfoPan",   "S2 LFO > Pan",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot2LfoSpace,  "slot2LfoSpace", "S2 LFO > Space", Noise,   0,      1,       0.15f,  U_Pct,  nullptr) \
    X (Slot2Swell,     "slot2Swell",    "Slot 2 Swell",   Noise,   0,      20,      4,      U_Sec,  nullptr) \
    X (Slot3Source,    "slot3Source",   "Slot 3 Source",  Noise,   0,      137,     0,      U_None, &slotSourceChoices()) \
    X (Slot3Level,     "slot3Level",    "Slot 3 Level",   Noise,   0,      1,       0,      U_Pct,  nullptr) \
    X (Slot3Mode,      "slot3Mode",     "Slot 3 Mode",    Noise,   0,      2,       0,      U_None, &kSlotModeChoices) \
    X (Slot3Rate,      "slot3Rate",     "Slot 3 Tune",    Noise,  -24,     24,      0,      U_Semi, nullptr) \
    X (Slot3FiltType,  "slot3FiltType", "Slot 3 Filter",  Noise,   0,      2,       0,      U_None, &kSlotFilterChoices) \
    X (Slot3Cutoff,    "slot3Cutoff",   "Slot 3 Cutoff",  Noise,   30,     18000,   2000,   U_Hz,   nullptr) \
    X (Slot3Res,       "slot3Res",      "Slot 3 Res",     Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot3Pan,       "slot3Pan",      "Slot 3 Pan",     Noise,  -1,      1,       0.40f,  U_Pct,  nullptr) \
    X (Slot3Drive,     "slot3Drive",    "Slot 3 Drive",   Noise,   0,      1,       0.10f,  U_Pct,  nullptr) \
    X (Slot3DelaySend, "slot3DelaySend","Slot 3 Delay",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot3RevSend,   "slot3RevSend",  "Slot 3 Space",   Noise,   0,      1,       0.35f,  U_Pct,  nullptr) \
    X (Slot3LfoRate,   "slot3LfoRate",  "Slot 3 LFO Rate",Noise,   0.01f,  8,       0.09f,  U_Hz,   nullptr) \
    X (Slot3LfoShape,  "slot3LfoShape", "Slot 3 LFO",     Noise,   0,      6,       5,      U_None, &kLfoShapeChoices) \
    X (Slot3LfoCut,    "slot3LfoCut",   "S3 LFO > Cut",   Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot3LfoPan,    "slot3LfoPan",   "S3 LFO > Pan",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot3LfoSpace,  "slot3LfoSpace", "S3 LFO > Space", Noise,   0,      1,       0.15f,  U_Pct,  nullptr) \
    X (Slot3Swell,     "slot3Swell",    "Slot 3 Swell",   Noise,   0,      20,      5,      U_Sec,  nullptr) \
    X (Slot4Source,    "slot4Source",   "Slot 4 Source",  Noise,   0,      137,     0,      U_None, &slotSourceChoices()) \
    X (Slot4Level,     "slot4Level",    "Slot 4 Level",   Noise,   0,      1,       0,      U_Pct,  nullptr) \
    X (Slot4Mode,      "slot4Mode",     "Slot 4 Mode",    Noise,   0,      2,       0,      U_None, &kSlotModeChoices) \
    X (Slot4Rate,      "slot4Rate",     "Slot 4 Tune",    Noise,  -24,     24,      0,      U_Semi, nullptr) \
    X (Slot4FiltType,  "slot4FiltType", "Slot 4 Filter",  Noise,   0,      2,       0,      U_None, &kSlotFilterChoices) \
    X (Slot4Cutoff,    "slot4Cutoff",   "Slot 4 Cutoff",  Noise,   30,     18000,   2000,   U_Hz,   nullptr) \
    X (Slot4Res,       "slot4Res",      "Slot 4 Res",     Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot4Pan,       "slot4Pan",      "Slot 4 Pan",     Noise,  -1,      1,       0,      U_Pct,  nullptr) \
    X (Slot4Drive,     "slot4Drive",    "Slot 4 Drive",   Noise,   0,      1,       0.10f,  U_Pct,  nullptr) \
    X (Slot4DelaySend, "slot4DelaySend","Slot 4 Delay",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot4RevSend,   "slot4RevSend",  "Slot 4 Space",   Noise,   0,      1,       0.35f,  U_Pct,  nullptr) \
    X (Slot4LfoRate,   "slot4LfoRate",  "Slot 4 LFO Rate",Noise,   0.01f,  8,       0.06f,  U_Hz,   nullptr) \
    X (Slot4LfoShape,  "slot4LfoShape", "Slot 4 LFO",     Noise,   0,      6,       6,      U_None, &kLfoShapeChoices) \
    X (Slot4LfoCut,    "slot4LfoCut",   "S4 LFO > Cut",   Noise,   0,      1,       0.30f,  U_Pct,  nullptr) \
    X (Slot4LfoPan,    "slot4LfoPan",   "S4 LFO > Pan",   Noise,   0,      1,       0.20f,  U_Pct,  nullptr) \
    X (Slot4LfoSpace,  "slot4LfoSpace", "S4 LFO > Space", Noise,   0,      1,       0.15f,  U_Pct,  nullptr) \
    X (Slot4Swell,     "slot4Swell",    "Slot 4 Swell",   Noise,   0,      20,      6,      U_Sec,  nullptr) \
                                                                                                              \
    X (DelayTimeL,     "delayTimeL",    "Delay L",        Space,   10,     2000,    420,    U_Ms,   nullptr) \
    X (DelayTimeR,     "delayTimeR",    "Delay R",        Space,   10,     2000,    560,    U_Ms,   nullptr) \
    X (DelayFeedback,  "delayFeedback", "Delay Feedback", Space,   0,      0.92f,   0.42f,  U_Pct,  nullptr) \
    X (DelayDamp,      "delayDamp",     "Delay Damping",  Space,   0,      1,       0.45f,  U_Pct,  nullptr) \
    X (DelayMix,       "delayMix",      "Delay Mix",      Space,   0,      1,       0.25f,  U_Pct,  nullptr) \
    X (DelayMod,       "delayMod",      "Delay Motion",   Space,   0,      1,       0.25f,  U_Pct,  nullptr) \
    X (DelayMode,      "delayMode",     "Delay Mode",     Space,   0,      1,       0,      U_None, &kDelayModeChoices) \
    X (ReverbSize,     "reverbSize",    "Space Size",     Space,   0,      1,       0.65f,  U_Pct,  nullptr) \
    X (ReverbDecay,    "reverbDecay",   "Space Decay",    Space,   0,      1,       0.55f,  U_Pct,  nullptr) \
    X (ReverbDamp,     "reverbDamp",    "Space Damping",  Space,   0,      1,       0.45f,  U_Pct,  nullptr) \
    X (ReverbPredelay, "reverbPredelay","Pre-Delay",      Space,   0,      200,     30,     U_Ms,   nullptr) \
    X (ReverbWidth,    "reverbWidth",   "Space Width",    Space,   0,      1,       0.90f,  U_Pct,  nullptr) \
    X (ReverbMix,      "reverbMix",     "Space Mix",      Space,   0,      1,       0.50f,  U_Pct,  nullptr) \
    X (EnsembleDepth,  "ensembleDepth", "Ensemble Depth", Space,   0,      1,       0.35f,  U_Pct,  nullptr) \
    X (EnsembleRate,   "ensembleRate",  "Ensemble Rate",  Space,   0.05f,  3,       0.35f,  U_Hz,   nullptr) \
    X (PadSpace,       "padSpace",      "Pad > Space",    Space,   0,      1,       0.35f,  U_Pct,  nullptr) \
                                                                                                              \
    X (GateMix,        "gateMix",       "Gate Mix",       Gate,    0,      1,       0,      U_Pct,  nullptr) \
    X (GateTrigger,    "gateTrigger",   "Gate Trigger",   Gate,    0,      2,       0,      U_None, &kGateTriggerChoices) \
    X (GateLength,     "gateLength",    "Gate Length",    Gate,    0,      5,       1,      U_None, &kGateLengthChoices) \
    X (GateRate,       "gateRate",      "Gate Free Rate", Gate,    0.05f,  20,      2,      U_Hz,   nullptr) \
    X (GatePhase,      "gatePhase",     "Gate Phase",     Gate,    0,      1,       0,      U_Pct,  nullptr) \
    X (GateMin,        "gateMin",       "Gate Min",       Gate,    0,      1,       0,      U_Pct,  nullptr) \
    X (GateMax,        "gateMax",       "Gate Max",       Gate,    0,      1,       1,      U_Pct,  nullptr) \
    X (GateAttack,     "gateAttack",    "Gate Attack",    Gate,    0.001f, 2,       0.02f,  U_Sec,  nullptr) \
    X (GateRelease,    "gateRelease",   "Gate Release",   Gate,    0.001f, 2,       0.05f,  U_Sec,  nullptr) \
    X (GateTension,    "gateTension",   "Gate Tension",   Gate,   -1,      1,       0,      U_Pct,  nullptr) \
    X (GateTarget,     "gateTarget",    "Gate Target",    Gate,    0,      2,       2,      U_None, &kGateTargetChoices) \
    X (GateStep1,      "gateStep1",     "Step 1",         Gate,    0,      1,       1.00f,  U_Pct,  nullptr) \
    X (GateStep2,      "gateStep2",     "Step 2",         Gate,    0,      1,       0.55f,  U_Pct,  nullptr) \
    X (GateStep3,      "gateStep3",     "Step 3",         Gate,    0,      1,       0.90f,  U_Pct,  nullptr) \
    X (GateStep4,      "gateStep4",     "Step 4",         Gate,    0,      1,       0.15f,  U_Pct,  nullptr) \
    X (GateStep5,      "gateStep5",     "Step 5",         Gate,    0,      1,       0.85f,  U_Pct,  nullptr) \
    X (GateStep6,      "gateStep6",     "Step 6",         Gate,    0,      1,       0.30f,  U_Pct,  nullptr) \
    X (GateStep7,      "gateStep7",     "Step 7",         Gate,    0,      1,       1.00f,  U_Pct,  nullptr) \
    X (GateStep8,      "gateStep8",     "Step 8",         Gate,    0,      1,       0.45f,  U_Pct,  nullptr) \
    X (GateStep9,      "gateStep9",     "Step 9",         Gate,    0,      1,       0.75f,  U_Pct,  nullptr) \
    X (GateStep10,     "gateStep10",    "Step 10",        Gate,    0,      1,       0.20f,  U_Pct,  nullptr) \
    X (GateStep11,     "gateStep11",    "Step 11",        Gate,    0,      1,       0.65f,  U_Pct,  nullptr) \
    X (GateStep12,     "gateStep12",    "Step 12",        Gate,    0,      1,       0.50f,  U_Pct,  nullptr) \
                                                                                                              \
    X (MasterGain,     "masterGain",    "Master",         Master, -24,     12,     -4,      U_Db,   nullptr) \
    X (MasterDrive,    "masterDrive",   "Master Drive",   Master,  0,      1,       0.18f,  U_Pct,  nullptr) \
    X (MasterLowCut,   "masterLowCut",  "Low Cut",        Master,  20,     400,     45,     U_Hz,   nullptr) \
    X (MasterWidth,    "masterWidth",   "Width",          Master,  0,      1.5f,    1.0f,   U_Pct,  nullptr)

#define MK_PARAM_ENUM_ENTRY(e, key, label, grp, mn, mx, def, unit, ch) e,

enum class P : int
{
    MK_PARAM_TABLE (MK_PARAM_ENUM_ENTRY)
    Count
};

#undef MK_PARAM_ENUM_ENTRY

constexpr int kParamCount = (int) P::Count;
constexpr int kNoiseSlotCount = 4;

inline int paramIndex (P p) noexcept { return (int) p; }

const ParamSpec& paramSpec (P p) noexcept;
const ParamSpec& paramSpecByIndex (int index) noexcept;
const char* paramKey (P p) noexcept;

// Find a parameter by its stable string id; returns P::Count when unknown.
P paramFromKey (const char* key) noexcept;

using ParamSnapshot = std::array<float, (size_t) kParamCount>;

ParamSnapshot defaultSnapshot() noexcept;

// Clamps and quantises a value according to the parameter spec.
float sanitiseParam (P p, float value) noexcept;

// Per-slot parameter lookup helpers used by the noise engine and the UI.
P slotParam (int slot, int offsetFromSource);

// Step parameter of the trance gate (0 .. kGateSteps-1).
P gateStepParam (int index);

// True when the Slot*Source parameter range exactly matches the source list.
bool slotSourceRangeIsValid() noexcept;

} // namespace mk
