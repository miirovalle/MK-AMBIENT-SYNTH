#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GatePresets.h"
#include <cmath>
#include <limits>

namespace
{
constexpr int gateStepCount = 32;
constexpr double cycleBeats[] = { 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                  1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375,
                                  1.0 / 12.0, 0.0625, 2.0 / 3.0, 1.5 };
const juce::StringArray cycleNames {
    "2 bars", "1 bar", "2 beats", "1/4", "1/8", "1/16", "1/32",
    "1/8 triplet", "1/16 triplet", "1/8 dotted", "1/16 dotted",
    "1/12 beat", "1/16 beat", "2/3 beat", "1.5 beats"
};

float onePoleCoefficient (float cutoff, double sampleRate) noexcept
{
    const auto safeCutoff = juce::jlimit (10.0f, static_cast<float> (sampleRate * 0.45), cutoff);
    return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * safeCutoff
                            / static_cast<float> (sampleRate));
}

float timeCoefficient (float milliseconds, double sampleRate) noexcept
{
    const auto seconds = juce::jmax (0.0001f, milliseconds * 0.001f);
    return 1.0f - std::exp (-1.0f / (seconds * static_cast<float> (sampleRate)));
}
}

juce::StringArray MKRattusTranceGateProcessor::getGatePresetNames()
{
    return {
        "2 Beat Gate", "1 Beat Gate", "1/2 Bt Gate", "1/3 Bt Gate", "1/4 Bt Gate",
        "1/6 Bt Gate", "1/8 Bt Gate", "1/12 Bt Gate", "1/16 Bt Gate",
        "Trance Gt 1", "Trance Gt 2", "Trance Gt 3", "Trance Gt 4",
        "Saw Gate 1", "Saw Gate 2", "1st Step", "1st Stp Fade", "2 Step Gate",
        "Off Beat", "1/4 Dynamic", "1/4 Swing", "1/4 Gate Out", "1/4 Gate In",
        "Spd Up Gate", "Slw Dn Gate", "End Fade", "End Gate", "Fade In", "Fade Out",
        "Fade Out In", "Tremelo Slw", "Tremelo Fst", "Sidechain", "Drum Loop",
        "Copter", "AM", "1/4 Classic Gate", "1/4 Trance Gate", "1/4 Offbeat Gate",
        "1/4 Sidechain Pump", "1/4 Deep Pump", "1/4 Soft Pump", "1/4 Hard Pump",
        "1/4 Alternating Gate", "1/4 Pulse Gate", "1/8 Classic Gate", "1/8 Trance Gate",
        "1/8 Offbeat Gate", "1/8 Sidechain Pump", "1/8 Deep Pump", "1/8 Soft Pump",
        "1/8 Hard Pump", "1/8 Swing Gate", "1/8 Alternating Gate", "1/8 Pulse Gate",
        "1/16 Classic Gate", "1/16 Trance Gate", "1/16 Offbeat Gate",
        "1/16 Sidechain Pump", "1/16 Deep Pump", "1/16 Soft Pump", "1/16 Hard Pump",
        "1/16 Swing Gate", "1/16 Alternating Gate", "1/16 Pulse Gate",
        "1/32 Classic Gate", "1/32 Trance Gate", "1/32 Stutter Gate",
        "1/32 Sidechain Pump", "1/32 Rapid Chopper", "1/32 Machine Gun Gate",
        "1/32 Micro Gate", "1/32 Glitch Gate", "1/32 Alternating Gate", "1/32 Pulse Gate",
        "1/8 Triplet Gate", "1/8 Triplet Trance", "1/8 Triplet Pump",
        "1/8 Triplet Chopper", "1/8 Triplet Swing", "1/8 Triplet Alternating",
        "1/8 Triplet Stutter", "1/8 Triplet Pulse", "1/8 Triplet Filter Gate",
        "1/8 Triplet Glitch", "1/16 Triplet Gate", "1/16 Triplet Trance",
        "1/16 Triplet Pump", "1/16 Triplet Chopper", "1/16 Triplet Swing",
        "1/16 Triplet Alternating", "1/16 Triplet Stutter", "1/16 Triplet Pulse",
        "1/16 Triplet Filter Gate", "1/16 Triplet Glitch", "1/8 Dotted Gate",
        "1/8 Dotted Trance", "1/8 Dotted Pump", "1/8 Dotted Chopper",
        "1/8 Dotted Swing", "1/8 Dotted Alternating", "1/8 Dotted Stutter",
        "1/8 Dotted Pulse", "1/8 Dotted Filter Gate", "1/8 Dotted Glitch",
        "1/16 Dotted Gate", "1/16 Dotted Trance", "1/16 Dotted Pump",
        "1/16 Dotted Chopper", "1/16 Dotted Swing", "1/16 Dotted Alternating",
        "1/16 Dotted Stutter", "1/16 Dotted Pulse", "1/16 Dotted Filter Gate",
        "1/16 Dotted Glitch", "Euphoric Trance Gate", "Uplifting Trance Gate",
        "Classic Supersaw Gate", "Festival Pump", "Progressive Gate", "Psytrance Gate",
        "Goa Trance Gate", "Reverse Sidechain", "Ghost Sidechain", "Rhythmic Ducking",
        "Sawtooth Gate", "Square Wave Gate", "Ramp Gate", "Fade In Gate",
        "Fade Out Gate", "Tremolo Gate", "Stereo Alternating Gate", "Ping Pong Gate",
        "Random Rhythm Gate", "Experimental Glitch Gate"
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout
MKRattusTranceGateProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    using Float = juce::AudioParameterFloat;
    using Bool = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Bool> (juce::ParameterID { "gateOn", 1 }, "Gate On", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "gatePreset", 1 }, "Gate Preset", getGatePresetNames(), 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "gateSteps", 1 }, "Gate Steps", 8, 32, 16));
    layout.add (std::make_unique<Float> (
        juce::ParameterID { "gateAttack", 1 }, "Gate Attack", Range (0.1f, 100.0f, 0.1f, 0.45f),
        2.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<Float> (
        juce::ParameterID { "gateRelease", 1 }, "Gate Release", Range (0.1f, 200.0f, 0.1f, 0.45f),
        15.0f, juce::AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "gateCycle", 1 }, "Gate Cycle", cycleNames, 2));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "gateShape", 1 }, "Gate Curve Shape",
        juce::StringArray { "Hold", "Curve", "S-curve", "Pulse", "Wave", "Triangle",
                            "Stairs", "Smooth Stairs", "Half Sine" }, 0));
    layout.add (std::make_unique<Bool> (
        juce::ParameterID { "gateRetrigger", 1 }, "Gate MIDI Retrigger", false));
    layout.add (std::make_unique<Float> (
        juce::ParameterID { "gateTension", 1 }, "Gate Curve Tension",
        Range (-1.0f, 1.0f, 0.001f), 0.0f));
    for (int step = 0; step < gateStepCount; ++step)
        layout.add (std::make_unique<Float> (juce::ParameterID { gateStepParameterID (step), 1 },
                                             "Gate Step " + juce::String (step + 1),
                                             Range (0.0f, 1.0f, 0.001f), step < 8 ? 1.0f : 0.0f));
    for (int point = 0; point < gateStepCount; ++point)
        layout.add (std::make_unique<Float> (
            juce::ParameterID { gatePositionParameterID (point), 1 },
            "Gate Point " + juce::String (point + 1) + " Time", Range (0.0f, 1.0f, 0.00001f),
            static_cast<float> (point) / 31.0f));
    layout.add (std::make_unique<Bool> (
        juce::ParameterID { "gateFilterOn", 1 }, "Gate Lowpass Filter", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "gateStereoMode", 1 }, "Gate Stereo",
        juce::StringArray { "Linked", "Alternating", "Ping Pong" }, 0));
    layout.add (std::make_unique<Float> (
        juce::ParameterID { "gateSwing", 1 }, "Gate Swing", Range (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<Float> (
        juce::ParameterID { "mix", 1 }, "Mix", Range (0.0f, 1.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<Float> (
        juce::ParameterID { "outputGain", 1 }, "Output", Range (-24.0f, 6.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

MKRattusTranceGateProcessor::MKRattusTranceGateProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MKRattusTranceGateState", createParameterLayout())
{
    applyGatePreset (0);
}

void MKRattusTranceGateProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    gatePhase = 0.0;
    smoothedGate = 0.0f;
    smoothedGateRight = 0.0f;
    gateFilterState = {};
    gatePlaybackPhase.store (0.0f);
    gateSwingSmoothing.reset (sampleRate, 0.02);
    gateSwingSmoothing.setCurrentAndTargetValue (parameters.getRawParameterValue ("gateSwing")->load());
    mixSmoothing.reset (sampleRate, 0.02);
    mixSmoothing.setCurrentAndTargetValue (parameters.getRawParameterValue ("mix")->load());
}

bool MKRattusTranceGateProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && layouts.getMainInputChannelSet() == output;
}

juce::String MKRattusTranceGateProcessor::gateStepParameterID (int step)
{
    return "gateStep" + juce::String (step + 1).paddedLeft ('0', 2);
}

juce::String MKRattusTranceGateProcessor::gatePositionParameterID (int point)
{
    return "gateX" + juce::String (point + 1).paddedLeft ('0', 2);
}

void MKRattusTranceGateProcessor::setGateStep (int step, float value)
{
    auto* parameter = parameters.getParameter (gateStepParameterID (step));
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (0.0f, 1.0f, value)));
}

float MKRattusTranceGateProcessor::getGateStep (int step) const
{
    if (step >= 0 && step < gateStepCount)
        return parameters.getRawParameterValue (gateStepParameterID (step))->load();
    return 0.0f;
}

int MKRattusTranceGateProcessor::getGateStepCount() const
{
    return juce::roundToInt (parameters.getRawParameterValue ("gateSteps")->load());
}

void MKRattusTranceGateProcessor::setGatePointCount (int count)
{
    count = juce::jlimit (8, 32, count);
    if (count != getGateStepCount())
    {
        std::vector<GatePreset::Point> points;
        const auto oldCount = getGateStepCount();
        const auto shape = juce::roundToInt (parameters.getRawParameterValue ("gateShape")->load());
        for (int i = 0; i < oldCount; ++i)
            points.push_back ({ (i + getGatePointOffset (i)) / (oldCount - 1), getGateStep (i) });
        while (static_cast<int> (points.size()) < count)
        {
            size_t longest = 0;
            for (size_t i = 1; i + 1 < points.size(); ++i)
                if (points[i + 1].x - points[i].x > points[longest + 1].x - points[longest].x)
                    longest = i;
            const auto first = points[longest], second = points[longest + 1];
            std::array<float, 32> levels {}, offsets {};
            for (size_t i = 0; i < points.size(); ++i)
            {
                levels[i] = points[i].y;
                offsets[i] = points[i].x * static_cast<float> (points.size() - 1) - static_cast<float> (i);
            }
            const auto x = (first.x + second.x) * 0.5f;
            Pattern evaluator (4);
            const auto y = evaluateGateCurve (evaluator, levels, offsets, static_cast<int> (points.size()),
                shape, x, parameters.getRawParameterValue ("gateTension")->load());
            points.insert (points.begin() + static_cast<std::ptrdiff_t> (longest + 1), { x, y });
        }
        while (static_cast<int> (points.size()) > count)
        {
            size_t leastImportant = 1;
            auto smallestError = std::numeric_limits<float>::max();
            for (size_t i = 1; i + 1 < points.size(); ++i)
            {
                const auto first = points[i - 1], current = points[i], next = points[i + 1];
                const auto predicted = shape == 0 ? first.y
                    : first.y + (next.y - first.y) * (current.x - first.x) / (next.x - first.x);
                const auto error = std::abs (predicted - current.y) * (next.x - first.x);
                if (error < smallestError) { smallestError = error; leastImportant = i; }
            }
            points.erase (points.begin() + static_cast<std::ptrdiff_t> (leastImportant));
        }
        for (int i = 0; i < count; ++i)
        {
            setGateStep (i, points[static_cast<size_t> (i)].y);
            auto* parameter = parameters.getParameter (gatePositionParameterID (i));
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (points[static_cast<size_t> (i)].x));
        }
    }
    auto* parameter = parameters.getParameter ("gateSteps");
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (count)));
    parameter->endChangeGesture();
}

float MKRattusTranceGateProcessor::getGatePointOffset (int point) const
{
    const auto count = getGateStepCount();
    if (point == 0 || point >= count - 1)
        return 0.0f;
    auto previous = 0.0f;
    // Host automation may cross points; keep the evaluator's binary search ordered.
    for (int i = 1; i <= point; ++i)
    {
        const auto requested = parameters.getRawParameterValue (gatePositionParameterID (i))->load();
        previous = juce::jlimit (previous + 0.000001f,
                                 1.0f - static_cast<float> (count - 1 - i) * 0.000001f, requested);
    }
    return previous * static_cast<float> (count - 1) - static_cast<float> (point);
}

juce::String MKRattusTranceGateProcessor::getGatePresetDescription() const
{
    const auto names = getGatePresetNames();
    return makeGatePreset (names[juce::jlimit (0, names.size() - 1, juce::roundToInt (
        parameters.getRawParameterValue ("gatePreset")->load()))]).description;
}

float MKRattusTranceGateProcessor::evaluateGateCurve (Pattern& evaluator, const std::array<float, 32>& levels,
                                                      const std::array<float, 32>& offsets,
                                                      int count, int shape, float phase, float tension)
{
    const auto x = [&offsets, count] (int point)
    {
        if (point == 0) return 0.0;
        if (point == count - 1) return 1.0;
        return (static_cast<double> (point) + offsets[static_cast<size_t> (point)])
             / static_cast<double> (count - 1);
    };
    phase = juce::jlimit (0.0f, 1.0f, phase);
    int low = 0, high = count;
    while (low <= high)
    {
        const auto segmentIndex = (low + high) / 2;
        const auto first = segmentIndex == 0 ? count - 1 : segmentIndex - 1;
        const auto second = segmentIndex == count ? 0 : segmentIndex;
        const auto x1 = segmentIndex == 0 ? x (first) - 1.0 : x (first);
        const auto x2 = segmentIndex == count ? x (second) + 1.0 : x (second);
        if (phase < x1)
            high = segmentIndex - 1;
        else if (phase > x2)
            low = segmentIndex + 1;
        else
        {
            const auto start = static_cast<size_t> (first);
            if (shape == 0)
                return levels[start];
            // Call Gate12's original segment functions without its per-sample mutex.
            if (x1 == x2)
                return levels[static_cast<size_t> (second)];
            const Segment segment { x1, x2, levels[start],
                                    levels[static_cast<size_t> (second)], tension, 0.0, shape };
            double value = levels[start];
            switch (shape)
            {
                case PointType::Curve: value = evaluator.get_y_curve (segment, phase); break;
                case PointType::SCurve: value = evaluator.get_y_scurve (segment, phase); break;
                case PointType::Pulse: value = evaluator.get_y_pulse (segment, phase); break;
                case PointType::Wave: value = evaluator.get_y_wave (segment, phase); break;
                case PointType::Triangle: value = evaluator.get_y_triangle (segment, phase); break;
                case PointType::Stairs: value = evaluator.get_y_stairs (segment, phase); break;
                case PointType::SmoothSt: value = evaluator.get_y_smooth_stairs (segment, phase); break;
                case PointType::HalfSine: value = evaluator.get_y_half_sine (segment, phase); break;
            }
            return static_cast<float> (juce::jlimit (0.0, 1.0, value));
        }
    }
    return levels[static_cast<size_t> (count - 1)];
}

double MKRattusTranceGateProcessor::swingGatePosition (double beats, float amount) noexcept
{
    if (amount == 0.0f)
        return beats;
    const auto pair = beats * 2.0;
    const auto start = std::floor (pair);
    const auto position = pair - start;
    const auto midpoint = 0.5 + static_cast<double> (amount) / 600.0;
    const auto straightPosition = position < midpoint
        ? position * 0.5 / midpoint
        : 0.5 + (position - midpoint) * 0.5 / (1.0 - midpoint);
    return (start + straightPosition) * 0.5;
}

void MKRattusTranceGateProcessor::applyGatePreset (int presetIndex)
{
    const auto names = getGatePresetNames();
    const auto boundedIndex = juce::jlimit (0, names.size() - 1, presetIndex);
    const auto preset = makeGatePreset (names[boundedIndex]);
    auto set = [this] (const juce::String& id, float value)
    {
        auto* parameter = parameters.getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    set ("gatePreset", static_cast<float> (boundedIndex));
    set ("gateSteps", static_cast<float> (preset.points.size()));
    set ("gateCycle", static_cast<float> (preset.cycle));
    set ("gateShape", static_cast<float> (preset.shape));
    set ("gateAttack", preset.attackMs);
    set ("gateRelease", preset.releaseMs);
    set ("gateTension", 0.0f);
    set ("gateFilterOn", preset.filter ? 1.0f : 0.0f);
    set ("gateStereoMode", static_cast<float> (preset.stereo));
    set ("gateOn", 1.0f);
    for (int step = 0; step < gateStepCount; ++step)
    {
        const auto& point = preset.points[juce::jmin (static_cast<size_t> (step), preset.points.size() - 1)];
        setGateStep (step, point.y);
        set (gatePositionParameterID (step), point.x);
    }
}

void MKRattusTranceGateProcessor::randomizeGate()
{
    uint32_t random = 0xa341316cu ^ static_cast<uint32_t> (juce::Time::getMillisecondCounter());
    for (int step = 0; step < gateStepCount; ++step)
    {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        const auto unit = static_cast<float> (random & 0xffffu) / 65535.0f;
        const auto value = unit < 0.42f ? 0.0f : (unit < 0.74f ? 0.35f + unit * 0.4f : 1.0f);
        setGateStep (step, value);
    }
}

void MKRattusTranceGateProcessor::updateControls (Controls& c)
{
    const auto read = [this] (const juce::String& id)
    {
        return parameters.getRawParameterValue (id)->load();
    };
    c.gateOn = read ("gateOn") > 0.5f;
    c.gateSteps = juce::jlimit (8, gateStepCount, juce::roundToInt (read ("gateSteps")));
    c.gateAttack = read ("gateAttack");
    c.gateRelease = read ("gateRelease");
    c.gateShape = juce::roundToInt (read ("gateShape"));
    c.gateTension = read ("gateTension");
    c.gateCycleBeats = cycleBeats[juce::jlimit (0, 14, juce::roundToInt (read ("gateCycle")))];
    c.gateRetrigger = read ("gateRetrigger") > 0.5f;
    c.gateFilterOn = read ("gateFilterOn") > 0.5f;
    c.gateStereoMode = juce::roundToInt (read ("gateStereoMode"));
    gateSwingSmoothing.setTargetValue (read ("gateSwing"));
    mixSmoothing.setTargetValue (read ("mix"));
    auto previousTime = 0.0f;
    for (int step = 0; step < gateStepCount; ++step)
    {
        c.gate[static_cast<size_t> (step)] = read (gateStepParameterID (step));
        if (step > 0 && step < c.gateSteps - 1)
        {
            previousTime = juce::jlimit (previousTime + 0.000001f,
                1.0f - static_cast<float> (c.gateSteps - 1 - step) * 0.000001f,
                read (gatePositionParameterID (step)));
            c.gateOffsets[static_cast<size_t> (step)] = previousTime * (c.gateSteps - 1) - step;
        }
        else
            c.gateOffsets[static_cast<size_t> (step)] = 0.0f;
    }
    c.outputGain = juce::Decibels::decibelsToGain (read ("outputGain"));
}

void MKRattusTranceGateProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto channelCount = buffer.getNumChannels();
    if (channelCount == 0)
        return;

    Controls controls;
    updateControls (controls);

    double bpm = 120.0;
    double ppq = 0.0;
    bool hasPpq = false;
    if (auto* hostPlayHead = getPlayHead())
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                if (std::isfinite (*hostBpm) && *hostBpm > 0.0)
                    bpm = *hostBpm;
            if (const auto hostPpq = position->getPpqPosition())
            {
                ppq = *hostPpq;
                hasPpq = std::isfinite (ppq);
            }
        }

    auto* left = buffer.getWritePointer (0);
    auto* right = channelCount > 1 ? buffer.getWritePointer (1) : nullptr;
    const auto gateAttackCoefficient = timeCoefficient (controls.gateAttack, currentSampleRate);
    const auto gateReleaseCoefficient = timeCoefficient (controls.gateRelease, currentSampleRate);
    auto midiEvent = midi.begin();
    const auto midiEnd = midi.end();

    for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        while (midiEvent != midiEnd && (*midiEvent).samplePosition <= sampleIndex)
        {
            if (controls.gateRetrigger && (*midiEvent).getMessage().isNoteOn())
                gatePhase = 0.0;
            ++midiEvent;
        }
        const auto dryL = left[sampleIndex];
        const auto dryR = right != nullptr ? right[sampleIndex] : dryL;
        auto l = dryL;
        auto r = dryR;

        const auto currentPpq = hasPpq && ! controls.gateRetrigger
            ? ppq + static_cast<double> (sampleIndex) * bpm / (60.0 * currentSampleRate)
            : gatePhase;
        const auto swungPpq = swingGatePosition (currentPpq, gateSwingSmoothing.getNextValue());
        auto phase = std::fmod (swungPpq / controls.gateCycleBeats, 1.0);
        if (phase < 0.0) phase += 1.0;
        gatePlaybackPhase.store (static_cast<float> (phase), std::memory_order_relaxed);
        float gateTarget = 1.0f;
        float gateTargetRight = 1.0f;
        if (controls.gateOn)
        {
            gateTarget = evaluateGateCurve (gatePattern, controls.gate, controls.gateOffsets,
                controls.gateSteps, controls.gateShape, static_cast<float> (phase), controls.gateTension);
            const auto rightPhase = controls.gateStereoMode != 0 ? std::fmod (phase + 0.5, 1.0) : phase;
            gateTargetRight = evaluateGateCurve (gatePattern, controls.gate, controls.gateOffsets,
                controls.gateSteps, controls.gateShape, static_cast<float> (rightPhase), controls.gateTension);
        }
        const auto gateCoefficient = gateTarget > smoothedGate ? gateAttackCoefficient : gateReleaseCoefficient;
        smoothedGate += gateCoefficient * (gateTarget - smoothedGate);
        if (gateTarget == 0.0f && smoothedGate < 0.00001f)
            smoothedGate = 0.0f;
        smoothedGateRight += (gateTargetRight > smoothedGateRight ? gateAttackCoefficient : gateReleaseCoefficient)
                           * (gateTargetRight - smoothedGateRight);
        if (gateTargetRight == 0.0f && smoothedGateRight < 0.00001f)
            smoothedGateRight = 0.0f;
        if (controls.gateOn && controls.gateFilterOn)
            for (int channel = 0; channel < 2; ++channel)
            {
                auto& value = channel == 0 ? l : r;
                const auto envelope = channel == 0 ? smoothedGate : smoothedGateRight;
                const auto coefficient = onePoleCoefficient (30.0f + 18000.0f * envelope * envelope, currentSampleRate);
                auto& states = gateFilterState[static_cast<size_t> (channel)];
                states[0] += coefficient * (value - states[0]);
                states[1] += coefficient * (states[0] - states[1]);
                value = states[1];
            }
        l *= smoothedGate * controls.outputGain;
        r *= smoothedGateRight * controls.outputGain;
        const auto mix = mixSmoothing.getNextValue();
        l = dryL + mix * (l - dryL);
        r = dryR + mix * (r - dryR);

        left[sampleIndex] = right != nullptr ? l : (l + r) * 0.5f;
        if (right != nullptr)
            right[sampleIndex] = r;
        gatePhase += bpm / (60.0 * currentSampleRate);
    }
    midi.clear();
}

void MKRattusTranceGateProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void MKRattusTranceGateProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MKRattusTranceGateProcessor::createEditor()
{
    return new MKRattusTranceGateEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MKRattusTranceGateProcessor();
}
