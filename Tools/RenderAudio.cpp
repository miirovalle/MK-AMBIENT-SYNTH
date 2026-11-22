// Offline renderer used to validate the instrument without a DAW.
// Writes 32-bit float WAV files plus a small statistics table.

#include "DSP/Params.h"
#include "DSP/Presets.h"
#include "DSP/SynthEngine.h"
#include "DSP/Wavetables.h"

#define _CRT_SECURE_NO_WARNINGS 1

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

void writeFloatWav (const std::string& path, const std::vector<float>& left,
                    const std::vector<float>& right, uint32_t sampleRate)
{
    std::FILE* file = std::fopen (path.c_str(), "wb");
    if (file == nullptr)
    {
        std::printf ("  could not write %s\n", path.c_str());
        return;
    }

    const uint32_t numFrames = (uint32_t) left.size();
    const uint16_t channels = 2;
    const uint16_t bitsPerSample = 32;
    const uint32_t dataBytes = numFrames * channels * (bitsPerSample / 8);
    const uint32_t riffSize = 36 + dataBytes;

    auto write32 = [file] (uint32_t v) { std::fwrite (&v, sizeof v, 1, file); };
    auto write16 = [file] (uint16_t v) { std::fwrite (&v, sizeof v, 1, file); };

    std::fwrite ("RIFF", 1, 4, file);
    write32 (riffSize);
    std::fwrite ("WAVE", 1, 4, file);
    std::fwrite ("fmt ", 1, 4, file);
    write32 (16);
    write16 (3);                    // IEEE float
    write16 (channels);
    write32 (sampleRate);
    write32 (sampleRate * channels * (bitsPerSample / 8));
    write16 (channels * (bitsPerSample / 8));
    write16 (bitsPerSample);
    std::fwrite ("data", 1, 4, file);
    write32 (dataBytes);

    for (uint32_t i = 0; i < numFrames; ++i)
    {
        std::fwrite (&left[i], sizeof (float), 1, file);
        std::fwrite (&right[i], sizeof (float), 1, file);
    }

    std::fclose (file);
}

struct Stats
{
    float peak = 0.0f;
    double rms = 0.0;
    double mean = 0.0;
    double correlation = 0.0;
    bool finite = true;
};

Stats analyse (const std::vector<float>& left, const std::vector<float>& right)
{
    Stats stats;
    double sumSq = 0.0, sum = 0.0, sumLR = 0.0;
    for (size_t i = 0; i < left.size(); ++i)
    {
        stats.peak = std::max (stats.peak, std::max (std::fabs (left[i]), std::fabs (right[i])));
        if (! std::isfinite (left[i]) || ! std::isfinite (right[i]))
            stats.finite = false;
        sumSq += (double) left[i] * left[i] + (double) right[i] * right[i];
        sum += left[i] + right[i];
        sumLR += (double) left[i] * right[i];
    }
    const double n = (double) std::max<size_t> (1, left.size());
    stats.rms = std::sqrt (sumSq / (2.0 * n));
    stats.mean = sum / (2.0 * n);
    const double halfEnergy = sumSq * 0.5;
    stats.correlation = halfEnergy > 1e-12 ? sumLR / halfEnergy : 0.0;
    return stats;
}

struct RenderResult
{
    std::vector<float> left, right;
};

RenderResult render (mk::SynthEngine& engine, double seconds, int blockSize)
{
    const int total = (int) std::llround (seconds * engine.sampleRate());
    RenderResult result;
    result.left.assign ((size_t) total, 0.0f);
    result.right.assign ((size_t) total, 0.0f);

    int position = 0;
    while (position < total)
    {
        const int n = std::min (blockSize, total - position);
        engine.process (result.left.data() + position, result.right.data() + position, n);
        position += n;
    }
    return result;
}

void resetDefaults (mk::SynthEngine& engine)
{
    engine.allNotesOff (true);
    engine.params().resetToDefaults();
    engine.params().snapToTargets();
    engine.reset();
}

void report (const std::string& name, const RenderResult& result, uint32_t sr)
{
    const auto stats = analyse (result.left, result.right);
    std::printf ("  %-34s peak %.3f  rms %.4f  dc %+.5f  corr %+.3f  %s\n",
                 name.c_str(), stats.peak, stats.rms, stats.mean, stats.correlation,
                 stats.finite ? "ok" : "NON-FINITE");
}

} // namespace

int main (int argc, char** argv)
{
    const std::string outputDir = argc > 1 ? argv[1] : "build/renders";
    constexpr uint32_t kRate = 48000;
    constexpr int kBlock = 256;

    {
        std::error_code ec;
        std::filesystem::create_directories (outputDir, ec);
        if (ec)
            std::printf ("  could not create %s: %s\n", outputDir.c_str(), ec.message().c_str());
    }

    mk::SynthEngine engine;
    engine.prepare (kRate, kBlock);

    std::printf ("MK Ambient offline renderer\n");
    std::printf ("  sample rate %u, block %d\n", kRate, kBlock);
    std::printf ("  wavetables %.1f ms, %.1f MB\n", mk::WaveTableBank::instance().buildMilliseconds(),
                 (double) mk::WaveTableBank::instance().memoryBytes() / (1024.0 * 1024.0));
    std::printf ("  samples    %.1f ms, %.1f MB\n", engine.sampleLibrary().generationMilliseconds(),
                 (double) engine.sampleLibrary().memoryBytes() / (1024.0 * 1024.0));

    // ---- factory presets: chord, hold, release, long tail ------------------
    const int notes[4] = { 48, 55, 60, 64 };

    std::printf ("[presets]\n");

    std::ofstream manifest (outputDir + "/presets.csv");
    manifest << "index,name,category,filename,peak,rms,corr\n";

    int clipped = 0;
    int silent = 0;
    int nonFinite = 0;
    double quietest = 1.0e9;
    double loudest = 0.0;

    for (int i = 0; i < mk::factoryPresetCount(); ++i)
    {
        resetDefaults (engine);
        mk::applyFactoryPreset (engine.params(), i);

        for (int n : notes)
            engine.noteOn (n, 0.8f);

        auto held = render (engine, 5.0, kBlock);

        for (int n : notes)
            engine.noteOff (n);

        auto tail = render (engine, 7.0, kBlock);

        held.left.insert (held.left.end(), tail.left.begin(), tail.left.end());
        held.right.insert (held.right.end(), tail.right.begin(), tail.right.end());

        const auto stats = analyse (held.left, held.right);
        if (! stats.finite)
            ++nonFinite;
        if (stats.peak > 0.999f)
            ++clipped;
        if (stats.rms < 1.0e-4)
            ++silent;

        quietest = std::min (quietest, stats.rms);
        loudest = std::max (loudest, stats.rms);

        // Slug for a stable, readable file name.
        std::string slug = mk::factoryPreset (i).name;
        for (char& ch : slug)
            ch = (std::isalnum (static_cast<unsigned char> (ch)) != 0) ? (char) std::tolower ((unsigned char) ch) : '_';

        char prefix[32];
        std::snprintf (prefix, sizeof prefix, "preset_%03d_", i + 1);

        std::printf ("  %3d %-30.30s peak %.3f  rms %.4f  corr %+.2f\n", i + 1, mk::factoryPreset (i).name,
                     stats.peak, stats.rms, stats.correlation);

        writeFloatWav (outputDir + "/" + prefix + slug + ".wav", held.left, held.right, kRate);
        manifest << (i + 1) << "," << mk::factoryPreset (i).name << "," << mk::factoryPreset (i).category
                 << "," << prefix << slug << ".wav," << stats.peak << "," << stats.rms << ","
                 << stats.correlation << "\n";

        engine.allNotesOff (true);
        (void) render (engine, 1.0, kBlock);
    }

    std::printf ("  summary: %d programs, rms range %.4f..%.4f, %d clipped, %d silent, %d non-finite\n",
                 mk::factoryPresetCount(), quietest, loudest, clipped, silent, nonFinite);
    manifest.close();

    // ---- noise colours, slot 1 only ---------------------------------------
    std::printf ("[noise colours]\n");
    const char* colourNames[6] = { "white", "pink", "brown", "blue", "violet", "grey" };
    for (int type = 0; type < 6; ++type)
    {
        resetDefaults (engine);
        auto& p = engine.params();
        p.set (mk::P::PadLevel, 0.0f);
        p.set (mk::P::NoiseLevel, 1.0f);
        p.set (mk::P::Slot1Source, (float) type);
        p.set (mk::P::Slot1Level, 1.0f);
        p.set (mk::P::Slot1Cutoff, 18000.0f);
        p.set (mk::P::Slot1Res, 0.0f);
        p.set (mk::P::Slot1Drive, 0.0f);
        p.set (mk::P::Slot1DelaySend, 0.0f);
        p.set (mk::P::Slot1RevSend, 0.0f);
        p.set (mk::P::Slot1Swell, 0.0f);
        p.set (mk::P::Slot1LfoCut, 0.0f);
        p.set (mk::P::Slot1LfoPan, 0.0f);
        p.set (mk::P::ReverbMix, 0.0f);
        p.set (mk::P::DelayMix, 0.0f);
        p.set (mk::P::EnsembleDepth, 0.0f);
        p.set (mk::P::MasterDrive, 0.0f);
        p.set (mk::P::MasterGain, -12.0f);
        p.snapToTargets();

        engine.noteOn (60, 0.8f);
        auto audio = render (engine, 5.0, kBlock);
        engine.noteOff (60);
        report (colourNames[type], audio, kRate);
        writeFloatWav (outputDir + "/noise_" + colourNames[type] + ".wav", audio.left, audio.right, kRate);
        engine.allNotesOff (true);
    }

    // ---- aliasing probe: one bright oscillator, very high note ------------
    std::printf ("[aliasing probe]\n");
    {
        resetDefaults (engine);
        auto& p = engine.params();
        p.set (mk::P::NoiseLevel, 0.0f);
        p.set (mk::P::OscAWave, 0.0f);      // analog saw: dense harmonics
        p.set (mk::P::OscAPos, 1.0f);
        p.set (mk::P::OscALevel, 0.9f);
        p.set (mk::P::OscAUnison, 1.0f);
        p.set (mk::P::OscBLevel, 0.0f);
        p.set (mk::P::Drift, 0.0f);
        p.set (mk::P::Cutoff, 18000.0f);
        p.set (mk::P::Resonance, 0.0f);
        p.set (mk::P::FiltEnvAmt, 0.0f);
        p.set (mk::P::FiltKeyTrack, 0.0f);
        p.set (mk::P::FilterMode, 0.0f);
        p.set (mk::P::AmpAttack, 0.01f);
        p.set (mk::P::AmpSustain, 1.0f);
        p.set (mk::P::AmpDecay, 0.01f);
        p.set (mk::P::AmpRelease, 0.05f);
        p.set (mk::P::Lfo1Cutoff, 0.0f);
        p.set (mk::P::Lfo2Cutoff, 0.0f);
        p.set (mk::P::Lfo3Cutoff, 0.0f);
        p.set (mk::P::Lfo1Pan, 0.0f);
        p.set (mk::P::EnsembleDepth, 0.0f);
        p.set (mk::P::ReverbMix, 0.0f);
        p.set (mk::P::DelayMix, 0.0f);
        p.set (mk::P::PadSpace, 0.0f);
        p.set (mk::P::MasterDrive, 0.0f);
        p.snapToTargets();

        engine.noteOn (96, 0.9f); // C7 ~ 2093 Hz
        auto audio = render (engine, 2.0, kBlock);
        engine.noteOff (96);
        report ("analog saw C7", audio, kRate);
        writeFloatWav (outputDir + "/probe_alias_c7.wav", audio.left, audio.right, kRate);
        engine.allNotesOff (true);
    }

    // ---- stereo width: unison spread vs no spread -------------------------
    std::printf ("[stereo width probe]\n");
    {
        resetDefaults (engine);
        auto& p = engine.params();
        p.set (mk::P::NoiseLevel, 0.0f);
        p.set (mk::P::PadLevel, 0.8f);
        p.set (mk::P::OscASpread, 0.0f);
        p.set (mk::P::OscBSpread, 0.0f);
        p.set (mk::P::EnsembleDepth, 0.0f);
        p.set (mk::P::ReverbMix, 0.0f);
        p.set (mk::P::DelayMix, 0.0f);
        p.set (mk::P::Lfo1Pan, 0.0f);
        p.set (mk::P::Lfo2Pan, 0.0f);
        p.set (mk::P::Lfo3Pan, 0.0f);
        p.set (mk::P::PadSpace, 0.0f);
        p.set (mk::P::AmpAttack, 0.2f);
        p.set (mk::P::AmpRelease, 0.4f);
        p.set (mk::P::MasterWidth, 1.0f);
        p.snapToTargets();

        engine.noteOn (57, 0.8f);
        auto narrow = render (engine, 2.0, kBlock);
        engine.allNotesOff (true);
        (void) render (engine, 0.5, kBlock);

        p.set (mk::P::OscASpread, 1.0f);
        p.set (mk::P::OscBSpread, 0.8f);
        p.snapToTargets();
        engine.noteOn (57, 0.8f);
        auto wide = render (engine, 2.0, kBlock);
        engine.allNotesOff (true);

        report ("spread 0", narrow, kRate);
        report ("spread 1", wide, kRate);
        writeFloatWav (outputDir + "/probe_width_narrow.wav", narrow.left, narrow.right, kRate);
        writeFloatWav (outputDir + "/probe_width_wide.wav", wide.left, wide.right, kRate);
    }

    // ---- trance gate probe: one pulse per pattern at 1/4 ----------------
    std::printf ("[trance gate]\n");
    {
        resetDefaults (engine);
        mk::applyFactoryPreset (engine.params(), 4); // a pad to gate
        auto& p = engine.params();
        p.set (mk::P::GateMix, 1.0f);
        p.set (mk::P::GateMin, 0.0f);
        p.set (mk::P::GateMax, 1.0f);
        p.set (mk::P::GateTrigger, 0.0f);
        p.set (mk::P::GateLength, 3.0f); // 1/4 = 0.5 s at 120 BPM
        p.set (mk::P::GateAttack, 0.006f);
        p.set (mk::P::GateRelease, 0.020f);
        p.set (mk::P::GateTarget, 0.0f);
        p.set (mk::P::NoiseLevel, 0.0f);
        p.set (mk::P::ReverbMix, 0.0f);
        p.set (mk::P::DelayMix, 0.0f);
        p.set (mk::P::PadSpace, 0.0f);
        p.set (mk::P::EnsembleDepth, 0.0f);
        p.set (mk::P::AmpAttack, 0.05f);
        p.set (mk::P::AmpSustain, 1.0f);
        for (int i = 0; i < mk::kGateSteps; ++i)
            p.set (mk::gateStepParam (i), i == 0 ? 1.0f : 0.0f);
        p.snapToTargets();
        engine.setTempo (120.0);

        for (int n : notes)
            engine.noteOn (n, 0.8f);

        auto audio = render (engine, 4.0, kBlock);
        report ("gated pad 1/4 @ 120", audio, kRate);
        writeFloatWav (outputDir + "/probe_gate.wav", audio.left, audio.right, kRate);

        // Eighth-note pattern: still one pulse per pattern, now at 4 Hz.
        p.set (mk::P::GateLength, 4.0f); // 1/8
        p.set (mk::P::GateAttack, 0.003f);
        p.set (mk::P::GateRelease, 0.006f);
        p.snapToTargets();

        auto fast = render (engine, 4.0, kBlock);
        report ("gated pad 1/8 @ 120", fast, kRate);
        writeFloatWav (outputDir + "/probe_gate_fast.wav", fast.left, fast.right, kRate);

        engine.allNotesOff (true);
        (void) render (engine, 1.0, kBlock);
    }

    // ---- performance sweep -------------------------------------------------
    {
        resetDefaults (engine);
        mk::applyFactoryPreset (engine.params(), 4);
        for (int n : notes)
            engine.noteOn (n, 0.8f);

        const auto start = std::chrono::steady_clock::now();
        auto audio = render (engine, 20.0, kBlock);
        const auto end = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double> (end - start).count();
        std::printf ("[performance] 20 s of the densest preset rendered in %.2f s (%.1fx realtime, %.1f%% CPU of one core)\n",
                     elapsed, 20.0 / elapsed, 100.0 * elapsed / 20.0);
        engine.allNotesOff (true);
        (void) audio;
    }

    std::printf ("\nwrote WAV files to %s\n", outputDir.c_str());
    return 0;
}
