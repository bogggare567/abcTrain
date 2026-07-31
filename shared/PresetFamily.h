#pragma once

#include <juce_core/juce_core.h>
#include <vector>

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
}
