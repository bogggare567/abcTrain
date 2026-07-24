#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

// Common shape for every ear-training exercise: play a processed test
// signal, offer a fixed set of labeled choices, score the player's pick.
// Concrete games (EQGame, CompressionGame, ...) own their DSP chain and
// randomization privately; GameManager and the editor only ever talk to
// this interface, so a new game needs no editor/processor changes.
class Game : public juce::ChangeBroadcaster
{
public:
    ~Game() override = default;

    virtual juce::String getName() const = 0;
    virtual juce::String getInstructions() const = 0;

    virtual void prepare (const juce::dsp::ProcessSpec&) = 0;
    virtual void process (juce::AudioBuffer<float>&) = 0;

    // Adjusts how hard the next round(s) will be, on a 1-10 scale (see
    // docs/decisions/002-difficulty-scaling.md). Takes effect starting
    // with the next newRound() call - doesn't retroactively change a
    // round already in progress. Safe to call before prepare().
    virtual void setDifficulty (int level) = 0;

    // Starts a new round: pick a new random target, reset answer state,
    // and broadcast a change so the UI can refresh.
    virtual void newRound() = 0;

    // Records the player's guess and broadcasts a change with the result.
    virtual void submitAnswer (int choiceIndex) = 0;

    virtual int getNumChoices() const = 0;
    virtual juce::String getChoiceLabel (int choiceIndex) const = 0;

    virtual bool hasAnswered() const = 0;
    virtual int getCorrectChoiceIndex() const = 0;
    virtual int getChosenChoiceIndex() const = 0;
    virtual bool wasLastAnswerCorrect() const = 0;
    virtual juce::String getFeedbackText() const = 0;

    virtual int getScore() const = 0;
    virtual int getRoundsPlayed() const = 0;
};
