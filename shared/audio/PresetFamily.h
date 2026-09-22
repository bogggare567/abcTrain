#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <cmath>
#include <utility>

// A category with several members, and a way to pick a harder one.
//
// The problem this solves: "Hall" used to be *one* setting with a small
// random nudge on it. That teaches recognition of a recording, not of a
// hall - the exact lesson EQGame already learned when its eight fixed
// octave centres turned out to be memorisable as positions rather than as
// frequencies. A category you can only hear one example of is a sample,
// not a category.
//
// So each answer gets a **family**: several genuinely different settings
// that all belong to it. A big wooden live room and a small tiled booth
// are both rooms, and somebody who can only recognise one of them has not
// learned what a room sounds like.
//
// **Difficulty is distance to the boundary, not the number of choices.**
// Every variant carries how *archetypal* it is: 1 is the textbook example,
// 0 is the one that sits right up against the neighbouring category - a
// hall so small it is nearly a chamber, a compression setting so light it
// is nearly untouched. Early levels draw from the archetypes, later ones
// from the edges. That is what "harder" means for a human ear: not more
// buttons, but two things that are genuinely close together.
//
// Header-only and dependency-free so every game can use it and
// tests/PresetFamilyTest can drive the selection rule directly.
namespace PresetFamily
{
    // 1 = the clearest example of its category, 0 = right on the border
    // with a neighbour. Only the ordering matters; the absolute value is
    // just a way of writing that ordering down.
    struct Weighted
    {
        int index = 0;
        float archetypal = 1.0f;
    };

    // The window of variants a tier may draw from, as a fraction of the
    // family sorted from most to least archetypal.
    //
    // Level 1 sees only the top third; level 10 sees everything, which
    // means it also sees the borderline ones. It is a *window*, not a
    // shift: the archetypes never stop appearing, because a hard tier
    // made only of edge cases stops teaching the category and starts
    // teaching the edge.
    inline float breadthForLevel (int level)
    {
        const auto clamped = juce::jlimit (1, 10, level);
        return juce::jmap ((float) clamped, 1.0f, 10.0f, 0.34f, 1.0f);
    }

    // Picks one variant from `family` for this level.
    //
    // Returns the family's own index, so a caller can keep its variants in
    // whatever order reads best in source and let this handle the
    // difficulty ordering.
    inline int choose (const std::vector<Weighted>& family, int level, juce::Random& random)
    {
        if (family.empty())
            return 0;

        // Sorted by how archetypal, hardest last.
        auto sorted = family;
        std::sort (sorted.begin(), sorted.end(),
                    [] (const Weighted& a, const Weighted& b)
                    { return a.archetypal > b.archetypal; });

        const auto breadth = breadthForLevel (level);

        // At least one, always: a family of two at level 1 must still be
        // able to produce something.
        const auto available = juce::jmax (1,
            (int) std::ceil (breadth * (float) sorted.size()));

        return sorted[(size_t) random.nextInt (available)].index;
    }

    // Draws the two categories a round asks about.
    //
    // `positions` places every category on one axis of character - how
    // bright a space is, how hard a clipper bites, where a band sits in
    // the spectrum - so "close together" means something a listener would
    // recognise rather than "adjacent in the array". Each game writes its
    // own axis down, which is where the claim about what is confusable
    // with what actually lives.
    //
    // A level sees a **window over the pairs ranked by distance**, sliding
    // from the far end to the near end: level 1 draws from the most
    // obviously different pairs, level 10 from the closest ones.
    //
    // Ranked, not measured against a threshold. The first version of this
    // admitted every pair whose distance fell between a sliding ceiling
    // and floor, which reads well on paper and falls apart on a small,
    // unevenly-spaced set - the exact case every game here is. Measured on
    // ReverbGame's five types it gave: one pair, forever, at level 10;
    // four levels (3 to 7) that were *identical* to each other; Spring in
    // 80% of level-1 rounds and then never again at any higher level. A
    // window over the ranking cannot do any of that: its size does not
    // depend on how the distances happen to cluster, so every level has
    // the same number of candidates and consecutive levels always differ.
    //
    // `distance` lets a game override what "far apart" means where a
    // single axis can't carry it - ReverbGame's Spring is a mechanism
    // rather than a size, so its distance to a room is not the gap
    // between two numbers. Pass nullptr for the plain axis distance.
    struct RankedPair { int a, b; float distance; };

    // Every pair, furthest apart first - so index 0 is the easiest
    // question - plus the slice of that ranking a level draws from.
    // Shared by drawPair and hardestPairForLevel, so "what a level
    // offers" and "what a level is described as" cannot drift apart.
    inline std::vector<RankedPair> rankPairs (const std::vector<float>& positions,
                                              const std::function<float (int, int)>& distance = {})
    {
        std::vector<RankedPair> all;
        const auto count = (int) positions.size();

        for (int a = 0; a < count; ++a)
            for (int b = a + 1; b < count; ++b)
                all.push_back ({ a, b, distance ? distance (a, b)
                                                : std::abs (positions[(size_t) a] - positions[(size_t) b]) });

        std::sort (all.begin(), all.end(),
                    [] (const RankedPair& x, const RankedPair& y) { return x.distance > y.distance; });
        return all;
    }

    // [start, start + length) into rankPairs' result for this level.
    inline std::pair<int, int> windowForLevel (int totalPairs, int level)
    {
        // Wide enough that a level is not one memorised question, narrow
        // enough that a level means something. Two is the floor: a game
        // with three categories has only three pairs to begin with.
        const auto window = juce::jmax (2, (int) std::ceil (0.45f * (float) totalPairs));
        const auto clamped = (float) juce::jlimit (1, 10, level);
        const auto start = juce::jlimit (0, juce::jmax (0, totalPairs - window),
            juce::roundToInt (juce::jmap (clamped, 1.0f, 10.0f, 0.0f, (float) (totalPairs - window))));

        return { start, juce::jmin (window, totalPairs - start) };
    }

    inline std::array<int, 2> drawPair (const std::vector<float>& positions,
                                         int level, juce::Random& random,
                                         const std::function<float (int, int)>& distance = {})
    {
        if (positions.size() < 2)
            return { { 0, 0 } };

        const auto all = rankPairs (positions, distance);
        const auto [start, length] = windowForLevel ((int) all.size(), level);
        const auto& picked = all[(size_t) (start + random.nextInt (length))];

        // Returned in random order, or the answer would drift to one side
        // of the panel.
        return random.nextBool() ? std::array<int, 2> { { picked.a, picked.b } }
                                 : std::array<int, 2> { { picked.b, picked.a } };
    }

    // The closest pair a level can put in front of the player - which is
    // what the level *means* for a two-alternative exercise, the way a
    // tolerance in octaves is what it means on a ruler (ADR 035).
    inline std::array<int, 2> hardestPairForLevel (const std::vector<float>& positions, int level,
                                                    const std::function<float (int, int)>& distance = {})
    {
        if (positions.size() < 2)
            return { { 0, 0 } };

        const auto all = rankPairs (positions, distance);
        const auto [start, length] = windowForLevel ((int) all.size(), level);
        const auto& p = all[(size_t) (start + length - 1)];
        return { { p.a, p.b } };
    }
}
