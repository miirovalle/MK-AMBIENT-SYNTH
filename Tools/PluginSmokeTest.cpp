// Headless smoke test for the plugin wrapper.
//
// Instantiates the real AudioProcessor (the same class the AUv3 extension and
// the standalone app use), drives it through prepareToPlay / MIDI / processBlock
// and verifies that host automation reaches the engine and that audio comes out.

#include "Plugin/PluginProcessor.h"
#include "UI/Panels.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{

int failures = 0;

void check (bool condition, const char* what)
{
    if (! condition)
    {
        std::printf ("  FAIL: %s\n", what);
        ++failures;
    }
}

struct Stats
{
    float peak = 0.0f;
    double rms = 0.0;
    double correlation = 0.0;
    bool finite = true;
};

Stats renderSeconds (mk::PluginProcessor& processor, double seconds, int blockSize = 256)
{
    const int total = (int) std::llround (seconds * 48000.0);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    Stats stats;
    double sumL = 0.0, sumR = 0.0, sumLR = 0.0;

    int rendered = 0;
    while (rendered < total)
    {
        const int n = std::min (blockSize, total - rendered);
        buffer.setSize (2, n, false, false, true);
        buffer.clear();
        processor.processBlock (buffer, midi);
        midi.clear();

        const float* left = buffer.getReadPointer (0);
        const float* right = buffer.getReadPointer (1);

        for (int i = 0; i < n; ++i)
        {
            stats.peak = std::max (stats.peak, std::max (std::abs (left[i]), std::abs (right[i])));
            if (! std::isfinite (left[i]) || ! std::isfinite (right[i]))
                stats.finite = false;
            sumL += (double) left[i] * left[i];
            sumR += (double) right[i] * right[i];
            sumLR += (double) left[i] * right[i];
        }

        rendered += n;
    }

    const double n = (double) std::max (1, total);
    stats.rms = std::sqrt ((sumL + sumR) / (2.0 * n));
    stats.correlation = sumLR / std::max (1e-12, std::sqrt (sumL * sumR));
    return stats;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    mk::PluginProcessor processor;

    std::printf ("MK Ambient plugin smoke test\n");
    std::printf ("  name: %s\n", processor.getName().toRawUTF8());
    std::printf ("  programs: %d\n", processor.getNumPrograms());
    check (processor.getNumPrograms() == 101, "init + 100 factory programs");
    check (processor.getParameters().size() > 100, "parameters exposed to the host");

    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);

    // ---- default program: chord through the wrapper ------------------------
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 90), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 85), 0);

    // Push the first block with the notes so the engine sees them.
    juce::AudioBuffer<float> buffer (2, 256);
    buffer.clear();
    processor.processBlock (buffer, midi);
    midi.clear();

    const auto held = renderSeconds (processor, 4.0);
    std::printf ("  init program: peak %.3f rms %.4f corr %+.3f\n", held.peak, held.rms, held.correlation);
    check (held.finite, "output is finite");
    check (held.peak > 0.01f, "output is audible");
    check (held.peak <= 1.0f, "output is limited");
    check (held.correlation < 0.999, "output has stereo width");

    // ---- automation reaches the engine -------------------------------------
    if (auto* cutoff = processor.apvts.getParameter ("cutoff"))    {
        cutoff->setValueNotifyingHost (cutoff->convertTo0to1 (400.0f));
        juce::AudioBuffer<float> block (2, 256);
        block.clear();
        juce::MidiBuffer none;
        processor.processBlock (block, none);

        const float engineCutoff = processor.engine.params().get (mk::P::Cutoff);
        check (std::abs (engineCutoff - 400.0f) < 1.0f, "host automation reaches the DSP engine");
    }
    else
    {
        check (false, "cutoff parameter exists");
    }

    // ---- program switching -------------------------------------------------
    processor.setCurrentProgram (4); // Glass / Shimmer Pluck
    check (processor.getCurrentProgram() == 4, "program index updates");
    check (processor.getProgramName (4).isNotEmpty(), "program has a name");

    juce::MidiBuffer chord;
    chord.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    juce::AudioBuffer<float> first (2, 256);
    first.clear();
    processor.processBlock (first, chord);

    const auto glass = renderSeconds (processor, 4.0);
    std::printf ("  glass program: peak %.3f rms %.4f corr %+.3f\n", glass.peak, glass.rms, glass.correlation);
    check (glass.finite, "program switch output is finite");
    check (glass.peak > 0.005f, "program switch produces audio");
    check (glass.peak <= 1.0f, "program switch stays limited");

    // ---- sustain pedal -----------------------------------------------------
    processor.engine.allNotesOff (true);
    juce::MidiBuffer clear;
    juce::AudioBuffer<float> clearBlock (2, 256);
    clearBlock.clear();
    processor.processBlock (clearBlock, clear);

    juce::MidiBuffer pedal;
    pedal.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    pedal.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    juce::AudioBuffer<float> pedalBlock (2, 256);
    pedalBlock.clear();
    processor.processBlock (pedalBlock, pedal);
    check (processor.engine.heldVoiceCount() == 1, "note is held");

    juce::MidiBuffer release;
    release.addEvent (juce::MidiMessage::noteOff (1, 64), 0);
    juce::AudioBuffer<float> releaseBlock (2, 256);
    releaseBlock.clear();
    processor.processBlock (releaseBlock, release);
    check (processor.engine.heldVoiceCount() == 1, "note off is deferred by the sustain pedal");

    juce::MidiBuffer pedalUp;
    pedalUp.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    juce::AudioBuffer<float> pedalUpBlock (2, 256);
    pedalUpBlock.clear();
    processor.processBlock (pedalUpBlock, pedalUp);
    check (processor.engine.heldVoiceCount() == 0, "pedal up releases the deferred note");

    // ---- on-screen keyboard reaches the engine -----------------------------
    processor.engine.allNotesOff (true);

    processor.keyboardNoteOn (60, 0.9f);
    juce::AudioBuffer<float> keyboardBlock (2, 256);
    keyboardBlock.clear();
    juce::MidiBuffer noHostMidi;
    processor.processBlock (keyboardBlock, noHostMidi);
    check (processor.engine.heldVoiceCount() == 1, "on-screen keyboard note is held");
    check (keyboardBlock.getMagnitude (0, keyboardBlock.getNumSamples()) > 0.0f,
           "on-screen keyboard note produces audio");

    processor.keyboardNoteOff (60);
    juce::AudioBuffer<float> keyboardRelease (2, 256);
    keyboardRelease.clear();
    juce::MidiBuffer emptyHostMidi;
    processor.processBlock (keyboardRelease, emptyHostMidi);
    check (processor.engine.heldVoiceCount() == 0, "on-screen keyboard release frees the note");

    // ---- state round-trip --------------------------------------------------
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    check (state.getSize() > 0, "state is serialised");

    processor.setCurrentProgram (1);
    processor.setStateInformation (state.getData(), (int) state.getSize());
    check (processor.getCurrentProgram() == 4, "program index survives a state round-trip");

    // ---- gate parameters are host-visible ----------------------------------
    for (int i = 0; i < mk::kGateSteps; ++i)
        check (processor.apvts.getParameter (mk::paramSpec (mk::gateStepParam (i)).key) != nullptr,
               "gate step parameter exists");
    check (processor.apvts.getParameter ("gateMix") != nullptr, "gate mix parameter exists");
    check (processor.apvts.getParameter ("gateLength") != nullptr, "gate length parameter exists");

    // ---- section on/off switches are host-visible ---------------------------
    for (const char* key : { "oscAOn", "oscBOn", "slot1On", "slot2On", "slot3On", "slot4On",
                             "lfo1On", "lfo2On", "lfo3On", "delayOn", "reverbOn", "ensembleOn", "gateOn" })
        check (processor.apvts.getParameter (key) != nullptr, "section switch exists");

    processor.releaseResources();

    // ---- offscreen UI snapshots -------------------------------------------
    if (argc > 1)
    {
        const juce::File outputDir { juce::String (argv[1]) };
        outputDir.createDirectory();

        auto writeImage = [&outputDir] (const juce::Image& image, const juce::String& fileName)
        {
            const auto file = outputDir.getChildFile (fileName + ".png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat png;
            if (stream.openedOk() && png.writeImageToStream (image, stream))
                std::printf ("  wrote %s\n", file.getFullPathName().toRawUTF8());
            else
                std::printf ("  FAILED to write %s\n", file.getFullPathName().toRawUTF8());
        };

        auto writeSnapshot = [&writeImage] (juce::Component& page, const juce::String& fileName)
        {
            page.setSize (1164, 656);
            page.setVisible (true);

            // Pages are transparent (the editor paints behind them), so give
            // the snapshot the editor backdrop to make it reviewable.
            const auto snapshot = page.createComponentSnapshot (page.getLocalBounds(), true, 1.0f);
            juce::Image image (juce::Image::ARGB, page.getWidth(), page.getHeight(), true);
            {
                juce::Graphics g (image);
                g.fillAll (mk::ui::col::background);
                g.drawImageAt (snapshot, 0, 0);
            }
            writeImage (image, fileName);
        };

        mk::ui::PadPage padPage (processor.apvts);
        mk::ui::TexturePage texturePage (processor.apvts);
        mk::ui::ModPage modPage (processor.apvts);
        mk::ui::SpacePage spacePage (processor.apvts);
        mk::ui::GatePage gatePage (processor.apvts, mk::ui::GateHostContext {
                                                       [&processor] { return processor.engine.gatePhase(); },
                                                       [&processor] { return processor.engine.gateLevel(); },
                                                       [&processor] { return processor.engine.tempo(); },
                                                       [&processor] { return processor.engine.sampleRate(); } });
        mk::ui::PresetPage presetPage ([] (int) {}, [] { return 1; });

        writeSnapshot (padPage, "page_pad");
        writeSnapshot (texturePage, "page_textures");
        writeSnapshot (modPage, "page_motion");
        writeSnapshot (spacePage, "page_space");
        writeSnapshot (gatePage, "page_gate");
        writeSnapshot (presetPage, "page_presets");

        // Whole interface (header, tabs, page and keyboard) plus a thumbnail of
        // it, so the layout can be reviewed without opening a plug-in host.
        if (std::unique_ptr<juce::AudioProcessorEditor> editor { processor.createEditor() })
        {
            editor->setSize (1200, 900);
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            writeImage (image, "editor");
            writeImage (image.rescaled (480, 360, juce::Graphics::highResamplingQuality), "thumbnail");
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "SMOKE TEST PASS" : "SMOKE TEST FAIL");

    // ---- optional: host the built VST3 bundle and play through it ----------
    if (argc > 2)
    {
        const juce::File bundle { juce::String (argv[2]) };
        std::printf ("[vst3 host] %s\n", bundle.getFullPathName().toRawUTF8());
        check (bundle.isDirectory(), "VST3 bundle exists");

        juce::AudioPluginFormatManager formats;
        formats.addDefaultFormats();

        juce::OwnedArray<juce::PluginDescription> types;
        for (auto* format : formats.getFormats())
            format->findAllTypesForFile (types, bundle.getFullPathName());

        check (types.size() == 1, "VST3 exposes exactly one plug-in");
        if (types.size() == 1)
        {
            const auto& description = *types[0];
            std::printf ("  found: %s by %s (%d in / %d out)\n", description.name.toRawUTF8(),
                         description.manufacturerName.toRawUTF8(),
                         description.numInputChannels, description.numOutputChannels);
            check (description.isInstrument, "VST3 is an instrument");

            juce::String error;
            std::unique_ptr<juce::AudioPluginInstance> instance (
                formats.createPluginInstance (description, 48000.0, 256, error));
            check (instance != nullptr, error.isEmpty() ? "VST3 instantiated" : error.toRawUTF8());

            if (instance != nullptr)
            {
                const auto layout = instance->getBusesLayout();
                std::printf ("  buses: %d in / %d out, main output %d ch, layout supported %d\n",
                             instance->getTotalNumInputChannels(),
                             instance->getTotalNumOutputChannels(),
                             layout.getMainOutputChannels(),
                             (int) instance->checkBusesLayoutSupported (layout));
                check (instance->getTotalNumInputChannels() == 0, "VST3 has no audio inputs");
                check (instance->getTotalNumOutputChannels() == 2, "VST3 has a stereo output");

                check (instance->getNumPrograms() == 101, "VST3 reports 101 programs");

                // VST3 advertises extra parameters (MIDI-CC emulation, bypass),
                // so count how many of ours survive the round-trip instead of
                // comparing totals.
                int missing = 0;
                int duplicates = 0;
                for (int i = 0; i < mk::kParamCount; ++i)
                {
                    const juce::String wanted (mk::paramSpecByIndex (i).label);
                    int matches = 0;
                    for (auto* parameter : instance->getParameters())
                        if (parameter->getName (64) == wanted)
                            ++matches;

                    if (matches == 0)
                        ++missing;
                    else if (matches > 1)
                        ++duplicates;
                }
                std::printf ("  VST3 parameter count %d (engine %d, missing %d, duplicated %d)\n",
                             (int) instance->getParameters().size(), mk::kParamCount, missing, duplicates);
                check (missing == 0, "VST3 exposes every engine parameter");
                check (duplicates == 0, "VST3 exposes no duplicated engine parameters");

                instance->prepareToPlay (48000.0, 256);

                juce::AudioBuffer<float> buffer (2, 256);
                juce::MidiBuffer hostMidi;
                hostMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);

                float peak = 0.0f;
                for (int block = 0; block < 200; ++block)
                {
                    buffer.clear();
                    instance->processBlock (buffer, hostMidi);
                    hostMidi.clear();
                    peak = std::max (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
                }

                std::printf ("  3 s through the hosted VST3: peak %.3f\n", peak);
                check (peak > 0.01f, "hosted VST3 produces audio");
                check (peak <= 1.0f, "hosted VST3 stays limited");

                instance->setCurrentProgram (1);
                check (instance->getCurrentProgram() == 1, "hosted VST3 switches programs");
                instance->releaseResources();
            }
        }

        std::printf ("\n%s\n", failures == 0 ? "SMOKE TEST PASS" : "SMOKE TEST FAIL");
    }

    return failures == 0 ? 0 : 1;
}
