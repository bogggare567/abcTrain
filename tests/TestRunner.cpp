#include <juce_core/juce_core.h>

// Individual UnitTest subclasses (EQGameTest, CompressionGameTest, ...)
// self-register into JUCE's global unit test list via file-scope static
// instances - see the other files in this directory.
int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runAllTests();

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        numFailures += runner.getResult (i)->failures;

    return numFailures > 0 ? 1 : 0;
}
