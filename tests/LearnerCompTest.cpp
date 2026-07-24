#include <juce_core/juce_core.h>
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../shared/TestUtils.h"

// DSP-level regression tests, following the same closed-form-math
// approach as LearnerEQTest: known input, deterministic processing,
// assert on measured output level.
//
// Note on the makeup-gain test: an earlier version of this spec proposed
// "-6 dB threshold + Ratio + 6 dB makeup returns to 0 dB" without pinning
// down the ratio - that only holds for a ratio approaching infinity (a
// limiter). This test instead reuses the same -6 dB/2:1 setup as the
// first test (which gives exactly -3 dB out) and adds +3 dB makeup,
// which by construction should land back at 0 dB - self-consistent
// numbers rather than the original inconsistent claim.
class LearnerCompTest : public juce::UnitTest
{
public:
    LearnerCompTest() : juce::UnitTest ("LearnerComp", "Plugins") {}

    void runTest() override
    {
        beginTest ("hard-knee compression matches the closed-form gain reduction");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int numSamples = 8192;

            LearnerCompProcessor processor;
            processor.prepareToPlay (sampleRate, numSamples);

            processor.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->store (-6.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::ratioParamId)->store (2.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::attackParamId)->store (1.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::kneeParamId)->store (0.0f);

            // 0 dBFS sine: 6 dB over a -6 dB threshold, 2:1 ratio ->
            // 3 dB over threshold at the output -> -3 dBFS.
            auto buffer = TestUtils::generateSineBuffer (1000.0f, sampleRate, numSamples, 2, 1.0f);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            // Measure the tail, well after the 1 ms attack has settled.
            const auto outputPeak = buffer.getMagnitude (0, numSamples - 1024, 1024);
            const auto outputDb = juce::Decibels::gainToDecibels (outputPeak);

            expectWithinAbsoluteError (outputDb, -3.0f, 0.5f);
        }

        beginTest ("makeup gain restores the level removed by compression");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int numSamples = 8192;

            LearnerCompProcessor processor;
            processor.prepareToPlay (sampleRate, numSamples);

            processor.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->store (-6.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::ratioParamId)->store (2.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::attackParamId)->store (1.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::kneeParamId)->store (0.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::makeupParamId)->store (3.0f);

            auto buffer = TestUtils::generateSineBuffer (1000.0f, sampleRate, numSamples, 2, 1.0f);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            const auto outputPeak = buffer.getMagnitude (0, numSamples - 1024, 1024);
            const auto outputDb = juce::Decibels::gainToDecibels (outputPeak);

            expectWithinAbsoluteError (outputDb, 0.0f, 0.5f);
        }

        beginTest ("bypass leaves the signal completely unchanged");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int numSamples = 512;

            LearnerCompProcessor processor;
            processor.prepareToPlay (sampleRate, numSamples);

            processor.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->store (-40.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::ratioParamId)->store (10.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::bypassParamId)->store (1.0f);

            const auto input = TestUtils::generateSineBuffer (1000.0f, sampleRate, numSamples, 2, 0.8f);
            auto buffer = input;
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < numSamples; ++i)
                    expectWithinAbsoluteError (buffer.getSample (ch, i), input.getSample (ch, i), 1.0e-6f);
        }

        beginTest ("applying a preset sets all of its parameter values");
        {
            LearnerCompProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            processor.applyPreset (1); // "Punchy Drums"

            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->load(), -12.0f, 0.1f);
            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerCompProcessor::ratioParamId)->load(), 4.0f, 0.1f);
            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerCompProcessor::attackParamId)->load(), 20.0f, 0.5f);
            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerCompProcessor::releaseParamId)->load(), 80.0f, 1.0f);
            expectWithinAbsoluteError (
                processor.apvts.getRawParameterValue (LearnerCompProcessor::kneeParamId)->load(), 0.0f, 0.1f);
        }

        beginTest ("an out-of-range preset index is ignored, not a crash");
        {
            LearnerCompProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            const auto thresholdBefore = processor.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->load();
            processor.applyPreset (99);
            processor.applyPreset (-1);

            expectEquals (processor.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->load(),
                          thresholdBefore);
        }

        // Note on what this does NOT test: SpectrumAnalyzerComponent
        // (shared/SpectrumAnalyzer.h) is a juce::Component with a
        // juce::Timer, so constructing one here would need a running JUCE
        // message loop - this console test binary deliberately doesn't run
        // one (see docs/testing-strategy.md's "no Component in the console
        // test binary" policy, and decisions/006-unified-visualization.md).
        // On Linux, initialising that GUI machinery in a container with no
        // display server is a real risk, not a hypothetical one - exactly
        // the kind of thing that has broken this project's CI before. What
        // *is* safe and worth covering here is the processor-side code this
        // task added: processBlock now computes a mono downmix of the input
        // every sample to feed a spectrum analyzer, guarded by a null check
        // for when no editor is attached (the normal state in every test,
        // and in a real host before the editor is opened).
        beginTest ("processBlock runs the new spectrum-feed path safely with no analyzer attached");
        {
            constexpr double sampleRate = 44100.0;
            constexpr int numSamples = 8192;

            LearnerCompProcessor processor;
            processor.prepareToPlay (sampleRate, numSamples);
            processor.setSpectrumAnalyzer (nullptr); // explicit, though this is also the default

            processor.apvts.getRawParameterValue (LearnerCompProcessor::thresholdParamId)->store (-6.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::ratioParamId)->store (2.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::attackParamId)->store (1.0f);
            processor.apvts.getRawParameterValue (LearnerCompProcessor::kneeParamId)->store (0.0f);

            auto buffer = TestUtils::generateSineBuffer (1000.0f, sampleRate, numSamples, 2, 1.0f);
            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);

            const auto outputPeak = buffer.getMagnitude (0, numSamples - 1024, 1024);
            const auto outputDb = juce::Decibels::gainToDecibels (outputPeak);
            expectWithinAbsoluteError (outputDb, -3.0f, 0.5f);

            // Same code path again with bypass on, since the mono-downmix
            // feed is computed separately in that branch.
            processor.apvts.getRawParameterValue (LearnerCompProcessor::bypassParamId)->store (1.0f);
            const auto input = TestUtils::generateSineBuffer (1000.0f, sampleRate, 512, 2, 0.5f);
            auto bypassBuffer = input;
            processor.processBlock (bypassBuffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    expectWithinAbsoluteError (bypassBuffer.getSample (ch, i), input.getSample (ch, i), 1.0e-6f);
        }
    }
};

static LearnerCompTest learnerCompTest;
