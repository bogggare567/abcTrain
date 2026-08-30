#pragma once

#include "../../shared/DifficultyRamp.h"
#include "../../shared/PinkNoiseGenerator.h"
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

    // Which colour the fallback noise is. Same shape and same reasoning as
    // the hook above: a default no-op, so the games that generate their
    // own signal in some other way - StereoWidthGame's two decorrelated
    // sources - are untouched and need no edit to keep working.
    virtual void setNoiseColour (NoiseColour) {}

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

    // Which picture actually shows what this exercise is asking about.
    //
    // Every game used to get the same pair - a vectorscope beside a
    // spectrum - which is right for two of the nine and decoration for
    // the rest. Neither one reveals compression: a spectrum shows where
    // the energy is, not whether the loud part was held back, and a
    // vectorscope shows the stereo field, which compression does not
    // touch. A hint the player *pays points for* has to be able to show
    // the thing.
    //
    //   spectrum - where the energy sits (frequency, range, distortion)
    //   stereo   - the field (pan, width)
    //   envelope - level against time (compression, gain, reverb, delay)
    enum class HintView { spectrum, stereo, envelope };

    virtual HintView getHintView() const { return HintView::spectrum; }

    // ---- where your misses land -----------------------------------------
    //
    // Every exercise divides its own subject somehow: frequency into named
    // ranges, panning into positions, reverb into types. This is that
    // division, so the results screen can say *where* somebody is weak
    // rather than only how often they were wrong.
    //
    // It is the one part of a results screen that changes what a player
    // does tomorrow. "You scored 27, 73% accurate" is a report on a run
    // that is already over; "almost every miss was between 500 Hz and
    // 4 kHz" is an instruction.
    //
    // 0 buckets means this exercise offers no map, which is a legitimate
    // answer and the default. A bucket is only ever recorded for a round
    // that has actually been answered.
    virtual int getNumSkillBuckets() const { return 0; }
    virtual juce::String getSkillBucketLabel (int) const { return {}; }

    // Which bucket the round that just ended belonged to - keyed on the
    // *correct* answer, not the player's, so the map reads "you miss here"
    // rather than "you guess here". -1 when there is nothing to record.
    virtual int getSkillBucketForRound() const { return -1; }

    // The English *name* of choice i, as opposed to its displayed label.
    //
    // Every categorical exercise offers two names out of its own family,
    // and those names are audio vocabulary that does not translate - a
    // plate is a Plate in every language. This is the key the editor looks
    // a *description* up by: one sentence saying what that option sounds
    // like, shown on the card while you are choosing between them.
    //
    // Two words on two buttons ask you to recognise a label you may never
    // have been taught. A sentence under each says what the label means,
    // so a wrong answer teaches something rather than only costing a
    // point. Empty by default; a game that has nothing to say draws
    // exactly what it drew before.
    virtual juce::String getChoiceKey (int index) const { return getChoiceLabel (index); }

    // What a hint should do on a ruler exercise: narrow the search, not
    // answer the question.
    //
    // Returns the half-width, in normalised axis units, of a region that
    // contains the answer. The editor shades everything outside it.
    //
    // This exists because the old hint was the wrong shape for this
    // product. It showed a live analyser - so on "find the frequency" the
    // spectrum drew the boost as a visible bump, and the player read the
    // answer off a picture. In Survival that cost a life; what they bought
    // with it was permission not to listen, in an application whose whole
    // subject is listening.
    //
    // Narrowing keeps the ear in the loop. Three times the accept band is
    // deliberate: enough that the search is genuinely easier, far too wide
    // to click blindly in the middle and be right. A game returning 0
    // offers no narrowing at all, which is the honest default for the
    // categorical ones - with two alternatives, eliminating one *is* the
    // answer.
    virtual float getHintHalfWidthNormalised() const
    {
        return usesContinuousScale() ? juce::jmin (0.5f, getToleranceNormalised() * 3.0f) : 0.0f;
    }

    // Where to put that region. Pure, and separate from the editor, so it
    // can be tested at all - the editor needs a message loop and a plugin
    // host, and this is the part with the arithmetic in it.
    //
    // `roll` is 0..1 from wherever the caller gets its randomness. The
    // result satisfies two things at once: the answer is inside the
    // region, and the region's centre is not the answer. The second is
    // load-bearing - a region centred on the answer is the answer with
    // extra steps, and a player would learn to click the middle without
    // listening.
    static float hintCentreFor (float answerNormalised, float halfWidth, float roll)
    {
        if (halfWidth <= 0.0f)
            return answerNormalised;

        // Up to 55% of the half width, either way. Enough that the centre
        // is never a reliable guess; not so much that the answer lands on
        // the region's own edge, where the shading would point at it just
        // as clearly.
        const auto drift = (juce::jlimit (0.0f, 1.0f, roll) * 2.0f - 1.0f) * halfWidth * 0.55f;

        // Clamped to keep the whole region on the axis. That can pull the
        // centre back toward the answer near either end, which is correct:
        // an answer at 0.02 is already narrowed by the axis itself.
        return juce::jlimit (juce::jmin (halfWidth, 0.5f),
                              juce::jmax (1.0f - halfWidth, 0.5f),
                              answerNormalised + drift);
    }

    // The two pieces of an answer the *editor* cannot derive on its own,
    // so it can compose the feedback sentence in the player's language
    // instead of showing getFeedbackText()'s English. Everything else it
    // needs is already on the interface: the winning label (categorical)
    // or the formatted value (continuous).
    //
    // Direction: +1 when this round's change was a boost, -1 a cut, 0 for
    // games where the question has no direction. Detail: a strictly
    // language-neutral suffix - numbers and units only, "412 Hz",
    // "-20 dB · 5:1" - or empty. Inert defaults, same pattern as every
    // optional hook on this interface; getFeedbackText() stays as the
    // English fallback for anything not wired up.
    virtual int getAnswerDirection() const { return 0; }
    virtual juce::String getAnswerDetail() const { return {}; }

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
