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

    inline juce::String describe (float freqHz)
    {
        if (freqHz < 150.0f)   return "Sub/bass fundamentals - weight and body.";
        if (freqHz < 400.0f)   return "Low mids - warmth, muddy in excess.";
        if (freqHz < 1000.0f)  return "Mids - body and fullness of most instruments.";
        if (freqHz < 3000.0f)  return "Presence - clarity and forwardness; harsh in excess.";
        if (freqHz < 6000.0f)  return "Upper presence - definition and bite.";
        if (freqHz < 12000.0f) return "Brilliance - air and sparkle; sibilance risk.";
        return "Air - a sense of openness, no fundamental content up here.";
    }
}
