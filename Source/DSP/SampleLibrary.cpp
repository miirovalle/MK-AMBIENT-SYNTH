#include "SampleLibrary.h"

#include "Filters.h"

#include <chrono>

namespace mk
{

// ---------------------------------------------------------------------------
// Catalogue
// ---------------------------------------------------------------------------

namespace
{

struct CategoryWords
{
    const char* adjectives[6];
    const char* nouns[4];
};

const CategoryWords& categoryWords (SampleCategory category) noexcept
{
    static const CategoryWords words[] = {
        { { "Dusty", "Dry", "Static", "Spark", "Ember", "Cinder" },
          { "Crackle", "Snap", "Grit", "Pop" } },
        { { "Worn", "Warm", "Vintage", "Scratched", "Deep", "Faded" },
          { "Vinyl", "Groove", "Surface", "Rot" } },
        { { "Warbly", "Saturated", "Old", "Faded", "Magnetic", "Hissy" },
          { "Tape", "Reel", "Cassette", "Hiss" } },
        { { "Wooden", "Distant", "Glassy", "Metal", "Soft", "Creaking" },
          { "Room", "Object", "Stone", "Cloth" } },
        { { "Granular", "Shimmering", "Frozen", "Micro", "Scattered", "Velvet" },
          { "Cloud", "Dust", "Sparkle", "Moss" } },
        { { "Distant", "Night", "Subway", "Rainy", "Hollow", "Neon" },
          { "City", "Traffic", "Plaza", "Tunnel" } }
    };
    const int idx = clampValue ((int) category, 0, kNumSampleCategories - 1);
    return words[idx];
}

float durationFor (SampleCategory category, int indexInCategory) noexcept
{
    switch (category)
    {
        case SampleCategory::Crackle:  return 0.80f + 0.10f * (float) (indexInCategory % 4);
        case SampleCategory::Vinyl:    return 1.00f + 0.13f * (float) (indexInCategory % 4);
        case SampleCategory::Tape:     return 1.50f + 0.18f * (float) (indexInCategory % 4);
        case SampleCategory::Foley:    return 0.35f + 0.13f * (float) (indexInCategory % 5);
        case SampleCategory::Granular: return 0.80f + 0.12f * (float) (indexInCategory % 5);
        case SampleCategory::City:     return 1.40f + 0.14f * (float) (indexInCategory % 4);
    }
    return 1.0f;
}

SampleCatalog buildCatalog()
{
    SampleCatalog catalog;
    catalog.entries.reserve ((size_t) kNumSampleSources);

    for (int c = 0; c < kNumSampleCategories; ++c)
    {
        const auto category = (SampleCategory) c;
        const auto& words = categoryWords (category);

        for (int i = 0; i < kSamplesPerCategory; ++i)
        {
            // Walk the 24 adjective/noun pairs with a stride coprime to 24 so
            // names stay varied and always unique.
            const int pair = (i * 5 + c * 7) % 24;
            SampleDescriptor d;
            d.category = category;
            d.indexInCategory = i;
            d.seed = 0x7F4A7C15u + (uint32_t) (c * 977 + i * 2654435761u);
            d.durationSeconds = durationFor (category, i);
            d.name = std::string (words.adjectives[pair % 6]) + " " + words.nouns[pair / 6];
            catalog.byCategory[(size_t) c].push_back ((int) catalog.entries.size());
            catalog.entries.push_back (std::move (d));
        }
    }
    return catalog;
}

// ---------------------------------------------------------------------------
// Generation helpers
// ---------------------------------------------------------------------------

inline float noise (XorShiftRng& rng) noexcept { return rng.nextBipolar(); }

void applyEdgeFades (std::vector<float>& buf, double sr, float fadeSeconds) noexcept
{
    const int fade = (int) std::min ((double) (buf.size() / 4), (double) fadeSeconds * sr);
    if (fade <= 1)
        return;
    for (int i = 0; i < fade; ++i)
    {
        const float g = (float) i / (float) fade;
        buf[(size_t) i] *= g;
        buf[buf.size() - 1 - (size_t) i] *= g;
    }
}

void normalise (std::vector<float>& buf, float peakTarget, float rmsTarget) noexcept
{
    float peak = 0.0f, sumSq = 0.0f;
    for (float v : buf)
    {
        peak = std::max (peak, std::fabs (v));
        sumSq += v * v;
    }
    if (peak < 1.0e-6f)
        return;
    const float rms = std::sqrt (sumSq / std::max<size_t> (1, buf.size()));
    float g = peakTarget / peak;
    if (rms * g > rmsTarget && rms > 1.0e-9f)
        g = rmsTarget / rms;
    for (float& v : buf)
        v *= g;
}

// Exponentially decaying sine burst, added into the buffer.
void addDecayingSine (std::vector<float>& out, double sr, int start, float amp, float freqHz,
                      float decaySeconds, float phase) noexcept
{
    const int n = (int) out.size();
    const int len = std::min (n - start, (int) (decaySeconds * 6.0f * (float) sr) + 8);
    if (len <= 0)
        return;

    const auto& sine = SineTable::instance();
    const float increment = freqHz / (float) sr;
    const float decayCoeff = std::exp (-1.0f / (decaySeconds * (float) sr));
    float phase01 = phase * (1.0f / kTwoPi) - std::floor (phase * (1.0f / kTwoPi));
    float env = amp;

    for (int k = 0; k < len; ++k)
    {
        out[(size_t) (start + k)] += env * sine.lookup (phase01);
        phase01 += increment;
        if (phase01 >= 1.0f)
            phase01 -= 1.0f;
        env *= decayCoeff;
    }
}

void addNoiseBurst (std::vector<float>& out, XorShiftRng& rng, double sr, int start, float amp,
                    float decaySeconds, float lowHz, float highHz) noexcept
{
    const int n = (int) out.size();
    const int len = std::min (n - start, (int) (decaySeconds * 6.0f * (float) sr) + 8);
    if (len <= 0)
        return;

    Svf filter;
    filter.prepare (sr);
    filter.updateCoefficients (std::sqrt (lowHz * highHz), 0.35f);

    const float decayCoeff = std::exp (-1.0f / (decaySeconds * (float) sr));
    float env = amp;
    for (int k = 0; k < len; ++k)
    {
        out[(size_t) (start + k)] += filter.processType (noise (rng) * env, SvfType::BandPass) * 1.6f;
        env *= decayCoeff;
    }
}

// Sparse click/crackle layer.
void addCrackle (std::vector<float>& out, XorShiftRng& rng, double sr, float density,
                 float brightness, float level) noexcept
{
    const float probability = density / (float) sr;
    const int n = (int) out.size();
    for (int i = 0; i < n; ++i)
    {
        if (rng.nextUnipolar() < probability)
        {
            const float amp = level * (0.12f + 0.88f * std::pow (rng.nextUnipolar(), 2.2f));
            const float freq = 800.0f + brightness * 6200.0f * std::pow (rng.nextUnipolar(), 0.7f);
            const float decay = 0.0006f + 0.0035f * rng.nextUnipolar();
            addDecayingSine (out, sr, i, amp, freq, decay, 0.0f);
            addNoiseBurst (out, rng, sr, i, amp * 0.55f, decay * 0.6f, 1200.0f, 9000.0f);
        }
    }
}

void addBrownRumble (std::vector<float>& out, XorShiftRng& rng, double sr, float level,
                     float cutoffHz) noexcept
{
    OnePoleLowPass lp1, lp2;
    lp1.setCutoff (sr, cutoffHz);
    lp2.setCutoff (sr, cutoffHz);
    float brown = 0.0f;
    for (float& v : out)
    {
        brown = (brown + noise (rng) * 0.02f) / 1.02f;
        v += lp2.process (lp1.process (brown * 3.5f)) * level;
    }
}

// ---------------------------------------------------------------------------
// Per-category generators
// ---------------------------------------------------------------------------

void generateCrackle (std::vector<float>& out, const SampleDescriptor& d, double sr, XorShiftRng& rng)
{
    addCrackle (out, rng, sr, 26.0f + 40.0f * rng.nextUnipolar(), 0.35f + 0.65f * rng.nextUnipolar(), 0.55f);
    if ((d.indexInCategory % 3) == 0)
        addCrackle (out, rng, sr, 4.0f, 0.15f, 0.9f); // occasional heavier pops
    addBrownRumble (out, rng, sr, 0.05f, 90.0f);
}

void generateVinyl (std::vector<float>& out, const SampleDescriptor& d, double sr, XorShiftRng& rng)
{
    Svf surface;
    surface.prepare (sr);
    surface.updateCoefficients (3800.0f, 0.15f);
    OnePoleLowPass hissLp;
    hissLp.setCutoff (sr, 7000.0f);

    const float wowRate = 0.35f + 0.5f * rng.nextUnipolar();
    const float wowPhase = rng.nextUnipolar();
    const float density = 9.0f + 16.0f * rng.nextUnipolar();

    for (size_t i = 0; i < out.size(); ++i)
    {
        const float t = (float) i / (float) sr;
        const float wow = 0.75f + 0.25f * std::sin (kTwoPi * (wowRate * t + wowPhase));
        out[i] += hissLp.process (noise (rng)) * 0.20f * wow;
        out[i] += surface.processType (noise (rng), SvfType::LowPass) * 0.55f;
    }

    addCrackle (out, rng, sr, density, 0.30f, 0.45f);
    if ((d.indexInCategory % 4) == 1)
        addCrackle (out, rng, sr, 2.0f, 0.10f, 1.0f);
    addBrownRumble (out, rng, sr, 0.12f, 110.0f);
}

void generateTape (std::vector<float>& out, const SampleDescriptor& d, double sr, XorShiftRng& rng)
{
    OnePoleLowPass lp;
    lp.setCutoff (sr, 9000.0f);
    OnePoleLowPass hpState;
    hpState.setCutoff (sr, 700.0f);

    const float wowRate = 0.25f + 0.6f * rng.nextUnipolar();
    const float flutterRate = 4.5f + 2.5f * rng.nextUnipolar();
    const float hissLevel = 0.22f + 0.16f * rng.nextUnipolar();

    std::vector<float> dropouts;
    const int numDropouts = 1 + (int) (rng.nextUnipolar() * 3.0f);
    for (int i = 0; i < numDropouts; ++i)
        dropouts.push_back (rng.nextUnipolar() * 0.85f);

    for (size_t i = 0; i < out.size(); ++i)
    {
        const float t = (float) i / (float) sr;
        const float wow = std::sin (kTwoPi * (wowRate * t + 0.13f)) * 0.18f
                        + std::sin (kTwoPi * (flutterRate * t + 0.71f)) * 0.06f;
        const float white = noise (rng);
        const float band = lp.process (white) - hpState.process (white);
        float gain = hissLevel * (1.0f + wow);

        for (float pos : dropouts)
        {
            const float dist = std::fabs (t / d.durationSeconds - pos);
            if (dist < 0.035f)
                gain *= 0.15f + 0.85f * (dist / 0.035f);
        }

        out[i] += band * gain;
    }

    // Slow low-frequency bias drift, tape "breathing".
    addBrownRumble (out, rng, sr, 0.035f, 60.0f);
}

void generateFoley (std::vector<float>& out, const SampleDescriptor& d, double sr, XorShiftRng& rng)
{
    const int n = (int) out.size();
    const int variant = d.indexInCategory % 4;

    switch (variant)
    {
        case 0: // wooden knock with body resonance
        {
            const float body = 140.0f + 320.0f * rng.nextUnipolar();
            addNoiseBurst (out, rng, sr, 0, 0.9f, 0.006f, 400.0f, 6000.0f);
            addDecayingSine (out, sr, 0, 0.8f, body, 0.09f + 0.1f * rng.nextUnipolar(), 0.0f);
            addDecayingSine (out, sr, 0, 0.3f, body * 2.7f, 0.03f, 1.1f);
            break;
        }
        case 1: // glass tick / ping
        {
            const float freq = 2200.0f + 4200.0f * rng.nextUnipolar();
            addNoiseBurst (out, rng, sr, 0, 0.35f, 0.0025f, 3000.0f, 11000.0f);
            addDecayingSine (out, sr, 0, 0.7f, freq, 0.10f + 0.25f * rng.nextUnipolar(), 0.0f);
            addDecayingSine (out, sr, 0, 0.35f, freq * 2.41f, 0.05f, 0.7f);
            addDecayingSine (out, sr, 0, 0.18f, freq * 3.77f, 0.03f, 1.9f);
            break;
        }
        case 2: // cloth / paper rustle: bursts of filtered noise
        {
            const int bursts = 8 + (int) (rng.nextUnipolar() * 14.0f);
            for (int b = 0; b < bursts; ++b)
            {
                const int start = (int) (rng.nextUnipolar() * (float) (n - 200));
                addNoiseBurst (out, rng, sr, start, 0.10f + 0.25f * rng.nextUnipolar(),
                               0.020f + 0.05f * rng.nextUnipolar(), 900.0f, 4800.0f);
            }
            break;
        }
        default: // stone / room hit with a short tail
        {
            const int start = (int) (rng.nextUnipolar() * 0.25f * (float) n);
            addNoiseBurst (out, rng, sr, start, 0.85f, 0.012f, 120.0f, 2200.0f);
            addDecayingSine (out, sr, start, 0.55f, 70.0f + 60.0f * rng.nextUnipolar(), 0.22f, 0.0f);
            addNoiseBurst (out, rng, sr, start + (int) (0.04 * sr), 0.12f, 0.05f, 300.0f, 2600.0f);
            break;
        }
    }
}

void generateGranular (std::vector<float>& out, const SampleDescriptor& d, double sr, XorShiftRng& rng)
{
    static const float scale[] = { 196.0f, 220.0f, 261.63f, 293.66f, 329.63f, 392.0f, 440.0f, 523.25f };
    const int n = (int) out.size();
    const int blips = 14 + (int) (rng.nextUnipolar() * 18.0f);

    for (int b = 0; b < blips; ++b)
    {
        const int start = (int) (rng.nextUnipolar() * (float) (n - 64));
        const float base = scale[rng.nextInt (8)];
        const float octave = (rng.nextUnipolar() < 0.35f) ? 2.0f : 1.0f;
        const float freq = base * octave * (1.0f + 0.004f * rng.nextBipolar());
        const float decay = 0.02f + 0.13f * rng.nextUnipolar();
        const float amp = 0.15f + 0.5f * std::pow (rng.nextUnipolar(), 1.5f);
        addDecayingSine (out, sr, start, amp, freq, decay, rng.nextUnipolar() * kTwoPi);
        addDecayingSine (out, sr, start, amp * 0.3f, freq * 2.02f, decay * 0.6f, 0.9f);
    }

    // A faint continuous air bed so the texture never feels completely empty.
    OnePoleLowPass air;
    air.setCutoff (sr, 6000.0f);
    for (float& v : out)
        v += air.process (noise (rng)) * 0.05f;

    (void) d;
}

void generateCity (std::vector<float>& out, const SampleDescriptor& d, double sr, XorShiftRng& rng)
{
    const int n = (int) out.size();
    OnePoleLowPass trafficLp;
    trafficLp.setCutoff (sr, 1400.0f);
    OnePoleLowPass airLp;
    airLp.setCutoff (sr, 7000.0f);

    const float amRate = 0.08f + 0.22f * rng.nextUnipolar();
    const float amDepth = 0.25f + 0.35f * rng.nextUnipolar();

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) sr;
        const float am = 1.0f - amDepth * 0.5f * (1.0f - std::cos (kTwoPi * amRate * t));
        out[(size_t) i] += trafficLp.process (noise (rng)) * 0.45f * am;
        out[(size_t) i] += airLp.process (noise (rng)) * 0.05f;
    }

    addBrownRumble (out, rng, sr, 0.55f, 140.0f);

    // Distant passing tone (siren / horn) on some variants.
    if ((d.indexInCategory % 3) != 2)
    {
        const int start = (int) ((0.15f + 0.4f * rng.nextUnipolar()) * (float) n);
        const int len = std::min (n - start, (int) (0.5f * (float) sr));
        const float f0 = 260.0f + 180.0f * rng.nextUnipolar();
        const auto& sine = SineTable::instance();
        float phase01 = 0.0f;
        for (int k = 0; k < len; ++k)
        {
            const float u = (float) k / (float) std::max (1, len);
            const float freq = f0 * (1.0f + 0.22f * std::sin (kPi * u));
            phase01 += freq / (float) sr;
            if (phase01 >= 1.0f)
                phase01 -= 1.0f;
            const float env = std::sin (kPi * u) * 0.035f;
            out[(size_t) (start + k)] += sine.lookup (phase01) * env;
        }
    }

    // Occasional rail/subway swell.
    if ((d.indexInCategory % 4) == 0)
    {
        const int start = (int) (rng.nextUnipolar() * 0.4f * (float) n);
        addNoiseBurst (out, rng, sr, start, 0.25f, 0.55f, 40.0f, 260.0f);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public catalogue / generation API
// ---------------------------------------------------------------------------

const SampleCatalog& sampleCatalog() noexcept
{
    static const SampleCatalog catalog = buildCatalog();
    return catalog;
}

const char* sampleCategoryName (SampleCategory category) noexcept
{
    switch (category)
    {
        case SampleCategory::Crackle:  return "Crackle";
        case SampleCategory::Vinyl:    return "Vinyl";
        case SampleCategory::Tape:     return "Tape";
        case SampleCategory::Foley:    return "Foley";
        case SampleCategory::Granular: return "Granular";
        case SampleCategory::City:     return "City";
    }
    return "Texture";
}

const char* proceduralSourceName (int index) noexcept
{
    static const char* const names[kNumProceduralSources] = {
        "White Noise", "Pink Noise", "Brown Noise", "Blue Noise", "Violet Noise", "Grey Noise"
    };
    return (index >= 0 && index < kNumProceduralSources) ? names[index] : "";
}

void renderCatalogueSample (const SampleDescriptor& descriptor, double sampleRate, std::vector<float>& out)
{
    const int numSamples = (int) std::max (1.0, (double) descriptor.durationSeconds * sampleRate);
    out.assign ((size_t) numSamples, 0.0f);

    XorShiftRng rng (descriptor.seed);

    switch (descriptor.category)
    {
        case SampleCategory::Crackle:  generateCrackle (out, descriptor, sampleRate, rng); break;
        case SampleCategory::Vinyl:    generateVinyl (out, descriptor, sampleRate, rng); break;
        case SampleCategory::Tape:     generateTape (out, descriptor, sampleRate, rng); break;
        case SampleCategory::Foley:    generateFoley (out, descriptor, sampleRate, rng); break;
        case SampleCategory::Granular: generateGranular (out, descriptor, sampleRate, rng); break;
        case SampleCategory::City:     generateCity (out, descriptor, sampleRate, rng); break;
    }

    applyEdgeFades (out, sampleRate, 0.012f);
    normalise (out, 0.85f, 0.22f);
    applyEdgeFades (out, sampleRate, 0.006f);
}

// ---------------------------------------------------------------------------
// SampleLibrary
// ---------------------------------------------------------------------------

void SampleLibrary::prepare()
{
    const auto startTime = std::chrono::steady_clock::now();
    const auto& catalog = sampleCatalog();

    buffers.resize (catalog.entries.size());

    // One scratch buffer reused for every entry keeps the peak allocation
    // small while generating.
    std::vector<float> scratch;

    for (size_t i = 0; i < catalog.entries.size(); ++i)
    {
        renderCatalogueSample (catalog.entries[i], kSampleStorageRate, scratch);

        auto& converted = buffers[i];
        converted.resize (scratch.size());
        for (size_t s = 0; s < scratch.size(); ++s)
        {
            const float value = clampf (scratch[s], -1.0f, 1.0f);
            converted[s] = (int16_t) std::lround (value * 32767.0f);
        }
    }

    ready = true;

    const auto endTime = std::chrono::steady_clock::now();
    generationMs = std::chrono::duration<double, std::milli> (endTime - startTime).count();
}

size_t SampleLibrary::memoryBytes() const noexcept
{
    size_t bytes = 0;
    for (const auto& b : buffers)
        bytes += b.size() * sizeof (int16_t);
    return bytes;
}

} // namespace mk
