#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

#include <cmath>
#include <functional>

// What a phone shows while the app comes up (ADR 060): the launch screen
// is the app's own background colour, and the first frames the app draws
// are the wordmark settling in - "abc" in the three family colours, a
// needle-like sweep - before the whole thing fades onto the first screen.
// A black screen and then everything at once was the alternative.
//
// It sits over the editor and takes no clicks once it has started fading.
class SplashOverlay : public juce::Component,
                      private juce::Timer
{
public:
    std::function<void()> onFinished;

    // The words under the mark, already in the player's language: the
    // three family words, what is happening, and the headphone note.
    void setTexts (juce::StringArray familyWords, juce::String preparing, juce::String note)
    {
        words = std::move (familyWords);
        preparingText = std::move (preparing);
        noteText = std::move (note);
        repaint();
    }

    SplashOverlay()
    {
        setInterceptsMouseClicks (true, false);
    }

    void start()
    {
        elapsedMs = 0.0;
        setAlpha (1.0f);
        setVisible (true);
        toFront (false);
        startTimerHz (60);
    }

    // For the snapshot tool: a still frame `ms` into the animation.
    void showFrame (double ms)
    {
        stopTimer();
        elapsedMs = ms;
        setAlpha (1.0f);
        setVisible (true);
        toFront (false);
        repaint();
    }

    // For the tools that render screens: no splash in a still frame.
    void finish()
    {
        stopTimer();
        setVisible (false);

        if (onFinished != nullptr)
            onFinished();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        g.fillAll (theme.windowBackground);

        const auto bounds = getLocalBounds().toFloat();
        const auto t = (float) (elapsedMs / 1000.0);

        // Three faint waves in the family colours, drifting: sound, before
        // there is any.
        for (int k = 0; k < 3; ++k)
        {
            juce::Path wave;
            const auto baseY = bounds.getCentreY() + 70.0f + (float) k * 40.0f;
            const auto amp = 14.0f - (float) k * 3.0f;

            for (int x = 0; x <= (int) bounds.getWidth(); x += 6)
            {
                const auto y = baseY + amp * std::sin ((float) x * (0.012f + 0.004f * (float) k) + t * (1.2f + 0.3f * (float) k));
                if (x == 0) wave.startNewSubPath ((float) x, y); else wave.lineTo ((float) x, y);
            }

            g.setColour (AbcTrainTheme::accentFor (familyFor (k)).withAlpha (0.35f - 0.07f * (float) k));
            g.strokePath (wave, juce::PathStrokeType (1.4f - 0.2f * (float) k));
        }

        const auto font = AbcTrainLookAndFeel::displayFont().withHeight (54.0f);
        const juce::String letters ("abcTrain");

        auto total = 0.0f;
        for (int i = 0; i < letters.length(); ++i)
            total += AbcTrainLookAndFeel::trackedTextWidth (letters.substring (i, i + 1), font, 0.0f);

        auto area = getLocalBounds().toFloat().withSizeKeepingCentre (total + 2.0f, 70.0f).translated (0.0f, -40.0f);
        auto x = area.getX();

        for (int i = 0; i < letters.length(); ++i)
        {
            const auto letter = letters.substring (i, i + 1);
            const auto width = AbcTrainLookAndFeel::trackedTextWidth (letter, font, 0.0f);
            const auto colour = i < 3 ? AbcTrainTheme::accentFor (familyFor (i)) : theme.textBright;
            const auto reveal = AbcTrainTheme::Ease::out (juce::jlimit (0.0f, 1.0f, (t - 0.08f * (float) i) / 0.35f));

            AbcTrainLookAndFeel::drawTrackedText (g, letter,
                                                   area.withX (x).withWidth (width + 1.0f)
                                                       .translated (0.0f, (1.0f - reveal) * 8.0f),
                                                   font, colour.withAlpha (reveal), 0.0f,
                                                   juce::Justification::centredLeft);
            x += width;
        }

        // The three words, each in its family colour, under the mark.
        if (words.size() == 3)
        {
            const auto font = AbcTrainLookAndFeel::titleFont();
            const auto reveal = AbcTrainTheme::Ease::out (juce::jlimit (0.0f, 1.0f, (t - 0.35f) / 0.4f));
            juce::String line = words.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  "));
            auto row = area.withY (area.getBottom() + 4.0f).withHeight (28.0f)
                           .withSizeKeepingCentre (juce::jmin (bounds.getWidth() - 32.0f, 420.0f), 28.0f);
            auto wx = row.getCentreX() - AbcTrainLookAndFeel::trackedTextWidth (line, font, 1.0f) * 0.5f;

            for (int i = 0; i < 3; ++i)
            {
                const auto word = words[i] + (i < 2 ? juce::String::fromUTF8 ("  \xc2\xb7  ") : juce::String());
                const auto w = AbcTrainLookAndFeel::trackedTextWidth (word, font, 1.0f);
                AbcTrainLookAndFeel::drawTrackedText (g, word, row.withX (wx).withWidth (w + 2.0f), font,
                                                       AbcTrainTheme::accentFor (familyFor (i)).withAlpha (reveal), 1.0f);
                wx += w;
            }
        }

        // At the foot: ten segments that light in turn, what is being
        // done, and the one thing worth knowing before the first round.
        auto foot = bounds.reduced (32.0f, 0.0f).removeFromBottom (120.0f).withTrimmedBottom (40.0f);
        auto segs = foot.removeFromTop (3.0f);
        const auto gap = 3.0f;
        const auto segW = (segs.getWidth() - 9.0f * gap) / 10.0f;

        for (int i = 0; i < 10; ++i)
        {
            const auto phase = std::fmod (t * 1.4f - (float) i * 0.1f + 10.0f, 1.4f);
            const auto lit = phase < 0.5f;
            g.setColour (lit ? theme.accent : theme.outline);
            g.fillRect (juce::Rectangle<float> (segs.getX() + (float) i * (segW + gap), segs.getY(), segW, 3.0f));
        }

        foot.removeFromTop (12.0f);
        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (preparingText), foot.removeFromTop (16.0f),
                                               AbcTrainLookAndFeel::microFont(), theme.textDim, 2.0f);
        foot.removeFromTop (8.0f);
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        AbcTrainLookAndFeel::fitLines (g, noteText, foot.toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
    }

private:
    static AbcTrainTheme::Family familyFor (int index)
    {
        switch (index)
        {
            case 0:  return AbcTrainTheme::Family::space;
            case 1:  return AbcTrainTheme::Family::dynamics;
            default: return AbcTrainTheme::Family::frequency;
        }
    }

    void timerCallback() override
    {
        elapsedMs += 1000.0 / 60.0;

        // About a second on screen, then a quarter of a second to fade:
        // long enough to read, short enough that nobody waits for it.
        constexpr double holdMs = 1000.0, fadeMs = 260.0;

        if (elapsedMs > holdMs)
        {
            setInterceptsMouseClicks (false, false);
            const auto fade = (float) juce::jlimit (0.0, 1.0, (elapsedMs - holdMs) / fadeMs);
            setAlpha (1.0f - AbcTrainTheme::Ease::out (fade));

            if (fade >= 1.0f)
            {
                finish();
                return;
            }
        }

        repaint();
    }

    double elapsedMs = 0.0;
    juce::StringArray words;
    juce::String preparingText, noteText;
};
