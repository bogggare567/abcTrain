#include <juce_audio_processors/juce_audio_processors.h>
#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../LearnerVerb/Source/PluginProcessor.h"
#include "../shared/PracticeAudioSource.h"
#include <cstdlib>
#include <new>

// ADR 038: what the audio thread may and may not do, checked rather than
// promised.
//
// The global allocation functions are replaced in this test binary so a
// test can count every heap allocation and free made *by the thread that
// is inside processBlock*, and only while it is (a thread-local switch).
// The rule is zero: an allocation on the audio thread can take a lock
// inside the allocator and miss a deadline, and a plugin that does it
// "only sometimes" glitches only sometimes, which is the worst kind.
//
// This is what found EQCoefficients::make being called every 32 samples
// per gliding band - a `new` per call - which no reading of the code had.
namespace RtAlloc
{
    thread_local bool armed = false;
    thread_local int allocations = 0;
    thread_local int frees = 0;
}

void* operator new (std::size_t size)
{
    if (RtAlloc::armed)
        ++RtAlloc::allocations;

    if (auto* p = std::malloc (size > 0 ? size : 1))
        return p;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    if (RtAlloc::armed)
        ++RtAlloc::allocations;

    if (auto* p = std::malloc (size > 0 ? size : 1))
        return p;

    throw std::bad_alloc();
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    if (RtAlloc::armed)
        ++RtAlloc::allocations;

    return std::malloc (size > 0 ? size : 1);
}

void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    if (RtAlloc::armed)
        ++RtAlloc::allocations;

    return std::malloc (size > 0 ? size : 1);
}

void operator delete (void* p) noexcept                        { if (p != nullptr && RtAlloc::armed) ++RtAlloc::frees; std::free (p); }
void operator delete[] (void* p) noexcept                      { if (p != nullptr && RtAlloc::armed) ++RtAlloc::frees; std::free (p); }
void operator delete (void* p, std::size_t) noexcept           { if (p != nullptr && RtAlloc::armed) ++RtAlloc::frees; std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept         { if (p != nullptr && RtAlloc::armed) ++RtAlloc::frees; std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { if (p != nullptr && RtAlloc::armed) ++RtAlloc::frees; std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { if (p != nullptr && RtAlloc::armed) ++RtAlloc::frees; std::free (p); }

class RealtimeSafetyTest : public juce::UnitTest
{
public:
    RealtimeSafetyTest() : juce::UnitTest ("Realtime safety", "Realtime") {}

    struct Report
    {
        int allocations = 0, frees = 0;
        bool finite = true;
        float peak = 0.0f;
    };

    // Runs `blocks` blocks of noise through `processor`, calling `automate`
    // between blocks (off the counted region - that is the message thread's
    // side, or a host writing a parameter), counting only what happens
    // inside processBlock.
    template <typename Automate>
    static Report run (juce::AudioProcessor& processor, double sampleRate, int blockSize, int blocks, Automate&& automate)
    {
        processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        juce::Random random (1234);
        Report report;

        const auto fill = [&]
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (c, i, (random.nextFloat() * 2.0f - 1.0f) * 0.5f);
        };

        // A few blocks first: anything a plugin legitimately sets up on
        // its very first callback is not what this test is about.
        for (int b = 0; b < 4; ++b)
        {
            fill();
            processor.processBlock (buffer, midi);
        }

        for (int b = 0; b < blocks; ++b)
        {
            automate (b);
            fill();

            RtAlloc::allocations = 0;
            RtAlloc::frees = 0;
            RtAlloc::armed = true;
            processor.processBlock (buffer, midi);
            RtAlloc::armed = false;

            report.allocations += RtAlloc::allocations;
            report.frees += RtAlloc::frees;

            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blockSize; ++i)
                {
                    const auto v = buffer.getSample (c, i);

                    if (! std::isfinite (v))
                        report.finite = false;
                    else
                        report.peak = juce::jmax (report.peak, std::abs (v));
                }
        }

        processor.releaseResources();
        return report;
    }

    static void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void check (const juce::String& what, const Report& r)
    {
        expectEquals (r.allocations, 0, what + ": heap allocations inside processBlock");
        expectEquals (r.frees, 0, what + ": heap frees inside processBlock");
        expect (r.finite, what + ": NaN or Inf in the output");
        expect (r.peak < 16.0f, what + ": output peaked at " + juce::String (r.peak));
    }

    void runTest() override
    {
        const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        const int sizes[] = { 32, 64, 128, 256, 512, 1024, 2048 };

        beginTest ("Learner EQ: every rate and block size, bands gliding, no allocation, no NaN");
        {
            for (auto rate : rates)
                for (auto size : sizes)
                {
                    LearnerEQProcessor eq;
                    eq.addBand (120.0f, 6.0f, EQCoefficients::BandType::lowShelf);
                    eq.addBand (3000.0f, -4.0f, EQCoefficients::BandType::bell);
                    eq.addBand (40.0f, 0.0f, EQCoefficients::BandType::highPass);
                    eq.addBand (12000.0f, 3.0f, EQCoefficients::BandType::highShelf);

                    const auto r = run (eq, rate, size, 24, [&] (int b)
                    {
                        // Sweeps across the whole range, so every band is
                        // gliding - the path that used to allocate.
                        const auto t = (float) b / 23.0f;
                        setParam (eq.apvts, LearnerEQProcessor::freqParamId (0), 20.0f * std::pow (1000.0f, t));
                        setParam (eq.apvts, LearnerEQProcessor::gainParamId (0), -18.0f + 36.0f * t);
                        setParam (eq.apvts, LearnerEQProcessor::qParamId (1), 0.1f + 17.0f * t);
                        setParam (eq.apvts, LearnerEQProcessor::freqParamId (1), 20000.0f - 19000.0f * t);
                        setParam (eq.apvts, LearnerEQProcessor::typeParamId (1), (float) (b % 6));
                        setParam (eq.apvts, LearnerEQProcessor::onParamId (3), (b / 4) % 2 == 0 ? 1.0f : 0.0f);
                        setParam (eq.apvts, LearnerEQProcessor::bypassParamId, b == 12 ? 1.0f : 0.0f);
                    });

                    check ("EQ at " + juce::String (rate) + " Hz / " + juce::String (size), r);
                }
        }

        beginTest ("Learner Comp: every rate and block size, knobs moving, no allocation, no NaN");
        {
            for (auto rate : rates)
                for (auto size : sizes)
                {
                    LearnerCompProcessor comp;
                    const auto r = run (comp, rate, size, 24, [&] (int b)
                    {
                        const auto t = (float) b / 23.0f;
                        setParam (comp.apvts, "threshold", -60.0f + 60.0f * t);
                        setParam (comp.apvts, "ratio", 1.0f + 19.0f * t);
                        setParam (comp.apvts, "attack", 0.1f + 100.0f * t);
                        setParam (comp.apvts, "release", 10.0f + 900.0f * t);
                        setParam (comp.apvts, "knee", 18.0f * t);
                        setParam (comp.apvts, "makeup", 24.0f * t);
                        setParam (comp.apvts, "dryWet", 100.0f * (1.0f - t));
                        setParam (comp.apvts, LearnerCompProcessor::bypassParamId, b == 12 ? 1.0f : 0.0f);
                    });

                    check ("Comp at " + juce::String (rate) + " Hz / " + juce::String (size), r);
                }
        }

        beginTest ("Learner Verb: every rate and block size, type switching, no allocation, no NaN");
        {
            for (auto rate : rates)
                for (auto size : sizes)
                {
                    LearnerVerbProcessor verb;
                    const auto r = run (verb, rate, size, 24, [&] (int b)
                    {
                        const auto t = (float) b / 23.0f;
                        setParam (verb.apvts, "type", (float) ((b / 3) % 4));
                        setParam (verb.apvts, "decay", 0.2f + 9.0f * t);
                        setParam (verb.apvts, "preDelay", 250.0f * t);
                        setParam (verb.apvts, "size", 100.0f * t);
                        setParam (verb.apvts, "damping", 100.0f * (1.0f - t));
                        setParam (verb.apvts, "width", 100.0f * t);
                        setParam (verb.apvts, LearnerVerbProcessor::bypassParamId, b == 12 ? 1.0f : 0.0f);
                    });

                    check ("Verb at " + juce::String (rate) + " Hz / " + juce::String (size), r);
                }
        }

        beginTest ("a block longer than the host announced does not allocate either");
        {
            // Some hosts send more samples than prepareToPlay promised
            // (offline bounces). The copies each processor keeps are sized
            // with headroom for exactly this.
            LearnerEQProcessor eq;
            eq.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> big (2, 4096);
            big.clear();
            juce::MidiBuffer midi;
            eq.processBlock (big, midi);   // first call may settle

            RtAlloc::allocations = 0;
            RtAlloc::armed = true;
            eq.processBlock (big, midi);
            RtAlloc::armed = false;
            expectEquals (RtAlloc::allocations, 0);
        }

        beginTest ("training beds are freed once no block can be reading them");
        {
            juce::PropertiesFile::Options options;
            options.applicationName = "RealtimeSafetyTest";
            options.filenameSuffix = ".settings";
            options.folderName = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("abcTrain-rt-test").getFullPathName();
            juce::PropertiesFile properties (options);
            ReferenceAudioLibrary library (properties);
            PracticeAudioSource source (library);
            source.setEnabled (true);
            source.prepare (44100.0);

            for (int i = 0; i < 20; ++i)
            {
                juce::AudioBuffer<float> bed (2, 1024);
                bed.clear();
                source.publishOverrideBuffer (std::move (bed));

                juce::AudioBuffer<float> block (2, 256);
                source.fillBlock (block);
            }

            // The published bed, and at most the one the last block used.
            expect (source.getNumHeldOverrides() <= 2,
                    "beds held: " + juce::String (source.getNumHeldOverrides()));

            source.clearOverrideBuffer();
            juce::AudioBuffer<float> block (2, 256);
            source.fillBlock (block);
            source.clearOverrideBuffer();
            expectEquals (source.getNumHeldOverrides(), 0);
        }
    }
};

static RealtimeSafetyTest realtimeSafetyTest;
