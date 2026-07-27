#include <juce_audio_basics/juce_audio_basics.h>
#include "../shared/LessonAudioBed.h"
#include "../shared/TestUtils.h"
#include <cmath>

// The beds are synthesized, so what a test can honestly check is not "does
// it sound like a kick" - no assertion knows that - but the properties the
// modules depend on: that every bed makes sound, that it loops without a
// seam, that a different seed is a different instance rather than the same
// file again, and that the two beds whose whole job is to have silence in
// them actually do.
class LessonAudioBedTest : public juce::UnitTest
{
public:
    LessonAudioBedTest() : juce::UnitTest ("LessonAudioBed") {}

    void runTest() override
    {
        using Bed = TrainingModule::Bed;

        const Bed everyBed[] = { Bed::drumLoop, Bed::bassNote, Bed::singleHit,
                                 Bed::brightHit, Bed::chord, Bed::pinkNoise };

        beginTest ("every bed renders real audio at a real length");
        {
            for (const auto bed : everyBed)
            {
                const auto buffer = LessonAudioBed::render (bed, 44100.0, 1);

                expectEquals (buffer.getNumChannels(), 2);
                expect (buffer.getNumSamples() > 44100 / 2, "bed is implausibly short");
                expect (TestUtils::rms (buffer) > 0.001f, "bed rendered silent");

                // Every bed must arrive at the same peak. Not tidiness: a
                // check that grades threshold in dB is only meaningful
                // against a known input level, so a bed landing 4 dB hotter
                // than another would make the same answer wrong on one and
                // right on the other.
                expectWithinAbsoluteError (buffer.getMagnitude (0, buffer.getNumSamples()),
                                            0.5f, 0.02f);
            }
        }

        beginTest ("beds loop without a seam");
        {
            // A click on the loop point is the fastest way to teach a
            // player to hear the click instead of the parameter. Both edges
            // must be faded, so the first and last samples are near zero.
            for (const auto bed : everyBed)
            {
                const auto buffer = LessonAudioBed::render (bed, 44100.0, 3);
                const auto last = buffer.getNumSamples() - 1;

                expect (std::abs (buffer.getSample (0, 0)) < 0.02f, "loop starts on a step");
                expect (std::abs (buffer.getSample (0, last)) < 0.02f, "loop ends on a step");
            }
        }

        beginTest ("a different seed is a different sound, not the same file");
        {
            // The whole argument for generating rather than shipping
            // samples: two rounds must not be the same recording. Compared
            // on the drum loop, where every hit draws its own pitch, decay
            // and click.
            const auto first = LessonAudioBed::render (Bed::drumLoop, 44100.0, 1);
            const auto second = LessonAudioBed::render (Bed::drumLoop, 44100.0, 2);

            expectEquals (first.getNumSamples(), second.getNumSamples());

            auto difference = 0.0;

            for (int i = 0; i < first.getNumSamples(); ++i)
                difference += std::abs (first.getSample (0, i) - second.getSample (0, i));

            expect (difference / (double) first.getNumSamples() > 0.001,
                    "two seeds produced effectively the same audio");
        }

        beginTest ("the same seed is reproducible");
        {
            // A check that cannot be replayed cannot be argued with, so a
            // seed has to pin the sound exactly.
            const auto first = LessonAudioBed::render (Bed::drumLoop, 44100.0, 9);
            const auto second = LessonAudioBed::render (Bed::drumLoop, 44100.0, 9);

            for (int i = 0; i < first.getNumSamples(); i += 97)
                expectEquals (first.getSample (0, i), second.getSample (0, i));
        }

        beginTest ("the tail beds really do leave silence to hear a tail in");
        {
            // singleHit and brightHit exist so pre-delay and decay have
            // somewhere to happen. If the hit filled the loop they would be
            // ordinary loops and the modules using them would teach
            // nothing.
            for (const auto bed : { Bed::singleHit, Bed::brightHit })
            {
                const auto buffer = LessonAudioBed::render (bed, 44100.0, 5);
                const auto quarter = buffer.getNumSamples() / 4;

                const auto atStart = buffer.getMagnitude (0, quarter);
                const auto atEnd = buffer.getMagnitude (buffer.getNumSamples() - quarter, quarter);

                expect (atStart > 0.05f, "no hit at the front");
                expect (atEnd < atStart * 0.1f, "no silence left for a tail");
            }
        }

        beginTest ("the chord bed is genuinely stereo, the bass bed is not");
        {
            // Width modules need a real side signal; a mono bed would make
            // the knob do nothing and the lesson a lie.
            const auto chord = LessonAudioBed::render (Bed::chord, 44100.0, 11);
            auto sideEnergy = 0.0;

            for (int i = 0; i < chord.getNumSamples(); ++i)
                sideEnergy += std::abs (chord.getSample (0, i) - chord.getSample (1, i));

            expect (sideEnergy / (double) chord.getNumSamples() > 0.005,
                    "chord bed has no side signal to widen");

            const auto bass = LessonAudioBed::render (Bed::bassNote, 44100.0, 11);

            for (int i = 0; i < bass.getNumSamples(); i += 53)
                expectEquals (bass.getSample (0, i), bass.getSample (1, i));
        }

        beginTest ("length matches what lengthSeconds promises");
        {
            for (const auto bed : everyBed)
            {
                const auto buffer = LessonAudioBed::render (bed, 48000.0, 1);
                const auto expected = (int) (LessonAudioBed::lengthSeconds (bed) * 48000.0);

                expectEquals (buffer.getNumSamples(), expected);
            }
        }
    }
};

static LessonAudioBedTest lessonAudioBedTest;
