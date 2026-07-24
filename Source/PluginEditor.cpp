#include "PluginEditor.h"
#include "../shared/Version.h"

namespace
{
    const juce::Colour correctColour { juce::Colours::limegreen };
    const juce::Colour wrongColour { juce::Colours::orangered };
}

EarTrainerEditor::EarTrainerEditor (EarTrainerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("Ear Trainer", juce::dontSendNotification);
    titleLabel.setFont (AbcTrainLookAndFeel::titleFont());
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    auto& gameManager = processor.getGameManager();
    const auto names = gameManager.getGameNames();
    for (int i = 0; i < names.size(); ++i)
        gameSelector.addItem (names[i], i + 1); // ComboBox item IDs are 1-based
    gameSelector.setSelectedId (gameManager.getActiveGameIndex() + 1, juce::dontSendNotification);
    gameSelector.onChange = [this] { gameSelected(); };
    addAndMakeVisible (gameSelector);

    instructionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (instructionLabel);

    newRoundButton.onClick = [this] { processor.getGameManager().getActiveGame().newRound(); };
    addAndMakeVisible (newRoundButton);

    scoreLabel.setJustificationType (juce::Justification::centredLeft);
    scoreLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (scoreLabel);

    feedbackLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (feedbackLabel);

    levelLabel.setJustificationType (juce::Justification::centredLeft);
    levelLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (levelLabel);

    addAndMakeVisible (levelProgressBar);

    streakLabel.setJustificationType (juce::Justification::centredRight);
    streakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    streakLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd98c5f));
    addAndMakeVisible (streakLabel);

    dailyChallengeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (dailyChallengeLabel);

    gameManager.getActiveGame().addChangeListener (this);
    processor.getProgressManager().addChangeListener (this);

    updateButton.onClick = [this]
    {
        juce::Component::SafePointer<juce::Component> safeThis (this);

        UpdateChecker::checkForUpdatesAsync (CurrentVersion::string, [safeThis] (bool foundNewer, UpdateChecker::ReleaseInfo release)
        {
            if (! foundNewer || safeThis == nullptr)
                return;

            const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
                juce::MessageBoxIconType::InfoIcon,
                "Update Available",
                "Version " + release.tagName + " is available - you're on " + juce::String (CurrentVersion::string) + ".",
                "Open Release Page", "Later",
                safeThis.getComponent());

            juce::AlertWindow::showAsync (options, [release] (int result)
            {
                // makeOptionsOkCancel adds two buttons; per AlertWindow's
                // documented N-button result mapping, button[0] ("Open
                // Release Page") returns 1, button[1] ("Later") returns 0.
                if (result == 1)
                    juce::URL (release.htmlUrl).launchInDefaultBrowser();
            });
        });
    };
    addAndMakeVisible (updateButton);

    rebuildChoiceButtons();
    refreshFromGameState();
    refreshFromProgressState();

    setSize (640, 440);
}

EarTrainerEditor::~EarTrainerEditor()
{
    processor.getGameManager().getActiveGame().removeChangeListener (this);
    processor.getProgressManager().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void EarTrainerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));
}

void EarTrainerEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto titleRow = area.removeFromTop (32);
    updateButton.setBounds (titleRow.removeFromRight (80));
    titleRow.removeFromRight (8);
    titleLabel.setBounds (titleRow);
    gameSelector.setBounds (area.removeFromTop (28).withSizeKeepingCentre (280, 24));
    area.removeFromTop (8);
    instructionLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    feedbackLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);

    auto buttonRow = area.removeFromTop (56);
    if (! choiceButtons.isEmpty())
    {
        const auto buttonWidth = buttonRow.getWidth() / choiceButtons.size();
        for (auto* button : choiceButtons)
            button->setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (4));
    }

    area.removeFromTop (16);

    auto bottomRow = area.removeFromTop (36);
    newRoundButton.setBounds (bottomRow.removeFromLeft (140));
    scoreLabel.setBounds (bottomRow.reduced (8, 0));

    area.removeFromTop (16);

    auto progressRow = area.removeFromTop (24);
    levelLabel.setBounds (progressRow.removeFromLeft (100));
    streakLabel.setBounds (progressRow.removeFromRight (100));
    levelProgressBar.setBounds (progressRow.reduced (8, 4));

    area.removeFromTop (8);
    dailyChallengeLabel.setBounds (area.removeFromTop (20));
}

void EarTrainerEditor::gameSelected()
{
    auto& gameManager = processor.getGameManager();
    gameManager.getActiveGame().removeChangeListener (this);

    gameManager.setActiveGameIndex (gameSelector.getSelectedId() - 1);

    gameManager.getActiveGame().addChangeListener (this);
    rebuildChoiceButtons();
    refreshFromGameState();
    resized();
}

void EarTrainerEditor::rebuildChoiceButtons()
{
    choiceButtons.clear();

    auto& game = processor.getGameManager().getActiveGame();
    auto& animator = juce::Desktop::getInstance().getAnimator();

    for (int i = 0; i < game.getNumChoices(); ++i)
    {
        auto* button = choiceButtons.add (new juce::TextButton (game.getChoiceLabel (i)));
        button->onClick = [this, i] { choiceButtonClicked (i); };
        addAndMakeVisible (button);

        // Basic fade-in whenever the choice buttons regenerate (switching
        // games, or a mid-session choice-count change like ReverbGame's
        // difficulty tiers) - one deliberately simple animation for this
        // pass, per the redesign's own iterative scope; see
        // decisions/009-look-and-feel.md for what else was deferred.
        button->setAlpha (0.0f);
        animator.fadeIn (button, 200);
    }
}

void EarTrainerEditor::choiceButtonClicked (int choiceIndex)
{
    processor.getGameManager().getActiveGame().submitAnswer (choiceIndex);
}

void EarTrainerEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Either the active game or ProgressManager could have fired this;
    // both refreshes are cheap, so just do both rather than tracking
    // which broadcaster it was.
    refreshFromGameState();
    refreshFromProgressState();
}

void EarTrainerEditor::refreshFromGameState()
{
    auto& game = processor.getGameManager().getActiveGame();

    // A difficulty change (via ProgressManager, on level-up) can change
    // the active game's choice count at runtime - currently only
    // ReverbGame does this. Only rebuild at the start of a fresh round,
    // not mid-reveal, so the button layout doesn't shift while showing
    // an answer.
    if (! game.hasAnswered() && choiceButtons.size() != game.getNumChoices())
    {
        rebuildChoiceButtons();
        resized();
    }

    instructionLabel.setText (game.getInstructions(), juce::dontSendNotification);

    scoreLabel.setText ("Score: " + juce::String (game.getScore()) + " / " + juce::String (game.getRoundsPlayed()),
                         juce::dontSendNotification);

    for (auto* button : choiceButtons)
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        button->setEnabled (! game.hasAnswered());
    }

    if (game.hasAnswered())
    {
        choiceButtons[game.getCorrectChoiceIndex()]->setColour (juce::TextButton::buttonColourId, correctColour);

        if (! game.wasLastAnswerCorrect())
            choiceButtons[game.getChosenChoiceIndex()]->setColour (juce::TextButton::buttonColourId, wrongColour);

        feedbackLabel.setText (game.getFeedbackText(), juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, game.wasLastAnswerCorrect() ? correctColour : wrongColour);
    }
    else
    {
        feedbackLabel.setText ({}, juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    }
}

void EarTrainerEditor::refreshFromProgressState()
{
    auto& progress = processor.getProgressManager();

    levelLabel.setText ("Level " + juce::String (progress.getLevel()), juce::dontSendNotification);
    levelProgressBar.setProgress (progress.getLevelProgressProportion());

    streakLabel.setText (juce::String (progress.getStreakDays()) + " day streak", juce::dontSendNotification);

    dailyChallengeLabel.setText (progress.getDailyChallengeDescription(), juce::dontSendNotification);
    dailyChallengeLabel.setColour (juce::Label::textColourId,
                                    progress.isDailyChallengeComplete() ? correctColour : juce::Colours::lightgrey);
}
