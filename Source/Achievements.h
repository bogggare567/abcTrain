#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>

// What a player has actually achieved, as a list of named things rather
// than a single number going up.
//
// This replaces a bar on the training screen. A bar answers "how far to
// the next level", which is a question about the app, not about hearing:
// it says nothing about *what* got better, it is the same shape whichever
// exercise you played, and it is on screen the entire time whether or not
// anything happened. Named achievements answer a different and more
// useful question - what have I actually got good at, and what is nearly
// within reach - and they only appear when one is earned.
//
// The design constraint that shaped this: **every achievement is a real
// claim about the player's own recorded history.** There are no
// participation trophies for opening the app, and nothing here compares
// the player to anyone else, because there is no server and inventing a
// percentile would be a lie (this came up directly - see docs/roadmap.md).
// Everything below is computed from numbers ProgressManager already keeps.
//
// Pure data and pure functions: no juce::Component, no PropertiesFile, no
// message loop. ProgressManager owns the persistence and calls
// evaluate(); tests call evaluate() directly with a hand-built Snapshot.
namespace Achievements
{
    // What an achievement asks about. Kept separate from the definition
    // list so the *shape* of a rule is fixed and only its numbers vary -
    // which is what stops a hundred bespoke lambdas accumulating here.
    enum class Kind
    {
        totalCorrect,        // n correct answers, all exercises
        exerciseRounds,      // n rounds in one specific exercise
        exerciseAccuracy,    // accuracy >= n% in one exercise, over a floor of rounds
        answerStreak,        // best streak of n in one exercise
        survivalScore,       // a Survival run scoring n
        blitzScore,          // a Blitz run scoring n
        dayStreak,           // n days in a row
        exerciseLevel,       // level n in one specific exercise
        everyExerciseLevel,  // level n in *every* exercise
        breadth              // at least one round in n different exercises
    };

    // How hard one is, expressed as colour rather than as a different
    // glyph per achievement. Four steps is enough to read at a glance;
    // more would be a taxonomy nobody asked for.
    //
    // The four metals, in the order everyone already knows them.
    // `platinum` is deliberately near-unreachable - a shelf where
    // everything is collectable in a month is a shelf nobody looks at
    // twice.
    //
    // Colour is doing real work here, which is why it is *only* used here:
    // a tier is a single fact with a natural ordering, and four metals say
    // it without a word of text. Everywhere else in the app the colour was
    // turned down (see AppIcons::drawBadged) so that this reads.
    enum class Tier
    {
        bronze,
        silver,
        gold,
        platinum
    };

    juce::Colour colourForTier (Tier) noexcept;

    // i18n key for the tier's name, resolved by the caller.
    const char* nameKeyForTier (Tier) noexcept;

    struct Definition
    {
        // Stable across releases: it is the persistence key. Renaming one
        // silently re-locks it for every existing player, so don't.
        const char* id;

        Kind kind;
        int threshold;
        Tier tier = Tier::bronze;

        // Only meaningful for the per-exercise kinds; -1 means "any".
        int gameIndex;

        // i18n keys, so the name and the one-line description translate
        // like everything else. Falls back to the key itself if missing,
        // per LocalisationManager::getText.
        const char* nameKey;
        const char* descriptionKey;
    };

    // Everything ProgressManager knows that an achievement can ask about.
    // Passed by const reference rather than read off ProgressManager
    // directly, so tests can build one by hand.
    struct Snapshot
    {
        struct PerGame
        {
            int roundsPlayed = 0;
            int correctAnswers = 0;
            int bestStreak = 0;
            int bestSurvivalScore = 0;
            int bestBlitzScore = 0;
            int level = 1;
        };

        std::vector<PerGame> games;
        int streakDays = 0;
    };

    // Minimum rounds before an accuracy achievement can be earned at all.
    // Without it, one lucky answer is 100% accuracy - which would make the
    // hardest-sounding achievements the first ones anyone gets.
    static constexpr int accuracyMinimumRounds = 20;

    const std::vector<Definition>& all();

    // nullptr for an id that isn't defined (a save file from a newer
    // build, say) rather than an assert - the same "unknown entry falls
    // back gracefully" rule the i18n and icon lookups follow.
    const Definition* find (const juce::String& id);

    // True if this snapshot satisfies the definition. Pure; no state.
    bool isEarned (const Definition&, const Snapshot&);

    // Every id this snapshot has earned, in definition order.
    std::vector<juce::String> evaluate (const Snapshot&);

    // How far along an unearned achievement is, 0..1 - so the UI can show
    // "nearly there" rather than a wall of identical locked boxes. Returns
    // 1.0 for anything already earned.
    float progressTowards (const Definition&, const Snapshot&);
}
