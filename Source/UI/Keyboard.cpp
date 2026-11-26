#include "Keyboard.h"

#include <cmath>

namespace mk::ui
{

namespace
{
constexpr float kVelocity = 0.85f;

inline float blackKeyHeight (juce::Rectangle<float> keys)
{
    return keys.getHeight() * 0.62f;
}
} // namespace

Keyboard::Keyboard (KeyboardHost hostToUse)
    : host (std::move (hostToUse))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

Keyboard::~Keyboard()
{
    releaseAllNotes();
}

bool Keyboard::isBlackKey (int note) noexcept
{
    switch (note % 12)
    {
        case 1: case 3: case 6: case 8: case 10: return true;
        default: return false;
    }
}

juce::String Keyboard::nameForNote (int note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int octave = note / 12 - 1;
    return juce::String (names[note % 12]) + juce::String (octave);
}

int Keyboard::whiteKeyIndex (int note) const
{
    int index = 0;
    for (int n = kLowestNote; n < note; ++n)
        if (! isBlackKey (n))
            ++index;
    return index;
}

int Keyboard::whiteKeyCount() const
{
    return whiteKeyIndex (kHighestNote + 1);
}

int Keyboard::noteForWhiteIndex (int index) const
{
    int count = 0;
    for (int note = kLowestNote; note <= kHighestNote; ++note)
    {
        if (isBlackKey (note))
            continue;
        if (count == index)
            return note;
        ++count;
    }
    return -1;
}

float Keyboard::contentWidth() const
{
    return (float) whiteKeyCount() * (float) kWhiteKeyWidth;
}

juce::Rectangle<float> Keyboard::keysArea() const
{
    auto bounds = getLocalBounds().toFloat();
    bounds.removeFromBottom ((float) kScrollBarHeight);
    return bounds.reduced (1.0f, 1.0f);
}

float Keyboard::maximumScroll() const
{
    return juce::jmax (0.0f, contentWidth() - keysArea().getWidth());
}

juce::Rectangle<float> Keyboard::thumbArea() const
{
    auto track = getLocalBounds().toFloat().removeFromBottom ((float) kScrollBarHeight).reduced (1.0f, 2.0f);
    const float total = juce::jmax (1.0f, contentWidth());
    const float proportion = juce::jlimit (0.08f, 1.0f, keysArea().getWidth() / total);
    const float thumbWidth = juce::jmax (36.0f, track.getWidth() * proportion);
    const float travel = juce::jmax (0.0f, track.getWidth() - thumbWidth);
    const float position = maximumScroll() > 0.0f ? scroll / maximumScroll() : 0.0f;
    return { track.getX() + travel * position, track.getY(), thumbWidth, track.getHeight() };
}

int Keyboard::noteAt (juce::Point<float> position) const
{
    const auto keys = keysArea();
    if (position.x < keys.getX() || position.x > keys.getRight())
        return -1;

    const float x = position.x - keys.getX() + scroll;
    const float blackHeight = blackKeyHeight (keys);
    const bool inBlackZone = position.y <= keys.getY() + blackHeight;

    if (inBlackZone)
    {
        for (int note = kLowestNote; note <= kHighestNote; ++note)
        {
            if (! isBlackKey (note))
                continue;

            const float centre = (float) whiteKeyIndex (note) * (float) kWhiteKeyWidth;
            if (std::abs (x - centre) <= (float) kWhiteKeyWidth * 0.32f)
                return note;
        }
    }

    return noteForWhiteIndex ((int) std::floor (x / (float) kWhiteKeyWidth));
}

void Keyboard::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (col::panelDeep);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (col::lineSoft);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const auto keys = keysArea();
    const float blackWidth = (float) kWhiteKeyWidth * 0.62f;
    const float blackHeight = blackKeyHeight (keys);
    const float keyBottom = keys.getBottom();

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (keys.toNearestInt());

        // White keys
        int index = 0;
        for (int note = kLowestNote; note <= kHighestNote; ++note)
        {
            if (isBlackKey (note))
                continue;

            const float x = keys.getX() + (float) index * (float) kWhiteKeyWidth - scroll;
            ++index;

            if (x + (float) kWhiteKeyWidth < keys.getX() || x > keys.getRight())
                continue;

            auto key = juce::Rectangle<float> (x, keys.getY(), (float) kWhiteKeyWidth - 1.4f, keys.getHeight());
            const bool on = active[(size_t) note];

            // Ivory body: light enough that the black keys read as black.
            juce::ColourGradient body (on ? col::accentBright : col::ivory,
                                       key.getCentreX(), key.getY(),
                                       on ? col::accent : col::ivoryShade,
                                       key.getCentreX(), key.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (key, 3.0f);
            g.setColour (on ? col::accentBright : col::line);
            g.drawRoundedRectangle (key.reduced (0.5f), 3.0f, 1.0f);

            if (note % 12 == 0 && key.getWidth() > 26.0f)
            {
                g.setColour (col::panelDeep.withAlpha (0.75f));
                g.setFont (font (8.5f));
                g.drawText (nameForNote (note),
                            juce::Rectangle<int> (key.toNearestInt().withTrimmedBottom (3).removeFromBottom (12)),
                            juce::Justification::centred, false);
            }
        }

        // Black keys on top
        for (int note = kLowestNote; note <= kHighestNote; ++note)
        {
            if (! isBlackKey (note))
                continue;

            const float centre = keys.getX()
                               + (float) whiteKeyIndex (note) * (float) kWhiteKeyWidth - scroll;
            auto key = juce::Rectangle<float> (blackWidth, blackHeight).withCentre ({ centre, keys.getY() + blackHeight * 0.5f });

            if (key.getRight() < keys.getX() || key.getX() > keys.getRight())
                continue;

            const bool on = active[(size_t) note];

            juce::ColourGradient body (on ? col::accentBright : col::panelDeep,
                                       key.getCentreX(), key.getY(),
                                       on ? col::accent : col::shadow,
                                       key.getCentreX(), key.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (key, 2.5f);
            g.setColour (on ? col::accentBright : col::line);
            g.drawRoundedRectangle (key.reduced (0.5f), 2.5f, 1.0f);
        }

        g.setColour (col::shadow.withAlpha (0.35f));
        g.drawLine (keys.getX(), keyBottom, keys.getRight(), keyBottom, 1.0f);
    }

    // Scroll bar, only when the range does not fit.
    if (scrollBarVisible())
    {
        auto track = getLocalBounds().toFloat().removeFromBottom ((float) kScrollBarHeight).reduced (1.0f, 2.0f);
        g.setColour (col::panelDeep);
        g.fillRoundedRectangle (track, track.getHeight() * 0.5f);

        const auto thumb = thumbArea();
        g.setColour (thumbDragging ? col::accent : col::line.withAlpha (0.9f));
        g.fillRoundedRectangle (thumb, thumb.getHeight() * 0.5f);
    }
}

void Keyboard::resized()
{
    scrollTo (scroll);
    repaint();
}

Keyboard::Pointer& Keyboard::pointerFor (int sourceIndex)
{
    for (auto& pointer : pointers)
        if (pointer.source == sourceIndex)
            return pointer;

    for (auto& pointer : pointers)
    {
        if (pointer.source < 0)
        {
            pointer = Pointer {};
            pointer.source = sourceIndex;
            return pointer;
        }
    }

    // More touches than slots: reuse the first one and release its note so it
    // cannot hang.
    auto& fallback = pointers[0];
    playNote (fallback, -1);
    fallback = Pointer {};
    fallback.source = sourceIndex;
    return fallback;
}

void Keyboard::playNote (Pointer& pointer, int note)
{
    if (note == pointer.note)
        return;

    if (pointer.note >= 0)
    {
        active[(size_t) pointer.note] = false;
        if (host.noteOff)
            host.noteOff (pointer.note);
    }

    pointer.note = note;

    if (note >= 0)
    {
        active[(size_t) note] = true;
        if (host.noteOn)
            host.noteOn (note, kVelocity);
    }

    repaint();
}

void Keyboard::stopPointer (Pointer& pointer)
{
    playNote (pointer, -1);
    pointer.scrolling = false;
    pointer.source = -1;
}

void Keyboard::scrollTo (float newScroll)
{
    const float clamped = juce::jlimit (0.0f, maximumScroll(), newScroll);
    if (std::abs (clamped - scroll) > 0.01f)
    {
        scroll = clamped;
        repaint();
    }
    else
    {
        scroll = clamped;
    }
}

void Keyboard::mouseDown (const juce::MouseEvent& e)
{
    auto& pointer = pointerFor (e.source.getIndex());

    auto track = getLocalBounds().toFloat().removeFromBottom ((float) kScrollBarHeight).expanded (0.0f, 3.0f);
    if (scrollBarVisible() && track.contains (e.position))
    {
        auto scrollTrack = getLocalBounds().toFloat().removeFromBottom ((float) kScrollBarHeight).reduced (1.0f, 2.0f);
        const float thumbWidth = thumbArea().getWidth();
        const float travel = juce::jmax (1.0f, scrollTrack.getWidth() - thumbWidth);
        const float position = juce::jlimit (0.0f, travel, e.position.x - scrollTrack.getX() - thumbWidth * 0.5f);
        scrollTo (position / travel * maximumScroll());

        pointer.scrolling = true;
        thumbDragging = true;
        return;
    }

    playNote (pointer, noteAt (e.position));
}

void Keyboard::mouseDrag (const juce::MouseEvent& e)
{
    auto& pointer = pointerFor (e.source.getIndex());

    if (pointer.scrolling)
    {
        auto scrollTrack = getLocalBounds().toFloat().removeFromBottom ((float) kScrollBarHeight).reduced (1.0f, 2.0f);
        const float thumbWidth = thumbArea().getWidth();
        const float travel = juce::jmax (1.0f, scrollTrack.getWidth() - thumbWidth);
        const float position = juce::jlimit (0.0f, travel, e.position.x - scrollTrack.getX() - thumbWidth * 0.5f);
        scrollTo (position / travel * maximumScroll());
        return;
    }

    // Sliding below the keys still plays the key under the finger.
    const auto keys = keysArea();
    const juce::Point<float> position (e.position.x, juce::jlimit (keys.getY(), keys.getBottom() - 1.0f, e.position.y));
    playNote (pointer, noteAt (position));
}

void Keyboard::mouseUp (const juce::MouseEvent& e)
{
    auto& pointer = pointerFor (e.source.getIndex());
    thumbDragging = false;
    stopPointer (pointer);
    repaint();
}

void Keyboard::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scrollTo (scroll - wheel.deltaY * 240.0f - wheel.deltaX * 240.0f);
}

void Keyboard::releaseAllNotes()
{
    for (auto& pointer : pointers)
        stopPointer (pointer);

    active.fill (false);
    repaint();
}

} // namespace mk::ui
