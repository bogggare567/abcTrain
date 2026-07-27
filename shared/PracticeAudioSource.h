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
        const auto* clip = library.getActiveBuffer();
        const auto wantsAudio = enabled.load() && clip != nullptr && clip->getNumSamples() > 0;

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
        const auto* source = clip != nullptr && clip->getNumSamples() > 0
                                 ? clip->getReadPointer (0) : nullptr;
        const auto clipLength = source != nullptr ? clip->getNumSamples() : 0;

        auto gain = currentGain;
        auto position = readPosition;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            gain += juce::jlimit (-step, step, target - gain);

            auto value = 0.0f;

            if (source != nullptr)
            {
                const auto index = (int) position;
                value = source[index % clipLength];
                position += 1.0;

                if (position >= (double) clipLength)
                    position -= (double) clipLength;
            }

            for (int channel = 0; channel < numChannels; ++channel)
            {
                auto* data = buffer.getWritePointer (channel);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PracticeAudioSource)
};
