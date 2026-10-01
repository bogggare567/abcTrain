#include <juce_core/juce_core.h>
#include "../LearnerComp/Source/PluginProcessor.h"
#include "TestUtils.h"

// Learner Comp's sidechain: the detector listens to the key, the program
// is what gets turned down, and the key never reaches the output unless
// Listen asks for it.
class SidechainTest : public juce::UnitTest
{
public:
    SidechainTest() : juce::UnitTest ("Sidechain", "Plugins") {}

    static void setUp (LearnerCompProcessor& p, int source, float hpf = LearnerCompProcessor::scHpfOffHz, bool listen = false)
    {
        p.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->store (-30.0f);
        p.apvts.getRawParameterValue (LearnerCompProcessor::ratioParamId)->store (8.0f);
        p.apvts.getRawParameterValue (LearnerCompProcessor::attackParamId)->store (1.0f);
        p.apvts.getRawParameterValue (LearnerCompProcessor::releaseParamId)->store (50.0f);
        p.apvts.getRawParameterValue (LearnerCompProcessor::scSourceParamId)->store ((float) source);
        p.apvts.getRawParameterValue (LearnerCompProcessor::scHpfParamId)->store (hpf);
        p.apvts.getRawParameterValue (LearnerCompProcessor::scListenParamId)->store (listen ? 1.0f : 0.0f);
    }

    // Main: a quiet steady tone under the threshold. Sidechain: a loud tone.
    static juce::AudioBuffer<float> fourChannels (double rate, int n, float keyFreq, float keyGain)
    {
        juce::AudioBuffer<float> b (4, n);
        for (int i = 0; i < n; ++i)
        {
            const auto main = 0.02f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) i / (float) rate);
            const auto key = keyGain * std::sin (juce::MathConstants<float>::twoPi * keyFreq * (float) i / (float) rate);
            b.setSample (0, i, main); b.setSample (1, i, main);
            b.setSample (2, i, key);  b.setSample (3, i, key);
        }
        return b;
    }

    static float tailDb (const juce::AudioBuffer<float>& b, int n)
    {
        return juce::Decibels::gainToDecibels (b.getMagnitude (0, n - 2048, 2048), -120.0f);
    }

    void runTest() override
    {
        constexpr double rate = 44100.0;
        constexpr int n = 16384;
        juce::MidiBuffer midi;

        const auto withSidechain = []
        {
            auto p = std::make_unique<LearnerCompProcessor>();
            auto layout = p->getBusesLayout();
            layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
            p->setBusesLayout (layout);
            return p;
        };

        beginTest ("Self: a quiet tone under the threshold passes untouched, and the key does not leak");
        {
            auto p = withSidechain();
            expect (p->getBus (true, 1)->isEnabled());
            p->prepareToPlay (rate, n);
            setUp (*p, LearnerCompProcessor::scSelf);
            auto b = fourChannels (rate, n, 100.0f, 0.9f);
            p->processBlock (b, midi);
            expectWithinAbsoluteError (tailDb (b, n), juce::Decibels::gainToDecibels (0.02f), 0.5f);
        }

        beginTest ("External: a loud key ducks the quiet program by many dB");
        {
            auto p = withSidechain();
            p->prepareToPlay (rate, n);
            setUp (*p, LearnerCompProcessor::scExternal);
            auto b = fourChannels (rate, n, 1000.0f, 0.9f);
            p->processBlock (b, midi);
            const auto ducked = tailDb (b, n) - juce::Decibels::gainToDecibels (0.02f);
            expect (ducked < -15.0f, "ducked by " + juce::String (ducked, 1) + " dB");
        }

        beginTest ("HPF on the key: a 40 Hz key barely ducks with the filter at 200 Hz");
        {
            auto off = withSidechain(), on = withSidechain();
            off->prepareToPlay (rate, n);
            on->prepareToPlay (rate, n);
            setUp (*off, LearnerCompProcessor::scExternal);
            setUp (*on, LearnerCompProcessor::scExternal, 200.0f);
            auto a = fourChannels (rate, n, 40.0f, 0.9f), c = fourChannels (rate, n, 40.0f, 0.9f);
            off->processBlock (a, midi);
            on->processBlock (c, midi);
            expect (tailDb (c, n) > tailDb (a, n) + 10.0f,
                    "with HPF " + juce::String (tailDb (c, n), 1) + " dB, without " + juce::String (tailDb (a, n), 1) + " dB");
        }

        beginTest ("Listen > Key plays the key, not the program");
        {
            auto p = withSidechain();
            p->prepareToPlay (rate, n);
            setUp (*p, LearnerCompProcessor::scExternal, LearnerCompProcessor::scHpfOffHz, true);
            auto b = fourChannels (rate, n, 1000.0f, 0.5f);
            p->processBlock (b, midi);
            expectWithinAbsoluteError (tailDb (b, n), juce::Decibels::gainToDecibels (0.5f), 0.5f);
        }

        beginTest ("Kick: works with no sidechain bus at all (the app's Studio) and the kick is heard");
        {
            LearnerCompProcessor p;
            expect (! p.getBus (true, 1)->isEnabled());
            p.prepareToPlay (rate, n);
            setUp (p, LearnerCompProcessor::scKick);
            auto b = TestUtils::generateSineBuffer (440.0f, rate, n, 2, 0.02f);
            p.processBlock (b, midi);
            // The kick itself lands in the output, far above the quiet tone.
            expect (b.getMagnitude (0, 0, 4410) > 0.2f);
        }
    }
};

static SidechainTest sidechainTest;
