#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_animation/juce_animation.h>
#include "PluginProcessor.h"
#include "ChoiceSliderComponent.h"
#include "TrainingSoundsComponent.h"
#include "GamePickerComponent.h"
#include "../shared/UpdateChecker.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AppIcons.h"
#include "../shared/i18n/LocalisationManager.h"
#include <cmath>

// Generic multiple-choice UI driven entirely by the active Game's
// interface (name/instructions/choice count/labels/feedback). Adding a
// new game to GameManager needs no changes here.
class EarTrainerEditor : public juce::AudioProcessorEditor,
                          private juce::ChangeListener
{
public:
    explicit EarTrainerEditor (EarTrainerProcessor&);
    ~EarTrainerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

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

            // Fast-track any still-running animation to completion first,
            // so its own onComplete callback removes *itself* from
            // `updater` before `currentAnimator` gets reassigned below -
            // otherwise that callback (which reads the member by name,
            // not by captured value) would end up removing the new
            // animator instead of the one it actually belongs to.
            if (! currentAnimator.isComplete())
                currentAnimator.complete();

            const auto startValue = displayedProgress;
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

            auto fillBounds = bounds.withWidth (juce::jmax (bounds.getHeight(),
                                                            bounds.getWidth() * displayedProgress));

            juce::ColourGradient fillGradient (theme.accent.darker (0.1f), fillBounds.getX(), fillBounds.getY(),
                                                theme.accent.brighter (0.2f), fillBounds.getRight(), fillBounds.getY(),
                                                false);
            g.setGradientFill (fillGradient);
            g.fillRoundedRectangle (fillBounds, radius);

            // The "breathing" glow: a soft, slowly-pulsing highlight right
            // at the leading edge of the fill - only drawn once there's
            // real progress to show, so an empty bar at level 1 doesn't
            // pulse for no reason.
            if (displayedProgress > 0.02f)
            {
                const auto glowAlpha = 0.10f + 0.12f * (0.5f + 0.5f * std::sin (breathPhase));
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

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Pushes the active palette into every widget that sets its own colours
    // explicitly (the LookAndFeel can't reach those). Called at construction
    // and again whenever the theme is toggled.
    void applyTheme();
    void toggleTheme();

    void refreshFromGameState();
    void refreshFromProgressState();
    void refreshLocalisedText();
    void rebuildGameSelectorItems();
    void rebuildGamePickerCards();
    void rebuildChoiceSlider();
    void choiceButtonClicked (int choiceIndex);
    void languageSelected();
    void levelSelected();

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
    juce::ComboBox languageSelector;
    // Icon for whichever game is currently selected (see AppIcons) - kept
    // in sync with the active game in refreshFromGameState(), so a pick
    // from the card grid or a difficulty-driven change both update it.
    AppIconComponent gameIcon;

    // Opens the card grid. Replaces the plain ComboBox list of nine game
    // names, which told a player nothing about what each exercise was for
    // or how they were doing at it - see GamePickerComponent.
    juce::TextButton gameSelectorButton;
    juce::Label currentGameLabel;
    juce::Label instructionLabel;
    juce::Label scoreLabel;
    juce::Label feedbackLabel;
    juce::TextButton newRoundButton { "New Round" };
    // Drag-to-select slider replacing the old row of separate choice
    // buttons - see decisions/015-choice-slider-and-training-sounds.md.
    ChoiceSliderComponent choiceSlider;

    juce::Label levelLabel;
    // Lets a player jump straight to any level (1-10) instead of only
    // ever reaching it by accumulating points - "progress you can see
    // and control", not just a passive auto-scaling number. Still backed
    // by the same points system underneath (see
    // ProgressManager::setLevelManually), so the two never disagree.
    juce::ComboBox levelSelector;
    LevelProgressBar levelProgressBar;
    juce::Label streakLabel;
    juce::Label dailyChallengeLabel;

    juce::TextButton updateButton { "Updates" };

    // Light/dark switch. The chosen mode is stored in the same shared
    // "abcTrain" PropertiesFile the language preference uses, so it's one
    // product-wide preference rather than per-plugin or per-instance.
    juce::TextButton themeButton { "Light" };

    // Section backdrops, computed in resized() and drawn in paint(). Held
    // as members because JUCE gives paint() no access to the layout pass,
    // and recomputing the same rectangles in both places is exactly how
    // a panel and its contents drift apart.
    juce::Rectangle<int> exerciseSection;
    juce::Rectangle<int> answerSection;
    juce::Rectangle<int> progressSection;

    // Overlay screen for picking which of the user's own reference-audio
    // folders (if any) the games should train on instead of pink noise -
    // see TrainingSoundsComponent/ReferenceAudioLibrary and decisions/015.
    // Has no default constructor (needs the processor), so it's
    // initialised in the constructor's member-init-list, after `processor`.
    juce::TextButton trainingSoundsButton { "Training Sounds" };
    TrainingSoundsComponent trainingSounds;
    GamePickerComponent gamePicker;

    // Product site link, shown in a corner of every one of the four
    // editors (see decisions/016) - a dedicated page for the plugins
    // themselves is planned there later, this just points at the site
    // that exists today.
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerEditor)
};
