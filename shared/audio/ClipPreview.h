#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <vector>

// Plays one clip once, from the Training Sounds page, so a player can hear
// a clip before choosing to train on it.
//
// Until this existed the page listed file names and the only way to find
// out what "Rhodes chord" sounded like was to pin it, open an exercise and
// start a round - three screens to answer "what is this". A sample browser
// in any DAW answers it with one click, and that is the shape the page now
// has (docs/design/approved-2026-09/Sounds.png).
//
// Threading follows PracticeAudioSource: the message thread publishes a
// buffer it owns, the audio thread announces the pointer it is about to
// read (`hazard`) and re-checks it is still published, and the message
// thread frees only buffers that are neither. No allocation, lock or I/O
// on the audio thread (ADR 038).
class ClipPreview
{
public:
    // A clip's shape and length, read once for drawing - peak per bucket,
    // 0..1, mono.
    struct Overview
    {
        std::vector<float> peaks;
        double seconds = 0.0;
    };

    // Message thread. Blocking file I/O: training clips are seconds long,
    // and this reads at most `maxSeconds`.
    static Overview readOverview (juce::AudioFormatManager& formats, const juce::File& file,
                                  int buckets, double maxSeconds = 20.0)
    {
        Overview result;
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0)
            return result;

        const auto length = (int) juce::jmin ((juce::int64) (maxSeconds * reader->sampleRate),
                                              reader->lengthInSamples);
        result.seconds = (double) reader->lengthInSamples / reader->sampleRate;

        if (length <= 0 || buckets <= 0)
            return result;

        juce::AudioBuffer<float> audio ((int) reader->numChannels, length);
        reader->read (&audio, 0, length, 0, true, true);

        result.peaks.assign ((size_t) buckets, 0.0f);
        auto loudest = 0.0f;

        for (int b = 0; b < buckets; ++b)
        {
            const auto start = (int) ((juce::int64) length * b / buckets);
            const auto end = juce::jmax (start + 1, (int) ((juce::int64) length * (b + 1) / buckets));

            auto peak = 0.0f;

            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                peak = juce::jmax (peak, audio.getMagnitude (ch, start, juce::jmin (end, length) - start));

            result.peaks[(size_t) b] = peak;
            loudest = juce::jmax (loudest, peak);
        }

        // Normalised to the clip's own loudest moment: this is a picture of
        // the shape, and every clip is level-matched when it plays anyway.
        if (loudest > 0.0f)
            for (auto& p : result.peaks)
                p /= loudest;

        return result;
    }

    // Message thread: prepare a clip for playback at `sampleRate`, mono.
    static bool readClip (juce::AudioFormatManager& formats, const juce::File& file,
                          double sampleRate, juce::AudioBuffer<float>& out, double maxSeconds = 20.0)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0)
            return false;

        const auto length = (int) juce::jmin ((juce::int64) (maxSeconds * reader->sampleRate),
                                              reader->lengthInSamples);
        if (length <= 0)
            return false;

        juce::AudioBuffer<float> source ((int) reader->numChannels, length);
        reader->read (&source, 0, length, 0, true, true);

        juce::AudioBuffer<float> mono (1, length);
        mono.clear();

        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            mono.addFrom (0, 0, source, ch, 0, length, 1.0f / (float) source.getNumChannels());

        if (sampleRate > 0.0 && ! juce::approximatelyEqual (sampleRate, reader->sampleRate))
        {
            const auto ratio = reader->sampleRate / sampleRate;
            const auto targetLength = juce::jmax (1, (int) ((double) length / ratio));
            out.setSize (1, targetLength);

            juce::LagrangeInterpolator interpolator;
            interpolator.process (ratio, mono.getReadPointer (0), out.getWritePointer (0), targetLength);
        }
        else
        {
            out = std::move (mono);
        }

        return true;
    }

    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }
    double getSampleRate() const noexcept { return sampleRate; }

    // Message thread. Starts `clip` from `startFraction` (0..1) of its
    // length, replacing whatever was playing.
    void play (juce::AudioBuffer<float>&& clip, float startFraction = 0.0f)
    {
        auto* owned = held.add (new juce::AudioBuffer<float> (std::move (clip)));
        const auto length = owned->getNumSamples();
        startAt.store ((int) ((float) length * juce::jlimit (0.0f, 0.999f, startFraction)));
        restart.store (true);
        published.store (owned);
        reclaim();
    }

    void stop() noexcept
    {
        published.store (nullptr);
        position.store (0);
        reclaim();
    }

    // Message thread, polled by the page's timer: frees retired clips and
    // reports whether the current one is still sounding.
    bool isPlaying() noexcept
    {
        reclaim();
        const auto* clip = published.load();
        return clip != nullptr && ! finished.load();
    }

    // 0..1 through the current clip.
    float getProgress() const noexcept
    {
        const auto* clip = published.load();
        if (clip == nullptr || clip->getNumSamples() <= 0)
            return 0.0f;

        return (float) position.load() / (float) clip->getNumSamples();
    }

    // Audio thread. Adds the clip into `buffer` (every channel) and returns
    // true while it is sounding.
    bool render (juce::AudioBuffer<float>& buffer) noexcept
    {
        const juce::AudioBuffer<float>* clip = nullptr;

        for (int attempt = 0; attempt < 4; ++attempt)
        {
            clip = published.load();
            hazard.store (clip);

            if (published.load() == clip)
                break;
        }

        if (published.load() != clip || clip == nullptr || clip->getNumSamples() == 0)
        {
            hazard.store (nullptr);
            return false;
        }

        if (restart.exchange (false))
        {
            position.store (startAt.load());
            finished.store (false);
            gain = 0.0f;
        }

        if (finished.load())
        {
            hazard.store (nullptr);
            return false;
        }

        const auto* source = clip->getReadPointer (0);
        const auto length = clip->getNumSamples();
        const auto fade = juce::jmax (1, (int) (sampleRate * 0.005));
        auto pos = position.load();

        for (int i = 0; i < buffer.getNumSamples() && pos < length; ++i, ++pos)
        {
            gain = juce::jmin (1.0f, gain + 1.0f / (float) fade);
            const auto tail = juce::jmin (1.0f, (float) (length - pos) / (float) fade);
            const auto x = source[pos] * gain * tail;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addSample (ch, i, x);
        }

        position.store (pos);

        if (pos >= length)
            finished.store (true);

        hazard.store (nullptr);
        return true;
    }

private:
    void reclaim() noexcept
    {
        const auto* current = published.load();
        const auto* inUse = hazard.load();

        for (int i = held.size(); --i >= 0;)
        {
            const auto* b = held.getUnchecked (i);

            if (b != current && b != inUse)
                held.remove (i);
        }
    }

    double sampleRate = 44100.0;
    float gain = 0.0f;   // audio thread only

    juce::OwnedArray<juce::AudioBuffer<float>> held;
    std::atomic<const juce::AudioBuffer<float>*> published { nullptr };
    std::atomic<const juce::AudioBuffer<float>*> hazard { nullptr };
    std::atomic<int> position { 0 }, startAt { 0 };
    std::atomic<bool> restart { false }, finished { false };
};
