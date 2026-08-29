#pragma once

#include "PinkNoiseGenerator.h"
#include "ReferenceAudioLibrary.h"

// Drop-in replacement for PinkNoiseGenerator (same nextSample() shape, same
// call sites) used by every game except StereoWidthGame - which needs two
// independently-decorrelated sources (see StereoWidthGame.h), something a
// single real recorded file can't provide, so it deliberately keeps a plain
// PinkNoiseGenerator instead of this class.
//
// When setLibrary() has been given a ReferenceAudioLibrary with a user-
// selected file loaded, this loops that file's audio instead of synthesized
// pink noise; otherwise (no library, or no selection made yet) it behaves
// exactly as PinkNoiseGenerator always did. nextSample() only ever does an
// atomic pointer load and an array read - no allocation, no file I/O - so
// it's safe to call from the audio thread exactly like the noise generator
// it replaces.
class TestSignalGenerator
{
public:
    void setLibrary (const ReferenceAudioLibrary* newLibrary) noexcept { library = newLibrary; }

    // Only reaches the fallback noise: a player who has selected their own
    // audio has already answered the question this setting asks.
    void setNoiseColour (NoiseColour colour) noexcept { pinkNoise.setColour (colour); }

    float nextSample() noexcept
    {
        if (library != nullptr)
        {
            if (const auto* buffer = library->getActiveBuffer())
            {
                if (const auto length = buffer->getNumSamples(); length > 0)
                {
                    // Wrap *before* reading, not after. The library swaps in a
                    // new clip between rounds without telling anyone, and
                    // readPosition survives the swap - so a long clip followed
                    // by a short one used to read past the end of the new
                    // buffer. PracticeAudioSource has always done it this way
                    // round; this is the same discipline.
                    if (readPosition >= length)
                        readPosition = 0;

                    const auto value = buffer->getSample (0, readPosition);
                    ++readPosition;
                    return value;
                }
            }
        }

        readPosition = 0;
        return pinkNoise.nextSample();
    }

private:
    PinkNoiseGenerator pinkNoise;
    const ReferenceAudioLibrary* library = nullptr;
    int readPosition = 0;
};
