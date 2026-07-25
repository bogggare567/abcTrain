#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

// Log-frequency <-> normalised-x mapping shared by the spectrum display,
// the EQ response curve, and the highlighted-band overlay, so all three
// always line up on screen. Also the short plain-language descriptions
// shown while a band's frequency is being dragged.
namespace FrequencyGuide
{
    constexpr float minFreq = 20.0f;
    constexpr float maxFreq = 20000.0f;

    inline float frequencyToProportion (float freqHz)
    {
        return (float) (std::log (freqHz / minFreq) / std::log (maxFreq / minFreq));
    }

    inline float proportionToFrequency (float proportion)
    {
        return (float) (minFreq * std::pow (maxFreq / minFreq, proportion));
    }

    // Text here mirrors docs/knowledge_base.md's "Эквализация" section -
    // keep both in sync. Original explanations from general, widely-
    // taught practice, not derived from any specific book (see
    // decisions/010-book-library-scope.md). "Learn more" names one
    // relevant book from docs/library_catalog.md - title/author only,
    // never a quote from it.
    inline juce::String describe (float freqHz)
    {
        if (freqHz < 150.0f)
            return "Sub/bass fundamentals - weight and body. Boosting here "
                   "adds low-end power; too much turns into boominess or mud "
                   "on smaller speakers. A high-pass just above this zone is "
                   "often more useful than a boost - judge it by ear in "
                   "context, not from one fixed cutoff number.\n"
                   "Learn more: Mike Senior - Mixing Secrets for the Small Studio.";
        if (freqHz < 400.0f)
            return "Low mids - warmth and fullness, but also where several "
                   "instruments stacked together turn muddy or boxy. Cutting "
                   "here is often more useful than boosting; if a real boost "
                   "is unavoidable, splitting it between this frequency and "
                   "its octave above can sound less resonant than one big "
                   "narrow bump.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";
        if (freqHz < 1000.0f)
            return "Mids - the body and fullness of most instruments and "
                   "vocals lives here. Small moves go a long way: this is "
                   "the range where the ear notices a boost fastest, "
                   "sometimes from as little as 2-3 dB.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";
        if (freqHz < 3000.0f)
            return "Presence - clarity and forwardness, close to the vocal "
                   "formant range the ear is most sensitive to (and to the "
                   "ear canal's own resonance around 2.6-3.5 kHz). Useful "
                   "for intelligibility, but a fixed deep cut here can strip "
                   "speech of clarity - a gentler, dynamic touch is safer.\n"
                   "Learn more: F. Alton Everest - Master Handbook of Acoustics.";
        if (freqHz < 6000.0f)
            return "Upper presence - definition and bite, the edge that helps "
                   "a source cut through a busy mix without turning the level "
                   "up. A narrow bell aimed right at the bite reads "
                   "differently than a broad shelf lifting the whole "
                   "region - pick the shape for the actual job, not just the "
                   "frequency.\n"
                   "Learn more: Roey Izhaki - Mixing Audio.";
        if (freqHz < 12000.0f)
            return "Brilliance - air and sparkle, but also where sibilance "
                   "('s'/'t' hiss on vocals) and cymbal harshness live - boost "
                   "carefully. If sibilance is the actual problem, a "
                   "dedicated de-esser targeting just this band usually beats "
                   "pulling the whole zone down with static EQ.\n"
                   "Learn more: Mike Senior - Mixing Secrets for the Small Studio.";
        return "Air - a sense of openness and space; there's no fundamental "
               "content up here, just harmonics, so it's easy to overdo "
               "without realizing it. Even a boost this high can raise the "
               "true peak more than expected - worth rechecking levels after "
               "a generous lift.\n"
               "Learn more: Mike Senior - Mixing Secrets for the Small Studio.";
    }
}
