#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AbcTrainTheme.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AppIcons.h"

// The one thing that appears on the training screen without being asked
// for: a card that slides down from the top edge when an achievement is
// earned, holds long enough to read, and leaves.
//
// It exists because the progress furniture it replaced did not work.
// A level bar is on screen through every round whether or not anything
// happened, and what it reports - distance to the next number - is a fact
// about the app rather than about the player's hearing. This says
// something specific, at the one moment it is true, and then gets out of
// the way. Nothing about it is clickable, so it can never be in the way of
// an answer.
//
// Self-contained: its own timer, its own easing, no state on the editor.
// setInterceptsMouseClicks(false) matters - a card drifting across the top
// of the answer area must not swallow a click meant for the scale under it.
class AchievementToast : public juce::Component,
                          private juce::Timer
{
public:
    AchievementToast()
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (60);
    }

    ~AchievementToast() override { stopTimer(); }

    // `heading` is the small caption ("Achievement unlocked"), `title` the
    // achievement's own name. Showing one while another is still up simply
    // retargets: the newest is the one worth reading, and a queue would
    // mean the second one arrives long after the moment that earned it.
    void show (const juce::String& newHeading, const juce::String& newTitle)
    {
        heading = newHeading;
        title = newTitle;
        holdMs = holdDurationMs;
        target = 1.0f;
        setVisible (true);
        repaint();
    }

    void dismiss()
    {
        holdMs = 0;
        target = 0.0f;
    }

    bool isShowing() const noexcept { return amount > 0.001f; }

    void paint (juce::Graphics& g) override
    {
        if (amount <= 0.001f || title.isEmpty())
            return;

        const auto& theme = AbcTrainTheme::current();
        const auto eased = AbcTrainTheme::Ease::out (amount);

        // Slides down from just above its own bounds as it fades up, so it
        // reads as arriving from off-screen rather than materialising.
        auto bounds = getLocalBounds().toFloat().reduced (1.0f)
                          .translated (0.0f, -(1.0f - eased) * (float) getHeight());

        juce::Path card;
        card.addRoundedRectangle (bounds, AbcTrainTheme::Radius::panel);

        juce::DropShadow (theme.shadow.withAlpha (0.5f * theme.shadowStrength * eased),
                           14, { 0, 4 }).drawForPath (g, card);

        juce::ColourGradient fill (theme.panelBackground.brighter (0.06f),
                                    bounds.getX(), bounds.getY(),
                                    theme.panelBackground, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (fill);
        g.setOpacity (eased);
        g.fillPath (card);
        g.setOpacity (1.0f);

        g.setColour (theme.positive.withAlpha (0.55f * eased));
        g.strokePath (card, juce::PathStrokeType (1.0f));

        auto content = bounds.reduced ((float) AbcTrainTheme::Spacing::medium,
                                        (float) AbcTrainTheme::Spacing::small);

        const auto iconBox = content.removeFromLeft (content.getHeight());
        AppIcons::draw (g, AppIcons::Icon::gain, iconBox.reduced (4.0f),
                         theme.positive.withAlpha (eased));

        content.removeFromLeft ((float) AbcTrainTheme::Spacing::small);

        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (heading),
                                               content.removeFromTop (14.0f),
                                               AbcTrainLookAndFeel::captionFont(),
                                               theme.textDim.withAlpha (eased), 1.6f);

        g.setColour (theme.textBright.withAlpha (eased));
        g.setFont (AbcTrainLookAndFeel::headingFont());
        g.drawText (title, content.toNearestInt(), juce::Justification::centredLeft, true);
    }

private:
    void timerCallback() override
    {
        if (holdMs > 0)
        {
            holdMs -= 1000 / 60;

            if (holdMs <= 0)
            {
                holdMs = 0;
                target = 0.0f;
            }
        }

        if (juce::approximatelyEqual (amount, target))
        {
            // Stop taking space in the hit-test/paint order once it has
            // fully left, rather than sitting invisible over the layout.
            if (target <= 0.0f && isVisible())
                setVisible (false);

            return;
        }

        // Arrives faster than it leaves: you want it there the instant it
        // is earned, and you want time to finish reading it on the way out.
        const auto duration = target > amount ? AbcTrainTheme::Duration::transition
                                              : AbcTrainTheme::Duration::transition * 1.6;
        const auto step = (float) (1000.0 / 60.0 / duration);

        amount = std::abs (target - amount) <= step
                     ? target
                     : amount + (target > amount ? step : -step);

        repaint();
    }

    static constexpr int holdDurationMs = 2600;

    juce::String heading, title;
    float amount = 0.0f;
    float target = 0.0f;
    int holdMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AchievementToast)
};
