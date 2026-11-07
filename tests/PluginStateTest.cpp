#include <juce_audio_processors/juce_audio_processors.h>
#include "LearnerEQ/Source/PluginProcessor.h"
#include "LearnerVerb/Source/PluginProcessor.h"
#include "shared/learning/PluginState.h"

// A Learner's knobs in a host project (ADR 069): the state says which
// format it is in, an old project without that number still opens, and a
// chunk that is not this plugin's leaves the knobs alone.
class PluginStateTest : public juce::UnitTest
{
public:
    PluginStateTest() : juce::UnitTest ("PluginState", "Plugins") {}

    void runTest() override
    {
        beginTest ("saved with its format number; read back with every knob");
        {
            LearnerVerbProcessor a, b;
            auto* mix = a.apvts.getParameter (LearnerVerbProcessor::decayParamId);
            expect (mix != nullptr);
            mix->setValueNotifyingHost (0.123f);

            juce::MemoryBlock saved;
            a.getStateInformation (saved);
            const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
            expect (xml != nullptr && xml->getIntAttribute ("stateVersion") == PluginState::currentVersion);

            b.setStateInformation (saved.getData(), (int) saved.getSize());
            auto* again = b.apvts.getParameter (mix->getParameterID());
            expectWithinAbsoluteError (again->getValue(), mix->getValue(), 1.0e-6f);
        }

        beginTest ("a project saved before 2.3 (no number) still opens");
        {
            LearnerVerbProcessor a, b;
            auto state = a.apvts.copyState();
            state.removeProperty (PluginState::versionProperty, nullptr);
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*state.createXml(), old);
            expect (PluginState::load (b.apvts, old.getData(), (int) old.getSize()));
        }

        beginTest ("another plugin's chunk, or a newer format, leaves the knobs alone");
        {
            LearnerEQProcessor eq;
            LearnerVerbProcessor verb;
            juce::MemoryBlock eqState;
            eq.getStateInformation (eqState);
            expect (! PluginState::load (verb.apvts, eqState.getData(), (int) eqState.getSize()));

            auto future = verb.apvts.copyState();
            future.setProperty (PluginState::versionProperty, PluginState::currentVersion + 1, nullptr);
            juce::MemoryBlock newer;
            juce::AudioProcessor::copyXmlToBinary (*future.createXml(), newer);
            expect (! PluginState::load (verb.apvts, newer.getData(), (int) newer.getSize()));
        }
    }
};

static PluginStateTest pluginStateTest;
