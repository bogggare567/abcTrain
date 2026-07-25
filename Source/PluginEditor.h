#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
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
    // Plain filled-rectangle progress bar - simple enough not to warrant
    // its own file.
    class LevelProgressBar : public juce::Component
    {
    public:
        void setProgress (float newProgress) noexcept
        {
            progress = juce::jlimit (0.0f, 1.0f, newProgress);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff3a3a4a));
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (juce::Colour (0xff5b9bd5));
            g.fillRoundedRectangle (bounds.withWidth (bounds.getWidth() * progress), 4.0f);
        }

    private:
        float progress = 0.0f;
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
    LevelProgressBar levelProgressBar;
    juce::Label streakLabel;
    juce::Label dailyChallengeLabel;

    juce::TextButton updateButton { "Updates" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerEditor)
};
