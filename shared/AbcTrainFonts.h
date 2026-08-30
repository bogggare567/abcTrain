#pragma once

#include <juce_graphics/juce_graphics.h>

// The interface typeface, carried in the binary.
//
// A plugin cannot ask its host's machine to have a font installed, and the
// previous approach - pick the best face this operating system happens to
// ship - meant the same window looked like three different products on
// three machines. These twelve faces are embedded through the FontData
// binary target, registered with JUCE at startup, and from then on can be
// asked for by family name like any installed font (that is the documented
// contract of Typeface::createSystemTypefaceFor over raw data, and it is
// what makes them usable in a fallback list too).
//
// **Barlow has no Cyrillic.** The family ships Latin, Latin-ext and
// Vietnamese, nothing else - so in the original design mockup every
// Russian word was already falling through to whatever the browser had,
// which in a browser is invisible and in a Russian-first app is not. The
// Cyrillic is carried here by a companion pinned out of Roboto's variable
// font at the same three widths, registered under `abcTrain Cyr*` names so
// it can never be confused with a Roboto the host machine has installed.
// Every font this module hands out names that companion as its fallback,
// so a heading with a Russian word and an English one in it is one shape
// and one weight throughout.
namespace AbcTrainFonts
{
    // The three widths the design uses, and the Cyrillic companion for
    // each. Latin and Cyrillic are separate files because each is subset
    // to the one script it is there for - that is what takes twelve faces
    // from 1.3 MB to 356 KB.
    namespace Family
    {
        inline constexpr const char* condensed     = "Barlow Condensed";
        inline constexpr const char* semiCondensed = "Barlow SemiCondensed";
        inline constexpr const char* body          = "Barlow";

        inline constexpr const char* cyrCondensed     = "abcTrain Cyr Condensed";
        inline constexpr const char* cyrSemiCondensed = "abcTrain Cyr SemiCondensed";
        inline constexpr const char* cyrBody          = "abcTrain Cyr";
    }

    // Registers all twelve faces, once. Safe to call from anywhere and as
    // often as you like; the first call does the work and the Typeface::Ptrs
    // are held for the lifetime of the process, which is what keeps them
    // registered.
    void ensureRegistered();

    // True once the faces are really available - false only if the binary
    // data failed to parse, in which case every font below silently falls
    // back to the system's own sans rather than drawing nothing.
    bool areEmbeddedFontsAvailable();

    // A font in one of the three widths, with the matching Cyrillic
    // companion already in its fallback list.
    juce::Font condensed     (float height, const juce::String& style);   // "SemiBold" | "Bold"
    juce::Font semiCondensed (float height, const juce::String& style);   // "Regular"  | "Medium"
    juce::Font body          (float height, const juce::String& style);   // "Regular"  | "Medium"
}
