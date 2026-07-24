#include <juce_core/juce_core.h>
#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../shared/TestUtils.h"

// The one test in this project that touches real DSP output rather than
// just game-logic state: it exists specifically to catch a broken filter
// chain, since this code could not be compiled/run at all while it was
// written (no local build toolchain was available at the time).
class LearnerEQTest : public juce::UnitTest
{
public:
    LearnerEQTest() : juce::UnitTest ("LearnerEQ", "Plugins") {}

    void runTest() override
    {
        beginTest ("boosting a band's gain raises RMS at that band's frequency");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int blockSize = 512;
            constexpr float testFreq = 800.0f; // band 1's default centre frequency

            LearnerEQProcessor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            const auto input = TestUtils::generateSineBuffer (testFreq, sampleRate, blockSize, 2);
            juce::MidiBuffer midi;

            auto flatBuffer = input;
            processor.processBlock (flatBuffer, midi);
            const auto flatRms = TestUtils::rms (flatBuffer);

            if (auto* rawGain = processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (1)))
                rawGain->store (12.0f);

            auto boostedBuffer = input;
            processor.processBlock (boostedBuffer, midi);
            const auto boostedRms = TestUtils::rms (boostedBuffer);

            expect (boostedRms > flatRms);
        }

        beginTest ("bypass leaves the signal completely unchanged");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int blockSize = 512;

            LearnerEQProcessor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            // A boosted band would normally change the signal a lot - bypass
            // should still leave it bit-for-bit untouched.
            if (auto* rawGain = processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (1)))
                rawGain->store (18.0f);
            processor.apvts.getRawParameterValue (LearnerEQProcessor::bypassParamId)->store (1.0f);

            const auto input = TestUtils::generateSineBuffer (800.0f, sampleRate, blockSize, 2, 0.8f);
            auto buffer = input;
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    expectWithinAbsoluteError (buffer.getSample (ch, i), input.getSample (ch, i), 1.0e-6f);
        }
    }
};

static LearnerEQTest learnerEQTest;
