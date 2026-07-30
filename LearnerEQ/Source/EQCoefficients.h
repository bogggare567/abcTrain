#pragma once

#include <juce_dsp/juce_dsp.h>

// Shared between the processor (real-time filtering) and the editor
// (drawing the response curve for display only) so both always agree on
// what a band is doing.
//
// **Type is a property of the band now, not of its slot.** It used to be
// derived from the index - band 0 was always a low shelf, band 3 always a
// high shelf - which is what a fixed four-band EQ does. This one lets any
// filter go anywhere and lets you add as many as the job needs, so the
// slot says nothing and the band carries its own type.
namespace EQCoefficients
{
    enum class BandType
    {
        bell = 0,
        lowShelf,
        highShelf,
        highPass,
        lowPass,
        notch
    };

    inline constexpr int numTypes = 6;

    inline const char* nameForType (BandType type)
    {
        switch (type)
        {
            case BandType::bell:      return "Bell";
            case BandType::lowShelf:  return "Low Shelf";
            case BandType::highShelf: return "High Shelf";
            case BandType::highPass:  return "High Pass";
            case BandType::lowPass:   return "Low Pass";
            case BandType::notch:     return "Notch";
        }

        return "Bell";
    }

    // Whether the band's gain does anything. A pass filter and a notch cut
    // by their shape rather than by an amount, so showing them a gain
    // control would be showing a control that lies. The editor greys it;
    // the DSP ignores it either way.
    inline bool usesGain (BandType type)
    {
        return type == BandType::bell
               || type == BandType::lowShelf
               || type == BandType::highShelf;
    }

    inline BandType typeFromIndex (int index)
    {
        return (BandType) juce::jlimit (0, numTypes - 1, index);
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr make (BandType type, double sampleRate,
                                                           float freqHz, float gainDb, float q)
    {
        // Clamped because Nyquist moves with the host's sample rate: a band
        // parked at 20 kHz in a 32 kHz session would otherwise ask JUCE for
        // a filter above Nyquist, which asserts in a debug build and
        // produces nonsense in a release one.
        const auto freq = juce::jlimit (10.0f, (float) (sampleRate * 0.49), freqHz);
        const auto safeQ = juce::jmax (0.05f, q);
        const auto gain = juce::Decibels::decibelsToGain (gainDb);

        using Coefficients = juce::dsp::IIR::Coefficients<float>;

        switch (type)
        {
            case BandType::lowShelf:  return Coefficients::makeLowShelf (sampleRate, freq, safeQ, gain);
            case BandType::highShelf: return Coefficients::makeHighShelf (sampleRate, freq, safeQ, gain);
            case BandType::highPass:  return Coefficients::makeHighPass (sampleRate, freq, safeQ);
            case BandType::lowPass:   return Coefficients::makeLowPass (sampleRate, freq, safeQ);
            case BandType::notch:     return Coefficients::makeNotch (sampleRate, freq, safeQ);
            case BandType::bell:
            default:                  return Coefficients::makePeakFilter (sampleRate, freq, safeQ, gain);
        }
    }
}
