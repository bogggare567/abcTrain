#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

// What waiting for an opponent looks like (visual review, ADR 063).
//
// Searching: a radar with the player's own rating in the middle and the
// rating window the server is searching drawn as a ring that widens on the
// server's own schedule - ±200, then +100 every ten seconds, anyone after
// forty (ADR 059) - so the wait explains itself instead of being a line of
// text with a clock in it. Found: a ring that drains over the seconds left
// to accept, with how many of the room have accepted so far.
//
// Drawn on the card's own background, over whatever the card would show
// there otherwise; the owner hides it when nothing is pending.
class SearchRadar : public juce::Component,
                    private juce::Timer
{
public:
    struct Strings
    {
        juce::String window = juce::String::fromUTF8 ("\xc2\xb1{{n}}");
        juce::String anyone = "any rating";
        juce::String widening = "the window widens every 10 s";
        juce::String inQueue = "{{n}} in the queue";
        juce::String accept = "Room found: {{n}} s to accept";
        juce::String accepted = "{{a}} of {{m}} accepted";
    };

    SearchRadar() { setInterceptsMouseClicks (false, false); }

    void setStrings (Strings s) { text = std::move (s); repaint(); }
    void setOwnRating (int rating) { ownRating = rating; repaint(); }

    void showSearching (int waitingMs, int inQueue)
    {
        ready = false;
        queue = inQueue;
        startedAt = juce::Time::getMillisecondCounterHiRes() - (double) waitingMs;
        begin();
    }

    void showReady (int deadlineInMs, int members, int acceptedCount)
    {
        if (! ready)
            readySince = juce::Time::getMillisecondCounterHiRes();

        ready = true;
        deadlineAt = juce::Time::getMillisecondCounterHiRes() + (double) deadlineInMs;
        totalMs = juce::jmax (totalMs, deadlineInMs);
        roomSize = members;
        accepted = acceptedCount;
        begin();
    }

    void stop()
    {
        stopTimer();
        totalMs = 0;
        setVisible (false);
    }

    // For snapshots: a frame at a fixed time instead of the clock.
    void freezeAt (double seconds) { frozen = seconds; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        auto bounds = getLocalBounds().toFloat();
        g.setColour (theme.panelBackground);
        g.fillRect (bounds);

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto seconds = frozen >= 0.0 ? frozen : (now - startedAt) * 0.001;

        auto caption = bounds.removeFromBottom (44.0f);
        const auto side = juce::jmin (bounds.getWidth(), bounds.getHeight());

        if (side < 60.0f)
            return;

        const auto centre = bounds.getCentre();
        const auto radius = side * 0.46f;

        if (ready)
            paintReady (g, centre, radius, caption, now);
        else
            paintSearching (g, centre, radius, caption, seconds);
    }

private:
    void begin()
    {
        setVisible (true);
        startTimerHz (30);
        repaint();
    }

    void timerCallback() override { repaint(); }

    void paintSearching (juce::Graphics& g, juce::Point<float> centre, float radius,
                         juce::Rectangle<float> caption, double seconds)
    {
        const auto& theme = AbcTrainTheme::current();

        // Rings: the outer edge is "anyone"; the window grows towards it.
        g.setColour (theme.textDim.withAlpha (0.18f));
        for (const auto k : { 1.0f, 0.72f, 0.44f })
            g.drawEllipse (juce::Rectangle<float> (radius * 2.0f * k, radius * 2.0f * k).withCentre (centre), 1.0f);

        const auto steps = (int) (seconds / 10.0);
        const auto anyone = seconds >= 40.0;
        const auto windowFraction = anyone ? 1.0f : juce::jmin (1.0f, 0.38f + 0.15f * (float) steps);
        const auto windowRadius = radius * windowFraction;
        g.setColour (theme.accent.withAlpha (0.07f));
        g.fillEllipse (juce::Rectangle<float> (windowRadius * 2.0f, windowRadius * 2.0f).withCentre (centre));

        juce::Path ring;
        ring.addEllipse (juce::Rectangle<float> (windowRadius * 2.0f, windowRadius * 2.0f).withCentre (centre));
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (ring, ring, dashes, 2);
        g.setColour (theme.accent.withAlpha (0.8f));
        g.fillPath (ring);

        // The sweep: a wedge trailing a bright line, one turn every 2.4 s.
        const auto angle = (float) std::fmod (seconds / 2.4, 1.0) * juce::MathConstants<float>::twoPi;
        juce::Path wedge;
        wedge.addPieSegment (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre),
                             angle - 0.9f, angle, 0.0f);
        g.setColour (theme.accent.withAlpha (0.13f));
        g.fillPath (wedge);
        g.setColour (theme.accent.brighter (0.2f));
        g.drawLine (juce::Line<float> (centre, centre.getPointOnCircumference (radius, angle)), 1.5f);

        // A ping from the centre every turn.
        const auto ping = (float) std::fmod (seconds / 1.8, 1.0);
        g.setColour (theme.accent.withAlpha (0.6f * (1.0f - ping)));
        g.drawEllipse (juce::Rectangle<float> (24.0f + 50.0f * ping, 24.0f + 50.0f * ping).withCentre (centre), 2.0f);

        // You, in the middle.
        const auto chip = juce::Rectangle<float> (56.0f, 30.0f).withCentre (centre);
        g.setColour (theme.panelBackground);
        g.fillRect (chip);
        g.setColour (theme.accent);
        g.drawRect (chip, 1.0f);
        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (14.0f));
        AbcTrainLookAndFeel::fitText (g, ownRating > 0 ? juce::String (ownRating) : juce::String ("?"), chip.toNearestInt(),
                                      juce::Justification::centred, false);

        // The window, written on its ring.
        g.setColour (theme.accent);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
        const auto windowText = anyone ? text.anyone : text.window.replace ("{{n}}", juce::String (200 + 100 * steps));
        AbcTrainLookAndFeel::fitText (g, windowText,
                                      juce::Rectangle<float> (160.0f, 16.0f).withCentre ({ centre.x, centre.y - windowRadius - 10.0f }).toNearestInt(),
                                      juce::Justification::centred, false);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        auto top = caption.removeFromTop (20.0f);
        AbcTrainLookAndFeel::fitText (g, anyone ? juce::String() : text.widening, top.toNearestInt(), juce::Justification::centred, true);

        if (queue > 0)
            AbcTrainLookAndFeel::fitText (g, text.inQueue.replace ("{{n}}", juce::String (queue)), caption.toNearestInt(),
                                          juce::Justification::centred, true);
    }

    void paintReady (juce::Graphics& g, juce::Point<float> centre, float radius,
                     juce::Rectangle<float> caption, double now)
    {
        const auto& theme = AbcTrainTheme::current();
        const auto left = juce::jmax (0.0, (deadlineAt - now) * 0.001);
        const auto share = totalMs > 0 ? (float) juce::jlimit (0.0, 1.0, left * 1000.0 / (double) totalMs) : 0.0f;
        const auto r = radius * 0.8f;

        g.setColour (theme.divider);
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 8.0f);

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, r, r, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * share, true);
        g.setColour (theme.accent);
        g.strokePath (arc, juce::PathStrokeType (8.0f));

        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (juce::jmin (40.0f, r * 0.7f)));
        AbcTrainLookAndFeel::fitText (g, juce::String ((int) std::ceil (left)),
                                      juce::Rectangle<float> (r * 2.0f, r).withCentre (centre).toNearestInt(),
                                      juce::Justification::centred, false);

        g.setColour (theme.accent);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitText (g, text.accept.replace ("{{n}}", juce::String ((int) std::ceil (left))),
                                      caption.removeFromTop (22.0f).toNearestInt(), juce::Justification::centred, true);

        if (roomSize > 0)
        {
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            AbcTrainLookAndFeel::fitText (g, text.accepted.replace ("{{a}}", juce::String (accepted)).replace ("{{m}}", juce::String (roomSize)),
                                          caption.toNearestInt(), juce::Justification::centred, true);
        }
    }

    Strings text;
    int ownRating = 0, queue = 0, roomSize = 0, accepted = 0, totalMs = 0;
    bool ready = false;
    double startedAt = 0.0, deadlineAt = 0.0, readySince = 0.0, frozen = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchRadar)
};
