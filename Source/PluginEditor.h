#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../shared/UpdateWindow.h"
#include <juce_animation/juce_animation.h>
#include "PluginProcessor.h"
#include "ChoiceSliderComponent.h"
#include "TrainingSoundsComponent.h"
#include "HomeScreenComponent.h"
#include "SupportScreenComponent.h"
#include "SettingsScreenComponent.h"
#include "RunResultsComponent.h"
#include "TopNavComponent.h"
#include "AchievementsScreenComponent.h"
#include "../shared/TourOverlay.h"
#include "../shared/IdleScreensaver.h"
#include "SessionManager.h"
#include "AchievementToast.h"
#include "../shared/UpdateChecker.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/CompactSelector.h"
#include "../shared/AppIcons.h"
#include "../shared/Vectorscope.h"
#include "../shared/WaveformDisplay.h"
#include "../shared/SpectrumAnalyzer.h"
#include "../shared/i18n/LocalisationManager.h"
#include <cmath>

// Generic multiple-choice UI driven entirely by the active Game's
// interface (name/instructions/choice count/labels/feedback). Adding a
// new game to GameManager needs no changes here.
class EarTrainerEditor : public juce::AudioProcessorEditor,
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    explicit EarTrainerEditor (EarTrainerProcessor&);
    ~EarTrainerEditor() override;

    // For tools/EditorSnapshots only - see SupportScreenComponent::
    // completeReveal for why a rendered still needs this.
    void completeWelcomeReveal() { supportScreen.completeReveal(); }

    // Also for tools/EditorSnapshots: opens a training so the screenshot
    // tool can photograph it. There is no other way in from outside - the
    // only route is a mouse click on a home-screen tile - and a product
    // gallery that cannot show the screen people spend all their time on
    // is not much of a gallery.
    // Opens the training-sounds window for the screenshot tool, for the
    // same reason as the training screen: it is a whole screen the gallery
    // could not otherwise show.
    // For tools/EditorSnapshots: a run-results screen with plausible
    // numbers in it, since there is no way to lose a Survival run from
    // outside and this is a whole screen the gallery could not show.
    void showRunResultsForSnapshot()
    {
        session.setMode (SessionManager::Mode::survival);
        showScreen (Screen::training);

        for (int i = 0; i < 14; ++i)
            session.registerAnswer (i % 4 != 3);

        // Seed the miss map through the real counter, not by writing the
        // summary. An empty map is a legitimate state - it is what a fresh
        // profile sees - but it is not the state worth looking at on a
        // contact sheet, and rendering only the empty one is how a feature
        // ships looking like it does nothing. Weighted so the misses pile
        // up in the mids, which is both realistic and the case the verdict
        // sentence exists for.
        {
            auto& progress = processor.getProgressManager();
            const auto index = processor.getGameManager().getActiveGameIndex();
            const int attempts[] = { 6, 9, 12, 22, 18, 8, 5 };
            const int misses[]   = { 1, 2,  4, 13,  9, 2, 1 };

            for (int b = 0; b < 7; ++b)
                for (int i = 0; i < attempts[b]; ++i)
                    progress.registerAnswer (index, i >= misses[b], 1.0f, b);
        }

        pendingPreviousBest = 9;
        showRunResults (11);
        runResults.completeAnimation();
    }

    void openAchievementsForSnapshot() { showAchievementsScreen(); }

    // The screen a player lands on after the welcome, and - until this
    // was added - the only screen in the product with no snapshot at all.
    // Everything else had one, so the one screen that decides whether
    // anybody starts a session was the one nobody was looking at.
    void openHomeForSnapshot()
    {
        showScreen (Screen::home);
    }

    void offerTourForSnapshot()
    {
        supportScreen.setTourOffer (localisation.getText ("tour.offer"),
                                     localisation.getText ("tour.accept"),
                                     localisation.getText ("tour.decline"));
        showScreen (Screen::support);
    }

    void openScreensaverForSnapshot()
    {
        screensaver.setVisible (true);
        screensaver.toFront (false);
    }

    void openTourForSnapshot()
    {
        startTour();
        tour.completeAnimation();
    }

    void openSettingsForSnapshot()
    {
        settingsScreen.setVisible (true);
        settingsScreen.refresh();
        settingsScreen.toFront (false);
    }

    void openSoundsForSnapshot()
    {
        trainingSounds.setVisible (true);
        trainingSounds.refresh();
        trainingSounds.toFront (false);
    }

    // Snapshot seam: the hint costs points, so there is no way to
    // photograph it from outside - and "the hint now matches the
    // exercise" is a claim about three different pictures.
    void revealHintForSnapshot()
    {
        // Through requestHint(), not by setting the flag.
        //
        // It used to set hintRevealed directly, which was fine while a
        // hint was purely a panel becoming visible. It is not any more -
        // on a ruler exercise a hint narrows the scale, and that region is
        // computed where the hint is bought. Setting the flag by hand
        // produced a contact sheet showing "hint bought" with no hint in
        // it, which is a bug in the seam pretending to be a bug in the
        // product. Practice never refuses, so this always succeeds.
        requestHint();
        applyWindowSize();
    }

    void openTrainingForSnapshot (int gameIndex)
    {
        processor.getGameManager().setActiveGameIndex (gameIndex);
        showScreen (Screen::training);
        refreshFromGameState();

        // The A/B captions belong to the *game*, and this seam skipped
        // them - so the contact sheet showed "EQ off / EQ on" over a
        // reverb exercise. The real app refreshes them inside
        // startNewRun(); a snapshot never starts a run.
        refreshBeforeAfter();
    }

    // Snapshot seam for the answered state - the moment stage 2 of ADR
    // 029 exists for, so it must be photographable. Submits a deliberate
    // near-miss so the shot shows the guess, the target *and* the band
    // between them; the tool never pumps a message loop, so the async
    // ProgressManager listener never fires and no real progress is
    // written.
    void answerForSnapshot()
    {
        auto& game = processor.getGameManager().getActiveGame();
        game.newRound();

        // Refresh *before* answering. In the real app a new round arrives
        // through the async ChangeBroadcaster, so the panel has already
        // been rebuilt by the time anyone can click; here the two ran back
        // to back and refreshFromGameState() deliberately skips a rebuild
        // once the round has been answered, so the shot showed the
        // previous round's two names beside this round's verdict.
        refreshFromGameState();

        if (game.usesContinuousScale())
            game.submitNormalisedAnswer (juce::jlimit (0.0f, 1.0f, game.getCorrectNormalised() + 0.18f));
        else
            game.submitAnswer (0);

        refreshFromGameState();
        choiceSlider.completeAnimation();
    }

    // Snapshot seam for a Survival run in progress: the HUD replaces the
    // mode pills, so it is a different screen and needs its own shot.
    // Started directly rather than through beginRunWithCountdown(), since
    // the countdown needs a message loop the snapshot tool never pumps.
    void startRunForSnapshot (SessionManager::Mode mode)
    {
        session.setMode (mode);
        runStarted = true;
        startNewRun();
        session.registerAnswer (true);
        session.registerAnswer (false);   // one heart gone, so the HUD shows both states
        refreshRunStatus();
        resized();
    }

    void paint (juce::Graphics&) override;
    void resized() override;

    // Space flips A/B. It is the control a player touches most - once or
    // twice every round, every round - and reaching for the mouse to do it
    // breaks the one thing the screen is for, which is listening.
    bool keyPressed (const juce::KeyPress&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void paintOverChildren (juce::Graphics&) override;

    // Where update progress and outcomes go on this editor. The Learner
    // plugins have a guide card to put a line of text in; this one has the
    // Updates button's own tooltip, which is small but is at least where
    // the pointer already is.
    void showUpdateOutcome (const juce::String& text);

private:
    // Plain filled-rectangle progress bar, its fill now eased via JUCE's
    // own juce_animation module (Animator/ValueAnimatorBuilder) instead
    // of snapping straight to the new value - the first step of the
    // phased animation pass described in decisions/013-ui-libraries.md.
    // Also "breathes" continuously - a slow, low-amplitude glow pulse at
    // the leading edge of the fill via a plain Timer, independent of the
    // eased fill-transition Animator above (see decisions/018).
    class LevelProgressBar : public juce::Component,
                              private juce::Timer
    {
    public:
        LevelProgressBar() { startTimerHz (30); }
        ~LevelProgressBar() override { stopTimer(); }

        void setProgress (float newProgress) noexcept
        {
            const auto target = juce::jlimit (0.0f, 1.0f, newProgress);
            if (juce::approximatelyEqual (target, targetProgress))
                return;

            targetProgress = target;

            // Where the bar actually *is* right now - read before the
            // complete() below, which jumps the outgoing animation to its
            // own end value. Reading it afterwards meant a retarget while
            // one was still running snapped the fill to the previous
            // target and only then eased to the new one: a visible jump
            // in the middle of what should be one continuous move. It
            // only shows up when two answers land inside 400 ms, which is
            // routine in Blitz and rare enough elsewhere to have hidden.
            const auto startValue = displayedProgress;

            // Fast-track any still-running animation to completion, so its
            // own onComplete callback removes *itself* from `updater`
            // before `currentAnimator` gets reassigned below - otherwise
            // that callback (which reads the member by name, not by
            // captured value) would end up removing the new animator
            // instead of the one it actually belongs to.
            if (! currentAnimator.isComplete())
                currentAnimator.complete();

            const auto endValue = target;

            currentAnimator = juce::ValueAnimatorBuilder{}
                                   .withEasing (juce::Easings::createEaseOut())
                                   .withDurationMs (400.0)
                                   .withValueChangedCallback ([this, startValue, endValue] (float t)
                                   {
                                       displayedProgress = startValue + (endValue - startValue) * t;
                                       repaint();
                                   })
                                   .build();

            updater.addAnimator (currentAnimator, [this] { updater.removeAnimator (currentAnimator); });
            currentAnimator.start();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& theme = AbcTrainTheme::current();
            auto bounds = getLocalBounds().toFloat();
            const auto radius = bounds.getHeight() * 0.5f;

            // Recessed groove, same treatment as the choice slider's track
            // and the linear sliders - one consistent idea of what "a
            // channel something sits in" looks like across the whole UI.
            juce::ColourGradient trackGradient (theme.displayBackground.darker (0.12f), bounds.getX(), bounds.getY(),
                                                 theme.widgetBackground, bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill (trackGradient);
            g.fillRoundedRectangle (bounds, radius);
            g.setColour (theme.outline.withAlpha (0.6f));
            g.drawRoundedRectangle (bounds, radius, 1.0f);

            if (displayedProgress <= 0.001f)
                return;

            // The fill can never be narrower than its own corner radius or
            // it stops being a rounded shape - so the first few percent
            // are shown by fading a minimum-width pill in rather than
            // popping one into existence at full opacity.
            const auto minWidth = bounds.getHeight();
            const auto naturalWidth = bounds.getWidth() * displayedProgress;
            auto fillBounds = bounds.withWidth (juce::jmax (minWidth, naturalWidth));
            const auto emergence = juce::jlimit (0.0f, 1.0f, naturalWidth / minWidth);

            juce::ColourGradient fillGradient (theme.accent.darker (0.1f), fillBounds.getX(), fillBounds.getY(),
                                                theme.accent.brighter (0.2f), fillBounds.getRight(), fillBounds.getY(),
                                                false);
            g.setGradientFill (fillGradient);
            g.setOpacity (AbcTrainTheme::Ease::out (emergence));
            g.fillRoundedRectangle (fillBounds, radius);
            g.setOpacity (1.0f);

            // The "breathing" glow: a soft, slowly-pulsing highlight right
            // at the leading edge of the fill - only drawn once there's
            // real progress to show, so an empty bar at level 1 doesn't
            // pulse for no reason.
            if (displayedProgress > 0.02f)
            {
                // Faded in over the same first stretch as the fill, so the
                // glow doesn't switch on the instant progress crosses 2%.
                const auto glowFade = juce::jlimit (0.0f, 1.0f, (displayedProgress - 0.02f) * 25.0f);
                const auto glowAlpha = glowFade * (0.10f + 0.12f * (0.5f + 0.5f * std::sin (breathPhase)));
                const auto glowWidth = juce::jmin (20.0f, fillBounds.getWidth());
                auto glowBounds = fillBounds.removeFromRight (glowWidth);
                g.setColour (theme.textBright.withAlpha (glowAlpha));
                g.fillRoundedRectangle (glowBounds, radius);
            }
        }

    private:
        void timerCallback() override
        {
            // A full slow cycle roughly every ~3.4s (2*pi / 0.03 rad per
            // 33ms tick at 30Hz) - unhurried, "calm" pacing rather than a
            // nervous flicker.
            breathPhase += 0.03f;
            if (breathPhase > juce::MathConstants<float>::twoPi)
                breathPhase -= juce::MathConstants<float>::twoPi;

            // Nothing breathes on an empty bar, so nothing needs
            // repainting either - this timer used to redraw the widget
            // 30 times a second for a picture that never changed.
            if (displayedProgress > 0.02f)
                repaint();
        }

        float targetProgress = 0.0f;
        float displayedProgress = 0.0f;
        float breathPhase = 0.0f;

        // Animator has no default constructor (only explicit
        // Animator(shared_ptr<Impl>)) - this placeholder is never
        // started, just a valid object to overwrite the first time
        // setProgress() actually runs.
        juce::Animator currentAnimator = juce::ValueAnimatorBuilder{}.build();
        juce::VBlankAnimatorUpdater updater { this };
    };

    // "+12" that leaps from the answer and drifts upward as it fades: the
    // one place in the loop where earned points should feel *gained*
    // rather than bookkept. Until this existed, the only trace of an
    // answer's worth was a label quietly holding a different number.
    // Non-interactive; a new award landing mid-flight simply restarts it -
    // the newest number is the one that matters, same rule as the toast.
    class PointsFlyup : public juce::Component
    {
    public:
        PointsFlyup() { setInterceptsMouseClicks (false, false); }

        void show (const juce::String& newText, juce::Colour newColour)
        {
            text = newText;
            colour = newColour;

            if (! animator.isComplete())
                animator.complete();

            animator = juce::ValueAnimatorBuilder{}
                           .withEasing (juce::Easings::createEaseOut())
                           .withDurationMs (1050.0)
                           .withValueChangedCallback ([this] (float t)
                           {
                               progress = t;
                               repaint();
                           })
                           .build();

            updater.addAnimator (animator, [this]
            {
                updater.removeAnimator (animator);
                setVisible (false);
            });

            setVisible (true);
            toFront (false);
            animator.start();
        }

        void paint (juce::Graphics& g) override
        {
            if (text.isEmpty())
                return;

            // Rises through the component; fades only over the last part,
            // so the number is readable for most of the flight.
            const auto travel = (float) getHeight() - 26.0f;
            const auto y = travel * (1.0f - progress);
            const auto alpha = progress < 0.6f ? 1.0f
                                               : juce::jmax (0.0f, 1.0f - (progress - 0.6f) / 0.4f);

            g.setColour (colour.withAlpha (alpha));
            g.setFont (AbcTrainLookAndFeel::titleFont());
            g.drawText (text,
                        juce::Rectangle<float> (0.0f, y, (float) getWidth(), 26.0f),
                        juce::Justification::centred, false);
        }

    private:
        juce::String text;
        juce::Colour colour;
        float progress = 0.0f;
        juce::Animator animator = juce::ValueAnimatorBuilder{}.build();
        juce::VBlankAnimatorUpdater updater { this };
    };

    // The 3-2-1 before a Survival/Blitz run. A run used to start the
    // instant a pill was clicked, which is how a checkbox behaves, not how
    // an event begins - there was no moment where you gather yourself, so
    // a run never felt like something you *entered*. Covers the answer
    // area (and swallows its clicks) for under two seconds; Practice
    // never sees it, because Practice is the mode without pressure.
    class RunCountdown : public juce::Component,
                          private juce::Timer
    {
    public:
        RunCountdown() { setInterceptsMouseClicks (true, true); }
        ~RunCountdown() override { stopTimer(); }

        void start (const juce::String& newCaption, std::function<void()> newOnDone)
        {
            caption = newCaption;
            onDone = std::move (newOnDone);
            elapsedMs = 0.0;
            setVisible (true);
            toFront (false);
            startTimerHz (30);
            repaint();
        }

        // Screen changes mid-countdown abandon the run start entirely -
        // firing onDone into a screen the player has left would begin a
        // run nobody is looking at.
        void cancel()
        {
            stopTimer();
            onDone = nullptr;
            setVisible (false);
        }

        void paint (juce::Graphics& g) override
        {
            const auto& theme = AbcTrainTheme::current();

            g.setColour (theme.windowBackground.withAlpha (0.72f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), AbcTrainTheme::Radius::panel);

            const auto step = (int) (elapsedMs / stepMs);           // 0,1,2
            const auto digit = juce::jmax (1, 3 - step);
            const auto within = (float) (elapsedMs - (double) step * stepMs) / (float) stepMs;

            // Each digit lands (scales down into place) then holds; the
            // fade-out belongs to the next digit's arrival, not to it.
            const auto eased = AbcTrainTheme::Ease::out (juce::jlimit (0.0f, 1.0f, within * 2.2f));
            const auto scale = 1.35f - 0.35f * eased;

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::headingFont());
            g.drawText (caption,
                        getLocalBounds().toFloat().withTrimmedTop ((float) getHeight() * 0.22f)
                                                   .withHeight (20.0f),
                        juce::Justification::centred, false);

            auto digitFont = AbcTrainLookAndFeel::displayFont();
            digitFont = digitFont.withHeight (digitFont.getHeight() * 2.2f * scale);
            g.setColour (theme.textBright.withAlpha (0.35f + 0.65f * eased));
            g.setFont (digitFont);
            g.drawText (juce::String (digit), getLocalBounds().toFloat(),
                        juce::Justification::centred, false);
        }

    private:
        void timerCallback() override
        {
            elapsedMs += 1000.0 / 30.0;

            if (elapsedMs >= stepMs * 3.0)
            {
                stopTimer();
                setVisible (false);

                // Moved off before calling: onDone may restart this very
                // countdown (play-again flows), and a callback that
                // clears itself mid-call is a callback that vanishes
                // under its own feet.
                auto done = std::move (onDone);
                onDone = nullptr;
                if (done != nullptr)
                    done();
                return;
            }

            repaint();
        }

        static constexpr double stepMs = 600.0;
        juce::String caption;
        std::function<void()> onDone;
        double elapsedMs = 0.0;
    };

    // Lives, clock and run score while a Survival/Blitz run is live -
    // *instead of* the mode pills, not beside them. A run you can silently
    // re-mode mid-flight is a setting; hiding the pills for the duration
    // is what makes a run something you finish (or walk out of via Home).
    class RunHud : public juce::Component
    {
    public:
        void set (SessionManager::Mode newMode, int newLives, int newSeconds, int newScore)
        {
            if (newLives < lives && lives >= 0)
                flashLostLife();

            mode = newMode;
            lives = newLives;
            seconds = newSeconds;
            score = newScore;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& theme = AbcTrainTheme::current();
            auto area = getLocalBounds().toFloat();

            // Run score first: it is the number the run is *about*.
            g.setColour (theme.textBright);
            g.setFont (AbcTrainLookAndFeel::titleFont());
            const auto scoreBox = area.removeFromLeft (52.0f);
            g.drawText (juce::String (score), scoreBox, juce::Justification::centredLeft, false);

            if (mode == SessionManager::Mode::survival)
            {
                // Hearts, drawn - the vocabulary every game player already
                // reads. The most recently lost one flashes out.
                const auto r = 9.0f;
                auto x = area.getX() + r;
                const auto cy = area.getCentreY();

                for (int i = 0; i < SessionManager::survivalLives; ++i)
                {
                    const auto alive = i < lives;
                    const auto justLost = (i == lives) && flash > 0.001f;

                    auto heart = heartPath (x, cy, r * (justLost ? 1.0f + 0.6f * flash : 1.0f));

                    if (alive)
                    {
                        g.setColour (theme.negative);
                        g.fillPath (heart);
                    }
                    else if (justLost)
                    {
                        g.setColour (theme.negative.withAlpha (flash * 0.9f));
                        g.fillPath (heart);
                    }
                    else
                    {
                        g.setColour (theme.outline);
                        g.strokePath (heart, juce::PathStrokeType (1.4f));
                    }

                    x += r * 2.0f + 7.0f;
                }
            }
            else if (mode == SessionManager::Mode::blitz)
            {
                const auto urgent = seconds <= 15;
                g.setColour (urgent ? theme.negative : theme.text);
                g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (
                    AbcTrainLookAndFeel::monoFontHeight * AbcTrainLookAndFeel::getTextScale() * 1.35f));
                g.drawText (juce::String::formatted ("%d:%02d", seconds / 60, seconds % 60),
                            area, juce::Justification::centredLeft, false);
            }
        }

    private:
        void flashLostLife()
        {
            if (! flashAnimator.isComplete())
                flashAnimator.complete();

            flashAnimator = juce::ValueAnimatorBuilder{}
                                .withEasing (juce::Easings::createEaseOut())
                                .withDurationMs (450.0)
                                .withValueChangedCallback ([this] (float t)
                                {
                                    flash = 1.0f - t;
                                    repaint();
                                })
                                .build();
            updater.addAnimator (flashAnimator, [this] { updater.removeAnimator (flashAnimator); });
            flashAnimator.start();
        }

        static juce::Path heartPath (float cx, float cy, float r)
        {
            juce::Path p;
            p.startNewSubPath (cx, cy + r * 0.95f);
            p.cubicTo (cx - r * 1.6f, cy - r * 0.1f, cx - r * 0.9f, cy - r * 1.1f, cx, cy - r * 0.35f);
            p.cubicTo (cx + r * 0.9f, cy - r * 1.1f, cx + r * 1.6f, cy - r * 0.1f, cx, cy + r * 0.95f);
            p.closeSubPath();
            return p;
        }

        SessionManager::Mode mode = SessionManager::Mode::practice;
        int lives = -1, seconds = 0, score = 0;
        float flash = 0.0f;
        juce::Animator flashAnimator = juce::ValueAnimatorBuilder{}.build();
        juce::VBlankAnimatorUpdater updater { this };
    };

    // The two reasons to come back tomorrow, on the screen you plan from.
    //
    // Both existed before this and both were a line of 12px text above the
    // grid: "Today: 5 in a row on X" and "7-day streak". That is a
    // footnote reporting the two things the whole return loop rests on.
    // Here the streak is a row of days you can see accumulating - the
    // point of a streak is the visible cost of breaking it - and the
    // challenge is progress toward a reward rather than a fact that flips
    // at the end.
    //
    // Deliberately not a "goal for today" or a nag: nothing here asks for
    // anything, it only shows where you already are.
    class DailyBanner : public juce::Component
    {
    public:
        struct State
        {
            int streakDays = 0;
            juce::String streakCaption;      // "7-day streak"
            juce::String challengeLine;      // "5 in a row on Guess the Reverb"
            int challengeDone = 0;
            int challengeTarget = 0;
            int bonusPoints = 0;
            bool challengeComplete = false;
            juce::Colour challengeAccent;
        };

        void setState (State s)
        {
            state = std::move (s);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& theme = AbcTrainTheme::current();
            auto area = getLocalBounds().toFloat();

            AbcTrainLookAndFeel::paintRaisedCard (g, area);

            area = area.reduced (AbcTrainTheme::Spacing::medium,
                                 AbcTrainTheme::Spacing::small);

            // --- right: the streak, as days rather than as a sentence ---
            auto streakArea = area.removeFromRight (juce::jmin (200.0f, area.getWidth() * 0.42f));
            {
                auto dots = streakArea.removeFromBottom (14.0f);
                auto caption = streakArea;

                g.setColour (state.streakDays > 0 ? theme.accentWarm : theme.textDim);
                g.setFont (AbcTrainLookAndFeel::monoFont());
                g.drawText (state.streakCaption, caption, juce::Justification::centredRight, false);

                // Seven days, the most recent on the right. Derived from
                // the streak count, never invented: a streak of 3 fills
                // exactly the last three.
                constexpr int shown = 7;
                const auto r = 3.5f;
                const auto gap = 7.0f;
                auto x = dots.getRight() - r;

                for (int i = 0; i < shown; ++i)
                {
                    const auto filled = i < juce::jmin (shown, state.streakDays);

                    if (filled)
                    {
                        g.setColour (theme.accentWarm);
                        g.fillEllipse (x - r, dots.getCentreY() - r, r * 2.0f, r * 2.0f);
                    }
                    else
                    {
                        g.setColour (theme.outline);
                        g.drawEllipse (x - r, dots.getCentreY() - r, r * 2.0f, r * 2.0f, 1.0f);
                    }

                    x -= r * 2.0f + gap;
                }
            }

            area.removeFromRight (AbcTrainTheme::Spacing::medium);

            // --- left: today's challenge, with how far in you are -------
            {
                auto pips = area.removeFromBottom (14.0f);
                auto line = area;

                g.setColour (state.challengeComplete ? theme.positive : theme.text);
                g.setFont (AbcTrainLookAndFeel::bodyFont());
                g.drawText (state.challengeLine, line, juce::Justification::centredLeft, true);

                const auto r = 3.5f;
                const auto gap = 7.0f;
                auto x = pips.getX() + r;

                for (int i = 0; i < state.challengeTarget; ++i)
                {
                    const auto done = state.challengeComplete || i < state.challengeDone;
                    const auto hue = state.challengeComplete ? theme.positive : state.challengeAccent;

                    if (done)
                    {
                        g.setColour (hue);
                        g.fillEllipse (x - r, pips.getCentreY() - r, r * 2.0f, r * 2.0f);
                    }
                    else
                    {
                        g.setColour (theme.outline);
                        g.drawEllipse (x - r, pips.getCentreY() - r, r * 2.0f, r * 2.0f, 1.0f);
                    }

                    x += r * 2.0f + gap;
                }

                // The reward, next to what earns it - a bonus you only
                // find out about after the fact is not an incentive.
                if (state.bonusPoints > 0)
                {
                    const auto rewardBox = pips.withLeft (x + AbcTrainTheme::Spacing::small);
                    g.setColour (state.challengeComplete ? theme.positive : theme.textDim);
                    g.setFont (AbcTrainLookAndFeel::microFont());
                    g.drawText ("+" + juce::String (state.bonusPoints),
                                rewardBox, juce::Justification::centredLeft, false);
                }
            }
        }

    private:
        State state;
    };

    // The promotion test, visible: one pip per required consecutive
    // correct answer. The mechanic existed and mattered - five in a row
    // takes the level - but lived only in a line of text, which is why a
    // player could be two answers from a level-up and not feel it.
    class PromotionPips : public juce::Component
    {
    public:
        void set (int newFilled, int newTotal)
        {
            if (newFilled == filled && newTotal == total)
                return;

            const auto gained = newFilled > filled;
            filled = newFilled;
            total = newTotal;

            // Pop the newest pip in; a reset (wrong answer during the
            // test) just repaints - the emptying itself is the message.
            if (gained)
            {
                if (! popAnimator.isComplete())
                    popAnimator.complete();

                popAnimator = juce::ValueAnimatorBuilder{}
                                  .withEasing (juce::Easings::createEaseOutBack())
                                  .withDurationMs (260.0)
                                  .withValueChangedCallback ([this] (float t)
                                  {
                                      pop = t;
                                      repaint();
                                  })
                                  .build();
                updater.addAnimator (popAnimator, [this] { updater.removeAnimator (popAnimator); });
                popAnimator.start();
            }

            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            if (total <= 0)
                return;

            const auto& theme = AbcTrainTheme::current();
            const auto radius = 4.0f;
            const auto gap = 6.0f;
            const auto span = (float) total * radius * 2.0f + (float) (total - 1) * gap;
            auto x = ((float) getWidth() - span) * 0.5f + radius;
            const auto y = (float) getHeight() * 0.5f;

            for (int i = 0; i < total; ++i)
            {
                const auto isNewest = (i == filled - 1);
                const auto scale = isNewest ? 0.6f + 0.4f * pop : 1.0f;
                const auto r = radius * scale;

                if (i < filled)
                {
                    // createEaseOutBack overshoots past 1.0 - that is the
                    // pop - so anything derived from it must be clamped
                    // before it becomes an alpha.
                    g.setColour (theme.accent.withAlpha (juce::jlimit (0.0f, 1.0f, 0.35f * (isNewest ? pop : 1.0f))));
                    g.fillEllipse (x - r - 2.0f, y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);
                    g.setColour (theme.accent);
                    g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
                }
                else
                {
                    g.setColour (theme.outline);
                    g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
                }

                x += radius * 2.0f + gap;
            }
        }

    private:
        int filled = 0, total = 0;
        float pop = 1.0f;
        juce::Animator popAnimator = juce::ValueAnimatorBuilder{}.build();
        juce::VBlankAnimatorUpdater updater { this };
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Pushes the active palette into every widget that sets its own colours
    // explicitly (the LookAndFeel can't reach those). Called at construction
    // and again whenever the theme is toggled.
    void applyTheme();
    void toggleTheme();

    void timerCallback() override;          // 1 Hz, drives the Blitz clock

    void modeSelected();
    void startNewRun();
    void refreshRunStatus();

    void refreshFromGameState();
    void refreshFromProgressState();
    void refreshLocalisedText();
    void rebuildGameSelectorItems();
    // Home <-> Training. The editor used to be one flat panel with a
    // picker overlay dropped on top; these are real screens now, and
    // exactly one is visible at a time.
    enum class Screen { support, home, training };
    void showScreen (Screen);
    void rebuildHomeSections();
    bool choiceSliderMatchesGame (Game& game) const;
    void rebuildChoiceSlider();
    void choiceButtonClicked (int choiceIndex);

    // Shared tail of both answer paths (discrete index and continuous
    // value): scoring, run state, and scheduling the auto-advance.
    void afterAnswer (bool wasCorrect);

    // Buys and reveals the scope hint, or explains why it can't be
    // afforded. See SessionManager::spendHint for the price per mode.
    void requestHint();
    void refreshHintButton();

    // Puts the hint back behind its price. Called from every path that
    // starts a round, including the auto-advance - which is why it is its
    // own method rather than a few lines inside startNewRun().
    void clearHint();
    void languageSelected();

    // Declared first so it's constructed before, and destroyed after,
    // every other Component below that might still reference it during
    // teardown - see the class comment on AbcTrainLookAndFeel.
    AbcTrainLookAndFeel lookAndFeel;

    // Also declared early, before any Component that reads localised
    // text during construction. localisationProperties must outlive
    // localisation (which holds a reference to it), hence that order.
    juce::PropertiesFile localisationProperties;
    LocalisationManager localisation;

    EarTrainerProcessor& processor;

    juce::Label titleLabel;
    CompactSelector languageSelector;
    // Icon for whichever game is currently selected (see AppIcons) - kept
    // in sync with the active game in refreshFromGameState(), so a pick
    // from the card grid or a difficulty-driven change both update it.
    AppIconComponent gameIcon;

    // Opens the card grid. Replaces the plain ComboBox list of nine game
    // names, which told a player nothing about what each exercise was for
    // or how they were doing at it - see GamePickerComponent.
    // Back to Home from the training screen.
    // A labelled button, not a house glyph: "how do I get back" is the
    // question a lost player asks, and an icon they have to decode is the
    // wrong shape for the answer.
    juce::Label currentGameLabel;
    juce::Label instructionLabel;
    juce::Label scoreLabel;

    // "До уровня 2: 20 / 100". Lives up in the exercise header rather than
    // in the crowded bottom row, where it had 84px and wrapped onto three
    // clipped lines - a progress readout that will not tell you the
    // progress. It also belongs beside the exercise's name: it is a fact
    // about this exercise, not about the run.
    juce::Label levelProgressLabel;
    juce::Label feedbackLabel;

    // The moment of answer, made visible (stage 2 of ADR 029): what the
    // answer earned, and how far into the promotion test this exercise is.
    PointsFlyup pointsFlyup;
    PromotionPips promotionPips;
    void handleAnswerScored (int gameIndex, const ProgressManager::AnswerOutcome&);

    // Collapses the instruction paragraph once an exercise is familiar
    // (a handful of lifetime correct answers), reclaiming the space for
    // the answer scale; the "?" button brings it back for re-reading.
    // Session-scoped on purpose - "show me again" is a moment, not a
    // setting.
    IconButton instructionsButton { AppIcons::Icon::help };
    bool instructionsPinnedOpen = false;
    bool shouldShowInstructions() const;
    // No "New Round" button: rounds advance on their own, and a button
    // that only duplicates something automatic is a button to remove.
    // This one exists *only* while a Survival/Blitz run is over, which is
    // the one moment nothing advances by itself.
    juce::TextButton restartButton;

    // The scope hint: a vectorscope (where it sits in the stereo field,
    // and whether it's in phase) over a spectrum (where the energy is).
    // Hidden until bought, and reset on every new round so the previous
    // round's picture can't linger into the next one.
    // A/B: the untreated signal against the treated one. Two buttons
    // rather than one toggle, so both states are always named and which
    // one you're hearing is readable without interpreting a label that
    // changes.
    juce::TextButton beforeButton, afterButton;
    void setPlayProcessed (bool);
    void refreshBeforeAfter();

    // One labelled button carrying its own price, rather than a bare glyph
    // plus a caption beside it.
    juce::TextButton hintButton;
    static constexpr int modeRadioGroup = 8201;
    Vectorscope vectorscope;
    SpectrumAnalyzerComponent hintSpectrum;
    WaveformDisplay hintWaveform;
    bool hintRevealed = false;

    // Which of the three the active exercise wants. One place, because
    // resized() and the visibility pass both need the same answer and two
    // copies of that switch would eventually disagree.
    Game::HintView activeHintView() const;

    // The hint panel does not exist until it is bought, and the window
    // grows to make room for it.
    //
    // It used to be reserved permanently - laid out on every round,
    // dimmed and captioned "not bought yet" - specifically so revealing a
    // hint could never resize the window under the player. That trade was
    // wrong: a training screen should hold the thing you are answering
    // with and nothing else, and a strip of grey placeholder sitting
    // there every round is worse than a window that changes size on the
    // rare occasions you actually buy one.
    static constexpr int hintRowHeight = 82;
    static constexpr int hintPanelHeight = hintRowHeight + AbcTrainTheme::Spacing::large * 2;

    // The *starting* logical size, and the floor the layout is designed
    // against - not a fixed size any more. The window is resizable, and
    // resized() spends whatever it is given rather than assuming this.
    //
    // uiScale still multiplies on top through a transform, because that is
    // a different question: "this window is small on a 4K display" is
    // answered by scaling the whole design, "I want to see more at once" by
    // dragging the corner.
    //
    // 1180 x 880 is the size the design mockup is drawn at, and it is not
    // a round number somebody liked: it is what the content needs once the
    // navigation is a bar across the top rather than a rail down the side.
    // Four exercise cards fit on a row at 276 each, the training screen's
    // control row gets its five groups side by side instead of stacked,
    // and the scale gets the full width - which is the one measurement in
    // this app where width *is* resolution, since the whole answer is
    // "where along this line".
    //
    // Still resizable, and uiScale still multiplies on top, for the
    // laptop screen where 880 is most of the height there is.
    static constexpr int logicalWidth = 1180;
    static constexpr int logicalBaseHeight = 880;

    // Wider than it was, because the rail takes 156 from the left and the
    // training screen's control row genuinely needs about 640: pills,
    // session tally, the A/B pair and the hint button, side by side. 840
    // is not an arbitrary bump - it is what LearnerComp already opens at,
    // so the four plugins now agree about how wide a window of theirs is.

    // Constant. The window does not change size when a hint appears.
    //
    // It used to grow by exactly the hint panel and shrink back again,
    // which is a defensible trade on paper and a bad one in a DAW: a
    // plugin window that jumps 122px while you are looking at it makes
    // every other window on the screen move, and it happens at the moment
    // the player has just spent something and is trying to read a picture.
    // The height is reserved instead, and the answer section - the only
    // thing here that genuinely benefits from more room and does not
    // suffer from less - is what lends it.
    int getLogicalHeight() const noexcept { return logicalBaseHeight; }

    void applyWindowSize();

    // The height the window has *without* the hint panel - either the
    // design height or whatever it was dragged to.
    //
    // Needed because two requirements collide: a manual resize must
    // survive, and the window must shrink again when the hint closes. The
    // first version satisfied only the first, with jmax(needed, current) -
    // which grows for the panel and then can never come back down, because
    // the current height *is* the grown one. Remembering the height
    // separately is what lets both be true.
    int heightWithoutHint = logicalBaseHeight;

    void setUiScale (float newScale);
    float uiScale = 1.0f;
    CompactSelector sizeSelector;

    // Output level. Deliberately a plain slider rather than a
    // CompactSelector: volume is the one control here you want to nudge
    // and hear, and a popup menu of fixed steps is the wrong shape for
    // that. The processor applies it after everything, smoothed - see
    // EarTrainerProcessor::setOutputGainDb for why "after everything"
    // matters to the exercises.
    juce::Slider volumeSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    AppIconComponent volumeIcon;
    void applyVolumeFromSlider();

    // Navigation across the top, permanently. See TopNavComponent for why
    // it moved off the left edge, and why the bottom of a window was the
    // wrong place for it before that.
    TopNavComponent topNav;
    void refreshRailStatus();

    // One second of real practice, and the one place the support ask can
    // ever be triggered from.
    void countPracticeSecond();

    // True where a bought hint shades the ruler rather than opening an
    // analyser - i.e. every exercise answered on a scale.
    bool hintNarrowsTheScale() const
    {
        return processor.getGameManager().getActiveGame().getHintHalfWidthNormalised() > 0.0f;
    }

    juce::Random hintRandom;
    float hintCentreForRound = 0.0f;

    // The bar is on for the two screens you navigate between and off for
    // the welcome screen, which is a single thing with one button.
    bool railIsVisible() const noexcept { return currentScreen != Screen::support; }

    // Everything the bar does not occupy. Overlays deliberately keep
    // using getLocalBounds(): a dialog that stopped short of the top edge
    // would look like it had failed to load.
    juce::Rectangle<int> contentBounds() const
    {
        return getLocalBounds().withTrimmedTop (railIsVisible() ? TopNavComponent::preferredHeight : 0);
    }

    // Puts the session and the three pills into whatever mode this
    // exercise was last left in, falling back to Practice when the timed
    // modes are not open for it yet. Called on every switch, so a mode set
    // in one exercise never follows you into another.
    void applyStoredModeForGame (int gameIndex);

    // Practice / Survival / Blitz. A run in the latter two ends on its
    // own terms (lives or clock) and posts a score against the exercise;
    // see SessionManager.
    SessionManager session;

    // Stage 3 of ADR 029: a run begins with a countdown and is played
    // under a HUD, not next to a row of mode pills.
    RunCountdown runCountdown;
    RunHud runHud;

    // Ends a live run without leaving the exercise - see the handler.
    juce::TextButton endRunButton;

    // True while a Survival/Blitz run is being played (not merely armed):
    // the pills and session score hide, the HUD shows. One predicate so
    // resized() and refreshRunStatus() can never disagree about it.
    bool isRunHudActive() const noexcept
    {
        return session.getMode() != SessionManager::Mode::practice
               && session.isRunActive() && runStarted;
    }

    // setMode()/startRun() arm a run; the countdown finishing is what
    // *starts* it. Without this flag the HUD appeared (and the pills
    // vanished) the moment a pill was clicked, while the countdown was
    // still saying "get ready".
    bool runStarted = false;

    void beginRunWithCountdown();
    // Three pills, one visibly on. A ComboBox meant the current mode was a
    // word in a well and the other two were behind a popup - so "what are
    // my options" cost a click, and the answer appeared as an OS menu that
    // belongs to no theme at all.
    juce::TextButton practiceButton, survivalButton, blitzButton;
    juce::Label runStatusLabel;

    // Kept so an auto-advance already in flight can be cancelled if the
    // player switches game/mode before it fires - otherwise a queued
    // newRound() would land on the exercise they just left.
    int pendingAdvanceId = 0;
    // Drag-to-select slider replacing the old row of separate choice
    // buttons - see decisions/015-choice-slider-and-training-sounds.md.
    ChoiceSliderComponent choiceSlider;

    // Lets a player jump straight to any level (1-10) instead of only
    // ever reaching it by accumulating points - "progress you can see
    // and control", not just a passive auto-scaling number. Still backed
    // by the same points system underneath (see
    // ProgressManager::setLevelManually), so the two never disagree.
    // Kept as the source of the localised strings the banner draws, but no
    // longer added to the editor: two 12px labels were what made the
    // return loop look like a footnote.
    juce::Label streakLabel;
    juce::Label dailyChallengeLabel;
    DailyBanner dailyBanner;

    // How many of Achievements::all() have been earned. Home screen only.

    // Height of the home screen's daily banner: a line of text over a row
    // of day/progress dots, with card padding around both. It was 20 - one
    // line of small text - when the streak and the challenge were labels.
    static constexpr int homeStatusHeight = 52;

    void showAchievementToast (const juce::String& achievementId);

    IconButton updateButton { AppIcons::Icon::download };

    // Light/dark switch. The chosen mode is stored in the same shared
    // "abcTrain" PropertiesFile the language preference uses, so it's one
    // product-wide preference rather than per-plugin or per-instance.
    IconButton themeButton { AppIcons::Icon::sun };

    // Section backdrops, computed in resized() and drawn in paint(). Held
    // as members because JUCE gives paint() no access to the layout pass,
    // and recomputing the same rectangles in both places is exactly how
    // a panel and its contents drift apart.
    juce::Rectangle<int> exerciseSection;
    juce::Rectangle<int> answerSection;
    juce::Rectangle<int> progressSection;
    juce::Rectangle<int> hintSection;

    // Overlay screen for picking which of the user's own reference-audio
    // folders (if any) the games should train on instead of pink noise -
    // see TrainingSoundsComponent/ReferenceAudioLibrary and decisions/015.
    // Has no default constructor (needs the processor), so it's
    // initialised in the constructor's member-init-list, after `processor`.
    // Declared here, but addChildComponent()'d last in the constructor so
    // it paints over everything - the same z-order rule the lesson and
    // training-sounds overlays already had to learn (decisions/015, 017).
    // Shown when a Survival or Blitz run ends. Added before the toast so
    // an achievement earned on the final answer still lands on top of it.
    RunResultsComponent runResults;

    // The full shelf, opened from the home screen's badge strip.
    AchievementsScreenComponent achievementsScreen;

    // The first-run walkthrough. Points at the real controls on the
    // training screen, so it can only run once one is open.
    TourOverlay tour;
    void startTour();

    // The DVD screensaver. Last child of all, and the only thing allowed
    // to cover the tour.
    IdleScreensaver screensaver;

    // A screen change fades rather than cuts. 180ms of eased alpha on the
    // arriving screen: long enough that the eye registers a transition and
    // does not have to re-find everything from scratch, short enough that
    // nobody waiting to answer a round notices it happened.
    // Driven by juce_animation on the display's own frame rate, not by the
    // editor's refresh Timer - that one runs at **1 Hz**, and stepping an
    // alpha by 0.16 per tick against it gave a six-second blackout advancing
    // one frame a second. Which is exactly what it looked like.
    float screenFade = 1.0f;
    juce::Animator screenFadeAnimator = juce::ValueAnimatorBuilder{}.build();
    juce::VBlankAnimatorUpdater screenFadeUpdater { this };
    void beginScreenFade();

    // The first screen is not a transition, it is an arrival. Without this
    // the editor opened behind a full-strength wash and waited for a timer
    // tick to reveal itself - which is invisible in the running app and
    // fatal to tools/EditorSnapshots, where no timer ever fires.
    bool hasShownAScreen = false;

public:
    // Jumps the transition wash to done. The snapshot tool deliberately
    // never pumps a message loop, so an eased value that starts at "hide
    // everything" stays there - it rendered every EarTrainer screenshot as
    // a flat rectangle of background, and I shipped that before looking.
    void completeScreenFade() { screenFade = 1.0f; }

private:
    void showAchievementsScreen();

    void showRunResults (int finalScore);

    // Read before ProgressManager is told about the run, so the results
    // screen can say "personal best" instead of comparing the new record
    // against itself.
    int pendingPreviousBest = 0;

    AchievementToast achievementToast;

    // Theme, window size, text size, wallpaper - the settings that are set
    // once. Added after trainingSounds so it paints over it, and before
    // the toast, which paints over everything.
    SettingsScreenComponent settingsScreen { localisation, localisationProperties };

    TrainingSoundsComponent trainingSounds;
    // The home screen lives inside a Viewport: nine trainings across four
    // categories already exceed the window, and the catalogue only grows.
    HomeScreenComponent homeScreen;

    // First launch only, and it blocks nothing - see the class comment.
    SupportScreenComponent supportScreen { localisation };
    Screen currentScreen = Screen::home;

    // Product site link, shown in a corner of every one of the four
    // editors (see decisions/016) - a dedicated page for the plugins
    // themselves is planned there later, this just points at the site
    // that exists today.
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };

    // Asking, once, where somebody is already deciding what to do next -
    // not on the training screen, where it would be interrupting the one
    // thing this app is for. Home only.
    juce::HyperlinkButton donateLink { "Support the project",
                                        juce::URL ("https://www.donationalerts.com/r/bogdankorablev") };

    // The update, as something you can watch. Added last of all the
    // children so it paints over everything, including the lesson
    // overlays - an update is the one thing that should not be behind
    // anything.
    UpdateWindow updateWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerEditor)
};
