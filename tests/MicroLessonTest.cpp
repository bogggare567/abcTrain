#include <juce_core/juce_core.h>
#include "../shared/MicroLesson.h"

// MicroLesson is deliberately pure logic (no APVTS/Component dependency),
// so this is directly testable without a processor, an editor, or the
// message-loop concerns that apply to ProgressManager's real
// ChangeListener wiring - see docs/testing-strategy.md.
class MicroLessonTest : public juce::UnitTest
{
public:
    MicroLessonTest() : juce::UnitTest ("MicroLesson", "Lessons") {}

    void runTest() override
    {
        beginTest ("is inactive until start() is called");
        {
            MicroLesson lesson ("Test Lesson", { { "Step 1", {} }, { "Step 2", {} } });
            expect (! lesson.isActive());

            lesson.start();
            expect (lesson.isActive());
            expectEquals (lesson.getCurrentStepIndex(), 0);
            expect (lesson.isOnFirstStep());
            expect (! lesson.isOnLastStep());
        }

        beginTest ("nextStep advances through all steps and stops at the end");
        {
            MicroLesson lesson ("Test", { { "A", {} }, { "B", {} }, { "C", {} } });
            lesson.start();

            expect (lesson.nextStep());
            expectEquals (lesson.getCurrentStepIndex(), 1);

            expect (lesson.nextStep());
            expectEquals (lesson.getCurrentStepIndex(), 2);
            expect (lesson.isOnLastStep());

            expect (! lesson.nextStep()); // already on the last step
            expectEquals (lesson.getCurrentStepIndex(), 2);
        }

        beginTest ("previousStep steps back and stops at the beginning");
        {
            MicroLesson lesson ("Test", { { "A", {} }, { "B", {} } });
            lesson.start();
            lesson.nextStep();

            expect (lesson.previousStep());
            expectEquals (lesson.getCurrentStepIndex(), 0);
            expect (lesson.isOnFirstStep());
            expect (! lesson.previousStep());
        }

        beginTest ("nextStep/previousStep do nothing before start() is called");
        {
            MicroLesson lesson ("Test", { { "A", {} }, { "B", {} } });
            expect (! lesson.nextStep());
            expect (! lesson.previousStep());
            expectEquals (lesson.getCurrentStepIndex(), 0);
        }

        beginTest ("stop() deactivates the lesson");
        {
            MicroLesson lesson ("Test", { { "A", {} }, { "B", {} } });
            lesson.start();
            lesson.stop();

            expect (! lesson.isActive());
            expect (! lesson.nextStep());
        }

        beginTest ("getCurrentStep exposes the step's text and target parameters");
        {
            MicroLesson lesson ("Test", {
                { "Boost presence", { { "band2Gain", 3.0f }, { "band2Freq", 3000.0f } } }
            });
            lesson.start();

            const auto& step = lesson.getCurrentStep();
            expectEquals (step.explanationText, juce::String ("Boost presence"));
            expectEquals ((int) step.targetParameters.size(), 2);
            expectEquals (step.targetParameters[0].first, juce::String ("band2Gain"));
            expectEquals (step.targetParameters[0].second, 3.0f);
        }

        beginTest ("getNumSteps and getTitle report what was constructed");
        {
            MicroLesson lesson ("My Lesson", { { "A", {} }, { "B", {} }, { "C", {} } });
            expectEquals (lesson.getTitle(), juce::String ("My Lesson"));
            expectEquals (lesson.getNumSteps(), 3);
        }
    }
};

static MicroLessonTest microLessonTest;
