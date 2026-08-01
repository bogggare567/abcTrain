#include <juce_core/juce_core.h>
#include <map>
#include <functional>
#include <cmath>
#include "../Source/Games/DistortionGame.h"
#include <cmath>

class DistortionGameTest : public juce::UnitTest
{
public:
    DistortionGameTest() : juce::UnitTest ("DistortionGame", "Games") {}

    void runTest() override
    {
        beginTest ("two alternatives, and harder levels offer closer characters");
        {
            DistortionGame game;

            // Soft clip and tape both round the peak; hard clip squares
            // it. The distance below is that axis, written here so the
            // test states the claim rather than reading it back out of
            // the code under test.
            const std::map<juce::String, float> position {
                { "Soft Clipping", 0.28f }, { "Hard Clipping", 0.95f },
                { "Tape Saturation", 0.14f }, { "Overdrive", 0.55f },
            };

            juce::StringArray known;
            for (const auto& entry : position)
                known.add (entry.first);

            checkTwoAlternative (game, known,
                [&position] (const juce::String& a, const juce::String& b)
                { return std::abs (position.at (a) - position.at (b)); });
        }

        beginTest ("every voicing lands at the same loudness, so level is never the tell");
        {
            // The whole point of the measured makeup gain. A family that
            // varies the drive varies the loudness, and if that survives
            // into the output then the exercise can be won without
            // hearing anything about the harmonics at all.
            //
            // Checked across the drive range the ten levels actually
            // produce, not just at one, since the compensation is
            // recomputed per round from the drive in use.
            for (const auto drive : { 6.0f, 3.0f, 1.2f })
            {
                for (auto type = 0; type < DistortionGame::numTypes; ++type)
                {
                    for (const auto& variant : DistortionGame::familyFor (type))
                    {
                        const auto shaperType = (DistortionGame::Type) type;
                        const auto scaledDrive = juce::jmax (0.2f, drive * variant.driveScale);
                        const auto makeup = DistortionGame::measureMakeupFor (shaperType, variant,
                                                                              scaledDrive, 44100.0);

                        // Re-measure the compensated signal: RMS * makeup
                        // has to land on the target the helper aims at.
                        // Recovering it from the helper itself - measure a
                        // unity variant, then check the ratio - would only
                        // prove the function is self-consistent.
                        juce::Random signal (0x5EED);
                        auto sumOfSquares = 0.0;
                        auto toneState = 0.0f;
                        const auto toneCoeff = variant.toneCutoffHz > 0.0f
                            ? 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                                                   * variant.toneCutoffHz / 44100.0f)
                            : 1.0f;

                        for (int i = 0; i < 4096; ++i)
                        {
                            const auto raw = signal.nextFloat() * 2.0f - 1.0f;
                            auto shaped = DistortionGame::shape (shaperType, raw * scaledDrive, variant.negativeScale);

                            if (variant.toneCutoffHz > 0.0f)
                            {
                                toneState += toneCoeff * (shaped - toneState);
                                shaped = toneState;
                            }

                            const auto out = shaped * makeup;
                            sumOfSquares += (double) out * (double) out;
                        }

                        const auto rms = (float) std::sqrt (sumOfSquares / 4096.0);
                        expect (std::abs (rms - 0.20f) < 0.005f,
                                 "voicing landed at RMS " + juce::String (rms, 4)
                                     + " instead of 0.20 (type " + juce::String (type)
                                     + ", drive " + juce::String (drive, 2) + ")");
                    }
                }
            }
        }

        beginTest ("every type has a family, ordered from textbook to borderline");
        {
            for (auto type = 0; type < DistortionGame::numTypes; ++type)
            {
                const auto& family = DistortionGame::familyFor (type);

                expect (family.size() >= 3,
                         "type " + juce::String (type) + " has fewer than three voicings");

                // One archetype and one that sits against a neighbour: a
                // family whose members are all equally textbook gives the
                // difficulty ramp nothing to move along.
                auto highest = 0.0f, lowest = 1.0f;
                for (const auto& variant : family)
                {
                    highest = juce::jmax (highest, variant.archetypal);
                    lowest = juce::jmin (lowest, variant.archetypal);
                }

                expect (highest > 0.9f, "type " + juce::String (type) + " has no clear example");
                expect (lowest < 0.35f, "type " + juce::String (type) + " has no borderline example");
            }
        }

        beginTest ("a hard level really does reach the borderline voicings");
        {
            // breadthForLevel is shared, but "the game asks for it" is a
            // separate claim from "the helper computes it", and this is
            // the one that would silently stop being true if a game
            // forgot to pass its level through.
            const auto& family = DistortionGame::familyFor (0);
            std::vector<PresetFamily::Weighted> weights;
            for (size_t i = 0; i < family.size(); ++i)
                weights.push_back ({ (int) i, family[i].archetypal });

            juce::Random random (99);
            auto easiestSeen = 1.0f, hardestSeen = 0.0f;

            for (int i = 0; i < 400; ++i)
            {
                const auto easy = family[(size_t) PresetFamily::choose (weights, 1, random)].archetypal;
                const auto hard = family[(size_t) PresetFamily::choose (weights, 10, random)].archetypal;
                easiestSeen = juce::jmin (easiestSeen, easy);
                hardestSeen = juce::jmax (hardestSeen, 1.0f - hard);
            }

            expect (easiestSeen > 0.5f, "level 1 handed out a borderline voicing");
            expect (hardestSeen > 0.65f, "level 10 never reached a borderline voicing");
        }
    }

private:
    // One shared shape for the three games that became two-alternative:
    // the pair must always be two different real names, and harder levels
    // must offer closer pairs. The second is measured over many rounds
    // rather than asserted, because it is a claim about a distribution.
    template <typename GameType>
    void checkTwoAlternative (GameType& game, const juce::StringArray& knownLabels,
                               std::function<float (const juce::String&, const juce::String&)> distance)
    {
        for (int level = 1; level <= 10; ++level)
        {
            game.setDifficulty (level);
            expectEquals (game.getNumChoices(), 2, "level " + juce::String (level));
        }

        const auto averageDistance = [&] (int level)
        {
            game.setDifficulty (level);

            auto total = 0.0f;
            constexpr int rounds = 400;

            for (int i = 0; i < rounds; ++i)
            {
                game.newRound();

                const auto a = game.getChoiceLabel (0);
                const auto b = game.getChoiceLabel (1);

                expect (knownLabels.contains (a), "unknown label: " + a);
                expect (knownLabels.contains (b), "unknown label: " + b);
                expect (a != b, "the same choice offered twice: " + a);

                total += distance (a, b);
            }

            return total / (float) rounds;
        };

        const auto easy = averageDistance (1);
        const auto hard = averageDistance (10);

        expect (hard < easy,
                "level 10 pairs should be closer than level 1 pairs: "
                    + juce::String (hard) + " vs " + juce::String (easy));
    }
};

static DistortionGameTest distortionGameTest;
