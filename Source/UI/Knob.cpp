#include "Knob.h"

namespace mk::ui
{

namespace
{
constexpr float kStartAngle = -2.44346f;  // -140 degrees
constexpr float kAngleRange = 4.88692f;   // 280 degrees
constexpr float kDragPixels = 210.0f;

inline float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
}

Knob::Knob (juce::RangedAudioParameter& parameter, juce::String labelText, bool bipolar)
    : param (parameter),
      attachment (parameter,
                  [this] (float newValue)
                  {
                      normalised = clamp01 (param.convertTo0to1 (newValue));
                      repaint();
                  }),
      label (std::move (labelText)),
      bipolarKnob (bipolar)
{
    // AudioProcessorParameter::getValue()/getText() work in the normalised
    // (0..1) domain; the attachment callbacks use the denormalised domain.
    normalised = clamp01 (param.getValue());
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Knob::setKnobLabel (const juce::String& newLabel)
{
    label = newLabel;
    repaint();
}

void Knob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float labelHeight = 15.0f;
    const float valueHeight = 14.0f;
    const float diameter = juce::jmin (bounds.getWidth() - 6.0f, bounds.getHeight() - labelHeight - valueHeight - 6.0f);

    auto knobArea = juce::Rectangle<float> (diameter, diameter)
                        .withCentre ({ bounds.getCentreX(), bounds.getY() + 3.0f + diameter * 0.5f });

    const auto centre = knobArea.getCentre();
    const float radius = diameter * 0.5f;

    // Rear bezel
    g.setColour (col::panelDeep.withAlpha (0.85f));
    g.fillEllipse (knobArea.reduced (-1.5f));

    // Track
    {
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                             kStartAngle, kStartAngle + kAngleRange, true);
        g.setColour (col::lineSoft.withAlpha (0.85f));
        g.strokePath (track, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Value arc
    {
        juce::Path arc;
        if (bipolarKnob)
        {
            const float mid = kStartAngle + kAngleRange * 0.5f;
            const float angle = kStartAngle + kAngleRange * normalised;
            const float from = juce::jmin (mid, angle);
            const float to = juce::jmax (mid, angle);
            if (to - from > 0.001f)
                arc.addCentredArc (centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f, from, to, true);
        }
        else
        {
            arc.addCentredArc (centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                               kStartAngle, kStartAngle + kAngleRange * normalised, true);
        }

        g.setColour (isMouseOver() || dragging ? col::accentBright : col::accent);
        g.strokePath (arc, juce::PathStrokeType (2.8f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Knob body
    auto body = knobArea.reduced (radius * 0.20f);
    {
        juce::ColourGradient bodyGradient (col::panelRaised, body.getCentreX(), body.getY(),
                                           col::panelDeep, body.getCentreX(), body.getBottom(), false);
        g.setGradientFill (bodyGradient);
        g.fillEllipse (body);
        g.setColour (col::lineSoft);
        g.drawEllipse (body, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (body.reduced (1.5f), 1.0f);
    }

    // Pointer
    {
        const float angle = kStartAngle + kAngleRange * normalised;
        const float pointerLength = body.getWidth() * 0.34f;
        const juce::Point<float> tip (centre.x + std::sin (angle) * pointerLength,
                                      centre.y - std::cos (angle) * pointerLength);
        const juce::Point<float> inner (centre.x + std::sin (angle) * pointerLength * 0.25f,
                                        centre.y - std::cos (angle) * pointerLength * 0.25f);
        g.setColour (col::accentBright);
        g.drawLine ({ inner, tip }, 2.2f);
    }

    // Label + value
    auto footer = bounds.removeFromBottom (labelHeight + valueHeight);
    auto valueArea = footer.removeFromTop (valueHeight);

    g.setColour (isMouseOver() || dragging ? col::text : col::textDim);
    g.setFont (font (11.5f));
    g.drawText (param.getText (normalised, 10), valueArea.toNearestInt(),
                juce::Justification::centred, false);

    g.setColour (col::textDim);
    g.setFont (font (11.0f));
    g.drawText (label, footer.toNearestInt(), juce::Justification::centred, false);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    dragStartY = e.position.y;
    dragStartNormalised = normalised;
    dragging = true;
    attachment.beginGesture();
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const float scale = e.mods.isShiftDown() ? 0.22f : 1.0f;
    const float delta = (dragStartY - e.position.y) / kDragPixels * scale;
    setNormalised (dragStartNormalised + delta, true);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        attachment.endGesture();
        repaint();
    }
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;
    setNormalised (normalised + delta * 0.35f, false);
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (normalised));
}

void Knob::setNormalised (float newNormalised, bool partOfGesture)
{
    normalised = clamp01 (newNormalised);
    const float denorm = param.convertFrom0to1 (normalised);
    if (partOfGesture)
        attachment.setValueAsPartOfGesture (denorm);
    else
        attachment.setValueAsCompleteGesture (denorm);
    repaint();
}

// ---------------------------------------------------------------------------
// ChoiceButton
// ---------------------------------------------------------------------------

ChoiceButton::ChoiceButton (juce::RangedAudioParameter& parameter, juce::String labelText)
    : param (parameter),
      attachment (parameter,
                  [this] (float newValue)
                  {
                      normalised = clamp01 (param.convertTo0to1 (newValue));
                      repaint();
                  }),
      label (std::move (labelText))
{
    normalised = clamp01 (param.getValue());
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ChoiceButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (hover ? col::panelRaised.brighter (0.08f) : col::panelRaised);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (col::lineSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    g.setColour (col::textFaint);
    g.setFont (font (9.5f));
    g.drawText (label, bounds.removeFromTop (bounds.getHeight() * 0.42f).reduced (7.0f, 2.0f).toNearestInt(),
                juce::Justification::centredLeft, false);

    g.setColour (col::text);
    g.setFont (font (12.5f));
    g.drawText (param.getCurrentValueAsText(),
                bounds.reduced (7.0f, 1.0f).toNearestInt(),
                juce::Justification::centredLeft, false);

    // Chevron
    const float cx = getWidth() - 12.0f;
    const float cy = getHeight() * 0.62f;
    g.setColour (col::accent);
    g.drawLine (cx - 4.0f, cy - 2.0f, cx, cy + 2.5f, 1.6f);
    g.drawLine (cx, cy + 2.5f, cx + 4.0f, cy - 2.0f, 1.6f);
}

void ChoiceButton::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
        return;
    showMenu();
}

void ChoiceButton::showMenu()
{
    const auto items = param.getAllValueStrings();
    juce::PopupMenu menu;

    // Group "Category: Item" labels into submenus (used by the texture library).
    juce::String currentCategory;
    juce::PopupMenu categoryMenu;

    const float currentDenorm = param.convertFrom0to1 (normalised);

    auto flushCategory = [&]
    {
        if (currentCategory.isNotEmpty())
        {
            menu.addSubMenu (currentCategory, categoryMenu);
            categoryMenu.clear();
            currentCategory.clear();
        }
    };

    for (int i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const int colon = item.indexOf (": ");

        if (colon > 0)
        {
            const auto category = item.substring (0, colon);
            if (category != currentCategory)
            {
                flushCategory();
                currentCategory = category;
                categoryMenu.setLookAndFeel (&getLookAndFeel());
            }
            categoryMenu.addItem (i + 1, item.substring (colon + 2), true,
                                  std::abs (currentDenorm - (float) i) < 0.5f);
        }
        else
        {
            flushCategory();
            menu.addItem (i + 1, item, true, std::abs (currentDenorm - (float) i) < 0.5f);
        }
    }
    flushCategory();

    auto options = juce::PopupMenu::Options()
                       .withTargetComponent (this)
                       .withMinimumWidth (juce::jmax (180, getWidth()))
                       .withStandardItemHeight (30);

    menu.showMenuAsync (options, [this] (int result)
    {
        if (result > 0)
            attachment.setValueAsCompleteGesture ((float) (result - 1));
    });
}

// ---------------------------------------------------------------------------
// SimpleButton
// ---------------------------------------------------------------------------

SimpleButton::SimpleButton (juce::String buttonText, std::function<void()> onPress)
    : text (std::move (buttonText)), onClick (std::move (onPress))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SimpleButton::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    auto base = active ? col::accent.withAlpha (0.22f)
                       : (hover ? col::panelRaised.brighter (0.07f) : col::panelRaised);
    if (pressed)
        base = base.darker (0.2f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (active ? col::accent : col::lineSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, active ? 1.4f : 1.0f);

    g.setColour (active ? col::accentBright : (hover ? col::text : col::textDim));
    g.setFont (font (13.0f, active));
    g.drawText (text, bounds.toNearestInt(), juce::Justification::centred, false);
}

void SimpleButton::mouseUp (const juce::MouseEvent&)
{
    const bool wasPressed = pressed;
    pressed = false;
    repaint();
    if (wasPressed && onClick)
        onClick();
}

// ---------------------------------------------------------------------------
// Meter
// ---------------------------------------------------------------------------

void Meter::setLevel (float newLevel)
{
    newLevel = juce::jlimit (0.0f, 1.0f, newLevel);
    if (std::abs (newLevel - level) > 0.002f)
    {
        level = newLevel;
        repaint();
    }
}

void Meter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (col::panelDeep);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (col::lineSoft);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // Meter scale in dB: -48 dB .. 0 dB
    auto inner = bounds.reduced (2.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float proportion = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);

    auto filled = inner;
    filled = filled.removeFromLeft (inner.getWidth() * proportion);

    juce::ColourGradient grad (col::led, inner.getX(), inner.getY(),
                               col::accent, inner.getRight(), inner.getY(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (filled, 2.0f);
}

} // namespace mk::ui
