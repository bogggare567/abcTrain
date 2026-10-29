#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "Games/Game.h"
#include <cmath>

/**
    The hint's place before the hint is bought (the phone mockups of
    2026-10-24, brought to every screen).

    The room under the scale is kept for the picture whether or not it is
    bought, so buying moves nothing. Empty, that room read as a screen that
    had not finished loading (the author: "пустота смущает"). Now it is a
    frame with a dimmed sketch of what will be there, a lock, and the name
    of the picture - so the player sees there is something here, and what.

    The sketch is drawn from nothing: a generic shape per kind of picture,
    never from the sound playing. A blurred copy of the real analyser
    would leak the answer to anyone who squints.

    The buy button itself is the editor's hintButton, placed over the
    middle of this frame (buttonArea()).
*/
class HintPlaceholder : public juce::Component
{
public:
    HintPlaceholder() { setInterceptsMouseClicks (false, false); }

    void setView (Game::HintView v, const juce::String& titleText)
    {
        if (v == view && titleText == title)
            return;

        view = v;
        title = titleText;
        repaint();
    }

    void setButtonShown (bool shown) { if (shown != withButton) { withButton = shown; repaint(); } }

    /** Where the editor puts the buy button, in the parent's coordinates. */
    juce::Rectangle<int> buttonArea() const
    {
        const auto c = centre();
        return juce::Rectangle<int> (0, 0, juce::jmin (280, getWidth() - 24), 38)
                   .withCentre ({ getX() + getWidth() / 2, getY() + (int) c.getY() + 52 });
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        auto box = getLocalBounds().toFloat().reduced (0.5f);

        // The page itself under it, as under the scale: no black well.
        g.setColour (theme.panelBackground.withAlpha (0.22f));
        g.fillRect (box);
        g.setColour (theme.outline.withAlpha (0.5f));
        g.drawRect (box, 1.0f);

        paintSketch (g, box.reduced (18.0f), theme.textDim.withAlpha (0.18f));

        // The lock, the name and (when there is one) the button below them.
        const auto c = centre();
        auto lock = juce::Rectangle<float> (26.0f, 26.0f).withCentre ({ c.getX(), c.getY() - 30.0f });
        g.setColour (theme.textBright);
        juce::Path shackle;
        shackle.addCentredArc (lock.getCentreX(), lock.getY() + 9.0f, 7.0f, 7.0f, 0.0f,
                               -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.6f));
        g.drawRect (lock.withTrimmedTop (10.0f).reduced (2.0f, 0.0f), 1.6f);

        g.setFont (AbcTrainLookAndFeel::headingFont().withHeight (20.0f));
        AbcTrainLookAndFeel::fitText (g, title,
                                      juce::Rectangle<float> (box.getWidth() - 24.0f, 26.0f).withCentre ({ c.getX(), c.getY() + 6.0f }),
                                      juce::Justification::centred);
    }

private:
    juce::Point<float> centre() const
    {
        // A little above the middle when the button sits below the name.
        return { getWidth() * 0.5f, getHeight() * 0.5f - (withButton ? 18.0f : 0.0f) };
    }

    void paintSketch (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink) const
    {
        g.setColour (ink);

        switch (view)
        {
            case Game::HintView::stereo:
            {
                // A goniometer: the circle, its axes and a cloud along the middle.
                auto sq = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()),
                                                   juce::jmin (r.getWidth(), r.getHeight()));
                g.drawEllipse (sq, 1.0f);
                g.drawLine (sq.getX(), sq.getCentreY(), sq.getRight(), sq.getCentreY(), 1.0f);
                g.drawLine (sq.getCentreX(), sq.getY(), sq.getCentreX(), sq.getBottom(), 1.0f);
                g.fillEllipse (sq.withSizeKeepingCentre (sq.getWidth() * 0.32f, sq.getHeight() * 0.62f));
                break;
            }

            case Game::HintView::envelope:
            {
                // Four hits and their decays, mirrored.
                juce::Path p;
                const auto mid = r.getCentreY(), half = r.getHeight() * 0.42f;
                p.startNewSubPath (r.getX(), mid);
                for (int i = 0; i <= 160; ++i)
                {
                    const auto u = (float) i / 160.0f;
                    const auto v = std::exp (-std::fmod (u * 4.0f, 1.0f) * 5.0f) * 0.9f + 0.06f;
                    p.lineTo (r.getX() + u * r.getWidth(), mid - v * half);
                }
                for (int i = 160; i >= 0; --i)
                {
                    const auto u = (float) i / 160.0f;
                    const auto v = std::exp (-std::fmod (u * 4.0f, 1.0f) * 5.0f) * 0.9f + 0.06f;
                    p.lineTo (r.getX() + u * r.getWidth(), mid + v * half);
                }
                p.closeSubPath();
                g.fillPath (p);
                break;
            }

            case Game::HintView::reflections:
            {
                // The direct sound, a few early spikes and a tail.
                const auto base = r.getBottom();
                g.fillRect (r.getX() + r.getWidth() * 0.06f, base - r.getHeight() * 0.9f, 4.0f, r.getHeight() * 0.9f);
                for (int i = 0; i < 7; ++i)
                {
                    const auto x = r.getX() + r.getWidth() * (0.14f + 0.035f * (float) i * (1.0f + 0.3f * (float) (i % 3)));
                    const auto h = r.getHeight() * (0.55f - 0.05f * (float) i);
                    g.fillRect (x, base - h, 3.0f, h);
                }
                juce::Path tail;
                tail.startNewSubPath (r.getX() + r.getWidth() * 0.45f, base);
                for (int i = 0; i <= 60; ++i)
                {
                    const auto u = (float) i / 60.0f;
                    tail.lineTo (r.getX() + r.getWidth() * (0.45f + 0.55f * u), base - r.getHeight() * 0.3f * std::exp (-u * 3.0f));
                }
                tail.lineTo (r.getRight(), base);
                tail.closeSubPath();
                g.fillPath (tail);
                break;
            }

            case Game::HintView::spectrum:
            default:
            {
                // A spectrum's usual slope with a couple of bumps.
                juce::Path p;
                p.startNewSubPath (r.getX(), r.getBottom());
                for (int i = 0; i <= 120; ++i)
                {
                    const auto u = (float) i / 120.0f;
                    const auto v = 0.75f - 0.5f * u + 0.10f * std::sin (u * 11.0f) + 0.06f * std::sin (u * 29.0f);
                    p.lineTo (r.getX() + u * r.getWidth(), r.getBottom() - v * r.getHeight());
                }
                p.lineTo (r.getRight(), r.getBottom());
                p.closeSubPath();
                g.fillPath (p);
                break;
            }
        }
    }

    Game::HintView view = Game::HintView::spectrum;
    juce::String title;
    bool withButton = true;
};
