#pragma once

#include "../../shared/DifficultyRamp.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <vector>
#include <cmath>

class ReferenceAudioLibrary;

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

    // Optional hook: a game whose test signal is a single TestSignalGenerator
    // overrides this to receive the shared ReferenceAudioLibrary, so the
    // player can practice on real reference audio instead of synthesized
    // noise (see TestSignalGenerator/ReferenceAudioLibrary). Default no-op
    // keeps every game that doesn't override this - including any future
    // one - working exactly as before; not every game's DSP is a single
    // noise source (StereoWidthGame needs two independently-decorrelated
    // ones, so it deliberately doesn't override this).
    virtual void setReferenceAudioLibrary (const ReferenceAudioLibrary*) {}

    // Smoothly interpolates a difficulty value across the whole 1-10
    // range, instead of three flat tiers.
    //
    // Every game used to switch between three hard-coded values at levels
    // 4 and 7, which meant seven of the ten levels changed nothing at all:
    // reaching level 5 felt identical to level 4, and the promotion test
    // you had just passed had bought you nothing. A ramp makes every level
    // a real step.
    //
    // Geometric, not linear, because these are all *tolerances* - a band
    // of ±1.0 octaves narrowing to ±0.35 is a series of halvings, and
    // equal ratios are what feel like equal steps. Linear interpolation
    // would make the early levels tighten sharply and the late ones barely
    // move.
    static float rampTolerance (int level, float atLevelOne, float atLevelTen) noexcept
    {
        return DifficultyRamp::geometric (level, atLevelOne, atLevelTen);
    }

    // The same ramp for a value that is not a tolerance and can legitimately
    // pass through zero or change sign.
    static float rampLinear (int level, float atLevelOne, float atLevelTen) noexcept
    {
        return DifficultyRamp::linear (level, atLevelOne, atLevelTen);
    }

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

    // For a continuous game (see below) these describe the *labelled grid
    // marks* along the scale rather than the only answers allowed; for a
    // discrete game they are the answers themselves.
    virtual int getNumChoices() const = 0;
    virtual juce::String getChoiceLabel (int choiceIndex) const = 0;

    // ---- continuous answers -------------------------------------------
    //
    // Some skills don't have N right answers, they have a right *value*:
    // the boosted frequency is 425 Hz, not "one of eight octave bands",
    // and a pan position is somewhere on a continuum. For those, the
    // player drags along a scale and is scored on how close they got,
    // with a tolerance band that narrows as difficulty rises - which is
    // also a far better difficulty knob than shrinking the boost until
    // it's inaudible.
    //
    // Everything here is in normalised 0..1 axis space. The game owns the
    // mapping to real units (log frequency, linear dB, ...) so the UI
    // stays a dumb ruler and can serve every game unchanged.
    //
    // Non-pure-virtual with inert defaults, the same shape as
    // setReferenceAudioLibrary above: the games that haven't been
    // converted keep working untouched, and so does the discrete
    // submitAnswer(int) path on the games that have.
    virtual bool usesContinuousScale() const { return false; }

    // Half-width of the accept band, in normalised units. A guess within
    // +/- this of the target counts as correct.
    virtual float getToleranceNormalised() const { return 0.0f; }

    // The round's actual answer, and what the player submitted. Both are
    // only meaningful once hasAnswered() - before that getChosenNormalised()
    // returns a negative sentinel.
    virtual float getCorrectNormalised() const { return 0.0f; }
    virtual float getChosenNormalised() const { return -1.0f; }

    // Real-unit text for an arbitrary point on the scale ("425 Hz"),
    // used for the readout that tracks the pointer.
    virtual juce::String formatNormalisedValue (float normalised) const
    {
        return juce::String (normalised, 2);
    }

    virtual void submitNormalisedAnswer (float normalised) { juce::ignoreUnused (normalised); }

    // How *well* the last answer was given, 0..1, where 1 is dead on the
    // target and 0 is at the very edge of the accept band. Points scale
    // with it, so a continuous exercise stops being pass/fail and starts
    // rewarding precision - which is the actual skill.
    //
    // 1.0 by default: a categorical answer is right or wrong and there is
    // no "nearly", so those games award the flat rate and nothing here
    // needs to know the difference.
    virtual float getAnswerQuality() const
    {
        // Derived from the continuous hooks rather than reimplemented per
        // game: every one of them already reports its target, its answer
        // and its band, so asking each to compute the same ratio again
        // would be four chances to get it slightly different.
        if (! usesContinuousScale() || ! hasAnswered())
            return 1.0f;

        const auto tolerance = getToleranceNormalised();

        if (tolerance <= 0.0f)
            return 1.0f;

        const auto distance = std::abs (getChosenNormalised() - getCorrectNormalised());

        return juce::jlimit (0.0f, 1.0f, 1.0f - distance / tolerance);
    }

    // ---- A/B: before vs after -----------------------------------------
    //
    // "Is this louder?" is a much harder question than "is B louder than
    // A?". Every exercise here hides a *change*, and the only way to hear
    // a change reliably is to switch between the two states - which is
    // what an engineer does with a bypass button all day.
    //
    // `before` is the untreated signal, `after` is the one with the round's
    // hidden change applied. A game that has no meaningful "before" (there
    // is no unprocessed version of "which reverb type is this") leaves
    // this off and the editor hides the switch.
    virtual bool supportsBeforeAfter() const { return false; }

    // Audio-thread visible: process() reads it every block, so it's an
    // atomic on the implementing side rather than a plain bool.
    virtual void setPlayProcessed (bool shouldPlayProcessed) { juce::ignoreUnused (shouldPlayProcessed); }
    virtual bool isPlayingProcessed() const { return true; }

    // Wording for the two states, since "EQ off / EQ on" and "before gain
    // / after gain" say something the generic pair doesn't.
    virtual juce::String getBeforeLabel() const { return "A"; }
    virtual juce::String getAfterLabel() const { return "B"; }

    // A labelled tick on the scale. `emphasised` marks the primary series
    // (e.g. the octave centres) so the UI can draw the secondary series
    // (the boundaries between them) more quietly and on its own label row.
    struct GridMark
    {
        float normalised = 0.0f;
        juce::String label;
        bool emphasised = true;
    };

    // Defaults to one mark per discrete choice, evenly spaced - which is
    // exactly right for a game whose choices *are* the scale. A continuous
    // game with a denser or unevenly-spaced ruler overrides this.
    virtual std::vector<GridMark> getGridMarks() const
    {
        std::vector<GridMark> marks;
        const auto count = getNumChoices();

        for (int i = 0; i < count; ++i)
            marks.push_back ({ count <= 1 ? 0.5f : (float) i / (float) (count - 1),
                               getChoiceLabel (i), true });

        return marks;
    }

    virtual bool hasAnswered() const = 0;
    virtual int getCorrectChoiceIndex() const = 0;
    virtual int getChosenChoiceIndex() const = 0;
    virtual bool wasLastAnswerCorrect() const = 0;
    virtual juce::String getFeedbackText() const = 0;

    virtual int getScore() const = 0;
    virtual int getRoundsPlayed() const = 0;
};
