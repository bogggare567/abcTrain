#include "PluginEditor.h"

namespace
{
    juce::String formatFrequency (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 1) + "k";
        return juce::String ((int) hz);
    }

    const juce::Colour correctColour { juce::Colours::limegreen };
    const juce::Colour wrongColour { juce::Colours::orangered };
}

EarTrainerEditor::EarTrainerEditor (EarTrainerProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setText ("Guess the Band", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    instructionLabel.setText ("Listen, then click the band you think was boosted or cut.",
                               juce::dontSendNotification);
    instructionLabel.setJustificationType (juce::Justification::centred);
    instructionLabel.setFont (juce::Font (14.0f));
    instructionLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (instructionLabel);

    for (int i = 0; i < EQGame::numBands; ++i)
    {
        auto* button = bandButtons.add (
            new juce::TextButton (formatFrequency (EQGame::bandFrequenciesHz[(size_t) i]) + " Hz"));
        button->onClick = [this, i] { bandButtonClicked (i); };
        addAndMakeVisible (button);
    }

    newRoundButton.onClick = [this] { processor.getGame().newRound(); };
    addAndMakeVisible (newRoundButton);

    scoreLabel.setJustificationType (juce::Justification::centredLeft);
    scoreLabel.setFont (juce::Font (14.0f));
    scoreLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (scoreLabel);

    feedbackLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (feedbackLabel);

    processor.getGame().addChangeListener (this);
    refreshFromGameState();

    setSize (640, 320);
}

EarTrainerEditor::~EarTrainerEditor()
{
    processor.getGame().removeChangeListener (this);
}

void EarTrainerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e24));
}

void EarTrainerEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (32));
    instructionLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    feedbackLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);

    auto buttonRow = area.removeFromTop (56);
    const auto buttonWidth = buttonRow.getWidth() / bandButtons.size();
    for (auto* button : bandButtons)
        button->setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (4));

    area.removeFromTop (16);

    auto bottomRow = area.removeFromTop (36);
    newRoundButton.setBounds (bottomRow.removeFromLeft (140));
    scoreLabel.setBounds (bottomRow.reduced (8, 0));
}

void EarTrainerEditor::bandButtonClicked (int bandIndex)
{
    processor.getGame().submitAnswer (bandIndex);
}

void EarTrainerEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromGameState();
}

void EarTrainerEditor::refreshFromGameState()
{
    auto& game = processor.getGame();

    scoreLabel.setText ("Score: " + juce::String (game.getScore()) + " / " + juce::String (game.getRoundsPlayed()),
                         juce::dontSendNotification);

    for (auto* button : bandButtons)
    {
        button->setEnabled (! game.hasAnswered());
        button->setColour (juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    }

    if (game.hasAnswered())
    {
        bandButtons[game.getCorrectBandIndex()]->setColour (juce::TextButton::buttonColourId, correctColour);

        if (! game.wasLastAnswerCorrect())
            bandButtons[game.getChosenBandIndex()]->setColour (juce::TextButton::buttonColourId, wrongColour);

        const juce::String direction = game.wasBoost() ? "boosted" : "cut";
        feedbackLabel.setText ((game.wasLastAnswerCorrect() ? juce::String ("Correct! ") : juce::String ("Not quite. "))
                                    + "It was " + direction + " at "
                                    + formatFrequency (EQGame::bandFrequenciesHz[(size_t) game.getCorrectBandIndex()])
                                    + " Hz.",
                                juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, game.wasLastAnswerCorrect() ? correctColour : wrongColour);
    }
    else
    {
        feedbackLabel.setText ({}, juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    }
}
