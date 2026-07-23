#pragma once

#include <juce_dsp/juce_dsp.h>

// Shared between the processor (real-time filtering) and the editor
// (drawing the response curve for display only) so both always agree on
// which band is which filter type.
namespace EQCoefficients
{
    enum class BandType { lowShelf, bell, highShelf };

    inline BandType typeForBand (int bandIndex)
    {
        if (bandIndex == 0)
            return BandType::lowShelf;
        if (bandIndex == 3)
            return BandType::highShelf;
        return BandType::bell;
    }

    inline const char* nameForBand (int bandIndex)
    {
        switch (typeForBand (bandIndex))
        {
            case BandType::lowShelf:  return "Low Shelf";
            case BandType::highShelf: return "High Shelf";
            default:                  return bandIndex == 1 ? "Bell 1" : "Bell 2";
        }
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr make (int bandIndex, double sampleRate,
                                                           float freqHz, float gainDb, float q)
    {
        const auto gain = juce::Decibels::decibelsToGain (gainDb);

        switch (typeForBand (bandIndex))
        {
            case BandType::lowShelf:  return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, freqHz, q, gain);
            case BandType::highShelf: return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, freqHz, q, gain);
            default:                  return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, freqHz, q, gain);
        }
    }
}
