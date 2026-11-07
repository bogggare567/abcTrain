#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/**
    How a Learner plugin saves its knobs into a host project and reads them
    back (ADR 069).

    The state carries its own format number, `stateVersion`. Today there is
    one format, so reading it changes nothing; it is here so that the day a
    parameter is renamed, split or given a new range, `migrate` can turn an
    old project's state into the new one instead of silently dropping the
    knob - a project saved in 2026 has to open in 2030. A state with no
    number is format 0: everything saved before 2.3.

    A state whose root is not this plugin's (another plugin's chunk pasted
    by a host, a corrupt file) is ignored rather than loaded: replaceState
    with a foreign tree would leave every knob at a default with no word.
*/
namespace PluginState
{
    inline constexpr int currentVersion = 1;
    inline const juce::Identifier versionProperty { "stateVersion" };
    inline const juce::Identifier pluginProperty { "plugin" };   // which Learner wrote it: all three name the tree PARAMETERS

    /** Brings a state written in an older format up to the current one. */
    inline void migrate (juce::ValueTree& state, int fromVersion)
    {
        // 0 -> 1: the format number itself was added; nothing moved.
        juce::ignoreUnused (state, fromVersion);
    }

    inline void save (juce::AudioProcessorValueTreeState& apvts, juce::MemoryBlock& destData)
    {
        auto state = apvts.copyState();
        if (! state.isValid())
            return;

        state.setProperty (versionProperty, currentVersion, nullptr);
        state.setProperty (pluginProperty, apvts.processor.getName(), nullptr);
        if (auto xml = state.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, destData);
    }

    /** False if the data is not this plugin's state (nothing is changed). */
    inline bool load (juce::AudioProcessorValueTreeState& apvts, const void* data, int sizeInBytes)
    {
        const auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
            return false;

        auto state = juce::ValueTree::fromXml (*xml);

        // Learner EQ's chunk handed to Learner Verb has the same root tag;
        // the plugin's name tells them apart. A state from before 2.3 has
        // no name and is taken as before.
        if (state.hasProperty (pluginProperty) && state[pluginProperty].toString() != apvts.processor.getName())
            return false;

        const int version = state.getProperty (versionProperty, 0);
        if (version > currentVersion)
            return false;   // written by a newer plugin: better a default than a misread knob

        migrate (state, version);
        state.setProperty (versionProperty, currentVersion, nullptr);
        apvts.replaceState (state);
        return true;
    }
}
