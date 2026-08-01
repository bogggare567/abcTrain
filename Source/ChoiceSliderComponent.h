#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_animation/juce_animation.h>
#include "Games/Game.h"
#include <functional>
#include <vector>

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
class ChoiceSliderComponent : public juce::Component,
                               private juce::Timer
{
public:
    ChoiceSliderComponent();
    ~ChoiceSliderComponent() override;

    // Called whenever the active game (or its choice count) changes.
    // Resets to an unanswered, nothing-picked state.
    void setChoices (const juce::StringArray& labels);
    int getNumChoices() const noexcept { return choiceLabels.size(); }

    // What the panel is currently showing, so a caller can tell whether it
    // still matches the round. Counting alone is not enough once a game
    // draws a fresh pair of names each round out of a fixed-size set.
    const juce::StringArray& getChoiceLabels() const noexcept { return choiceLabels; }

    // Caption under the scale, e.g. "< 100 Hz - 12.8k Hz >". The editor
    // derives it from the first and last choice labels, so it's correct
    // for every game without any per-game code.
    void setAxisCaption (const juce::String& caption);

    // ---- continuous mode ----
    // Switches the widget from "N zones you click" to a ruler the pointer
    // slides along: the readout tracks the cursor, and a tolerance band
    // travels with it showing how much slack the current difficulty gives.
    // `formatter` turns a normalised 0..1 position into real-unit text.
    void setContinuousScale (std::vector<Game::GridMark> marks,
                             float toleranceNormalised,
                             std::function<juce::String (float)> formatter);
    void setDiscreteScale();

    // Reveals the round's answer on a continuous scale: the target gets an
    // accent marker and its tolerance band, the guess keeps its own line.
    void showContinuousAnswer (float chosenNormalised, float targetNormalised, bool wasCorrect);

    // Fires on release with the normalised position, in continuous mode.
    std::function<void (float)> onContinuousChoice;

    // Shown in place of the big value readout before anything is picked.
    void setPlaceholderText (const juce::String& text);

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

    // Jumps every eased value to its end state, for tools/EditorSnapshots
    // (which never pumps a message loop). Same seam, same reason, as
    // RunResultsComponent::completeAnimation.
    void completeAnimation()
    {
        enterAmount = 1.0f;
        revealAmount = 1.0f;
        touchAmount = touchTarget;
        if (! feedbackAnimator.isComplete())
            feedbackAnimator.complete();
        repaint();
    }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // paint() is split in two because the zoned panel is drawn inside a
    // rounded clip region, and the value readout above it and caption
    // below it sit outside that panel - drawing them within the clip
    // would silently crop them.
    void paintScale (juce::Graphics&);
    void paintOverPanel (juce::Graphics&);

    // The recessed scale panel itself - the zoned band the choices live
    // in, excluding the big value readout above and the caption below.
    juce::Rectangle<int> getScaleArea() const;

    // One choice occupies one vertical zone of the scale, rather than a
    // point on a line. A zone is a much larger click target than a tick,
    // and (per the reference this was rebuilt to match) it makes the
    // scale read as a set of regions you choose between rather than a
    // continuous value you have to land exactly on.
    juce::Rectangle<float> zoneForIndex (int index, juce::Rectangle<float> scaleArea) const;
    int indexForX (float x, juce::Rectangle<float> scaleArea) const;

    void updatePreviewFromMouse (const juce::MouseEvent& e);

    juce::StringArray choiceLabels;
    juce::String axisCaption;
    juce::String placeholderText { "Drag to choose" };

    // Continuous-mode state. cursorNormalised is where the pointer last
    // was; it is the *answer in progress*, so it survives until the round
    // is reset rather than snapping back when the pointer leaves.
    bool continuousMode = false;
    bool cursorEngaged = false;
    float cursorNormalised = 0.5f;
    float toleranceNormalised = 0.0f;
    float targetNormalised = -1.0f;
    std::vector<Game::GridMark> gridMarks;
    std::function<juce::String (float)> valueFormatter;

    void paintContinuousScale (juce::Graphics&);
    void paintContinuousOverlay (juce::Graphics&);

    // Live drag position before release, or (once answered) the index the
    // player actually picked - whichever is relevant is what paint() shows
    // enlarged at the top and highlighted on the track.
    int previewIndex = -1;

    bool answered = false;
    int correctIndex = -1;
    int chosenIndex = -1;
    bool lastCorrect = false;

    // Post-answer feedback (see decisions/018): a correct guess fades a
    // soft glow out around the thumb over ~900ms; a wrong one gives the
    // thumb/big label a brief, decaying "disappointed" wobble rather than
    // an aggressive shake. Both driven by juce_animation, independent of
    // the drag-preview logic above.
    void startFeedbackAnimation (bool wasCorrect);
    float feedbackGlow = 0.0f;
    float feedbackWobblePx = 0.0f;
    juce::Animator feedbackAnimator = juce::ValueAnimatorBuilder{}.build();
    juce::VBlankAnimatorUpdater feedbackUpdater { this };

    // Touch response: the thumb swells and lights up while the pointer is
    // over or dragging the track, easing rather than snapping. Driven by
    // this component's own 60 Hz timer instead of an Animator, since it's a
    // continuously-retargeted state rather than a one-shot transition -
    // the same split shared/WidgetStateRegistry makes for LookAndFeel
    // widgets, applied to a component that draws itself.
    float touchAmount = 0.0f;
    float touchTarget = 0.0f;

    // The answer reveal: 0 at the moment of answering, easing to 1. In
    // continuous mode the tolerance band *travels* from the player's guess
    // to the true value along this - the distance of the miss is shown as
    // motion rather than left to be read off two numbers - and the target
    // marker sweeps in with it. In discrete mode the zone colours fade in
    // on it. Instant reveals were correct but weightless: the moment the
    // whole round exists for deserves more than a repaint.
    float revealAmount = 1.0f;

    // Game-switch transition: setChoices() restarts this at 0, and the new
    // choices rise into place while fading up rather than appearing
    // instantly. The old choices don't animate out - the widget is
    // re-labelled in place, so there is nothing left on screen to remove.
    float enterAmount = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceSliderComponent)
};
