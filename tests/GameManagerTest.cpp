#include <juce_core/juce_core.h>
#include "../Source/GameManager.h"

class GameManagerTest : public juce::UnitTest
{
public:
    GameManagerTest() : juce::UnitTest ("GameManager", "Games") {}

    void runTest() override
    {
        beginTest ("registers both games");
        {
            GameManager manager;
            expectEquals (manager.getGameNames().size(), 2);
            expectEquals (manager.getActiveGameIndex(), 0);
        }

        beginTest ("prepares and can process through the active game");
        {
            GameManager manager;
            const juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            manager.prepare (spec);

            juce::AudioBuffer<float> buffer (2, 512);
            manager.process (buffer);

            expect (true); // reaching here without asserting/crashing is the point
        }

        beginTest ("switching games changes the active game");
        {
            GameManager manager;
            manager.setActiveGameIndex (1);
            expectEquals (manager.getActiveGameIndex(), 1);
        }

        beginTest ("out-of-range index is ignored");
        {
            GameManager manager;
            manager.setActiveGameIndex (99);
            expectEquals (manager.getActiveGameIndex(), 0);

            manager.setActiveGameIndex (-1);
            expectEquals (manager.getActiveGameIndex(), 0);
        }
    }
};

static GameManagerTest gameManagerTest;
