#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

/**
    The sound of «Свой плагин» (ADR 065): one practice loop through two
    instances of the same third-party plugin, A (the player's setting) and
    B (the round's hidden one), and a crossfade between them.

    Both instances run on every block, whichever one is heard, so
    switching is instant and a reverb's tail or a compressor's release is
    never cut: what changes at the switch is only which output is heard,
    over 25 ms. Both are the same plugin from the same state, so their
    latencies are equal and need no compensation between them.

    Level is never the tell: B is matched to A by a slow RMS follower -
    except for a knob whose job is level, where matching would erase the
    answer (OwnPluginRound::isLevelKnob).

    Threads. The instances and the loop are built on the message thread and
    handed over as one Rig through an atomic pointer; the audio thread never
    allocates, locks or waits. The old rig is freed by the message thread
    only after the audio thread has left it (an "in use" pointer the audio
    thread publishes around each block).
*/
class OwnPluginPlayer
{
public:
    enum class Listen { a = 0, b = 1, dry = 2 };

    struct Rig
    {
        std::unique_ptr<juce::AudioPluginInstance> a, b;
        juce::AudioBuffer<float> loop;   // stereo, at the device rate
    };

    OwnPluginPlayer() = default;
    ~OwnPluginPlayer();

    /** Audio thread is not running yet, or calls this itself in prepareToPlay. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Message thread. Takes the rig (nullptr: silence) and frees the old one
        once the audio thread has let go of it. The instances must already be
        prepared at the rate and block size given to prepare(). */
    void setRig (std::unique_ptr<Rig>);
    Rig* getRig() const noexcept { return rig.load(); }

    void setListen (Listen l) noexcept           { listen.store ((int) l); }
    Listen getListen() const noexcept            { return (Listen) listen.load(); }
    void setLoudnessMatch (bool shouldMatch) noexcept { match.store (shouldMatch); }

    /** Audio thread: fills the buffer (the input is ignored). */
    void process (juce::AudioBuffer<float>&) noexcept;

    /** How different B is from A right now, dB relative to A (before the
        loudness match). Message thread reads it to drop rounds nobody can
        hear (OwnPluginRound::inaudibleBelowDb). Reset when B changes. */
    float getDifferenceDb() const noexcept { return differenceDb.load(); }
    void resetDifference() noexcept { resetDiff.store (true); }

    /** B's raw level over A's, dB (before the match). */
    float getLevelGapDb() const noexcept { return levelGapDb.load(); }

    double getSampleRate() const noexcept { return rate; }
    int getBlockSize() const noexcept     { return block; }

    static constexpr int maxChannels = 8;

private:
    void runChunk (Rig&, juce::AudioBuffer<float>& out, int start, int n) noexcept;

    std::atomic<Rig*> rig { nullptr };
    std::atomic<Rig*> inUse { nullptr };
    std::atomic<int> listen { (int) Listen::a };
    std::atomic<bool> match { true };
    std::atomic<float> differenceDb { 0.0f };
    std::atomic<float> levelGapDb { 0.0f };
    std::atomic<bool> resetDiff { false };
    float meanSqDiff = 1.0e-6f, checkSqA = 1.0e-6f, checkSqB = 1.0e-6f;   // the round check's own short window

    double rate = 48000.0;
    int block = 512;

    juce::AudioBuffer<float> scratchA, scratchB, scratchDry;
    juce::MidiBuffer noMidi;
    int loopPos = 0;

    // Crossfade gains (per output) and the loudness followers.
    float gainA = 1.0f, gainB = 0.0f, gainDry = 0.0f;
    float meanSqA = 1.0e-6f, meanSqB = 1.0e-6f, matchGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE (OwnPluginPlayer)
};
