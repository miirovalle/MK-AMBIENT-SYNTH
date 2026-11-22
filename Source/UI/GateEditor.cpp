#include "GateEditor.h"

#include "Knob.h"

namespace mk::ui
{

namespace
{
constexpr float kNodeRadius = 8.0f;
constexpr float kHitRadius = 26.0f;   // generous for fingers

juce::Colour curveColour() { return col::accent; }
}

// ---------------------------------------------------------------------------
// Pattern strip: eight factory shapes
// ---------------------------------------------------------------------------

class GateEditor::PatternStrip : public juce::Component
{
public:
    explicit PatternStrip (std::function<void (int)> onPick) : callback (std::move (onPick))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        const int count = (int) shapes.size();
        const float cellWidth = area.getWidth() / (float) count;

        for (int i = 0; i < count; ++i)
        {
            auto cell = juce::Rectangle<float> (area.getX() + cellWidth * (float) i, area.getY(),
                                                cellWidth, area.getHeight()).reduced (3.0f, 2.0f);

            const bool hot = (i == hoverIndex);
            g.setColour (hot ? col::panelRaised.brighter (0.10f) : col::panelDeep);
            g.fillRoundedRectangle (cell, 4.0f);
            g.setColour (hot ? col::accent : col::lineSoft);
            g.drawRoundedRectangle (cell.reduced (0.5f), 4.0f, 1.0f);

            juce::Path path;
            const auto& shape = shapes[(size_t) i];
            for (int s = 0; s < mk::kGateSteps; ++s)
            {
                const float x = cell.getX() + 4.0f + (cell.getWidth() - 8.0f) * ((float) s + 0.5f) / (float) mk::kGateSteps;
                const float y = cell.getBottom() - 4.0f - (cell.getHeight() - 8.0f) * shape[(size_t) s];

                if (s == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);
            }

            g.setColour (hot ? col::accentBright : col::textDim);
            g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int index = indexAt (e.position.x);
        if (index != hoverIndex)
        {
            hoverIndex = index;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverIndex = -1;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown())
            return;
        const int index = indexAt (e.position.x);
        if (index >= 0 && callback)
            callback (index);
    }

    static std::array<float, (size_t) mk::kGateSteps> shapeAt (int index)
    {
        return shapes[(size_t) juce::jlimit (0, (int) shapes.size() - 1, index)];
    }

    static int shapeCount() { return (int) shapes.size(); }

private:
    int indexAt (float x) const
    {
        if (getWidth() <= 0)
            return -1;
        const int index = (int) (x / ((float) getWidth() / (float) shapes.size()));
        return juce::jlimit (0, (int) shapes.size() - 1, index);
    }

    std::function<void (int)> callback;
    int hoverIndex = -1;

    static const inline std::vector<std::array<float, (size_t) mk::kGateSteps>> shapes = {
        // ramp up
        { 0.08f, 0.16f, 0.25f, 0.33f, 0.42f, 0.50f, 0.58f, 0.67f, 0.75f, 0.83f, 0.92f, 1.00f },
        // ramp down
        { 1.00f, 0.92f, 0.83f, 0.75f, 0.67f, 0.58f, 0.50f, 0.42f, 0.33f, 0.25f, 0.16f, 0.08f },
        // triangle
        { 0.20f, 0.40f, 0.60f, 0.80f, 1.00f, 0.80f, 0.60f, 0.40f, 0.20f, 0.10f, 0.05f, 0.02f },
        // square (all on)
        { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
        // classic 16th gate
        { 1.00f, 0.00f, 0.85f, 0.00f, 1.00f, 0.00f, 0.70f, 0.00f, 1.00f, 0.00f, 0.85f, 0.00f },
        // alternating
        { 1.00f, 0.30f, 1.00f, 0.30f, 1.00f, 0.30f, 1.00f, 0.30f, 1.00f, 0.30f, 1.00f, 0.30f },
        // sine
        { 0.50f, 0.75f, 0.93f, 1.00f, 0.93f, 0.75f, 0.50f, 0.25f, 0.07f, 0.00f, 0.07f, 0.25f },
        // stutter (multi peak)
        { 1.00f, 0.10f, 0.70f, 0.10f, 1.00f, 0.45f, 1.00f, 0.10f, 0.60f, 0.10f, 0.90f, 0.25f }
    };
};

// ---------------------------------------------------------------------------
// Small toolbar button
// ---------------------------------------------------------------------------

class GateEditor::ToolButton : public juce::Component
{
public:
    ToolButton (juce::String text, std::function<void()> onPress)
        : label (std::move (text)), callback (std::move (onPress))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setActive (bool shouldBeActive)
    {
        if (active != shouldBeActive)
        {
            active = shouldBeActive;
            repaint();
        }
    }

    void setEnabled (bool shouldBeEnabled)
    {
        enabled = shouldBeEnabled;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        auto base = active ? col::accent.withAlpha (0.22f) : col::panelRaised;
        if (! enabled)
            base = base.withAlpha (0.35f);
        if (hover && enabled)
            base = base.brighter (0.08f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (active && enabled ? col::accent : col::lineSoft);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, active ? 1.4f : 1.0f);

        g.setColour (enabled ? (active ? col::accentBright : col::text) : col::textFaint);
        g.setFont (font (11.5f, active));
        g.drawText (label, bounds.toNearestInt(), juce::Justification::centred, false);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (enabled && ! e.mouseWasDraggedSinceMouseDown() && callback)
            callback();
    }

private:
    juce::String label;
    std::function<void()> callback;
    bool active = false, hover = false, enabled = true;
};

// ---------------------------------------------------------------------------
// GateEditor
// ---------------------------------------------------------------------------

GateEditor::GateEditor (juce::AudioProcessorValueTreeState& state, GateHostContext context)
    : apvts (state), host (std::move (context))
{
    auto get = [this] (mk::P id) { return apvts.getParameter (mk::paramSpec (id).key); };
    for (int i = 0; i < mk::kGateSteps; ++i)
        controls.steps[i] = get (mk::gateStepParam (i));
    controls.minLevel = get (mk::P::GateMin);
    controls.maxLevel = get (mk::P::GateMax);
    controls.attack = get (mk::P::GateAttack);
    controls.release = get (mk::P::GateRelease);
    controls.tension = get (mk::P::GateTension);
    controls.phase = get (mk::P::GatePhase);
    controls.mix = get (mk::P::GateMix);
    controls.trigger = get (mk::P::GateTrigger);
    controls.length = get (mk::P::GateLength);
    controls.rate = get (mk::P::GateRate);

    patterns = std::make_unique<PatternStrip> ([this] (int index) { loadPattern (index); });
    addAndMakeVisible (*patterns);

    auto addTool = [this] (const juce::String& text, std::function<void()> fn) -> ToolButton*
    {
        auto* button = new ToolButton (text, std::move (fn));
        tools.emplace_back (button);
        addAndMakeVisible (button);
        return button;
    };

    addTool ("POINT", [this] { paintMode = false; updateUndoButtons(); repaint(); })->setActive (true);
    addTool ("PAINT", [this] { paintMode = true; updateUndoButtons(); repaint(); });
    addTool ("SNAP", [this] { snapToGrid = ! snapToGrid; updateUndoButtons(); repaint(); })->setActive (true);
    addTool ("MIN", [this] { setAll (0.0f); });
    addTool ("MAX", [this] { setAll (1.0f); });
    addTool ("FLIP X", [this] { flipX(); });
    addTool ("RANDOM", [this] { randomise(); });
    addTool ("UNDO", [this] { undo(); });
    addTool ("REDO", [this] { redo(); });

    std::array<float, (size_t) mk::kGateSteps> initial {};
    readSteps (initial);
    undoStack.push_back (initial);
    undoIndex = 0;
    updateUndoButtons();

    startTimerHz (30);
}

GateEditor::~GateEditor()
{
    stopTimer();
    if (gestureActive)
        for (auto* param : controls.steps)
            if (param != nullptr)
                param->endChangeGesture();
}

void GateEditor::timerCallback()
{
    const float phase = host.phase ? host.phase() : 0.0f;
    const float level = host.level ? host.level() : 1.0f;
    const bool running = controls.mix != nullptr && controls.mix->getValue() > 0.0001f;

    if (std::abs (phase - displayPhase) > 0.0005f || std::abs (level - displayLevel) > 0.002f || running != engineRunning)
    {
        displayPhase = phase;
        displayLevel = level;
        engineRunning = running;
        repaint();
    }
}

mk::GateSettings GateEditor::currentSettings() const
{
    mk::GateSettings settings;

    auto value = [] (const juce::RangedAudioParameter* parameter, float fallback)
    {
        return parameter != nullptr ? parameter->convertFrom0to1 (parameter->getValue()) : fallback;
    };

    settings.minLevel = value (controls.minLevel, 0.0f);
    settings.maxLevel = value (controls.maxLevel, 1.0f);
    settings.attackSeconds = value (controls.attack, 0.02f);
    settings.releaseSeconds = value (controls.release, 0.05f);
    settings.tension = value (controls.tension, 0.0f);
    settings.phase01 = value (controls.phase, 0.0f);
    settings.mix = value (controls.mix, 0.0f);
    settings.triggerMode = (int) std::round (value (controls.trigger, 0.0f));
    settings.lengthIndex = (int) std::round (value (controls.length, 1.0f));
    settings.freeRateHz = value (controls.rate, 2.0f);
    settings.bpm = host.tempo ? host.tempo() : 120.0;

    for (int i = 0; i < mk::kGateSteps; ++i)
        settings.steps[(size_t) i] = value (controls.steps[i], 1.0f);

    return settings;
}

float GateEditor::stepSamplesFor (const mk::GateSettings& settings, double sampleRate) const
{
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;

    if (settings.triggerMode == 0)
    {
        const double bpm = settings.bpm > 1.0 ? settings.bpm : 120.0;
        const double beats = (double) mk::gateLengthInBeats (settings.lengthIndex);
        return (float) (beats * 60.0 / bpm * sr / (double) mk::kGateSteps);
    }

    const float rate = juce::jlimit (0.02f, 40.0f, settings.freeRateHz);
    return (float) sr / (rate * (float) mk::kGateSteps);
}

juce::Rectangle<float> GateEditor::plotArea() const
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    area.removeFromTop (94.0f); // pattern strip + toolbar
    return area;
}

float GateEditor::valueToY (float value, juce::Rectangle<float> area) const
{
    return area.getBottom() - juce::jlimit (0.0f, 1.0f, value) * area.getHeight();
}

float GateEditor::yToValue (float y, juce::Rectangle<float> area) const
{
    return juce::jlimit (0.0f, 1.0f, (area.getBottom() - y) / juce::jmax (1.0f, area.getHeight()));
}

int GateEditor::columnAt (float x) const
{
    const auto area = plotArea();
    const float columnWidth = area.getWidth() / (float) mk::kGateSteps;
    if (columnWidth <= 0.0f)
        return -1;
    return juce::jlimit (0, mk::kGateSteps - 1, (int) ((x - area.getX()) / columnWidth));
}

float GateEditor::snapValue (float value) const
{
    if (! snapToGrid)
        return juce::jlimit (0.0f, 1.0f, value);

    constexpr float steps = 16.0f;
    return juce::jlimit (0.0f, 1.0f, std::round (value * steps) / steps);
}

void GateEditor::readSteps (std::array<float, (size_t) mk::kGateSteps>& out) const
{
    for (int i = 0; i < mk::kGateSteps; ++i)
        out[(size_t) i] = controls.steps[i] != nullptr
                              ? controls.steps[i]->convertFrom0to1 (controls.steps[i]->getValue())
                              : 1.0f;
}

void GateEditor::writeStep (int index, float value)
{
    if (index < 0 || index >= mk::kGateSteps)
        return;

    auto* parameter = controls.steps[index];
    if (parameter == nullptr)
        return;

    const float clamped = juce::jlimit (0.0f, 1.0f, value);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (clamped));
}

void GateEditor::writeSteps (const std::array<float, (size_t) mk::kGateSteps>& values)
{
    pushUndo();

    for (int i = 0; i < mk::kGateSteps; ++i)
    {
        if (auto* parameter = controls.steps[i])
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (juce::jlimit (0.0f, 1.0f, values[(size_t) i])));
            parameter->endChangeGesture();
        }
    }

    repaint();
}

void GateEditor::pushUndo()
{
    std::array<float, (size_t) mk::kGateSteps> current {};
    readSteps (current);

    if (undoIndex >= 0 && undoStack[(size_t) undoIndex] == current)
        return;

    // Drop the redo tail.
    if (undoIndex + 1 < (int) undoStack.size())
        undoStack.erase (undoStack.begin() + undoIndex + 1, undoStack.end());

    undoStack.push_back (current);
    if (undoStack.size() > 32)
        undoStack.erase (undoStack.begin());

    undoIndex = (int) undoStack.size() - 1;
    updateUndoButtons();
}

void GateEditor::undo()
{
    if (undoIndex <= 0)
        return;

    --undoIndex;
    const auto values = undoStack[(size_t) undoIndex];

    for (int i = 0; i < mk::kGateSteps; ++i)
        writeStep (i, values[(size_t) i]);

    updateUndoButtons();
    repaint();
}

void GateEditor::redo()
{
    if (undoIndex + 1 >= (int) undoStack.size())
        return;

    ++undoIndex;
    const auto values = undoStack[(size_t) undoIndex];

    for (int i = 0; i < mk::kGateSteps; ++i)
        writeStep (i, values[(size_t) i]);

    updateUndoButtons();
    repaint();
}

void GateEditor::updateUndoButtons()
{
    if (tools.size() >= 9)
    {
        tools[0]->setActive (! paintMode);
        tools[1]->setActive (paintMode);
        tools[2]->setActive (snapToGrid);
        tools[7]->setEnabled (undoIndex > 0);
        tools[8]->setEnabled (undoIndex + 1 < (int) undoStack.size());
    }
}

void GateEditor::loadPattern (int index)
{
    writeSteps (PatternStrip::shapeAt (index));
}

void GateEditor::flipX()
{
    std::array<float, (size_t) mk::kGateSteps> values {};
    readSteps (values);
    std::reverse (values.begin(), values.end());
    writeSteps (values);
}

void GateEditor::randomise()
{
    juce::Random random;
    std::array<float, (size_t) mk::kGateSteps> values {};

    // Keep it musical: mostly on/off with a few intermediate levels.
    for (auto& value : values)
    {
        const float r = random.nextFloat();
        value = r < 0.45f ? 1.0f : (r < 0.7f ? random.nextFloat() * 0.7f : random.nextFloat() * 0.25f);
    }

    writeSteps (values);
}

void GateEditor::setAll (float value)
{
    std::array<float, (size_t) mk::kGateSteps> values {};
    values.fill (juce::jlimit (0.0f, 1.0f, value));
    writeSteps (values);
}

// ---------------------------------------------------------------------------

void GateEditor::resized()
{
    auto area = getLocalBounds().reduced (10, 8);

    auto strip = area.removeFromTop (44);
    if (patterns != nullptr)
        patterns->setBounds (strip);

    area.removeFromTop (4);
    auto toolsRow = area.removeFromTop (30);

    const int widths[] = { 62, 62, 58, 48, 48, 66, 74, 58, 58 };
    int x = toolsRow.getX();
    for (size_t i = 0; i < tools.size(); ++i)
    {
        const int w = widths[juce::jmin (i, sizeof (widths) / sizeof (widths[0]) - 1)];
        tools[i]->setBounds (x, toolsRow.getY(), w, toolsRow.getHeight());
        x += w + 5;
    }
}

void GateEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    drawPanelBackground (g, bounds);

    auto title = getLocalBounds().reduced (12, 7).removeFromTop (16);
    g.setColour (col::accent);
    g.setFont (font (12.0f, true));
    g.drawText ("STEP SEQUENCER", title, juce::Justification::centredLeft, false);

    auto status = getLocalBounds().reduced (12, 7).removeFromTop (16);
    g.setColour (engineRunning ? col::accentBright : col::textFaint);
    g.setFont (font (11.0f));
    const juce::String timing = controls.trigger != nullptr && std::round (controls.trigger->convertFrom0to1 (controls.trigger->getValue())) > 0
                                    ? "FREE"
                                    : juce::String (juce::roundToInt (host.tempo ? host.tempo() : 120.0)) + " BPM";
    g.drawText (engineRunning ? ("RUNNING  " + timing) : "BYPASSED  " + timing, status,
                juce::Justification::centredRight, false);

    const auto area = plotArea();
    const auto settings = currentSettings();
    const double sr = host.sampleRate ? host.sampleRate() : 48000.0;
    const float stepSamples = stepSamplesFor (settings, sr);

    // Plot background and grid.
    g.setColour (col::panelDeep);
    g.fillRoundedRectangle (area, 6.0f);

    const float columnWidth = area.getWidth() / (float) mk::kGateSteps;

    for (int i = 0; i <= mk::kGateSteps; ++i)
    {
        const float x = area.getX() + columnWidth * (float) i;
        const bool beat = (i % 3) == 0; // bar / beat-ish accents for 12 steps
        g.setColour (col::lineSoft.withAlpha (beat ? 0.55f : 0.22f));
        g.drawLine (x, area.getY(), x, area.getBottom(), beat ? 1.2f : 0.8f);
    }

    for (int i = 0; i <= 4; ++i)
    {
        const float y = area.getY() + area.getHeight() * (float) i / 4.0f;
        g.setColour (col::lineSoft.withAlpha (i == 4 || i == 0 ? 0.5f : 0.2f));
        g.drawLine (area.getX(), y, area.getRight(), y, 0.8f);
    }

    // Step bars.
    for (int i = 0; i < mk::kGateSteps; ++i)
    {
        const float level = mk::gateStepLevel (settings.steps[(size_t) i], settings.minLevel, settings.maxLevel);
        const float x = area.getX() + columnWidth * (float) i;
        auto bar = juce::Rectangle<float> (x + columnWidth * 0.16f,
                                           valueToY (level, area),
                                           columnWidth * 0.68f,
                                           area.getBottom() - valueToY (level, area));
        g.setColour (col::accent.withAlpha (0.16f + 0.14f * level));
        g.fillRect (bar);
    }

    // Envelope curve from the DSP maths.
    const int points = juce::jmax (64, (int) area.getWidth());
    juce::Path curve;
    juce::Path filled;

    for (int i = 0; i <= points; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) points;
        const float stepPosition = (float) mk::kGateSteps * (float) i / (float) points;
        const float value = mk::gateEnvelopeAt (settings, stepPosition, stepSamples, sr);
        const float y = valueToY (value, area);

        if (i == 0)
        {
            curve.startNewSubPath (x, y);
            filled.startNewSubPath (x, area.getBottom());
            filled.lineTo (x, y);
        }
        else
        {
            curve.lineTo (x, y);
            filled.lineTo (x, y);
        }
    }
    filled.lineTo (area.getRight(), area.getBottom());
    filled.closeSubPath();

    g.setColour (col::accent.withAlpha (0.10f));
    g.fillPath (filled);

    g.setColour (curveColour());
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Step handles at the centre of each column.
    for (int i = 0; i < mk::kGateSteps; ++i)
    {
        const float level = mk::gateStepLevel (settings.steps[(size_t) i], settings.minLevel, settings.maxLevel);
        const float x = area.getX() + columnWidth * ((float) i + 0.5f);
        const float y = valueToY (level, area);
        const juce::Point<float> centre (x, y);

        const bool hot = (i == dragColumn);
        g.setColour (col::panelDeep.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (kNodeRadius * 2.0f, kNodeRadius * 2.0f).withCentre (centre));
        g.setColour (hot ? col::accentBright : col::text);
        g.drawEllipse (juce::Rectangle<float> (kNodeRadius * 2.0f, kNodeRadius * 2.0f).withCentre (centre), 1.6f);

        if (hot)
        {
            g.setColour (col::accent.withAlpha (0.55f));
            g.fillEllipse (juce::Rectangle<float> (kNodeRadius * 1.1f, kNodeRadius * 1.1f).withCentre (centre));
        }
    }

    // Playhead.
    if (engineRunning)
    {
        const float x = area.getX() + area.getWidth() * juce::jlimit (0.0f, 0.999f, displayPhase);
        g.setColour (col::led.withAlpha (0.85f));
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.6f);

        const float dotY = valueToY (mk::gateStepLevel (displayLevel, 0.0f, 1.0f), area);
        g.setColour (col::led);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, dotY }));
    }

    g.setColour (col::lineSoft);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);
}

// ---------------------------------------------------------------------------

void GateEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto area = plotArea();
    if (! area.contains (e.position))
        return;

    const int column = columnAt (e.position.x);
    if (column < 0)
        return;

    dragColumn = column;
    lastPaintedColumn = column;

    if (paintMode)
    {
        dragStartValue = snapValue (yToValue (e.position.y, area));
        writeStep (column, dragStartValue);
    }
    else
    {
        // Point mode: jump to the pointer's value on the pressed column.
        dragStartValue = snapValue (yToValue (e.position.y, area));
        writeStep (column, dragStartValue);

        if (auto* parameter = controls.steps[column])
            parameter->beginChangeGesture();
        gestureActive = true;
    }

    repaint();
}

void GateEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragColumn < 0)
        return;

    const auto area = plotArea();
    const float value = snapValue (yToValue (e.position.y, area));
    const int column = columnAt (e.position.x);

    if (paintMode)
    {
        // Paint every column crossed on the way, so a fast swipe still fills.
        if (column >= 0 && column != lastPaintedColumn)
        {
            const int direction = column > lastPaintedColumn ? 1 : -1;
            for (int c = lastPaintedColumn; c != column; c += direction)
                writeStep (c, dragStartValue);
            lastPaintedColumn = column;
        }
        writeStep (column < 0 ? dragColumn : column, dragStartValue);
    }
    else
    {
        writeStep (dragColumn, value);
    }

    repaint();
}

void GateEditor::mouseUp (const juce::MouseEvent&)
{
    if (gestureActive)
    {
        for (auto* parameter : controls.steps)
            if (parameter != nullptr)
                parameter->endChangeGesture();
        gestureActive = false;
    }

    if (dragColumn >= 0)
        pushUndo();

    dragColumn = -1;
    lastPaintedColumn = -1;
    repaint();
}

void GateEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int column = columnAt (e.position.x);
    if (column < 0 || ! plotArea().contains (e.position))
        return;

    pushUndo();
    writeStep (column, 1.0f);
    repaint();
}

// ---------------------------------------------------------------------------

} // namespace mk::ui
