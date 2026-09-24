#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <complex>

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

    // A bell whose shape does not collapse near Nyquist.
    //
    // The RBJ cookbook bell (juce's makePeakFilter) comes from the bilinear
    // transform, which squeezes the whole analog axis into 0..fs/2. Low down
    // nobody can tell; high up the bell gets narrower and lopsided - at
    // 44.1 kHz a Q 2 bell at 16 kHz is 0.9 dB half an octave above and below
    // its centre where the analog bell it claims to be is 3.1 dB. For an EQ
    // that teaches what a frequency sounds like, and for an exercise whose
    // top targets were quietly harder than its level said, that is wrong.
    //
    // This is Vicanek's matched design ("Matched Second Order Digital
    // Filters", 2016): poles by impulse invariance, which places them
    // exactly, then the numerator chosen so the magnitude equals the analog
    // prototype's at DC, at the centre and at Nyquist. A cut is the exact
    // reciprocal of the boost - which is also true of the analog bell - so
    // it is built by inverting one; matching a deep cut directly near
    // Nyquist is numerically fragile. Checked against the analog bell over
    // 22.05-192 kHz, 20 Hz-0.45 fs, Q 0.3-10, +/-18 dB: worst 3.4 dB, at
    // 0.45 fs, where the RBJ bell is 17 dB out.
    //
    // Same arguments and the same {b0, b1, b2, a0, a1, a2} layout as juce's
    // ArrayCoefficients, and allocation-free, so it drops in anywhere.
    inline std::array<float, 6> makeMatchedBell (double sampleRate, float freqHz, float q, float gainDb) noexcept
    {
        const auto fs = sampleRate > 0.0 ? sampleRate : 44100.0;
        const auto f0 = juce::jlimit (10.0, fs * 0.49, (double) freqHz);
        const auto Q = juce::jmax (0.05, (double) q);
        const auto absDb = std::abs ((double) gainDb);

        if (absDb < 1.0e-4)
            return { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };

        // The analog bell (s^2 + s A/Q + 1) / (s^2 + s/(A Q) + 1), as a
        // boost: its poles have Q = A*Q and its centre gain is A^2.
        const auto A = std::pow (10.0, absDb / 40.0);
        const auto G = A * A;
        const auto poleQ = Q * A;

        const auto w0 = juce::MathConstants<double>::twoPi * f0 / fs;
        const auto zeta = 1.0 / (2.0 * poleQ);

        const auto a1 = zeta <= 1.0
                          ? -2.0 * std::exp (-zeta * w0) * std::cos (std::sqrt (1.0 - zeta * zeta) * w0)
                          : -2.0 * std::exp (-zeta * w0) * std::cosh (std::sqrt (zeta * zeta - 1.0) * w0);
        const auto a2 = std::exp (-2.0 * zeta * w0);

        // |H|^2 written in phi-space: phi1 = sin^2(w/2), phi0 = 1 - phi1,
        // phi2 = 4 phi0 phi1. Denominator terms are fixed by the poles.
        const auto A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2);
        const auto A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2);
        const auto A2 = -4.0 * a2;

        const auto phi1 = std::pow (std::sin (w0 * 0.5), 2.0);
        const auto phi0 = 1.0 - phi1;
        const auto phi2 = 4.0 * phi0 * phi1;

        // The analog magnitude squared at Nyquist.
        const auto nyquistRatio = (fs * 0.5) / f0;
        const auto re = 1.0 - nyquistRatio * nyquistRatio;
        const auto numIm = nyquistRatio * A / Q;
        const auto denIm = nyquistRatio / (A * Q);
        const auto nyquistMag2 = (re * re + numIm * numIm) / (re * re + denIm * denIm);

        const auto B0 = A0;
        const auto B1 = A1 * nyquistMag2;
        const auto B2 = (G * G * (A0 * phi0 + A1 * phi1 + A2 * phi2) - B0 * phi0 - B1 * phi1) / phi2;

        const auto W = 0.5 * (std::sqrt (B0) + std::sqrt (B1));
        const auto b0 = 0.5 * (W + std::sqrt (juce::jmax (0.0, W * W + B2)));
        const auto b1 = 0.5 * (std::sqrt (B0) - std::sqrt (B1));
        const auto b2 = -B2 / (4.0 * b0);

        if (gainDb >= 0.0f)
            return { (float) b0, (float) b1, (float) b2, 1.0f, (float) a1, (float) a2 };

        // The cut: numerator and denominator swapped. The boost's zeros are
        // the minimum-phase factorisation, so as poles they are stable.
        return { 1.0f, (float) a1, (float) a2, (float) b0, (float) b1, (float) b2 };
    }

    // The same filter as make(), as six plain numbers on the stack.
    //
    // This is the one the audio thread uses. make() returns a
    // reference-counted object built with `new`, and the processor was
    // calling it every 32 samples per gliding band - a heap allocation on
    // the audio thread, the one thing a plugin must never do (found by the
    // external review in ADR 038, confirmed by tests/RealtimeSafetyTest).
    // Assigning an array into the filter's existing coefficients object
    // copies six floats and allocates nothing.
    inline std::array<float, 6> makeArray (BandType type, double sampleRate,
                                           float freqHz, float gainDb, float q)
    {
        const auto freq = juce::jlimit (10.0f, (float) (sampleRate * 0.49), freqHz);
        const auto safeQ = juce::jmax (0.05f, q);
        const auto gain = juce::Decibels::decibelsToGain (gainDb);

        using A = juce::dsp::IIR::ArrayCoefficients<float>;

        switch (type)
        {
            case BandType::lowShelf:  return A::makeLowShelf (sampleRate, freq, safeQ, gain);
            case BandType::highShelf: return A::makeHighShelf (sampleRate, freq, safeQ, gain);
            case BandType::highPass:  return A::makeHighPass (sampleRate, freq, safeQ);
            case BandType::lowPass:   return A::makeLowPass (sampleRate, freq, safeQ);
            case BandType::notch:     return A::makeNotch (sampleRate, freq, safeQ);
            case BandType::bell:
            default:                  return makeMatchedBell (sampleRate, freq, safeQ, gainDb);
        }
    }

    // Slope of a pass filter, as real EQs offer it: 6, 12, 24 or 48 dB per
    // octave. Stored as an index; 12 is what every band had before.
    inline constexpr int numSlopes = 4;
    inline constexpr int defaultSlope = 1;

    inline int slopeDbPerOctave (int index) noexcept
    {
        constexpr int values[] { 6, 12, 24, 48 };
        return values[juce::jlimit (0, numSlopes - 1, index)];
    }

    inline bool usesSlope (BandType type) noexcept
    {
        return type == BandType::highPass || type == BandType::lowPass;
    }

    // A band as a cascade of up to four biquads - one for every type but
    // a steep pass filter. 24 and 48 dB/oct are Butterworth cascades whose
    // last section carries the band's Q (0.707 = plain Butterworth, higher
    // = a resonant bump at the corner, as on the EQs this teaches).
    // Allocation-free: the audio thread calls it.
    struct Sections
    {
        std::array<std::array<float, 6>, 4> coefficients {};
        int count = 1;
    };

    inline Sections makeSections (BandType type, double sampleRate, float freqHz, float gainDb, float q,
                                  int slopeIndex) noexcept
    {
        Sections out;

        if (! usesSlope (type) || juce::jlimit (0, numSlopes - 1, slopeIndex) == 1)
        {
            out.coefficients[0] = makeArray (type, sampleRate, freqHz, gainDb, q);
            return out;
        }

        const auto freq = juce::jlimit (10.0f, (float) (sampleRate * 0.49), freqHz);
        const auto resonance = juce::jmax (0.05f, q) / 0.70710678f;
        const auto high = type == BandType::highPass;
        using A = juce::dsp::IIR::ArrayCoefficients<float>;

        const auto section = [&] (float sectionQ)
        {
            return high ? A::makeHighPass (sampleRate, freq, sectionQ) : A::makeLowPass (sampleRate, freq, sectionQ);
        };

        switch (juce::jlimit (0, numSlopes - 1, slopeIndex))
        {
            case 0:
            {
                // First order, written as a biquad with its second-order
                // terms at zero so it shares the stage storage.
                const auto c = high ? A::makeFirstOrderHighPass (sampleRate, freq)
                                    : A::makeFirstOrderLowPass (sampleRate, freq);
                out.coefficients[0] = { c[0], c[1], 0.0f, c[2], c[3], 0.0f };
                out.count = 1;
                break;
            }

            case 2:
                out.coefficients[0] = section (0.54119610f);
                out.coefficients[1] = section (1.30656296f * resonance);
                out.count = 2;
                break;

            default:
                out.coefficients[0] = section (0.50979558f);
                out.coefficients[1] = section (0.60134489f);
                out.coefficients[2] = section (0.89997622f);
                out.coefficients[3] = section (2.56291545f * resonance);
                out.count = 4;
                break;
        }

        return out;
    }

    // |H| of one {b0, b1, b2, a0, a1, a2} section at a frequency - for the
    // editor's curve, which has to draw exactly what the stages do.
    inline double magnitudeOf (const std::array<float, 6>& c, double freqHz, double sampleRate) noexcept
    {
        const auto w = juce::MathConstants<double>::twoPi * freqHz / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const auto num = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
        const auto den = (double) c[3] + (double) c[4] * z1 + (double) c[5] * z2;
        return std::abs (num) / juce::jmax (1.0e-12, std::abs (den));
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
            default:
            {
                const auto c = makeMatchedBell (sampleRate, freq, safeQ, gainDb);
                return new Coefficients (c[0], c[1], c[2], c[3], c[4], c[5]);
            }
        }
    }
}
