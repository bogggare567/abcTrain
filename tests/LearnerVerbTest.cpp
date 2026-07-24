#include <juce_core/juce_core.h>
#include "../LearnerVerb/Source/PluginProcessor.h"
#include "../shared/TestUtils.h"

// Reverb has no clean closed-form target the way LearnerComp's compression
// math does, so these are behavioral/smoke tests: does a tail persist
// after the input stops, does dryWet = 0 give a pure passthrough, does
// every type produce sound without crashing, does applyPreset set what
// it claims to.
class LearnerVerbTest : public juce::UnitTest
{
public:
    LearnerVerbTest() : juce::UnitTest ("LearnerVerb", "Plugins") {}

    void runTest() override
    {
        beginTest ("reverb tail persists after the input signal stops");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int burstSamples = 1000;
            constexpr int silenceSamples = 4000;
            constexpr int totalSamples = burstSamples + silenceSamples;

            LearnerVerbProcessor processor;
            processor.prepareToPlay (sampleRate, totalSamples);

            processor.apvts.getRawParameterValue (LearnerVerbProcessor::typeParamId)->store (1.0f); // Hall
            processor.apvts.getRawParameterValue (LearnerVerbProcessor::decayParamId)->store (3.0f);
            processor.apvts.getRawParameterValue (LearnerVerbProcessor::dryWetParamId)->store (100.0f);

            juce::AudioBuffer<float> buffer (2, totalSamples);
            buffer.clear();

            juce::Random random;
            for (int i = 0; i < burstSamples; ++i)
            {
                const auto sample = random.nextFloat() * 2.0f - 1.0f;
                buffer.setSample (0, i, sample);
                buffer.setSample (1, i, sample);
            }

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            // Right after the burst ends, the reverb tail should still be
            // audible - a dry passthrough would be silent here.
            const auto tailPeak = buffer.getMagnitude (0, burstSamples, 1000);
            expect (tailPeak > 0.001f);
        }

        beginTest ("dryWet = 0 gives a pure dry passthrough");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int numSamples = 512;

            LearnerVerbProcessor processor;
            processor.prepareToPlay (sampleRate, numSamples);

            processor.apvts.getRawParameterValue (LearnerVerbProcessor::dryWetParamId)->store (0.0f);

            const auto input = TestUtils::generateSineBuffer (1000.0f, sampleRate, numSamples, 2, 0.5f);
            auto buffer = input;
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < numSamples; ++i)
                    expectWithinAbsoluteError (buffer.getSample (ch, i), input.getSample (ch, i), 1.0e-4f);
        }

        beginTest ("every reverb type produces sound without crashing");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int numSamples = 2048;

            for (int typeIndex = 0; typeIndex < 4; ++typeIndex)
            {
                LearnerVerbProcessor processor;
                processor.prepareToPlay (sampleRate, numSamples);
                processor.apvts.getRawParameterValue (LearnerVerbProcessor::typeParamId)->store ((float) typeIndex);
                processor.apvts.getRawParameterValue (LearnerVerbProcessor::dryWetParamId)->store (100.0f);

                auto buffer = TestUtils::generateSineBuffer (500.0f, sampleRate, numSamples, 2, 0.8f);
                juce::MidiBuffer midi;
                processor.processBlock (buffer, midi);

                const auto peak = buffer.getMagnitude (0, 0, numSamples);
                expect (peak > 0.0f);
            }
        }

        beginTest ("applying a preset sets all of its parameter values");
        {
            LearnerVerbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            processor.applyPreset (2); // "Small Room"

            expectEquals ((int) processor.apvts.getRawParameterValue (LearnerVerbProcessor::typeParamId)->load(), 0);
            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerVerbProcessor::decayParamId)->load(), 0.6f, 0.05f);
            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerVerbProcessor::preDelayParamId)->load(), 10.0f, 0.5f);
            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerVerbProcessor::dryWetParamId)->load(), 20.0f, 0.5f);
        }

        beginTest ("an out-of-range preset index is ignored, not a crash");
        {
            LearnerVerbProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            const auto decayBefore = processor.apvts.getRawParameterValue (LearnerVerbProcessor::decayParamId)->load();
            processor.applyPreset (99);
            processor.applyPreset (-1);

            expectEquals (processor.apvts.getRawParameterValue (LearnerVerbProcessor::decayParamId)->load(), decayBefore);
        }
    }
};

static LearnerVerbTest learnerVerbTest;
