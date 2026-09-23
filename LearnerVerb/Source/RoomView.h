#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

// The room the knobs describe, drawn: walls that move apart with Size, a
// source and a listener whose distance follows Pre-delay, a few reflection
// paths, and the walls' material from Damping.
//
// A reverb's knobs are numbers standing for a place, and people who are
// learning think in places - "a bigger room", "further away", "softer
// walls" - long before they think in seconds. This is the translation the
// knobs never show (docs/design/approved-2026-09/LearnerVerb.png).
//
// It is a picture, not a simulation: the echogram beside it is the
// measurement (EchogramView). A plate and a spring are not rooms at all,
// and for those it draws what they are instead of pretending.
class RoomView : public juce::Component
{
public:
    struct Strings
    {
        juce::String caption = "The room these knobs describe";
        juce::String source = "source", you = "you";
        juce::String plateCaption = "Not a room: a metal plate";
        juce::String springCaption = "Not a room: a spring in a box";
        juce::String metres = "m";
    };

    void setStrings (Strings s)           { text = std::move (s); repaint(); }
    void setAccentColour (juce::Colour c) { accent = c; repaint(); }
    void setDescription (juce::String d)  { description = std::move (d); repaint(); }

    // type 0..3 (room, hall, plate, spring), size and damping 0..1,
    // pre-delay in ms.
    void setRoom (int newType, float newSize, float newPreDelayMs, float newDamping)
    {
        if (newType == type && juce::approximatelyEqual (newSize, size)
            && juce::approximatelyEqual (newPreDelayMs, preDelayMs) && juce::approximatelyEqual (newDamping, damping))
            return;

        type = newType;
        size = newSize;
        preDelayMs = newPreDelayMs;
        damping = newDamping;
        repaint();
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
            g.drawFittedText (description, descriptionArea.toNearestInt(), juce::Justification::bottomLeft, 2, 0.9f);
        }
    }

private:
    void paintRoom (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto& theme = AbcTrainTheme::current();

        // One-point perspective: the back wall shrinks toward the centre
        // as the room gets longer, so Size reads as depth and width both.
        const auto length = lengthFor (type, size);
        const auto depth = juce::jmap (juce::jlimit (0.0f, 1.0f, size), 0.72f, 0.42f);

        const auto front = area.reduced (area.getWidth() * 0.04f, 0.0f);
        const auto back = front.withSizeKeepingCentre (front.getWidth() * depth, front.getHeight() * depth)
                               .translated (0.0f, -front.getHeight() * 0.06f);

        // Wall material: hard walls are drawn bright, curtains soft.
        const auto wallAlpha = juce::jmap (damping, 0.30f, 0.10f);
        const auto floorColour = accent.withAlpha (0.10f);

        juce::Path floor;
        floor.startNewSubPath (front.getBottomLeft());
        floor.lineTo (front.getBottomRight());
        floor.lineTo (back.getBottomRight());
        floor.lineTo (back.getBottomLeft());
        floor.closeSubPath();
        g.setColour (floorColour);
        g.fillPath (floor);

        g.setColour (accent.withAlpha (wallAlpha));
        g.fillRect (back);

        g.setColour (accent.withAlpha (0.55f));
        g.drawRect (back, 1.0f);
        g.drawLine ({ front.getTopLeft(), back.getTopLeft() }, 1.0f);
        g.drawLine ({ front.getTopRight(), back.getTopRight() }, 1.0f);
        g.drawLine ({ front.getBottomLeft(), back.getBottomLeft() }, 1.0f);
        g.drawLine ({ front.getBottomRight(), back.getBottomRight() }, 1.0f);
        g.drawLine ({ front.getBottomLeft(), front.getBottomRight() }, 1.0f);

        // Listener near the front, source further in: pre-delay is the
        // extra path the first reflection travels, so a longer pre-delay
        // is a source further from the walls around you.
        const auto distance = juce::jlimit (0.15f, 0.85f, preDelayMs / 120.0f);
        const auto you = juce::Point<float> (front.getCentreX() + front.getWidth() * 0.16f,
                                             front.getBottom() - front.getHeight() * 0.18f);
        const auto sourceTarget = juce::Point<float> (back.getCentreX() - back.getWidth() * 0.22f,
                                                      back.getBottom() - back.getHeight() * 0.25f);
        const auto source = you + (sourceTarget - you) * juce::jmap (distance, 0.35f, 1.0f);

        // A few first reflections: off the floor, a side wall and the back.
        {
            const float dashes[] = { 3.0f, 3.0f };
            const juce::Point<float> bounces[] = {
                { front.getX() + front.getWidth() * 0.1f + (back.getX() - front.getX()) * 0.5f, (front.getY() + back.getY()) * 0.5f + front.getHeight() * 0.35f },
                { back.getCentreX() + back.getWidth() * 0.3f, back.getY() + back.getHeight() * 0.4f },
                { (front.getCentreX() + back.getCentreX()) * 0.5f, front.getBottom() - front.getHeight() * 0.05f } };

            g.setColour (accent.withAlpha (0.5f));

            for (const auto& b : bounces)
            {
                juce::Path path;
                path.startNewSubPath (source);
                path.lineTo (b);
                path.lineTo (you);
                juce::Path dashed;
                juce::PathStrokeType (0.8f).createDashedStroke (dashed, path, dashes, 2);
                g.fillPath (dashed);
            }
        }

        g.setColour (theme.textBright.withAlpha (0.85f));
        g.drawLine ({ source, you }, 1.6f);

        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (source));
        g.setColour (theme.textBright);
        g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (you));

        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.setColour (accent);
        g.drawText (text.source, juce::Rectangle<float> (80.0f, 16.0f).withCentre (source.translated (0.0f, -16.0f)),
                    juce::Justification::centred, false);
        g.setColour (theme.textBright);
        g.drawText (text.you, juce::Rectangle<float> (60.0f, 16.0f).withPosition (you.translated (10.0f, -8.0f)),
                    juce::Justification::centredLeft, false);

        // Dimensions, in the words the knob notes use.
        g.setColour (theme.textDim);
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x89\x88 ")) + juce::String (juce::roundToInt (length)) + " " + text.metres,
                    juce::Rectangle<float> (front.getCentreX() - 50.0f, front.getBottom() - 16.0f, 100.0f, 16.0f),
                    juce::Justification::centred, false);
        g.drawText (juce::String (juce::roundToInt (heightFor (type, size))) + " " + text.metres,
                    juce::Rectangle<float> (front.getX() + 4.0f, (front.getY() + back.getY()) * 0.5f + front.getHeight() * 0.3f, 60.0f, 16.0f),
                    juce::Justification::centredLeft, false);
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
};
