#pragma once

// Short texture library.
//
// The catalogue (names, categories, seeds, durations) is static and available
// before any audio is prepared, so parameter choice lists and the UI can be
// built immediately. The audio itself is generated deterministically at a
// fixed storage rate, which keeps memory small on iPad and makes the content
// identical on every machine, sample rate and build.

#include "MkCommon.h"

#include <array>
#include <string>
#include <vector>

namespace mk
{

enum class SampleCategory : int
{
    Crackle = 0,
    Vinyl,
    Tape,
    Foley,
    Granular,
    City
};

constexpr int kNumSampleCategories = 6;
constexpr int kSamplesPerCategory = 22;
constexpr int kNumSampleSources = kNumSampleCategories * kSamplesPerCategory; // 132
constexpr int kNumProceduralSources = 6;
constexpr double kSampleStorageRate = 24000.0;

struct SampleDescriptor
{
    std::string name;
    SampleCategory category = SampleCategory::Crackle;
    int indexInCategory = 0;
    uint32_t seed = 1;
    float durationSeconds = 1.0f;
};

// Read-only view of one texture. Samples are stored as 16-bit to halve the
// resident memory on memory-constrained devices; noise textures at 24 kHz do
// not need more than ~90 dB of headroom.
struct SampleView
{
    const int16_t* data = nullptr;
    int length = 0;
    float scale = 1.0f; // 1 / 32767

    bool isValid() const noexcept { return data != nullptr && length > 0; }
};

struct SampleCatalog
{
    std::vector<SampleDescriptor> entries;
    std::array<std::vector<int>, (size_t) kNumSampleCategories> byCategory;
};

const SampleCatalog& sampleCatalog() noexcept;
const char* sampleCategoryName (SampleCategory category) noexcept;

// Names of the six procedural generators, in slot-source index order.
const char* proceduralSourceName (int index) noexcept;

// Generates one catalogue entry in mono at the given rate. Deterministic.
void renderCatalogueSample (const SampleDescriptor& descriptor, double sampleRate, std::vector<float>& out);

class SampleLibrary
{
public:
    // Generates every sample in the catalogue. Not real-time safe; call from
    // prepareToPlay (it takes a few tens of milliseconds and ~1.2 s of audio
    // per entry, then converts each one to 16-bit).
    void prepare();

    bool isReady() const noexcept { return ready; }
    int numSamples() const noexcept { return (int) buffers.size(); }
    SampleView get (int index) const noexcept
    {
        if (index < 0 || index >= (int) buffers.size())
            return {};
        const auto& buffer = buffers[(size_t) index];
        return { buffer.data(), (int) buffer.size(), 1.0f / 32767.0f };
    }

    double storageRate() const noexcept { return kSampleStorageRate; }
    size_t memoryBytes() const noexcept;
    double generationMilliseconds() const noexcept { return generationMs; }

private:
    std::vector<std::vector<int16_t>> buffers;
    bool ready = false;
    double generationMs = 0.0;
};

} // namespace mk
