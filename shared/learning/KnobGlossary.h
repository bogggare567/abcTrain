#pragma once

#include <juce_core/juce_core.h>

/**
    What a knob of somebody else's plugin does, by its name (the author,
    2026-10-24: "как будто человек первый раз зашёл и ему рассказывают, что
    за что отвечает").

    One dictionary for every plugin: about thirty names that mean the same
    thing on every compressor, equaliser or reverb - Threshold, Ratio,
    Attack, Decay, Pre-delay, Mix, Drive... A vendor's manual is not read
    and cannot be: there is no common format, and a script per plugin is a
    script nobody would keep up. A knob whose name is not here is shown as
    "no description - turn it and listen", and is not asked about first.

    The words themselves are i18n strings: own.g.<key>.what (what it does)
    and own.g.<key>.listen (what to listen for). The knob's own name stays
    as the plugin writes it (parameter names are not translated).
*/
namespace KnobGlossary
{
    /** The dictionary key for a parameter name ("Attack" -> "attack",
        "Pre-Delay" -> "predelay"), or empty when the name is not known. */
    juce::String keyFor (const juce::String& parameterName);

    /** Every key, in the order the familiarisation shows them. */
    const juce::StringArray& keys();

    /** Where the knob comes in the familiarisation: the essentials of a
        kind of plugin first (lower is earlier). */
    int orderOf (const juce::String& key);

    /** The unit a lesson writes this knob's value in (lessons/FORMAT.md,
        "plugin: any"), whatever unit the plugin itself shows: "dB", "ms",
        "s", "Hz", "%", "ratio" (4 means 4:1), "Q" (a bare number), or "" when plugins disagree
        too much to name one (Drive, Tone, Depth...) - such a knob is
        placed by position (pos=0.7) or by min / low / high / max. */
    juce::String unitOf (const juce::String& key);

    /** The values a lesson may ask for in that unit - wide enough for any
        plugin, narrow enough to catch a typo (-200 dB, 40 s of attack). */
    juce::Range<double> rangeOf (const juce::String& key);
}
