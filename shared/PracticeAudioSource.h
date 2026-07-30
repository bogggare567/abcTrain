#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ReferenceAudioLibrary.h"
#include <atomic>

// Lets the three Learner plugins play something without a host feeding
// them audio.
//
// The gap this closes: open Learner EQ as a standalone app and it is
// silent. Every knob works, the spectrum is empty, and there is nothing
// to learn from - the plugin that teaches you what an EQ does needs
// something to do it to. In a DAW that is the track; outside one there
// was nothing, which meant the teaching half of this product was
// unusable to anyone who had not already set up a session.
//
// It plays from the same ReferenceAudioLibrary Ear Trainer uses, so a
// player who has imported their own music into the trainer already has
// material here - the two halves of the product share one library rather
// than each keeping its own. See docs/user-journey.md: "it trains on your
// own material" is one of the three things that are genuinely ours, and
// it was only true in one of the four plugins.
//
// **Off by default, always.** Inside a DAW this injects audio into a
// track, which is not something a plugin may decide for itself. It turns
// on only when someone presses the control that says so.
//
// Real-time safe: one atomic pointer load and a read, no locks and no
// allocation, same contract as TestSignalGenerator.
class PracticeAudioSource
{
public:
    explicit PracticeAudioSource (ReferenceAudioLibrary& libraryToRead) noexcept
        : library (libraryToRead) {}

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        readPosition = 0.0;
    }

    // Message thread. The audio thread only ever reads this flag.
    void setEnabled (bool shouldBeEnabled) noexcept { enabled.store (shouldBeEnabled); }
    bool isEnabled() const noexcept { return enabled.load(); }

    // Plays this buffer instead of the library's selection, for as long as
    // it is set. A training module generates a bed and wants it heard
    // regardless of what the player picked in the title row.
    //
    // **This takes ownership, and that is the whole point.**
    //
    // It used to take a raw pointer to a buffer the module panel owned -
    // and the panel belongs to the *editor*, which a host destroys the
    // moment someone closes the plugin window, while audio keeps running.
    // So: the audio thread loads the pointer at the top of a block, the
    // window closes mid-block, the panel's buffers are freed, and the rest
    // of that block reads freed memory. That is a use-after-free on the
    // audio thread - the shape of crash that shows up as "the plugin dies
    // while I'm using it" and never in a test. Clearing the pointer in the
    // panel's destructor does not help: the block is already in flight.
    //
    // Beds now live here instead, in the *processor*, which the host only
    // destroys after it has stopped calling processBlock. Every bed is
    // kept for the processor's lifetime rather than freed when the next
    // one arrives - the same accepted memory tradeoff as
    // ReferenceAudioLibrary::loadedBuffers, for the same reason: there is
    // no safe moment on the message thread to know the audio thread has
    // finished with one.
    //
    // Message thread only for the caller; the audio thread only ever loads
    // the atomic.
    void publishOverrideBuffer (juce::AudioBuffer<float>&& buffer)
    {
        auto* owned = ownedOverrides.add (new juce::AudioBuffer<float> (std::move (buffer)));
        override.store (owned);
    }

    void clearOverrideBuffer() noexcept
    {
        override.store (nullptr);
    }

    // Replaces the block with the library's current clip, looped, and
    // returns true if it actually played anything.
    //
    // Enabling and disabling crossfade over ~30 ms rather than switching
    // on a block boundary: a hard switch into or out of a waveform mid-
    // cycle is a click, and a plugin that clicks when you press its own
    // button reads as broken. During the fade the host's audio is still
    // there underneath, which is what makes the transition a fade at all
    // rather than a fade from silence.
    bool fillBlock (juce::AudioBuffer<float>& buffer) noexcept
    {
        // Loaded once, not twice. Reading the atomic a second time to ask
        // "is an override set" let the message thread clear it in between,
        // so a block could decide it had a clip and then decide it wanted
        // no audio - an audible stutter at exactly the moment a check
        // ends.
        const auto* overrideClip = override.load();
        const auto* clip = overrideClip != nullptr ? overrideClip : library.getActiveBuffer();

        const auto wantsAudio = (enabled.load() || overrideClip != nullptr)
                                && clip != nullptr && clip->getNumSamples() > 0;

        if (! wantsAudio && currentGain <= 0.0001f)
        {
            currentGain = 0.0f;
            return false;
        }

        const auto target = wantsAudio ? 1.0f : 0.0f;
        const auto step = (float) (1.0 / juce::jmax (1.0, sampleRate * 0.03));

        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = buffer.getNumChannels();

        // A clip that has gone away mid-fade leaves nothing to fade *from*,
        // so fade the host's audio back in against silence instead of
        // reading a null buffer.
        const auto usable = clip != nullptr && clip->getNumSamples() > 0;
        const auto* sourceLeft = usable ? clip->getReadPointer (0) : nullptr;
        const auto* sourceRight = usable ? clip->getReadPointer (clip->getNumChannels() > 1 ? 1 : 0)
                                          : nullptr;
        const auto clipLength = usable ? clip->getNumSamples() : 0;

        auto gain = currentGain;
        auto position = readPosition;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            gain += juce::jlimit (-step, step, target - gain);

            auto left = 0.0f;
            auto right = 0.0f;

            if (sourceLeft != nullptr)
            {
                const auto index = (int) position % clipLength;
                left = sourceLeft[index];
                right = sourceRight[index];
                position += 1.0;

                if (position >= (double) clipLength)
                    position -= (double) clipLength;
            }

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* data = buffer.getWritePointer (channel);
                const auto value = channel == 0 ? left : right;
                data[sample] = value * gain + data[sample] * (1.0f - gain);
            }
        }

        currentGain = gain;
        readPosition = position;

        return true;
    }

private:
    ReferenceAudioLibrary& library;

    double sampleRate = 44100.0;
    double readPosition = 0.0;
    float currentGain = 0.0f;

    std::atomic<bool> enabled { false };
    std::atomic<const juce::AudioBuffer<float>*> override { nullptr };

    // Owned here so they outlive every editor. Never emptied while this
    // object is alive - see publishOverrideBuffer.
    juce::OwnedArray<juce::AudioBuffer<float>> ownedOverrides;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticeAudioSource)
};
