#include "Params.h"

#include "SampleLibrary.h"

#include <array>
#include <string>
#include <vector>

namespace mk
{

// ---------------------------------------------------------------------------
// Choice lists
// ---------------------------------------------------------------------------

namespace
{
const char* const kWaveNames[] = { "Analog Saw", "Warm Pad", "Hollow", "Glass", "Formant", "Organ" };
const char* const kIntervalNames[] = { "-24", "-12", "-7", "-5", "0", "+5", "+7", "+12" };
const char* const kFilterModeNames[] = { "Low Pass 12", "Low Pass 24", "Band Pass", "High Pass" };
const char* const kLfoShapeNames[] = { "Sine", "Triangle", "Ramp Up", "Ramp Down", "Square", "Sample & Hold", "Smooth Random" };
const char* const kSlotModeNames[] = { "Loop", "One Shot", "Grain" };
const char* const kSlotFilterNames[] = { "Low Pass", "Band Pass", "High Pass" };
const char* const kDelayModeNames[] = { "Stereo", "Ping Pong" };
const char* const kGateTriggerNames[] = { "Sync", "Free", "Note" };
const char* const kGateLengthNames[] = { "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16" };
const char* const kGateTargetNames[] = { "Pad", "Textures", "Both" };
} // namespace

const ChoiceList kWaveChoices { kWaveNames, 6 };
const ChoiceList kIntervalChoices { kIntervalNames, 8 };
const ChoiceList kFilterModeChoices { kFilterModeNames, 4 };
const ChoiceList kLfoShapeChoices { kLfoShapeNames, 7 };
const ChoiceList kSlotModeChoices { kSlotModeNames, 3 };
const ChoiceList kSlotFilterChoices { kSlotFilterNames, 3 };
const ChoiceList kDelayModeChoices { kDelayModeNames, 2 };
const ChoiceList kGateTriggerChoices { kGateTriggerNames, 3 };
const ChoiceList kGateLengthChoices { kGateLengthNames, 6 };
const ChoiceList kGateTargetChoices { kGateTargetNames, 3 };

namespace
{
struct SlotSourceStorage
{
    std::vector<std::string> names;
    std::vector<const char*> pointers;
    ChoiceList list { nullptr, 0 };
};

SlotSourceStorage buildSlotSourceStorage()
{
    SlotSourceStorage storage;
    const auto& catalog = sampleCatalog();

    storage.names.reserve ((size_t) (kNumProceduralSources + (int) catalog.entries.size()));

    for (int i = 0; i < kNumProceduralSources; ++i)
        storage.names.emplace_back (proceduralSourceName (i));

    for (const auto& entry : catalog.entries)
        storage.names.emplace_back (std::string (sampleCategoryName (entry.category)) + ": " + entry.name);

    storage.pointers.reserve (storage.names.size());
    for (const auto& name : storage.names)
        storage.pointers.push_back (name.c_str());

    storage.list.names = storage.pointers.data();
    storage.list.count = (int) storage.pointers.size();
    return storage;
}
} // namespace

const ChoiceList& slotSourceChoices() noexcept
{
    static const SlotSourceStorage storage = buildSlotSourceStorage();
    return storage.list;
}

// ---------------------------------------------------------------------------
// Specs
// ---------------------------------------------------------------------------

namespace
{
std::array<ParamSpec, (size_t) kParamCount> buildSpecs()
{
    std::array<ParamSpec, (size_t) kParamCount> specs {{
#define MK_PARAM_SPEC_ENTRY(e, key, label, grp, mn, mx, def, unit, ch) \
        ParamSpec { key, label, grp, (float) (mn), (float) (mx), (float) (def), unit, ch },
        MK_PARAM_TABLE (MK_PARAM_SPEC_ENTRY)
#undef MK_PARAM_SPEC_ENTRY
    }};
    return specs;
}
} // namespace

const ParamSpec& paramSpecByIndex (int index) noexcept
{
    static const auto specs = buildSpecs();
    const int i = clampValue (index, 0, kParamCount - 1);
    return specs[(size_t) i];
}

const ParamSpec& paramSpec (P p) noexcept { return paramSpecByIndex ((int) p); }

const char* paramKey (P p) noexcept { return paramSpec (p).key; }

P paramFromKey (const char* key) noexcept
{
    if (key == nullptr)
        return P::Count;
    for (int i = 0; i < kParamCount; ++i)
        if (std::strcmp (paramSpecByIndex (i).key, key) == 0)
            return (P) i;
    return P::Count;
}

ParamSnapshot defaultSnapshot() noexcept
{
    ParamSnapshot snapshot {};
    for (int i = 0; i < kParamCount; ++i)
        snapshot[(size_t) i] = paramSpecByIndex (i).defaultValue;
    return snapshot;
}

float sanitiseParam (P p, float value) noexcept
{
    const auto& spec = paramSpec (p);
    return spec.clamp (value);
}

const char* groupName (Group group) noexcept
{
    switch (group)
    {
        case Pad:      return "Pad";
        case Filter:   return "Filter";
        case Envelope: return "Envelope";
        case Mod:      return "Movement";
        case Noise:    return "Textures";
        case Space:    return "Space";
        case Gate:     return "Gate";
        case Master:   return "Master";
    }
    return "";
}

const char* unitSuffix (Unit unit) noexcept
{
    switch (unit)
    {
        case U_Pct:  return "%";
        case U_Sec:  return " s";
        case U_Hz:   return " Hz";
        case U_Db:   return " dB";
        case U_Semi: return " st";
        case U_Cent: return " ct";
        case U_Ms:   return " ms";
        case U_None:   return "";
        case U_Switch: return "";
    }
    return "";
}

// ---------------------------------------------------------------------------
// Noise slot layout helpers
// ---------------------------------------------------------------------------

namespace
{
// Every slot exposes the same contiguous parameters, starting at Source and
// ending with its on/off switch.
static_assert ((int) P::Slot2Source - (int) P::Slot1Source == kSlotStride, "slot parameter block must be contiguous");
static_assert ((int) P::Slot3Source - (int) P::Slot2Source == kSlotStride, "slot parameter block must be contiguous");
static_assert ((int) P::Slot4Source - (int) P::Slot3Source == kSlotStride, "slot parameter block must be contiguous");
static_assert ((int) P::Slot1Swell - (int) P::Slot1Source == kSlotStride - 2, "slot block must end at Swell + On");
static_assert ((int) P::Slot1On - (int) P::Slot1Source == kSlotEnableOffset, "slot enable must be the last entry");

static_assert ((int) P::Lfo2Rate - (int) P::Lfo1Rate == kLfoStride, "LFO parameter block must be contiguous");
static_assert ((int) P::Lfo3Rate - (int) P::Lfo2Rate == kLfoStride, "LFO parameter block must be contiguous");
static_assert ((int) P::Lfo1On - (int) P::Lfo1Rate == kLfoEnableOffset, "LFO enable must be the last entry");
} // namespace

P slotParam (int slot, int offsetFromSource)
{
    const int clampedSlot = clampValue (slot, 0, kNoiseSlotCount - 1);
    const int offset = clampValue (offsetFromSource, 0, kSlotStride - 1);
    return (P) ((int) P::Slot1Source + clampedSlot * kSlotStride + offset);
}

P lfoParam (int index, int offsetFromRate)
{
    const int clampedIndex = clampValue (index, 0, 2);
    const int offset = clampValue (offsetFromRate, 0, kLfoStride - 1);
    return (P) ((int) P::Lfo1Rate + clampedIndex * kLfoStride + offset);
}

P gateStepParam (int index)
{
    return (P) ((int) P::GateStep1 + clampValue (index, 0, kGateSteps - 1));
}

// The slot source range must cover exactly the procedural types plus the
// whole catalogue, so the choice list and the parameter range never diverge.
static const bool slotSourceRangeValidated = []
{
    const auto& choices = slotSourceChoices();
    const bool ok = choices.count == kNumProceduralSources + kNumSampleSources
                 && paramSpec (P::Slot1Source).maxValue == (float) (choices.count - 1);
    return ok;
}();

bool slotSourceRangeIsValid() noexcept { return slotSourceRangeValidated; }

} // namespace mk
