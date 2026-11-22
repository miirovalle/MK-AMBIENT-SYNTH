#pragma once

// Factory presets.
//
// 100 programs are generated from 20 designed families (five variations each).
// Every program is a full parameter snapshot expressed against the shared
// parameter table, so it stays valid when DSP internals change.

#include "ParamStore.h"
#include "Params.h"

namespace mk
{

struct FactoryPreset
{
    const char* name;
    const char* category;
    const char* description;
};

int factoryPresetCount() noexcept;
const FactoryPreset& factoryPreset (int index) noexcept;
int findFactoryPreset (const char* name) noexcept;

// Number of host programs: Init + all factory presets.
int factoryProgramCount() noexcept;

// Loads defaults, then the preset's overrides, then snaps the smoothers so the
// sound is immediate and click-free.
void applyFactoryPreset (ParamStore& store, int index);

// Defaults overlaid with the preset's overrides (0 .. factoryPresetCount()-1).
ParamSnapshot factoryPresetSnapshot (int index);

// Defaults only (the "Init" program).
ParamSnapshot initSnapshot();

// Category in display order, with the preset indices that belong to it.
struct PresetCategory
{
    const char* name;
    int firstIndex;
    int count;
};

int presetCategoryCount() noexcept;
PresetCategory presetCategory (int index) noexcept;

} // namespace mk
