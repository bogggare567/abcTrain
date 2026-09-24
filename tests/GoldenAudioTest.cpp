#include <juce_audio_formats/juce_audio_formats.h>
#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../LearnerVerb/Source/PluginProcessor.h"

// Golden audio (the review in notes/, 2026-09-24): a fixed input through each
// plugin with fixed settings, compared with a stored reference output. The
// behavioural tests say "the tail persists", "the ratio is 4:1"; this one says
// "nothing else moved either" - a change to the DSP that was not meant to
// change the sound fails here.
//
// A change that *is* meant to change the sound regenerates the references:
//     GOLDEN_WRITE=1 EarTrainerTests GoldenAudio
// and the commit says why (the diff shows the .flac files changed).
//
// Compared: max sample deviation, RMS error, peak error, and the largest
// difference in any third-octave band - the four the review asked for.
class GoldenAudioTest : public juce::UnitTest
{
public:
    GoldenAudioTest() : juce::UnitTest ("GoldenAudio", "DSP") {}

    static juce::File goldenDir()
    {
        return juce::File (__FILE__).getParentDirectory().getChildFile ("golden");
    }

    // One second, deterministic: a noise burst, two sines, three drum-like
    // hits, then silence for the tails. Stereo, slightly different per side.
    static juce::AudioBuffer<float> input (double fs)
    {
        const auto n = (int) fs;
        juce::AudioBuffer<float> b (2, n);
        b.clear();
        juce::Random random (20260925);
        float pinkL = 0.0f, pinkR = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const auto t = (double) i / fs;
            float l = 0.0f, r = 0.0f;

            if (t < 0.2)
            {
                pinkL = 0.97f * pinkL + 0.03f * (random.nextFloat() * 2.0f - 1.0f);
                pinkR = 0.97f * pinkR + 0.03f * (random.nextFloat() * 2.0f - 1.0f);
                l = pinkL * 6.0f;
                r = pinkR * 6.0f;
            }
            else if (t < 0.4)
            {
                const auto s = 0.25 * std::sin (juce::MathConstants<double>::twoPi * 220.0 * t)
                             + 0.12 * std::sin (juce::MathConstants<double>::twoPi * 1760.0 * t);
                l = (float) s;
                r = (float) (s * 0.8);
            }
            else if (t < 0.55)
            {
                const auto since = std::fmod (t - 0.4, 0.05);
                const auto env = (float) std::exp (-since * 90.0);
                const auto noise = random.nextFloat() * 2.0f - 1.0f;
                l = r = 0.7f * env * noise;
            }

            b.setSample (0, i, l);
            b.setSample (1, i, r);
        }

        return b;
    }

    template <typename Processor>
    static juce::AudioBuffer<float> run (Processor& p, double fs)
    {
        auto buffer = input (fs);
        const int block = 256;
        p.prepareToPlay (fs, block);

        juce::MidiBuffer midi;
        for (int start = 0; start < buffer.getNumSamples(); start += block)
        {
            const auto len = juce::jmin (block, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, start, len);
            p.processBlock (view, midi);
        }

        return buffer;
    }

    static void set (juce::AudioProcessorValueTreeState& s, const juce::String& id, float value)
    {
        if (auto* v = s.getRawParameterValue (id))
            v->store (value);
    }

    static float bandLevelDb (const juce::AudioBuffer<float>& b, double fs, double centre)
    {
        juce::dsp::IIR::Filter<float> f1, f2;
        f1.coefficients = f2.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (fs, (float) centre, 4.32f);
        double e = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const auto y = f2.processSample (f1.processSample (b.getSample (0, i)));
            e += (double) y * y;
        }
        return (float) (10.0 * std::log10 (e + 1.0e-20));
    }

    // FLAC holds integers: anything past full scale would be clipped in the
    // reference and read back as a difference. Stored 18 dB down.
    static constexpr float storeGain = 0.125f;

    void compare (const juce::String& name, const juce::AudioBuffer<float>& out, double fs)
    {
        const auto file = goldenDir().getChildFile (name + ".flac");
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        if (juce::SystemStats::getEnvironmentVariable ("GOLDEN_WRITE", {}).isNotEmpty())
        {
            goldenDir().createDirectory();
            file.deleteFile();
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> writer (flac.createWriterFor (stream.get(), fs, 2, 24, {}, 5));
            expect (writer != nullptr, "writer for " + file.getFullPathName());
            if (writer != nullptr)
            {
                stream.release();
                auto scaled = out;
                scaled.applyGain (storeGain);
                expect (scaled.getMagnitude (0, scaled.getNumSamples()) < 1.0f, name + ": still clips at -18 dB");
                writer->writeFromAudioSampleBuffer (scaled, 0, scaled.getNumSamples());
            }
            logMessage ("wrote " + file.getFullPathName());
            return;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        expect (reader != nullptr, name + ": no reference - run with GOLDEN_WRITE=1 once");
        if (reader == nullptr)
            return;

        juce::AudioBuffer<float> ref (2, (int) reader->lengthInSamples);
        reader->read (&ref, 0, ref.getNumSamples(), 0, true, true);
        ref.applyGain (1.0f / storeGain);
        expectEquals (ref.getNumSamples(), out.getNumSamples(), name + ": length");

        const auto n = juce::jmin (ref.getNumSamples(), out.getNumSamples());
        double maxDev = 0.0, errE = 0.0, refE = 0.0;
        float refPeak = 0.0f, outPeak = 0.0f;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const auto a = ref.getSample (ch, i), b = out.getSample (ch, i);
                maxDev = juce::jmax (maxDev, (double) std::abs (a - b));
                errE += (double) (a - b) * (a - b);
                refE += (double) a * a;
                refPeak = juce::jmax (refPeak, std::abs (a));
                outPeak = juce::jmax (outPeak, std::abs (b));
            }

        float worstBand = 0.0f;
        for (double centre = 31.5; centre < juce::jmin (16000.0, fs * 0.4); centre *= std::pow (2.0, 1.0 / 3.0))
            worstBand = juce::jmax (worstBand, std::abs (bandLevelDb (ref, fs, centre) - bandLevelDb (out, fs, centre)));

        const auto rmsErrorDb = 10.0 * std::log10 (errE / juce::jmax (1.0e-20, refE) + 1.0e-20);
        const auto peakErrorDb = juce::Decibels::gainToDecibels (outPeak) - juce::Decibels::gainToDecibels (refPeak);

        logMessage (juce::String::formatted ("%s: max deviation %.2e, error %.1f dB below signal, peak %+.3f dB, worst third-octave %.3f dB",
                                             name.toRawUTF8(), maxDev, -rmsErrorDb, peakErrorDb, worstBand));

        // 24-bit reference 18 dB down: quantisation alone is 5e-7. The margin is for a
        // different compiler or CPU (fused multiply-add) - not for a change.
        expect (maxDev < 2.0e-4, name + ": max sample deviation " + juce::String (maxDev));
        expect (rmsErrorDb < -60.0, name + ": error only " + juce::String (-rmsErrorDb, 1) + " dB below the signal");
        expect (std::abs (peakErrorDb) < 0.05, name + ": peak moved " + juce::String (peakErrorDb, 3) + " dB");
        expect (worstBand < 0.1f, name + ": a third-octave band moved " + juce::String (worstBand, 3) + " dB");
    }

    void runTest() override
    {
        beginTest ("Learner EQ: a bell at 1 kHz +6 dB and a 24 dB/oct high-pass at 80 Hz, at 48 and 96 kHz");
        for (double fs : { 48000.0, 96000.0 })
        {
            LearnerEQProcessor eq;
            const auto bell = eq.addBand (1000.0f, 6.0f, EQCoefficients::BandType::bell);
            const auto hp = eq.addBand (80.0f, 0.0f, EQCoefficients::BandType::highPass);
            set (eq.apvts, LearnerEQProcessor::qParamId (bell), 1.0f);
            set (eq.apvts, LearnerEQProcessor::slopeParamId (hp), 2.0f);   // 24 dB/oct
            compare ("eq-" + juce::String ((int) fs), run (eq, fs), fs);
        }

        beginTest ("Learner Comp: -18 dB, 4:1, 10 ms, 100 ms");
        {
            LearnerCompProcessor comp;
            set (comp.apvts, LearnerCompProcessor::thresholdParamId, -18.0f);
            set (comp.apvts, LearnerCompProcessor::ratioParamId, 4.0f);
            set (comp.apvts, LearnerCompProcessor::attackParamId, 10.0f);
            set (comp.apvts, LearnerCompProcessor::releaseParamId, 100.0f);
            set (comp.apvts, LearnerCompProcessor::kneeParamId, 6.0f);
            set (comp.apvts, LearnerCompProcessor::makeupParamId, 0.0f);
            set (comp.apvts, LearnerCompProcessor::dryWetParamId, 100.0f);
            compare ("comp-44100", run (comp, 44100.0), 44100.0);
        }

        beginTest ("Learner Verb: every type at RT60 1.2 s, 100 % wet");
        for (int type = 0; type < 4; ++type)
        {
            LearnerVerbProcessor verb;
            set (verb.apvts, LearnerVerbProcessor::typeParamId, (float) type);
            set (verb.apvts, LearnerVerbProcessor::decayParamId, 1.2f);
            set (verb.apvts, LearnerVerbProcessor::sizeParamId, 50.0f);
            set (verb.apvts, LearnerVerbProcessor::dampingParamId, 40.0f);
            set (verb.apvts, LearnerVerbProcessor::dryWetParamId, 100.0f);
            const char* names[] { "room", "hall", "plate", "spring" };
            compare (juce::String ("verb-") + names[type] + "-44100", run (verb, 44100.0), 44100.0);
        }
    }
};

static GoldenAudioTest goldenAudioTest;
