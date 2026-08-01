#pragma once

#include "Game.h"
#include "../../shared/TestSignalGenerator.h"
#include <array>
#include <vector>
#include "../../shared/PresetFamily.h"

// "Guess the reverb type" exercise: a repeating percussive noise burst
// through one of four reverb characters. Room/Hall/Plate are approximated
// with different juce::dsp::Reverb (Freeverb-derived algorithm) parameter
// presets - not physically modeled per type, just tuned by ear to sound
// distinct, same approach CompressionGame takes with its presets. Spring
// is different enough (a metallic comb/allpass "boing" that Freeverb-style
// algorithms don't produce) that it's built separately, as a cascade of
// resonant allpass filters instead of the Reverb DSP object.
class ReverbGame : public Game
{
public:
    // Five, not four. Room vs. Hall on its own is a size question and
    // nothing else - one is short and one is long, and a first-time player
    // gets it right by accident. Chamber sits deliberately *between* them,
    // so the easy tier is no longer a coin flip with a giveaway, and the
    // whole set has to be told apart by character rather than by length.
    static constexpr int numTypes = 5;

    juce::String getName() const override { return "Guess the Reverb"; }
    juce::String getInstructions() const override
    {
        return "Listen to the repeating hit, then guess the reverb type. "
               "Room and Hall differ mainly in decay length, Plate is a "
               "bright studio character with no real geometry, and Spring "
               "has a distinctive metallic clang.";
    }

    void prepare (const juce::dsp::ProcessSpec&) override;
    void process (juce::AudioBuffer<float>&) override;
    void setDifficulty (int level) override;
    void setReferenceAudioLibrary (const ReferenceAudioLibrary* library) override { noise.setLibrary (library); }

    void newRound() override;
    void submitAnswer (int choiceIndex) override;

    // **Always two.**
    //
    // It used to be two, three, four or five depending on level, which
    // makes a hard round harder in the wrong way: more buttons is more
    // reading and more luck, not finer hearing. Two alternatives is what
    // every listening test worth the name uses - the question becomes
    // "which of these two", and difficulty is how close together the two
    // are. Level 1 offers a tiled booth against a cathedral; level 10
    // offers a big live room against a small chamber.
    //
    // It also means the choice count never changes mid-session, which is
    // the one thing ADR 002 made the editor cope with.
    int getNumChoices() const override { return 2; }
    juce::String getChoiceLabel (int choiceIndex) const override;

    bool hasAnswered() const override { return answered; }
    int getCorrectChoiceIndex() const override { return correctTypeIndex; }
    int getChosenChoiceIndex() const override { return chosenTypeIndex; }
    bool wasLastAnswerCorrect() const override { return lastAnswerCorrect; }
    juce::String getFeedbackText() const override;

    // How far apart two type *names* are on the character axis, for
    // tests/ReverbGameTest - which needs to check that harder levels
    // really do offer closer pairs, and has only the labels to go on.
    // Public because the alternative is a friend declaration for one
    // assertion, which hides the seam rather than naming it.
    static float confusabilityForTest (const juce::String& labelA, const juce::String& labelB);

    int getScore() const override { return correctCount; }
    int getRoundsPlayed() const override { return totalCount; }

private:
    // One member of the chosen type's family, redrawn every round.
    //
    // Each type has several genuinely different settings rather than one
    // with a nudge on it - a tiled booth and a big wooden live room are
    // both Rooms, and somebody who recognises only one of them has learned
    // a recording, not a room. Which member you get depends on the level:
    // early on the archetypes, later the ones sitting against the
    // neighbouring type. See shared/PresetFamily.h.
    struct Variant
    {
        float roomSize;
        float damping;
        float width;
        float wet;
        float archetypal;   // 1 = textbook, 0 = borderline
    };

    static const std::vector<Variant>& familyFor (int type);

    Variant roundVariant {};
    int difficultyLevel = 1;

    // Index into typeLabels, not a slot. Chamber was inserted at index 1,
    // so Spring moved from 3 to 4 - the kind of silent shift a named
    // constant exists to make loud.
    static constexpr int plateTypeIndex = 3;
    static constexpr int springTypeIndex = 4;
    static constexpr float springQ = 4.0f;
    static const std::array<float, 4> springFrequenciesHz;
    static const std::array<const char*, numTypes> typeLabels;

    // Unlock order: Room, Hall, Plate, Chamber, Spring. Indices into
    // typeLabels - see setDifficulty for why this order and not label
    // order.
    static constexpr std::array<int, numTypes> typeOrder { { 0, 2, 3, 1, 4 } };

    // Draws the two types this round asks about, ordered randomly so the
    // answer does not favour one side.
    std::array<int, 2> drawPair();

    static std::vector<PresetFamily::Weighted> weightsFor (const std::vector<Variant>&);

    void updateReverbForType();

    TestSignalGenerator noise;
    juce::dsp::Reverb reverb;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>, 4> springAllpass;
    double sampleRate = 44100.0;
    // Defaults to the easy tier (matches EQGame/CompressionGame both
    // defaulting to their easy values) in case something ever constructs
    // a game and calls newRound() before setDifficulty() - the real app
    // always calls setDifficulty() during ProgressManager construction,
    // before the host's first prepareToPlay(), so this default is a
    // defensive fallback, not something normally observed.
    // The two types on offer this round, as indices into typeLabels.
    // pairTypes[0] is choice 0.
    std::array<int, 2> pairTypes { { 0, 2 } };

    // How confusable a pair is, 0..1: 1 = the two furthest-apart types,
    // 0 = adjacent ones. Which pairs a level may draw is the whole of its
    // difficulty now.
    static float confusabilityOf (int typeA, int typeB);

    int samplesSinceBurstStart = 0;
    int attackSamples = 1;
    int decayTauSamples = 1;
    int burstPeriodSamples = 1;

    juce::Random random;


    int correctTypeIndex = 0;
    int chosenTypeIndex = -1;
    bool answered = false;
    bool lastAnswerCorrect = false;

    int correctCount = 0;
    int totalCount = 0;
};
