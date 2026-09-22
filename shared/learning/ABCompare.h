#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Two settings of the same plugin, A and B, one click apart - how anyone
// actually decides between two compressions: not by remembering the first
// one, by hearing both again.
//
// The slots live in the APVTS state tree itself, as a child the parameters
// never read, so they are saved with the host's project and restored with
// it like any knob. Every parameter is captured except the ones named in
// `excluded` (Bypass: flipping A/B must never silently switch the plugin
// off). Selecting the other slot first stores the current knobs into the
// one being left, then loads the one being entered; an empty slot keeps
// the current knobs, so B starts as a copy of A and you change one thing.
class ABCompare
{
public:
    ABCompare (juce::AudioProcessorValueTreeState& state, juce::StringArray excludedIds)
        : apvts (state), excluded (std::move (excludedIds)) {}

    int getActive() const
    {
        return (int) apvts.state.getChildWithName (treeId).getProperty ("active", 0);
    }

    void select (int slot)
    {
        slot = juce::jlimit (0, 1, slot);
        const auto current = getActive();

        if (slot == current)
            return;

        store (current);

        auto tree = slots();
        const auto target = tree.getChildWithName (slotId (slot));

        if (target.isValid())
            load (target);

        tree.setProperty ("active", slot, nullptr);
    }

    // The other slot becomes what you hear now.
    void copyToOther()
    {
        store (1 - getActive());
    }

private:
    static inline const juce::Identifier treeId { "ABCompare" };

    static juce::Identifier slotId (int slot) { return slot == 0 ? juce::Identifier ("A") : juce::Identifier ("B"); }

    juce::ValueTree slots()
    {
        return apvts.state.getOrCreateChildWithName (treeId, nullptr);
    }

    void store (int slot)
    {
        auto tree = slots();
        auto child = tree.getChildWithName (slotId (slot));

        if (! child.isValid())
        {
            child = juce::ValueTree (slotId (slot));
            tree.appendChild (child, nullptr);
        }

        for (auto* p : apvts.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! excluded.contains (ranged->getParameterID()))
                    child.setProperty (ranged->getParameterID(), ranged->getValue(), nullptr);
    }

    void load (const juce::ValueTree& child)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto id = ranged->getParameterID();

                if (excluded.contains (id) || ! child.hasProperty (id))
                    continue;

                ranged->beginChangeGesture();
                ranged->setValueNotifyingHost ((float) child.getProperty (id));
                ranged->endChangeGesture();
            }
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray excluded;
};
