// CPU benchmark for the portable engine.
//
// Reports how much wall-clock time it takes to render one second of audio for
// several worst-case configurations, which is the number that matters on a
// tablet: the audio thread must stay far below 100% of one core.

#include "DSP/Params.h"
#include "DSP/Presets.h"
#include "DSP/SynthEngine.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

struct Case
{
    std::string name;
    int voices;
    bool textures;
    bool reverb;
    int preset;
};

void configure (mk::SynthEngine& engine, const Case& c)
{
    engine.allNotesOff (true);
    mk::applyFactoryPreset (engine.params(), c.preset);

    auto& p = engine.params();
    p.set (mk::P::Polyphony, (float) c.voices);
    p.set (mk::P::NoiseLevel, c.textures ? 0.85f : 0.0f);
    p.set (mk::P::ReverbMix, c.reverb ? 0.6f : 0.0f);

    if (c.textures)
    {
        // All four slots busy with the most demanding sources.
        p.set (mk::P::Slot1Source, 150.0f); // granular sample
        p.set (mk::P::Slot1Level, 0.5f);
        p.set (mk::P::Slot1Mode, 2.0f);
        p.set (mk::P::Slot2Source, 121.0f); // city sample
        p.set (mk::P::Slot2Level, 0.4f);
        p.set (mk::P::Slot3Source, 2.0f);   // brown noise
        p.set (mk::P::Slot3Level, 0.4f);
        p.set (mk::P::Slot4Source, 5.0f);   // grey noise
        p.set (mk::P::Slot4Level, 0.4f);
    }

    p.snapToTargets();
}

double measure (mk::SynthEngine& engine, const Case& c, double seconds, int blockSize)
{
    configure (engine, c);

    // Hold a full chord plus a few extra notes so the voice limit is reached.
    const int notes[] = { 48, 52, 55, 59, 60, 64, 67, 71, 72, 74, 76, 79, 81, 83, 86, 88 };
    for (int i = 0; i < c.voices; ++i)
        engine.noteOn (notes[i % 16], 0.8f);

    const int total = (int) (seconds * engine.sampleRate());
    std::vector<float> l ((size_t) blockSize, 0.0f), r ((size_t) blockSize, 0.0f);

    // Warm up caches and filters.
    for (int i = 0; i < 20; ++i)
        engine.process (l.data(), r.data(), blockSize);

    const auto start = std::chrono::steady_clock::now();
    int done = 0;
    while (done < total)
    {
        const int n = std::min (blockSize, total - done);
        engine.process (l.data(), r.data(), n);
        done += n;
    }
    const auto end = std::chrono::steady_clock::now();

    engine.allNotesOff (true);
    (void) engine.process (l.data(), r.data(), blockSize);

    return std::chrono::duration<double> (end - start).count();
}

} // namespace

int main (int argc, char** argv)
{
    const double sr = argc > 1 ? std::atof (argv[1]) : 48000.0;
    const int blockSize = argc > 2 ? std::atoi (argv[2]) : 256;
    const double seconds = 5.0;

    mk::SynthEngine engine;
    engine.prepare (sr, blockSize);

    const Case cases[] = {
        { "4 voices, pad only",            4,  false, true,  1 },
        { "8 voices, pad only",            8,  false, true,  1 },
        { "8 voices + 4 textures",         8,  true,  true,  4 },
        { "16 voices + 4 textures, reverb",16, true,  true,  4 },
        { "16 voices, no reverb",          16, false, false, 4 },
    };

    std::printf ("MK Ambient CPU benchmark\n");
    std::printf ("  %.0f Hz, %d-sample blocks, %.0f s per case\n\n", sr, blockSize, seconds);
    std::printf ("  %-34s%10s%10s%12s\n", "case", "seconds", "realtime", "1 core %");

    double worst = 0.0;
    for (const auto& c : cases)
    {
        const double elapsed = measure (engine, c, seconds, blockSize);
        const double factor = seconds / std::max (1.0e-9, elapsed);
        const double cpu = 100.0 * elapsed / seconds;
        worst = std::max (worst, cpu);
        std::printf ("  %-34s%10.3f%9.1fx%11.2f%%\n", c.name.c_str(), elapsed, factor, cpu);
    }

    std::printf ("\n  worst case %.2f%% of one core\n", worst);

    const auto& library = engine.sampleLibrary();
    const double wtMb = (double) mk::WaveTableBank::instance().memoryBytes() / (1024.0 * 1024.0);
    const double sampleMb = (double) library.memoryBytes() / (1024.0 * 1024.0);
    std::printf ("\nresident content\n");
    std::printf ("  wavetables                %7.2f MB\n", wtMb);
    std::printf ("  noise/sample library      %7.2f MB  (%d sounds)\n",
                 sampleMb, library.numSamples());
    std::printf ("  engine + voices           %7.2f MB\n",
                 (double) sizeof (mk::SynthEngine) / (1024.0 * 1024.0));
    std::printf ("  total static footprint    %7.2f MB\n", wtMb + sampleMb);
    return 0;
}
