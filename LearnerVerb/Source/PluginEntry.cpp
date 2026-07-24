#include "PluginProcessor.h"

// See LearnerEQ/Source/PluginEntry.cpp for why this is a separate file
// from PluginProcessor.cpp: EarTrainerTests links PluginProcessor.cpp
// directly (to get a real LearnerVerbProcessor for LearnerVerbTest), but
// must not also link this factory function - LearnerEQ's and
// LearnerComp's equivalents would collide with it in the same test binary.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LearnerVerbProcessor();
}
