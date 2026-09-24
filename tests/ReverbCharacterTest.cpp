#include <juce_dsp/juce_dsp.h>
#include "shared/dsp/ReverbEngine.h"
#include "shared/dsp/ReverbMeasure.h"

// The knobs mean the same thing on all four reverbs (the author, 2026-09-25:
// "the spring and the plate should react the way the room does").
//
//   Decay    - RT60 in seconds, measured, on every type (the spring too).
//   Damping  - the ratio of the high-frequency RT60 to the low one:
//              1 at 0 %, about 1 - 0.86 = 0.14 at 100 %. Measured in octave
//              bands around 500 Hz and 5 kHz, the way an acoustician reads a
//              hall's treble ratio.
//   Size     - changes the structure (echo density, spacing, for the spring
//              the length of the coil and so its chirp) without changing how
//              long the tail lasts.
//
// REVERB_REPORT=1 prints the whole table (docs/research/2026-09-reverb-types.md).
class ReverbCharacterTest : public juce::UnitTest
{
public:
    ReverbCharacterTest() : juce::UnitTest ("ReverbCharacter", "DSP") {}

    static constexpr double fs = 44100.0;

    static std::vector<float> ir (ReverbEngine::Type type, float decay, float size, float damping, double seconds)
    {
        ReverbMeasure::Setting s;
        s.type = type;
        s.decaySeconds = decay;
        s.size = size;
        s.damping = damping;
        return ReverbMeasure::impulseResponse (s, fs, seconds);
    }

    // An octave band: two cascaded second-order band-passes (Q = 1.41).
    static std::vector<float> band (const std::vector<float>& x, double centreHz)
    {
        auto out = x;
        for (int pass = 0; pass < 2; ++pass)
        {
            juce::dsp::IIR::Filter<float> f;
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (fs, (float) centreHz, 1.41f);
            for (auto& v : out)
                v = f.processSample (v);
        }
        return out;
    }

    // Abel & Huang's normalised echo density: the share of samples in a 20 ms
    // window lying more than one standard deviation from zero, over what
    // Gaussian noise gives (0.3173). 1 = as dense as noise. Returns the time
    // it first reaches `target`.
    static double timeToDensity (const std::vector<float>& x, double target)
    {
        const int window = (int) (0.02 * fs);
        for (int start = 0; start + window < (int) x.size(); start += 64)
        {
            double sum = 0.0;
            for (int i = start; i < start + window; ++i) sum += (double) x[(size_t) i] * x[(size_t) i];
            const auto sd = std::sqrt (sum / window);
            if (sd <= 1.0e-12) continue;

            int outside = 0;
            for (int i = start; i < start + window; ++i) outside += std::abs (x[(size_t) i]) > sd ? 1 : 0;

            if ((double) outside / window / 0.3173 >= target)
                return (double) (start + window / 2) / fs;
        }
        return (double) x.size() / fs;
    }

    // Where the energy of a band first peaks after `from` seconds.
    static double peakTime (const std::vector<float>& x, double from, double to)
    {
        float best = 0.0f;
        size_t at = 0;
        for (auto i = (size_t) (from * fs); i < (size_t) (to * fs) && i < x.size(); ++i)
            if (std::abs (x[i]) > best) { best = std::abs (x[i]); at = i; }
        return (double) at / fs;
    }

    struct Reading { double rt, rtLow, rtHigh, dense; };

    static Reading read (ReverbEngine::Type type, float decay, float size, float damping)
    {
        const auto x = ir (type, decay, size, damping, decay * 1.6 + 0.6);
        return { ReverbMeasure::rt60 (x, fs),
                 ReverbMeasure::rt60 (band (x, 500.0), fs),
                 ReverbMeasure::rt60 (band (x, 5000.0), fs),
                 timeToDensity (x, 0.8) };
    }

    void runTest() override
    {
        using T = ReverbEngine::Type;
        const char* names[] { "room", "hall", "plate", "spring" };
        const auto report = juce::SystemStats::getEnvironmentVariable ("REVERB_REPORT", {}).isNotEmpty();

        if (report)
        {
            logMessage ("type    size damp |  RT    RT500  RT5k  ratio(want) | dense@0.8");
            for (auto type : { T::room, T::hall, T::plate, T::spring })
                for (float size : { 0.1f, 0.9f })
                    for (float damping : { 0.0f, 0.5f, 1.0f })
                    {
                        const auto r = read (type, 2.0f, size, damping);
                        logMessage (juce::String::formatted ("%-7s %.1f  %.1f  | %.2f  %.2f  %.2f  %.2f(%.2f)  | %.3f s",
                                                             names[(int) type], size, damping, r.rt, r.rtLow, r.rtHigh,
                                                             r.rtHigh / juce::jmax (1.0e-6, r.rtLow), 1.0 - 0.86 * damping, r.dense));
                    }

            for (float size : { 0.1f, 0.5f, 0.9f })
            {
                const auto x = ir (T::spring, 2.0f, size, 0.0f, 0.4);
                const auto lo = band (x, 400.0), hi = band (x, 3200.0);
                logMessage (juce::String::formatted ("spring size %.1f: first return 400 Hz at %.1f ms, 3.2 kHz at %.1f ms",
                                                     size, 1000.0 * peakTime (lo, 0.004, 0.2), 1000.0 * peakTime (hi, 0.004, 0.2)));
            }
        }

        beginTest ("Decay is seconds on every type, the spring included");
        for (auto type : { T::room, T::hall, T::plate, T::spring })
            for (float decay : { 1.0f, 3.0f })
            {
                const auto rt = ReverbMeasure::rt60 (ir (type, decay, 0.5f, 0.0f, decay * 1.6 + 0.6), fs);
                expectWithinAbsoluteError ((float) rt, decay, decay * 0.25f,
                                           juce::String (names[(int) type]) + " at " + juce::String (decay) + " s measured " + juce::String (rt, 2));
            }

        beginTest ("Damping is the treble ratio on every type");
        for (auto type : { T::room, T::hall, T::plate, T::spring })
            for (float damping : { 0.0f, 1.0f })
            {
                const auto r = read (type, 2.0f, 0.5f, damping);
                const auto ratio = r.rtHigh / r.rtLow;
                const auto want = 1.0 - 0.86 * damping;
                expectWithinAbsoluteError ((float) ratio, (float) want, damping > 0.5f ? 0.12f : 0.25f,
                                           juce::String (names[(int) type]) + " damping " + juce::String (damping)
                                               + ": RT5k/RT500 = " + juce::String (ratio, 2));
            }

        // A spring never becomes "dense" - it is a train of chirps, one per
        // trip round the coil - so its Size is checked by the chirp below.
        beginTest ("Size changes the structure, not the length, on every type");
        for (auto type : { T::room, T::hall, T::plate, T::spring })
        {
            if (type == T::spring)
            {
                expectWithinAbsoluteError ((float) read (type, 2.0f, 0.9f, 0.3f).rt, (float) read (type, 2.0f, 0.1f, 0.3f).rt, 0.5f,
                                           "spring: size must not change RT60");
                continue;
            }

            const auto small = read (type, 2.0f, 0.1f, 0.3f);
            const auto large = read (type, 2.0f, 0.9f, 0.3f);
            expectWithinAbsoluteError ((float) large.rt, (float) small.rt, 0.5f,
                                       juce::String (names[(int) type]) + ": size must not change RT60");
            expect (large.dense > small.dense * 1.2,
                    juce::String (names[(int) type]) + ": a larger space takes longer to become dense ("
                        + juce::String (small.dense, 3) + " -> " + juce::String (large.dense, 3) + " s)");
        }

        beginTest ("the spring chirps: highs come round the coil before lows, and a longer coil spreads them further");
        {
            const auto spread = [] (float size)
            {
                const auto x = ir (T::spring, 2.0f, size, 0.0f, 0.4);
                return peakTime (band (x, 400.0), 0.004, 0.2) - peakTime (band (x, 3200.0), 0.004, 0.2);
            };

            const auto shortCoil = spread (0.1f), longCoil = spread (0.9f);
            expect (shortCoil > 0.0005, "lows arrive later than highs: " + juce::String (shortCoil * 1000.0, 2) + " ms");
            expect (longCoil > shortCoil * 1.3, "a longer coil disperses more: " + juce::String (shortCoil * 1000.0, 2)
                                                    + " -> " + juce::String (longCoil * 1000.0, 2) + " ms");
        }
    }
};

static ReverbCharacterTest reverbCharacterTest;
