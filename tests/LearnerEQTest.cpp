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
            // Band 0 is the one band that starts on, a flat bell at 1 kHz
            // (see createParameterLayout). Bands are free-typed now, so
            // this is a property of the defaults rather than of the slot.
            constexpr float testFreq = 1000.0f;

            LearnerEQProcessor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            const auto input = TestUtils::generateSineBuffer (testFreq, sampleRate, blockSize, 2);
            juce::MidiBuffer midi;

            auto flatBuffer = input;
            processor.processBlock (flatBuffer, midi);
            const auto flatRms = TestUtils::rms (flatBuffer);

            if (auto* rawGain = processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (0)))
                rawGain->store (12.0f);

            auto boostedBuffer = input;
            processor.processBlock (boostedBuffer, midi);
            const auto boostedRms = TestUtils::rms (boostedBuffer);

            expect (boostedRms > flatRms);
        }

        beginTest ("a band that is off does nothing, whatever it is set to");
        {
            // The new band model's central claim: eight slots exist as
            // parameters at all times, and only the ones switched on are in
            // the chain. A band left off with an extreme setting must be
            // inaudible, or every unused slot is a landmine.
            constexpr double sampleRate = 44100.0;
            constexpr int blockSize = 512;

            LearnerEQProcessor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            const auto input = TestUtils::generateSineBuffer (1000.0f, sampleRate, blockSize, 2, 0.5f);
            juce::MidiBuffer midi;

            auto before = input;
            processor.processBlock (before, midi);

            // Band 3 is off by default. Make it a brutal low-pass right on
            // the test tone; nothing should change.
            processor.apvts.getRawParameterValue (LearnerEQProcessor::typeParamId (3))
                ->store ((float) (int) EQCoefficients::BandType::lowPass);
            processor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (3))->store (60.0f);

            auto after = input;
            processor.processBlock (after, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    expectWithinAbsoluteError (after.getSample (ch, i), before.getSample (ch, i), 1.0e-6f);
        }

        beginTest ("a high-pass removes low content and leaves high content");
        {
            // The pass filters are new, and they are the ones a lesson is
            // about to teach. A high-pass that quietly did nothing would
            // make that lesson a lie.
            constexpr double sampleRate = 44100.0;
            constexpr int blockSize = 1024;

            LearnerEQProcessor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            processor.apvts.getRawParameterValue (LearnerEQProcessor::typeParamId (0))
                ->store ((float) (int) EQCoefficients::BandType::highPass);
            processor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (0))->store (500.0f);

            juce::MidiBuffer midi;

            auto low = TestUtils::generateSineBuffer (60.0f, sampleRate, blockSize, 2, 0.5f);
            const auto lowBefore = TestUtils::rms (low);
            processor.processBlock (low, midi);
            expect (TestUtils::rms (low) < lowBefore * 0.35f, "60 Hz should be well down");

            auto high = TestUtils::generateSineBuffer (5000.0f, sampleRate, blockSize, 2, 0.5f);
            const auto highBefore = TestUtils::rms (high);
            processor.processBlock (high, midi);
            expect (TestUtils::rms (high) > highBefore * 0.8f, "5 kHz should be nearly untouched");
        }

        beginTest ("bypass leaves the signal completely unchanged");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int blockSize = 512;

            LearnerEQProcessor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            // A boosted band would normally change the signal a lot - bypass
            // should still leave it bit-for-bit untouched.
            if (auto* rawGain = processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (0)))
                rawGain->store (18.0f);
            processor.apvts.getRawParameterValue (LearnerEQProcessor::bypassParamId)->store (1.0f);

            const auto input = TestUtils::generateSineBuffer (1000.0f, sampleRate, blockSize, 2, 0.8f);
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
