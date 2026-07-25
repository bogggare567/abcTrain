#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_animation/juce_animation.h>
#include "PluginProcessor.h"
#include "../shared/UpdateChecker.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/i18n/LocalisationManager.h"

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
    class LevelProgressBar : public juce::Component
    {
    public:
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
            auto bounds = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff3a3a4a));
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (juce::Colour (0xff5b9bd5));
            g.fillRoundedRectangle (bounds.withWidth (bounds.getWidth() * displayedProgress), 4.0f);
        }

    private:
        float targetProgress = 0.0f;
        float displayedProgress = 0.0f;

        // Animator has no default constructor (only explicit
        // Animator(shared_ptr<Impl>)) - this placeholder is never
        // started, just a valid object to overwrite the first time
        // setProgress() actually runs.
        juce::Animator currentAnimator = juce::ValueAnimatorBuilder{}.build();
        juce::VBlankAnimatorUpdater updater { this };
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshFromGameState();
    void refreshFromProgressState();
    void refreshLocalisedText();
    void rebuildGameSelectorItems();
    void rebuildChoiceButtons();
    void choiceButtonClicked (int choiceIndex);
    void gameSelected();
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
    juce::ComboBox gameSelector;
    juce::Label instructionLabel;
    juce::Label scoreLabel;
    juce::Label feedbackLabel;
    juce::TextButton newRoundButton { "New Round" };
    juce::OwnedArray<juce::TextButton> choiceButtons;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerEditor)
};
