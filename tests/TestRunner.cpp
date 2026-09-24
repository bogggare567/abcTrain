#include <juce_core/juce_core.h>

// Individual UnitTest subclasses (EQGameTest, CompressionGameTest, ...)
// self-register into JUCE's global unit test list via file-scope static
// instances - see the other files in this directory.
//
//   EarTrainerTests                 every test
//   EarTrainerTests ReverbCharacter only the tests with that name (or category)
int main (int argc, char** argv)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    if (argc > 1)
    {
        const juce::String only (argv[1]);
        juce::Array<juce::UnitTest*> chosen;

        for (auto* test : juce::UnitTest::getAllTests())
            if (test->getName() == only || test->getCategory() == only)
                chosen.add (test);

        runner.runTests (chosen);
    }
    else
    {
        runner.runAllTests();
    }

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        numFailures += runner.getResult (i)->failures;

    return numFailures > 0 ? 1 : 0;
}
