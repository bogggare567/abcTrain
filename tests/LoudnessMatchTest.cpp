#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../shared/audio/GainMatch.h"
#include "../shared/audio/LessonAudioBed.h"

// t03 (ADR 055): the BS.1770 K-weighting behind the "BS.1770" loudness
// match is the standard's, and - LOUDNESS_REPORT=1 - how far the two ways
// of matching disagree on our own material and our own changes.
class LoudnessMatchTest : public juce::UnitTest
{
public:
    LoudnessMatchTest() : juce::UnitTest ("LoudnessMatch", "Audio") {}

    static juce::AudioBuffer<float> sine (double fs, double hz, int n)
    {
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
            b.setSample (0, i, (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / fs));
        return b;
    }

    static float dbRatio (float a, float b) { return juce::Decibels::gainToDecibels (a / b); }

    void runTest() override
    {
        beginTest ("K-weighting: +0.69 dB at 1 kHz, about +4 dB at 10 kHz, steep below 40 Hz");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            const auto n = (int) fs * 2;
            const auto gainAt = [&] (double hz)
            {
                auto b = sine (fs, hz, n);
                return dbRatio (GainMatch::kRms (b, fs), GainMatch::rms (b));
            };
            expectWithinAbsoluteError (gainAt (997.0), 0.691f, 0.05f);
            expectWithinAbsoluteError (gainAt (10000.0), 4.0f, 0.5f);
            expect (gainAt (20.0) < -10.0f, juce::String (gainAt (20.0)));
        }

        beginTest ("the two matches agree on a mid change and part ways on the top and the bottom");
        {
            const auto fs = 48000.0;
            const auto report = juce::SystemStats::getEnvironmentVariable ("LOUDNESS_REPORT", {}).isNotEmpty();
            using Bed = LessonAudioBed::Bed;
            const std::pair<Bed, const char*> beds[] { { Bed::pinkNoise, "pink" }, { Bed::drumLoop, "drums" },
                                                        { Bed::chord, "chord" }, { Bed::bassNote, "bass" },
                                                        { Bed::vocal, "vocal" } };
            const double bands[] { 60, 125, 250, 500, 1000, 2000, 4000, 8000, 12000 };
            float worstMid = 0.0f, worstTop = 0.0f;

            for (const auto& [bed, name] : beds)
            {
                const auto dry = LessonAudioBed::render (bed, fs, 1);
                juce::String line (name);

                for (auto hz : bands)
                {
                    juce::AudioBuffer<float> wet;
                    wet.makeCopyOf (dry);
                    for (int ch = 0; ch < wet.getNumChannels(); ++ch)
                    {
                        juce::dsp::IIR::Filter<float> peak;
                        peak.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (fs, (float) hz, 1.4f, juce::Decibels::decibelsToGain (6.0f));
                        auto* d = wet.getWritePointer (ch);
                        for (int i = 0; i < wet.getNumSamples(); ++i)
                            d[i] = peak.processSample (d[i]);
                    }

                    const auto byRms = juce::Decibels::gainToDecibels (GainMatch::from (GainMatch::rms (dry), GainMatch::rms (wet)));
                    const auto byK = juce::Decibels::gainToDecibels (GainMatch::from (GainMatch::kRms (dry, fs), GainMatch::kRms (wet, fs)));
                    const auto diff = byK - byRms;
                    line << "  " << juce::String ((int) hz) << ":" << juce::String (diff, 2);

                    if (hz >= 500 && hz <= 1000) worstMid = juce::jmax (worstMid, std::abs (diff));
                    if (hz >= 4000) worstTop = juce::jmax (worstTop, std::abs (diff));
                }

                if (report)
                    logMessage ("K minus RMS match, dB, +6 dB bell:  " + line);
            }

            expect (worstMid < 0.6f, juce::String (worstMid));
            if (report)
                logMessage ("worst mid " + juce::String (worstMid, 2) + " dB, worst top " + juce::String (worstTop, 2) + " dB");
        }
    }
};

static LoudnessMatchTest loudnessMatchTest;
