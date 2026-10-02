#include <juce_core/juce_core.h>
#include "../Source/BattleShare.h"
#include "../shared/ui/CommunityLink.h"

// The text a player pastes into the community chat (ADR 053): it carries
// what the battle produced and nothing it did not.
class BattleShareTest : public juce::UnitTest
{
public:
    BattleShareTest() : juce::UnitTest ("BattleShare", "Live") {}

    void runTest() override
    {
        beginTest ("a bot battle: names, the animal, outcome, HP, score, link");
        {
            BattleShare::Result r;
            r.exercise = "Guess the Band";
            r.you = "Bogdan";
            r.them = "Cat";
            r.themBot = "cat";
            r.outcome = BattleShare::Outcome::won;
            r.hpYou = 78;
            r.hpThem = 42;
            r.scoreYou = 6;
            r.scoreThem = 4;
            r.rounds = "10 rounds";

            BattleShare::Words w;
            const auto text = BattleShare::format (r, w);

            expect (text.contains ("abcTrain Battle"));
            expect (text.contains ("Bogdan vs " + BattleShare::animalOf ("cat") + " Cat"), text);
            expect (text.contains ("Win"));
            expect (text.contains ("78 HP vs 42 HP"), text);
            expect (text.contains ("6 : 4"), text);
            expect (text.contains ("10 rounds"));
            expect (text.endsWith (CommunityLink::shortUrl));
            expect (! text.contains ("Decibelo"), "no rating line without a rated battle");
        }

        beginTest ("a battle with a person: no animal, the server's notes come through");
        {
            BattleShare::Result r;
            r.you = "player1";
            r.them = "player2";
            r.outcome = BattleShare::Outcome::lost;
            r.notes = juce::StringArray::fromLines (juce::String::fromUTF8 ("Decibelo \xe2\x88\x92" "12 \xe2\x86\x92 1488\n"));

            const auto text = BattleShare::format (r, {});
            expect (text.contains ("Loss"));
            expect (text.contains ("Decibelo"), text);
            for (auto id : { "hound", "cat", "viper", "owl", "bat", "elephant" })
                expect (! text.contains (BattleShare::animalOf (id)));
        }

        beginTest ("every bot has its animal");
        for (auto id : { "hound", "cat", "viper", "owl", "bat", "elephant" })
            expect (BattleShare::animalOf (id).isNotEmpty(), id);
    }
};

static BattleShareTest battleShareTest;
