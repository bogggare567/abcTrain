#include <juce_audio_processors/juce_audio_processors.h>
#include "../shared/ModuleProgress.h"
#include "../shared/TrainingModule.h"
#include "../LearnerVerb/Source/PluginProcessor.h"
#include "../LearnerVerb/Source/ReverbEngine.h"
#include "../LearnerVerb/Source/ReverbMeasure.h"
#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../LearnerComp/Source/CompressorEngine.h"
#include "../shared/ABCompare.h"
#include "../shared/TestUtils.h"

// ADR 037: the Learner plugins made honest - a staircase per module, an
// accept band in the knob's own units, a reverb whose Decay is seconds and
// whose Size does something, and an EQ that can be checked.
class LearnerRedesignTest : public juce::UnitTest
{
public:
    LearnerRedesignTest() : juce::UnitTest ("Learner redesign", "Learner") {}

    static juce::PropertiesFile::Options makeOptions (const juce::String& suffix)
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_LearnerRedesign_" + suffix;
        options.commonToAllUsers = false;
        options.getDefaultFile().deleteFile();
        return options;
    }

    static double measureRt60 (const std::vector<float>& ir, double fs) { return ReverbMeasure::rt60 (ir, fs); }

    static std::vector<float> impulseResponse (ReverbEngine::Type type, float decay, float size, double seconds)
    {
        ReverbMeasure::Setting s;
        s.type = type;
        s.decaySeconds = decay;
        s.size = size;
        s.damping = 0.0f;
        return ReverbMeasure::impulseResponse (s, 44100.0, seconds);
    }

    void runTest() override
    {
        beginTest ("a module's staircase: three passes step up, a miss steps down, the record stays");
        {
            juce::PropertiesFile file (makeOptions ("stair"));
            ModuleProgress progress (file);

            expectEquals (progress.get ("m").level, 1);

            for (int i = 0; i < 3; ++i)
                progress.recordAttempt ("m", true);

            expectEquals (progress.get ("m").level, 2);
            expectEquals (progress.get ("m").bestLevel, 2);

            const auto miss = progress.recordAttempt ("m", false);
            expect (miss.steppedDown);
            expectEquals (miss.after.level, 1);
            expectEquals (miss.after.bestLevel, 2);
            expectEquals (miss.after.attempts, 4);
            expectEquals (miss.after.passes, 3);
        }

        beginTest ("an old tier becomes both the starting step and the record");
        {
            juce::PropertiesFile file (makeOptions ("migrate"));
            file.setValue ("module.comp.attack.tier", 2);
            ModuleProgress progress (file);

            expectEquals (progress.get ("comp.attack").level, 7);
            expectEquals (progress.get ("comp.attack").bestLevel, 7);
            expectEquals (progress.getTierPassed ("comp.attack"), 2);
        }

        beginTest ("the accept band narrows over ten steps, from tier one's to the top tier's");
        {
            TrainingModule::Check check;
            check.unit = TrainingModule::Unit::decibels;
            check.toleranceAtTierOne = 5.0f;
            check.toleranceAtTopTier = 1.5f;

            expectWithinAbsoluteError (TrainingModule::toleranceForLevel (check, 1), 5.0f, 0.001f);
            expectWithinAbsoluteError (TrainingModule::toleranceForLevel (check, 10), 1.5f, 0.001f);
            expect (TrainingModule::toleranceForLevel (check, 5) < 5.0f);
            expect (TrainingModule::passesAtLevel (check, -18.0f, -15.5f, 1));
            expect (! TrainingModule::passesAtLevel (check, -18.0f, -15.5f, 10));
        }

        beginTest ("the band drawn on the scale is exactly what would pass");
        {
            TrainingModule::Check check;
            check.unit = TrainingModule::Unit::proportion;
            check.minTarget = 1.0f;
            check.maxTarget = 80.0f;
            check.toleranceAtTierOne = 0.7f;
            check.toleranceAtTopTier = 0.22f;

            for (int level : { 1, 5, 10 })
            {
                const auto range = TrainingModule::acceptRange (check, 20.0f, level);
                expect (TrainingModule::passesAtLevel (check, 20.0f, range.getStart() * 1.001f, level));
                expect (TrainingModule::passesAtLevel (check, 20.0f, range.getEnd() * 0.999f, level));
                expect (! TrainingModule::passesAtLevel (check, 20.0f, range.getEnd() * 1.01f, level));
            }

            // The old drawing used tolerance * 0.5 of the scale: at 1..80 ms
            // on a log axis a ±22% band is about ±5% of the scale, not ±11%.
            const auto r = TrainingModule::acceptRange (check, 20.0f, 10);
            const auto span = std::log (r.getEnd() / r.getStart()) / std::log (80.0f / 1.0f);
            expect (span < 0.12f);
        }

        beginTest ("the result speaks the knob's units");
        {
            TrainingModule::Check knee;
            knee.unit = TrainingModule::Unit::rangeFraction;
            knee.minTarget = 0.0f;
            knee.maxTarget = 18.0f;
            knee.toleranceAtTierOne = 0.3f;
            knee.toleranceAtTopTier = 0.12f;

            const auto r = TrainingModule::readout (knee, 6.0f, 9.0f, 1);
            expectWithinAbsoluteError (r.signedError, 3.0f, 0.001f);
            expectWithinAbsoluteError (r.tolerance, 5.4f, 0.001f);   // 30% of an 18 dB range
        }

        beginTest ("Decay is seconds: the measured RT60 matches the knob");
        {
            for (auto type : { ReverbEngine::Type::room, ReverbEngine::Type::hall, ReverbEngine::Type::plate })
                for (float decay : { 1.0f, 2.5f })
                {
                    const auto rt = measureRt60 (impulseResponse (type, decay, 0.5f, decay * 1.8 + 0.5), 44100.0);
                    expectWithinAbsoluteError ((float) rt, decay, decay * 0.3f,
                                               "type " + juce::String ((int) type) + " at " + juce::String (decay) + " s measured " + juce::String (rt));
                }
        }

        beginTest ("Size changes a room without changing its length");
        {
            const auto small = impulseResponse (ReverbEngine::Type::room, 1.5f, 0.1f, 3.0);
            const auto large = impulseResponse (ReverbEngine::Type::room, 1.5f, 0.9f, 3.0);

            // Different sound: the first reflections land in different places.
            double difference = 0.0, energy = 0.0;

            for (size_t i = 0; i < 4410; ++i)
            {
                difference += std::abs (small[i] - large[i]);
                energy += std::abs (small[i]);
            }

            expect (difference > energy * 0.5, "Size must change the early part of the response");

            // Same length, within the tolerance of the measurement.
            expectWithinAbsoluteError ((float) measureRt60 (large, 44100.0), (float) measureRt60 (small, 44100.0), 0.45f);
        }

        beginTest ("the spring rings and decays");
        {
            const auto ir = impulseResponse (ReverbEngine::Type::spring, 2.0f, 0.5f, 3.0);
            float early = 0.0f, late = 0.0f;

            for (size_t i = 0; i < 22050; ++i)          early = juce::jmax (early, std::abs (ir[i]));
            for (size_t i = ir.size() - 4410; i < ir.size(); ++i) late = juce::jmax (late, std::abs (ir[i]));

            expect (early > 0.01f);
            expect (late < early * 0.2f);
        }

        beginTest ("Learner EQ can be checked: the override reaches the DSP, not the knob");
        {
            constexpr double fs = 44100.0;
            LearnerEQProcessor processor;
            processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (0))->store (0.0f);
            processor.prepareToPlay (fs, 512);

            processor.setCheckOverride (LearnerEQProcessor::gainParamId (0), 12.0f);

            juce::MidiBuffer midi;
            float louder = 0.0f;

            for (int block = 0; block < 12; ++block)
            {
                auto buffer = TestUtils::generateSineBuffer (1000.0f, fs, 512, 2, 0.1f);
                processor.processBlock (buffer, midi);
                louder = TestUtils::rms (buffer);
            }

            expect (louder > 0.1f * 0.707f * 2.5f, "a +12 dB reference at 1 kHz must be louder");
            expectEquals (processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (0))->load(), 0.0f);

            processor.clearCheckOverride();
        }
        beginTest ("A/B: each slot keeps its own knobs, Bypass belongs to neither");
        {
            LearnerCompProcessor processor;
            auto* threshold = processor.apvts.getParameter ("threshold");
            auto* bypass = processor.apvts.getParameter (LearnerCompProcessor::bypassParamId);
            const auto set = [] (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            const auto get = [] (juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); };

            ABCompare ab (processor.apvts, { LearnerCompProcessor::bypassParamId });
            expectEquals (ab.getActive(), 0);

            set (threshold, -10.0f);
            ab.select (1);                                   // B starts as a copy of A
            expectWithinAbsoluteError (get (threshold), -10.0f, 0.2f);

            set (threshold, -30.0f);
            set (bypass, 1.0f);
            ab.select (0);
            expectWithinAbsoluteError (get (threshold), -10.0f, 0.2f);
            expect (get (bypass) > 0.5f, "switching slots must not touch Bypass");

            ab.select (1);
            expectWithinAbsoluteError (get (threshold), -30.0f, 0.2f);

            // The slots travel with the plugin's saved state.
            juce::MemoryBlock saved;
            processor.getStateInformation (saved);
            LearnerCompProcessor restored;
            restored.setStateInformation (saved.getData(), (int) saved.getSize());
            ABCompare ab2 (restored.apvts, { LearnerCompProcessor::bypassParamId });
            expectEquals (ab2.getActive(), 1);
            ab2.select (0);
            expectWithinAbsoluteError (get (restored.apvts.getParameter ("threshold")), -10.0f, 0.2f);
        }

        beginTest ("the transfer curve is the gain computer the DSP runs");
        {
            // Hard knee: 6 dB over at 4:1 comes out 1.5 dB over.
            expectWithinAbsoluteError (CompressorEngine::staticReductionDb (-6.0f, -12.0f, 4.0f, 0.0f), 4.5f, 0.001f);
            expectWithinAbsoluteError (CompressorEngine::staticReductionDb (-20.0f, -12.0f, 4.0f, 0.0f), 0.0f, 0.001f);

            // A soft knee is continuous at both of its edges.
            const auto knee = 6.0f;
            expectWithinAbsoluteError (CompressorEngine::staticReductionDb (-12.0f - knee * 0.5f, -12.0f, 4.0f, knee), 0.0f, 0.001f);
            expectWithinAbsoluteError (CompressorEngine::staticReductionDb (-12.0f + knee * 0.5f, -12.0f, 4.0f, knee),
                                       knee * 0.5f * 0.75f, 0.001f);
        }

        beginTest ("the echogram's onset is the pre-delay");
        {
            ReverbMeasure::Setting s;
            s.type = ReverbEngine::Type::hall;
            s.decaySeconds = 1.2f;
            s.preDelayMs = 60.0f;
            const auto ir = ReverbMeasure::impulseResponse (s, 22050.0, 1.5);
            const auto onset = ReverbMeasure::onsetSeconds (ir, 22050.0);
            expect (onset >= 0.055 && onset < 0.2, "onset " + juce::String (onset));
        }
    }
};


static LearnerRedesignTest learnerRedesignTest;
