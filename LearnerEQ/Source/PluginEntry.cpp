#include "PluginProcessor.h"

// Kept in its own file, separate from PluginProcessor.cpp: EarTrainerTests
// links PluginProcessor.cpp directly to get a real LearnerEQProcessor for
// LearnerEQTest, but must not link this factory function too - both it and
// LearnerComp's equivalent define createPluginFilter(), and linking two
// definitions of the same JUCE-required entry point into one binary would
// collide. Only the real plugin targets (juce_add_plugin) include this file.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LearnerEQProcessor();
}
