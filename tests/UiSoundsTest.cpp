#include <juce_audio_formats/juce_audio_formats.h>
#include <set>
#include "../shared/audio/UiSounds.h"

// The app's one-shots must not become a dripping tap (ADR 054): no take
// twice in a row, a little pitch and level movement every time, a quick
// burst of the same event backing off - and render() plays what was
// triggered.
class UiSoundsTest : public juce::UnitTest
{
public:
    UiSoundsTest() : juce::UnitTest ("UiSounds", "Audio") {}

    static juce::MemoryBlock wavOf (float freq)
    {
        juce::AudioBuffer<float> b (1, 4410);
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * freq * (float) i / 44100.0f));

        juce::MemoryBlock block;
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::MemoryOutputStream (block, false), 44100.0, 1, 16, {}, 0));
            writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
        return block;
    }

    void runTest() override
    {
        UiSounds ui;
        std::vector<juce::MemoryBlock> blocks { wavOf (440.0f), wavOf (660.0f), wavOf (880.0f) };
        for (auto& b : blocks)
            ui.addTake (UiSounds::Event::correct, b.getData(), b.getSize());
        ui.prepare (48000.0);

        beginTest ("every take decodes; names map both ways");
        expectEquals (ui.takesFor (UiSounds::Event::correct), 3);
        for (int i = 0; i < UiSounds::numEvents; ++i)
            expectEquals (UiSounds::eventFromId (UiSounds::idOf ((UiSounds::Event) i)), i);
        expectEquals (UiSounds::eventFromId ("nope"), -1);

        beginTest ("never the same take twice in a row; pitch and level move within bounds");
        {
            int previous = -1;
            std::set<int> seen;
            for (int n = 0; n < 60; ++n)
            {
                ui.trigger (UiSounds::Event::correct);
                const auto shot = ui.lastShot();
                expect (shot.take != previous, "repeated take " + juce::String (shot.take));
                previous = shot.take;
                seen.insert (shot.take);

                const auto cents = 1200.0f * std::log2 (shot.rate);
                expect (std::abs (cents) <= 35.01f, juce::String (cents));
            }
            expectEquals ((int) seen.size(), 3);
        }

        beginTest ("a burst of the same event backs off; the floor holds");
        {
            UiSounds burst;
            burst.addTake (UiSounds::Event::wrong, blocks[0].getData(), blocks[0].getSize());
            burst.prepare (44100.0);
            burst.trigger (UiSounds::Event::wrong);
            const auto first = juce::Decibels::gainToDecibels (burst.lastShot().gain);
            for (int n = 0; n < 10; ++n)
                burst.trigger (UiSounds::Event::wrong);
            const auto tenth = juce::Decibels::gainToDecibels (burst.lastShot().gain);
            expect (tenth < first - 4.0f, juce::String (first) + " -> " + juce::String (tenth));
            expect (tenth > -20.0f - 8.0f - 1.6f, juce::String (tenth));
        }

        beginTest ("render plays what was triggered, and nothing when switched off");
        {
            UiSounds play;
            play.addTake (UiSounds::Event::open, blocks[1].getData(), blocks[1].getSize());
            play.prepare (44100.0);

            juce::AudioBuffer<float> out (2, 512);
            out.clear();
            play.render (out);
            expectEquals (out.getMagnitude (0, 512), 0.0f);

            play.trigger (UiSounds::Event::open);
            play.render (out);
            expect (out.getMagnitude (0, 512) > 0.01f);

            play.enabled = false;
            for (int n = 0; n < 20; ++n) { out.clear(); play.render (out); }   // let the voice end
            play.trigger (UiSounds::Event::open);
            out.clear();
            play.render (out);
            expectEquals (out.getMagnitude (0, 512), 0.0f);
        }
    }
};

static UiSoundsTest uiSoundsTest;
