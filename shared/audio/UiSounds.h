#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <vector>

// The app's own short sounds - an answer landing, a step up, a battle won
// (ADR 054). Bogdan's recordings, several takes per event.
//
// A one-shot heard two hundred times an evening must not turn into a
// dripping tap, so every trigger is a little different and a burst gets
// quieter:
//   - a random take, never the one played last for that event;
//   - a random pitch within +-35 cents and level within +-1.5 dB;
//   - the same event again inside 1.5 s comes 2 dB quieter each time
//     (down to -8 dB), the way a room stops noticing a repeated sound;
//   - the whole layer has its own level and switch in Settings, and sits
//     well under the material: a cue says "noted", it never covers the
//     sound being judged.
//
// Threads: load() and prepare() on the message thread; trigger() from the
// message thread (it only writes into a lock-free FIFO); render() on the
// audio thread - no allocation, no lock (ADR 038).
class UiSounds
{
public:
    enum class Event
    {
        correct,       // an answer inside the band
        wrong,         // outside it
        stepUp,        // the staircase took a step harder
        newRecord,     // the personal best moved
        achievement,   // a toast for something earned
        runEnd,        // a blitz or survival run is over
        battleWon,
        battleLost,
        roundStart,    // a battle round begins
        open,          // a page or window opens (lessons, results)
        numEvents
    };

    static constexpr int numEvents = (int) Event::numEvents;

    // The folder name of each event under assets/ui-sounds/.
    static const char* idOf (Event);

    // The answer cues are one sound each, always the same (Bogdan: a rim
    // on a wrong answer, and either one sound or none on a right one) - a
    // signal you learn, not a texture. No take choice, no pitch or level
    // movement, no burst back-off. The rest keep their variety.
    static bool isFixed (Event e) noexcept { return e == Event::correct || e == Event::wrong; }
    static int eventFromId (const juce::String&);   // -1 when unknown

    UiSounds();

    // Decodes one take (flac/wav/ogg bytes) for an event.
    void addTake (Event, const void* data, size_t size);
    int takesFor (Event e) const noexcept { return (int) takes[(size_t) e].size(); }

    // Resamples every take to the device rate. Message thread, before
    // render() can see the new buffers (call while the audio is stopped
    // or from prepareToPlay).
    void prepare (double sampleRate);

    void trigger (Event);

    // Mixes the playing voices into the buffer. Audio thread.
    void render (juce::AudioBuffer<float>&) noexcept;

    std::atomic<bool> enabled { true };
    std::atomic<float> levelDb { -20.0f };

    // What trigger() decided, exposed for the tests.
    struct Shot { int event = 0, take = 0; float gain = 1.0f, rate = 1.0f; };
    Shot lastShot() const noexcept { return last; }

private:
    struct Take
    {
        juce::AudioBuffer<float> original;
        double originalRate = 44100.0;
        juce::AudioBuffer<float> ready;   // at the device rate, stereo
    };

    std::array<std::vector<Take>, (size_t) Event::numEvents> takes;
    std::array<int, (size_t) Event::numEvents> lastTake {};
    std::array<double, (size_t) Event::numEvents> lastTime {};
    std::array<int, (size_t) Event::numEvents> burst {};
    juce::Random random;
    double deviceRate = 44100.0;
    Shot last;

    juce::AbstractFifo fifo { 32 };
    std::array<Shot, 32> queue {};

    struct Voice { const juce::AudioBuffer<float>* buffer = nullptr; double pos = 0.0; float gain = 0.0f, rate = 1.0f; };
    std::array<Voice, 6> voices {};

    juce::AudioFormatManager formats;
};
