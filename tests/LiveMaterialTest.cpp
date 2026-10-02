#include <juce_audio_basics/juce_audio_basics.h>
#include "../shared/audio/LessonAudioBed.h"
#include "../shared/audio/ReferenceAudioLibrary.h"

// ADR 058: every lesson material decodes from the live multitrack, and the
// sustain measure tells hits from a held sound - what decides whether
// delay, reverb and compression can use a chosen clip.
class LiveMaterialTest : public juce::UnitTest
{
public:
    LiveMaterialTest() : juce::UnitTest ("LiveMaterial", "Audio") {}

    void runTest() override
    {
        beginTest ("every live material decodes, ~9 s, stereo, not silent");
        for (auto name : { "vocal", "kick", "snare", "drums", "bass", "guitar", "mix" })
            for (int seed : { 1, 2 })
            {
                const auto b = LessonAudioBed::renderLive (name, 48000.0, seed);
                expect (b.getNumChannels() == 2, name);
                expect (b.getNumSamples() > 48000 * 8 && b.getNumSamples() < 48000 * 10, juce::String (name) + " " + juce::String (b.getNumSamples()));
                expect (b.getRMSLevel (0, 0, b.getNumSamples()) > 0.01f, name);
            }
        expect (LessonAudioBed::renderLive ("chord", 48000.0, 1).getNumSamples() == 0, "no recording: the synth fallback decides");

        beginTest ("sustain: drums are hits, a held tone and the voice-mix are not");
        {
            const auto kick = LessonAudioBed::renderLive ("kick", 44100.0, 1);
            juce::AudioBuffer<float> tone (1, 44100 * 4);
            for (int i = 0; i < tone.getNumSamples(); ++i)
                tone.setSample (0, i, 0.3f * std::sin (0.05f * (float) i));
            const auto k = ReferenceAudioLibrary::sustainOf (kick, 44100.0);
            const auto t = ReferenceAudioLibrary::sustainOf (tone, 44100.0);
            expect (k < 0.35f, "kick " + juce::String (k));
            expect (t > 0.8f, "tone " + juce::String (t));
        }
    }
};

static LiveMaterialTest liveMaterialTest;
