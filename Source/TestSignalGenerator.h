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

    float nextSample() noexcept
    {
        if (library != nullptr)
        {
            if (const auto* buffer = library->getActiveBuffer())
            {
                if (buffer->getNumSamples() > 0)
                {
                    const auto value = buffer->getSample (0, readPosition);
                    readPosition = (readPosition + 1) % buffer->getNumSamples();
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
