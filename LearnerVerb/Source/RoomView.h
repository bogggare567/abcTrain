#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include <array>
#include <functional>

// The room the knobs describe, drawn: walls that move apart with Size, a
// source and a listener whose distance follows Pre-delay, a few reflection
// paths, and the walls' material from Damping.
//
// A reverb's knobs are numbers standing for a place, and people who are
// learning think in places - "a bigger room", "further away", "softer
// walls" - long before they think in seconds. This is the translation the
// knobs never show (docs/design/approved-2026-09/LearnerVerb.png).
//
// And it is interactive, and it computes. The room is a shoebox of the
// size Size gives it; the source and the listener are points in it; the
// first reflections are traced with image sources off the four walls and
// the ceiling (the floor is taken as the audience - absorbent, and the
// one surface a pre-delay knob has never meant). Pre-delay is then what
// it physically is: the time between the direct sound and the first of
// those reflections. Drag the source and the Pre-delay knob turns; drag
// the corner of the back wall and Size does. Turn either knob and the
// drawing follows.
//
// The echogram beside it is still the measurement of the engine
// (EchogramView). A plate and a spring are not rooms at all, and for those
// it draws what they are instead of pretending.
class RoomView : public juce::Component
{
public:
    // Asked of the editor, which owns the parameters: the view proposes,
    // the knob moves, and the new value comes back through setRoom().
    std::function<void (float preDelayMs)> onPreDelayDragged;
    std::function<void (float size01)> onSizeDragged;
    std::function<void()> onDragEnded;

    static constexpr float speedOfSound = 343.0f;   // m/s

    struct Strings
    {
        juce::String caption = "The room these knobs describe";
        juce::String source = "source", you = "you";
        juce::String plateCaption = "Not a room: a metal plate";
        juce::String springCaption = "Not a room: a spring in a box";
        juce::String metres = "m";
        juce::String firstReflection = "first reflection +{{ms}} ms";
    };

    void setStrings (Strings s)           { text = std::move (s); repaint(); }
    void setAccentColour (juce::Colour c) { accent = c; repaint(); }
    void setDescription (juce::String d)  { description = std::move (d); repaint(); }

    // type 0..3 (room, hall, plate, spring), size and damping 0..1,
    // pre-delay in ms.
    void setRoom (int newType, float newSize, float newPreDelayMs, float newDamping)
    {
        if (placed && newType == type && juce::approximatelyEqual (newSize, size)
            && juce::approximatelyEqual (newPreDelayMs, preDelayMs) && juce::approximatelyEqual (newDamping, damping))
            return;

        type = newType;
        size = newSize;
        preDelayMs = newPreDelayMs;
        damping = newDamping;

        // A knob turned elsewhere: move the source until its geometry
        // gives that pre-delay. Not while the source itself is being
        // dragged - then the geometry is the cause, not the effect.
        if (dragging != Drag::source)
            placeSourceFor (preDelayMs);

        placed = true;

        repaint();
    }

    // What the geometry gives now, for the knob note: the first reflection
    // gap in ms, and whether the knob asks for more than this room can
    // make (then the source sits as far as it can and the note says so).
    float getGeometricPreDelayMs() const { return firstReflection().gapMs; }
    bool isPreDelayBeyondRoom() const   { return type <= 1 && preDelayMs > maxGapMs() + 1.0f; }
    float getSourceDistanceMetres() const { return distance (source(), listener()); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto over = hitTestHandle (e.position);
        setMouseCursor (over == Drag::none ? juce::MouseCursor::NormalCursor
                                           : juce::MouseCursor::DraggingHandCursor);
        if (over != hovered) { hovered = over; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hovered != Drag::none) { hovered = Drag::none; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = hitTestHandle (e.position);
        sizeAtDragStart = size;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == Drag::source)
        {
            const auto floor = unproject (e.position);
            const auto w = widthFor (type, size), l = lengthFor (type, size);
            sourcePos = { juce::jlimit (0.3f, w - 0.3f, floor.x), juce::jlimit (0.3f, l - 0.3f, floor.y) };
            if (onPreDelayDragged != nullptr)
                onPreDelayDragged (firstReflection().gapMs);
            repaint();
        }
        else if (dragging == Drag::size)
        {
            const auto dx = (float) e.getDistanceFromDragStartX() - (float) e.getDistanceFromDragStartY();
            const auto newSize = juce::jlimit (0.0f, 1.0f, sizeAtDragStart + dx / juce::jmax (80.0f, roomArea.getWidth() * 0.8f));
            if (onSizeDragged != nullptr)
                onSizeDragged (newSize);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging != Drag::none && onDragEnded != nullptr)
            onDragEnded();

        dragging = Drag::none;
    }

    // The room's length in metres for this Size - shared with the knob
    // notes, so the drawing and the words agree.
    static float lengthFor (int type, float size)
    {
        const auto base = type == 1 ? 12.0f : 4.0f;      // a hall starts bigger than a room
        const auto span = type == 1 ? 28.0f : 14.0f;
        return base + span * juce::jlimit (0.0f, 1.0f, size);
    }

    static float widthFor (int type, float size)  { return lengthFor (type, size) * 0.7f; }
    static float heightFor (int type, float size) { return 2.6f + lengthFor (type, size) * 0.22f; }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto bounds = getLocalBounds().toFloat();

        AbcTrainLookAndFeel::paintDisplayWell (g, bounds);

        auto inner = bounds.reduced (12.0f, 10.0f);
        const auto captionFont = AbcTrainLookAndFeel::microFont();
        const auto captionText = type == 2 ? text.plateCaption : type == 3 ? text.springCaption : text.caption;
        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (captionText),
                                              inner.removeFromTop (16.0f), captionFont, accent, 1.3f);

        auto descriptionArea = inner.removeFromBottom (description.isNotEmpty() ? 34.0f : 0.0f);
        inner.removeFromBottom (4.0f);

        if (type == 2)       paintPlate (g, inner);
        else if (type == 3)  paintSpring (g, inner);
        else                 paintRoom (g, inner);

        if (description.isNotEmpty())
        {
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            AbcTrainLookAndFeel::fitLines (g, description, descriptionArea.toNearestInt(), juce::Justification::bottomLeft, 2, 0.9f);
        }
    }

private:
    // ---- the model ----------------------------------------------------
    //
    // Floor coordinates in metres: x across (0..W), y into the room
    // (0 = the front edge nearest the viewer, L = the back wall), z up.
    struct Vec { float x, y, z; };

    static float distance (Vec a, Vec b)
    {
        return std::sqrt ((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
    }

    static constexpr float earHeight = 1.2f, sourceHeight = 1.5f;

    Vec listener() const
    {
        return { widthFor (type, size) * 0.5f, lengthFor (type, size) * 0.3f, earHeight };
    }

    Vec source() const { return { sourcePos.x, sourcePos.y, sourceHeight }; }

    struct Reflection
    {
        Vec image;        // the mirror image of the source in the surface
        Vec bounce;       // where on the surface the ray reflects
        float gapMs = 0;  // how much later than the direct sound it arrives
    };

    // Four walls and the ceiling. The floor is left out on purpose (see
    // the class comment).
    std::array<Reflection, 5> reflections() const
    {
        const auto w = widthFor (type, size), l = lengthFor (type, size), h = heightFor (type, size);
        const auto s = source(), r = listener();
        const auto direct = distance (s, r);

        std::array<Reflection, 5> out;
        const Vec images[] = { { -s.x, s.y, s.z }, { 2.0f * w - s.x, s.y, s.z },
                               { s.x, -s.y, s.z }, { s.x, 2.0f * l - s.y, s.z },
                               { s.x, s.y, 2.0f * h - s.z } };

        for (size_t i = 0; i < 5; ++i)
        {
            const auto im = images[i];
            // Where the line from the image to the listener crosses the
            // surface: the fraction along it at which the mirrored
            // coordinate meets the plane.
            float t = 0.5f;
            if (i == 0) t = (0.0f - im.x) / (r.x - im.x);
            if (i == 1) t = (w - im.x) / (r.x - im.x);
            if (i == 2) t = (0.0f - im.y) / (r.y - im.y);
            if (i == 3) t = (l - im.y) / (r.y - im.y);
            if (i == 4) t = (h - im.z) / (r.z - im.z);
            t = juce::jlimit (0.0f, 1.0f, t);

            out[i].image = im;
            out[i].bounce = { im.x + (r.x - im.x) * t, im.y + (r.y - im.y) * t, im.z + (r.z - im.z) * t };
            out[i].gapMs = (distance (im, r) - direct) / speedOfSound * 1000.0f;
        }

        return out;
    }

    Reflection firstReflection() const
    {
        const auto all = reflections();
        auto best = all[0];

        for (const auto& refl : all)
            if (refl.gapMs < best.gapMs)
                best = refl;

        return best;
    }

    // The largest gap this room can make at all - source right beside the
    // listener, whose nearest surface is then the one that limits it.
    float maxGapMs() const
    {
        const auto w = widthFor (type, size), l = lengthFor (type, size), h = heightFor (type, size);
        const auto r = listener();
        const auto nearest = juce::jmin (juce::jmin (r.x, w - r.x), juce::jmin (r.y, l - r.y), h - sourceHeight);
        return 2.0f * nearest / speedOfSound * 1000.0f;
    }

    // Moves the source along its current bearing from the listener until
    // the first reflection arrives `targetMs` after the direct sound.
    void placeSourceFor (float targetMs)
    {
        const auto w = widthFor (type, size), l = lengthFor (type, size);
        const auto r = listener();

        auto dir = juce::Point<float> (sourcePos.x - r.x, sourcePos.y - r.y);
        if (dir.getDistanceFromOrigin() < 0.05f)
            dir = { -0.5f, 1.0f };
        dir /= dir.getDistanceFromOrigin();

        auto bestPos = sourcePos;
        auto bestError = std::numeric_limits<float>::max();

        for (int i = 0; i <= 200; ++i)
        {
            const auto reach = (float) i / 200.0f * juce::jmax (w, l);
            const juce::Point<float> p { r.x + dir.x * reach, r.y + dir.y * reach };

            if (p.x < 0.3f || p.x > w - 0.3f || p.y < 0.3f || p.y > l - 0.3f)
                break;

            const auto saved = sourcePos;
            sourcePos = p;
            const auto error = std::abs (firstReflection().gapMs - targetMs);
            sourcePos = saved;

            if (error < bestError)
            {
                bestError = error;
                bestPos = p;
            }
        }

        sourcePos = bestPos;
    }

    // ---- the drawing -------------------------------------------------
    //
    // A simple two-point-rail perspective: the floor is a trapezoid from
    // the front edge (bottom of the area) to the back wall, and every
    // point in the room is placed by interpolating between the two.
    juce::Point<float> project (Vec v) const
    {
        const auto w = widthFor (type, size), l = lengthFor (type, size), h = heightFor (type, size);
        const auto t = juce::jlimit (0.0f, 1.0f, v.y / l);
        const auto left = juce::jmap (t, frontFace.getX(), backFace.getX());
        const auto right = juce::jmap (t, frontFace.getRight(), backFace.getRight());
        const auto bottom = juce::jmap (t, frontFace.getBottom(), backFace.getBottom());
        const auto top = juce::jmap (t, frontFace.getY(), backFace.getY());
        return { left + (right - left) * (v.x / w), bottom - (bottom - top) * (v.z / h) };
    }

    // The inverse, for a point on the floor.
    juce::Point<float> unproject (juce::Point<float> screen) const
    {
        const auto w = widthFor (type, size), l = lengthFor (type, size);
        const auto t = juce::jlimit (0.0f, 1.0f, (frontFace.getBottom() - screen.y)
                                                     / juce::jmax (1.0f, frontFace.getBottom() - backFace.getBottom()));
        const auto left = juce::jmap (t, frontFace.getX(), backFace.getX());
        const auto right = juce::jmap (t, frontFace.getRight(), backFace.getRight());
        return { (screen.x - left) / juce::jmax (1.0f, right - left) * w, t * l };
    }

    enum class Drag { none, source, size };

    Drag hitTestHandle (juce::Point<float> p) const
    {
        if (type > 1)
            return Drag::none;

        if (p.getDistanceFrom (project (source())) < 14.0f)
            return Drag::source;

        if (sizeHandle.expanded (6.0f).contains (p))
            return Drag::size;

        return Drag::none;
    }

    void paintRoom (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto& theme = AbcTrainTheme::current();
        const auto w = widthFor (type, size), l = lengthFor (type, size), h = heightFor (type, size);

        // Depth and height scale with the room, so Size reads as both.
        roomArea = area;
        const auto depth = juce::jmap (juce::jlimit (0.0f, 1.0f, size), 0.66f, 0.40f);
        frontFace = area.reduced (area.getWidth() * 0.03f, 0.0f).withTrimmedTop (area.getHeight() * 0.04f);
        backFace = frontFace.withSizeKeepingCentre (frontFace.getWidth() * depth, frontFace.getHeight() * depth)
                            .translated (0.0f, -frontFace.getHeight() * 0.08f);

        const auto corner = [&] (float x, float y, float z) { return project ({ x, y, z }); };

        // Floor, back wall, edges.
        juce::Path floor;
        floor.startNewSubPath (corner (0, 0, 0));
        floor.lineTo (corner (w, 0, 0));
        floor.lineTo (corner (w, l, 0));
        floor.lineTo (corner (0, l, 0));
        floor.closeSubPath();
        g.setColour (accent.withAlpha (0.10f));
        g.fillPath (floor);

        // Hard walls are drawn bright, curtains soft.
        g.setColour (accent.withAlpha (juce::jmap (damping, 0.26f, 0.08f)));
        g.fillRect (backFace);

        g.setColour (accent.withAlpha (0.55f));
        g.drawRect (backFace, 1.0f);
        for (const auto& edge : { std::pair<Vec, Vec> { { 0, 0, h }, { 0, l, h } }, { { w, 0, h }, { w, l, h } },
                                  { { 0, 0, 0 }, { 0, l, 0 } }, { { w, 0, 0 }, { w, l, 0 } },
                                  { { 0, 0, 0 }, { w, 0, 0 } } })
            g.drawLine ({ project (edge.first), project (edge.second) }, 1.0f);

        // The reflections, each a dashed path source -> surface -> you;
        // the first to arrive solid, since it is the one Pre-delay names.
        const auto s = project (source()), r = project (listener());
        const auto first = firstReflection();
        const float dashes[] = { 3.0f, 3.0f };

        for (const auto& refl : reflections())
        {
            juce::Path path;
            path.startNewSubPath (s);
            path.lineTo (project (refl.bounce));
            path.lineTo (r);

            const auto isFirst = juce::approximatelyEqual (refl.gapMs, first.gapMs);

            if (isFirst)
            {
                g.setColour (accent);
                g.strokePath (path, juce::PathStrokeType (1.4f));
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (project (refl.bounce)));
            }
            else
            {
                juce::Path dashed;
                juce::PathStrokeType (0.8f).createDashedStroke (dashed, path, dashes, 2);
                g.setColour (accent.withAlpha (0.45f));
                g.fillPath (dashed);
            }
        }

        // Direct sound.
        g.setColour (theme.textBright.withAlpha (0.85f));
        g.drawLine ({ s, r }, 1.6f);

        // The source is the handle: it grows under the pointer.
        const auto big = hovered == Drag::source || dragging == Drag::source;
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (big ? 16.0f : 12.0f, big ? 16.0f : 12.0f).withCentre (s));
        if (big)
        {
            g.setColour (accent.withAlpha (0.3f));
            g.drawEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (s), 1.5f);
        }
        g.setColour (theme.textBright);
        g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (r));

        // The size handle on the back wall's top corner.
        sizeHandle = juce::Rectangle<float> (10.0f, 10.0f).withCentre (backFace.getTopRight());
        g.setColour (hovered == Drag::size || dragging == Drag::size ? theme.textBright : accent);
        g.fillRect (sizeHandle);

        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.setColour (accent);
        AbcTrainLookAndFeel::fitText (g, text.source, juce::Rectangle<float> (90.0f, 16.0f).withCentre (s.translated (0.0f, -17.0f)),
                                      juce::Justification::centred, false);
        g.setColour (theme.textBright);
        AbcTrainLookAndFeel::fitText (g, text.you, juce::Rectangle<float> (60.0f, 16.0f).withPosition (r.translated (10.0f, -8.0f)),
                                      juce::Justification::centredLeft, false);

        // The numbers the knobs are made of, where they happen.
        g.setColour (theme.textDim);
        const auto approx = juce::String (juce::CharPointer_UTF8 ("\xe2\x89\x88 "));
        AbcTrainLookAndFeel::fitText (g, approx + juce::String (juce::roundToInt (w)) + " " + text.metres,
                                      juce::Rectangle<float> (frontFace.getCentreX() - 50.0f, frontFace.getBottom() - 16.0f, 100.0f, 16.0f),
                                      juce::Justification::centred, false);
        AbcTrainLookAndFeel::fitText (g, juce::String (juce::roundToInt (h)) + " " + text.metres,
                                      juce::Rectangle<float> (frontFace.getX() + 4.0f, frontFace.getCentreY() - 8.0f, 60.0f, 16.0f),
                                      juce::Justification::centredLeft, false);
        AbcTrainLookAndFeel::fitText (g, approx + juce::String (juce::roundToInt (l)) + " " + text.metres,
                                      juce::Rectangle<float> (backFace.getRight() + 6.0f, backFace.getBottom() - 16.0f, 70.0f, 16.0f),
                                      juce::Justification::centredLeft, false);

        g.setColour (accent);
        AbcTrainLookAndFeel::fitText (g, text.firstReflection.replace ("{{ms}}", juce::String (juce::roundToInt (first.gapMs))),
                                      juce::Rectangle<float> (area.getRight() - area.getWidth() * 0.45f, area.getBottom() - 16.0f,
                                                              area.getWidth() * 0.45f, 16.0f),
                                      juce::Justification::centredRight, false);
    }

    void paintPlate (juce::Graphics& g, juce::Rectangle<float> area)
    {
        // A sheet of steel on a frame, the pickup and the driver on it.
        const auto sheet = area.withSizeKeepingCentre (area.getWidth() * 0.7f, area.getHeight() * 0.55f);
        juce::Path p;
        p.startNewSubPath (sheet.getX() + sheet.getWidth() * 0.12f, sheet.getY());
        p.lineTo (sheet.getRight(), sheet.getY());
        p.lineTo (sheet.getRight() - sheet.getWidth() * 0.12f, sheet.getBottom());
        p.lineTo (sheet.getX(), sheet.getBottom());
        p.closeSubPath();

        g.setColour (accent.withAlpha (juce::jmap (damping, 0.28f, 0.12f)));
        g.fillPath (p);
        g.setColour (accent.withAlpha (0.7f));
        g.strokePath (p, juce::PathStrokeType (1.2f));

        g.setColour (AbcTrainTheme::current().textBright);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (sheet.getCentre().translated (-sheet.getWidth() * 0.2f, 0.0f)));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (sheet.getCentre().translated (sheet.getWidth() * 0.22f, sheet.getHeight() * 0.15f)));
    }

    void paintSpring (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto box = area.withSizeKeepingCentre (area.getWidth() * 0.8f, area.getHeight() * 0.3f);
        g.setColour (accent.withAlpha (0.5f));
        g.drawRect (box, 1.0f);

        juce::Path coil;
        const auto turns = 22;
        const auto y = box.getCentreY();
        const auto amplitude = box.getHeight() * 0.28f;

        for (int i = 0; i <= turns * 12; ++i)
        {
            const auto t = (float) i / (float) (turns * 12);
            const auto x = box.getX() + 14.0f + (box.getWidth() - 28.0f) * t;
            const auto yy = y + amplitude * std::sin (t * juce::MathConstants<float>::twoPi * (float) turns);

            if (i == 0) coil.startNewSubPath (x, yy);
            else        coil.lineTo (x, yy);
        }

        g.setColour (accent);
        g.strokePath (coil, juce::PathStrokeType (1.3f));
    }

    Strings text;
    juce::Colour accent { AbcTrainTheme::current().accent };
    juce::String description;
    int type = 0;
    float size = 0.5f, preDelayMs = 20.0f, damping = 0.4f;

    juce::Point<float> sourcePos { 2.5f, 7.0f };   // metres, on the floor
    juce::Rectangle<float> roomArea, frontFace, backFace, sizeHandle;
    Drag dragging = Drag::none, hovered = Drag::none;
    float sizeAtDragStart = 0.5f;
    bool placed = false;
};
