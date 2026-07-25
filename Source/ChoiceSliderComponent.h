#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Drag-to-select answer widget: a single horizontal track with one evenly-
// spaced tick per choice, a big label showing whichever choice is currently
// highlighted, and a draggable thumb that snaps to the nearest tick. Replaces
// the old row of separate TextButtons (one per choice) that every EarTrainer
// game's editor used - a real user complaint was that those buttons could
// disappear/stop responding (see decisions/014), and separately that the
// list-of-buttons look read as "childish" next to the reference ear-training
// apps the user pointed to, which all use exactly this kind of tick-marked
// slider for both numeric (dB/Hz/pan) and named/categorical choices alike.
// getNumChoices()/getChoiceLabel(int) is all this needs from a Game, so it
// works unmodified for every game, numeric or categorical.
class ChoiceSliderComponent : public juce::Component
{
public:
    ChoiceSliderComponent() = default;

    // Called whenever the active game (or its choice count) changes.
    // Resets to an unanswered, nothing-picked state.
    void setChoices (const juce::StringArray& labels);
    int getNumChoices() const noexcept { return choiceLabels.size(); }

    // Back to the plain "nothing picked yet" state for a fresh round with
    // the same choice count (setChoices() already implies this once).
    void resetForNewRound();

    // Colours the correct tick green, and - only if the player actually
    // got it wrong - the chosen tick red, same post-answer feedback the
    // old button row gave. Also locks the slider against further drags
    // until the next resetForNewRound()/setChoices().
    void showAnswer (int correctIndex, int chosenIndex, bool wasCorrect);

    // Fires once, on mouse release, with the tick index nearest the
    // release point - never while just dragging/previewing.
    std::function<void (int)> onChoiceSelected;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> getTrackArea() const;
    float xForIndex (int index, const juce::Rectangle<int>& trackArea) const;
    int indexForX (float x, const juce::Rectangle<int>& trackArea) const;
    void updatePreviewFromMouse (const juce::MouseEvent& e);

    juce::StringArray choiceLabels;

    // Live drag position before release, or (once answered) the index the
    // player actually picked - whichever is relevant is what paint() shows
    // enlarged at the top and highlighted on the track.
    int previewIndex = -1;

    bool answered = false;
    int correctIndex = -1;
    int chosenIndex = -1;
    bool lastCorrect = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceSliderComponent)
};
