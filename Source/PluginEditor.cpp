#include "PluginEditor.h"

namespace
{
    const juce::Colour correctColour { juce::Colours::limegreen };
    const juce::Colour wrongColour { juce::Colours::orangered };
}

EarTrainerEditor::EarTrainerEditor (EarTrainerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setText ("Ear Trainer", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    auto& gameManager = processor.getGameManager();
    const auto names = gameManager.getGameNames();
    for (int i = 0; i < names.size(); ++i)
        gameSelector.addItem (names[i], i + 1); // ComboBox item IDs are 1-based
    gameSelector.setSelectedId (gameManager.getActiveGameIndex() + 1, juce::dontSendNotification);
    gameSelector.onChange = [this] { gameSelected(); };
    addAndMakeVisible (gameSelector);

    instructionLabel.setJustificationType (juce::Justification::centred);
    instructionLabel.setFont (juce::Font (14.0f));
    instructionLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (instructionLabel);

    newRoundButton.onClick = [this] { processor.getGameManager().getActiveGame().newRound(); };
    addAndMakeVisible (newRoundButton);

    scoreLabel.setJustificationType (juce::Justification::centredLeft);
    scoreLabel.setFont (juce::Font (14.0f));
    scoreLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (scoreLabel);

    feedbackLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (feedbackLabel);

    levelLabel.setJustificationType (juce::Justification::centredLeft);
    levelLabel.setFont (juce::Font (14.0f));
    levelLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (levelLabel);

    addAndMakeVisible (levelProgressBar);

    streakLabel.setJustificationType (juce::Justification::centredRight);
    streakLabel.setFont (juce::Font (14.0f));
    streakLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible (streakLabel);

    dailyChallengeLabel.setJustificationType (juce::Justification::centred);
    dailyChallengeLabel.setFont (juce::Font (13.0f));
    dailyChallengeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (dailyChallengeLabel);

    gameManager.getActiveGame().addChangeListener (this);
    processor.getProgressManager().addChangeListener (this);

    rebuildChoiceButtons();
    refreshFromGameState();
    refreshFromProgressState();

    setSize (640, 440);
}

EarTrainerEditor::~EarTrainerEditor()
{
    processor.getGameManager().getActiveGame().removeChangeListener (this);
    processor.getProgressManager().removeChangeListener (this);
}

void EarTrainerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e24));
}

void EarTrainerEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (32));
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
    for (int i = 0; i < game.getNumChoices(); ++i)
    {
        auto* button = choiceButtons.add (new juce::TextButton (game.getChoiceLabel (i)));
        button->onClick = [this, i] { choiceButtonClicked (i); };
        addAndMakeVisible (button);
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
