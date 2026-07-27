#include "LessonAudioBed.h"
#include "PinkNoiseGenerator.h"
#include <cmath>

namespace LessonAudioBed
{
    namespace
    {
        constexpr float twoPi = 6.28318530718f;

        float randomIn (juce::Random& random, float low, float high) noexcept
        {
            return low + (float) random.nextDouble() * (high - low);
        }

        // Adds `value` to both channels at `sample`, if it is in range.
        void add (juce::AudioBuffer<float>& buffer, int sample, float left, float right) noexcept
        {
            if (sample < 0 || sample >= buffer.getNumSamples())
                return;

            buffer.addSample (0, sample, left);

            if (buffer.getNumChannels() > 1)
                buffer.addSample (1, sample, right);
        }

        // A kick is not a file. It is a starting pitch, how fast that pitch
        // falls, how fast the level falls, and how much click sits on the
        // front - and every one of those is drawn per hit, so no two are
        // the same and none can be memorised.
        void renderKick (juce::AudioBuffer<float>& buffer, int startSample,
                         double sampleRate, juce::Random& random, float velocity)
        {
            const auto startHz = randomIn (random, 95.0f, 135.0f);
            const auto endHz = randomIn (random, 42.0f, 55.0f);
            const auto pitchFallSeconds = randomIn (random, 0.03f, 0.075f);
            const auto decaySeconds = randomIn (random, 0.28f, 0.5f);
            const auto clickAmount = randomIn (random, 0.05f, 0.18f);

            const auto numSamples = (int) (decaySeconds * 3.0 * sampleRate);
            auto phase = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto t = (float) i / (float) sampleRate;
                const auto pitch = endHz + (startHz - endHz) * std::exp (-t / pitchFallSeconds);

                phase += twoPi * pitch / (float) sampleRate;

                const auto body = std::sin (phase) * std::exp (-t / decaySeconds);
                const auto click = (random.nextFloat() * 2.0f - 1.0f) * clickAmount
                                       * std::exp (-t / 0.004f);
                const auto value = juce::jlimit (-1.0f, 1.0f, (body + click) * velocity);

                add (buffer, startSample + i, value, value);
            }
        }

        // Noise for the rattle plus a tuned body for the shell. The ratio
        // between them is what separates a snare from a clap, so it moves.
        void renderSnare (juce::AudioBuffer<float>& buffer, int startSample,
                          double sampleRate, juce::Random& random, float velocity)
        {
            const auto bodyHz = randomIn (random, 165.0f, 215.0f);
            const auto decaySeconds = randomIn (random, 0.11f, 0.19f);
            const auto noiseMix = randomIn (random, 0.55f, 0.78f);

            const auto numSamples = (int) (decaySeconds * 4.0 * sampleRate);
            auto phase = 0.0f;
            auto highpassState = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto t = (float) i / (float) sampleRate;
                phase += twoPi * bodyHz / (float) sampleRate;

                const auto raw = random.nextFloat() * 2.0f - 1.0f;
                highpassState += (raw - highpassState) * 0.35f;
                const auto noise = raw - highpassState;

                const auto envelope = std::exp (-t / decaySeconds);
                const auto value = (noise * noiseMix + std::sin (phase) * (1.0f - noiseMix))
                                       * envelope * velocity;

                add (buffer, startSample + i, value, value);
            }
        }

        // Bright, short and slightly off-centre in the image, which is what
        // a hat usually is in a real mix.
        void renderHat (juce::AudioBuffer<float>& buffer, int startSample,
                        double sampleRate, juce::Random& random, float velocity, bool open)
        {
            const auto decaySeconds = open ? randomIn (random, 0.18f, 0.3f)
                                           : randomIn (random, 0.028f, 0.055f);
            const auto pan = randomIn (random, 0.55f, 0.75f);

            const auto numSamples = (int) (decaySeconds * 4.0 * sampleRate);
            auto lowState = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto t = (float) i / (float) sampleRate;
                const auto raw = random.nextFloat() * 2.0f - 1.0f;

                lowState += (raw - lowState) * 0.72f;
                const auto bright = raw - lowState;

                const auto value = bright * std::exp (-t / decaySeconds) * velocity;
                add (buffer, startSample + i, value * (1.0f - pan) * 2.0f, value * pan * 2.0f);
            }
        }

        // A filtered saw. Sustained on purpose: a threshold is only findable
        // on something that holds still long enough to sit against it.
        void renderBassNote (juce::AudioBuffer<float>& buffer, double sampleRate,
                             juce::Random& random)
        {
            const auto hz = randomIn (random, 48.0f, 72.0f);
            const auto cutoff = randomIn (random, 0.06f, 0.13f);

            const auto numSamples = buffer.getNumSamples();
            auto phase = 0.0f;
            auto lowState = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                phase += hz / (float) sampleRate;

                if (phase >= 1.0f)
                    phase -= 1.0f;

                const auto saw = phase * 2.0f - 1.0f;
                lowState += (saw - lowState) * cutoff;

                // A slow swell and fall, so there is something for a
                // compressor to actually respond to over the loop.
                const auto t = (float) i / (float) numSamples;
                const auto shape = 0.55f + 0.45f * std::sin (twoPi * t);

                const auto value = lowState * 0.7f * shape;
                add (buffer, i, value, value);
            }
        }

        // Three detuned voices spread across the image. The only bed where
        // the stereo field is the subject.
        void renderChord (juce::AudioBuffer<float>& buffer, double sampleRate,
                          juce::Random& random)
        {
            const auto root = randomIn (random, 110.0f, 165.0f);
            const float intervals[] = { 1.0f, 1.1892f, 1.4983f }; // root, minor third, fifth
            const float pans[] = { 0.5f, 0.18f, 0.82f };

            const auto numSamples = buffer.getNumSamples();

            for (int voice = 0; voice < 3; ++voice)
            {
                const auto hz = root * intervals[voice] * randomIn (random, 0.997f, 1.003f);
                auto phase = randomIn (random, 0.0f, 1.0f);
                auto lowState = 0.0f;

                for (int i = 0; i < numSamples; ++i)
                {
                    phase += hz / (float) sampleRate;

                    if (phase >= 1.0f)
                        phase -= 1.0f;

                    const auto saw = phase * 2.0f - 1.0f;
                    lowState += (saw - lowState) * 0.22f;

                    const auto value = lowState * 0.24f;
                    add (buffer, i, value * (1.0f - pans[voice]) * 1.4f,
                          value * pans[voice] * 1.4f);
                }
            }
        }

        void renderPinkNoise (juce::AudioBuffer<float>& buffer)
        {
            PinkNoiseGenerator left, right;

            for (int i = 0; i < buffer.getNumSamples(); ++i)
                add (buffer, i, left.nextSample() * 0.35f, right.nextSample() * 0.35f);
        }

        // Every bed leaves here at the same peak, and that is a
        // requirement rather than tidiness. A module's check grades
        // threshold in dB, and "threshold at -18 dB" only means anything
        // against a known input level - a bed that happened to land 4 dB
        // hotter would silently make the same answer wrong. Overlapping
        // hits summing past full scale (a kick and a snare on the same
        // eighth, hats stacking) was how this surfaced.
        constexpr float bedPeak = 0.5f; // -6 dBFS, leaving room to be driven

        void normalise (juce::AudioBuffer<float>& buffer)
        {
            const auto peak = buffer.getMagnitude (0, buffer.getNumSamples());

            if (peak > 0.0001f)
                buffer.applyGain (bedPeak / peak);
        }

        // Fades the first and last few milliseconds so a loop does not click
        // on the seam. Every bed gets this - a bed that ticks once a bar
        // teaches the player to hear the tick.
        void fadeEdges (juce::AudioBuffer<float>& buffer, double sampleRate)
        {
            const auto fadeSamples = juce::jmin (buffer.getNumSamples() / 4,
                                                  (int) (0.008 * sampleRate));

            if (fadeSamples <= 0)
                return;

            buffer.applyGainRamp (0, fadeSamples, 0.0f, 1.0f);
            buffer.applyGainRamp (buffer.getNumSamples() - fadeSamples, fadeSamples, 1.0f, 0.0f);
        }
    }

    double lengthSeconds (TrainingModule::Bed bed) noexcept
    {
        switch (bed)
        {
            case TrainingModule::Bed::drumLoop:  return 4.8;  // two bars at 100 BPM
            case TrainingModule::Bed::bassNote:  return 4.0;
            case TrainingModule::Bed::singleHit: return 2.4;  // hit, then room to hear a tail
            case TrainingModule::Bed::brightHit: return 3.0;
            case TrainingModule::Bed::chord:     return 4.0;
            case TrainingModule::Bed::pinkNoise: return 4.0;
        }

        return 4.0;
    }

    juce::AudioBuffer<float> render (TrainingModule::Bed bed, double sampleRate,
                                     int variationSeed)
    {
        const auto numSamples = juce::jmax (1, (int) (lengthSeconds (bed) * sampleRate));

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        juce::Random random (variationSeed);

        switch (bed)
        {
            case TrainingModule::Bed::drumLoop:
            {
                // 100 BPM, two bars. Kick on 1 and 3 plus the "and" of 3,
                // snare on 2 and 4, eighth-note hats: enough transients to
                // hear an envelope work, spaced enough that each one has
                // somewhere to decay into.
                const auto beat = 60.0 / 100.0;
                const auto step = beat / 2.0;

                for (int bar = 0; bar < 2; ++bar)
                {
                    const auto barStart = bar * beat * 4.0;

                    renderKick (buffer, (int) (barStart * sampleRate), sampleRate, random, 0.9f);
                    renderKick (buffer, (int) ((barStart + beat * 2.0) * sampleRate), sampleRate,
                                 random, 0.82f);
                    renderKick (buffer, (int) ((barStart + beat * 2.5) * sampleRate), sampleRate,
                                 random, 0.55f);

                    renderSnare (buffer, (int) ((barStart + beat) * sampleRate), sampleRate,
                                  random, 0.75f);
                    renderSnare (buffer, (int) ((barStart + beat * 3.0) * sampleRate), sampleRate,
                                  random, 0.78f);

                    for (int eighth = 0; eighth < 8; ++eighth)
                        renderHat (buffer, (int) ((barStart + eighth * step) * sampleRate),
                                    sampleRate, random, eighth % 2 == 0 ? 0.3f : 0.18f,
                                    eighth == 7 && bar == 1);
                }

                break;
            }

            case TrainingModule::Bed::bassNote:
                renderBassNote (buffer, sampleRate, random);
                break;

            case TrainingModule::Bed::singleHit:
                // One hit near the front, then nothing. The silence is the
                // subject: pre-delay and decay live entirely inside it.
                renderKick (buffer, (int) (0.05 * sampleRate), sampleRate, random, 0.9f);
                renderSnare (buffer, (int) (0.05 * sampleRate), sampleRate, random, 0.7f);
                break;

            case TrainingModule::Bed::brightHit:
                // Repeated, so the difference between a bright tail and a
                // dark one has something to repeat against.
                for (int hit = 0; hit < 2; ++hit)
                {
                    const auto at = (int) ((0.05 + hit * 1.5) * sampleRate);
                    renderHat (buffer, at, sampleRate, random, 0.85f, true);
                    renderSnare (buffer, at, sampleRate, random, 0.4f);
                }

                break;

            case TrainingModule::Bed::chord:
                renderChord (buffer, sampleRate, random);
                break;

            case TrainingModule::Bed::pinkNoise:
                renderPinkNoise (buffer);
                break;
        }

        normalise (buffer);
        fadeEdges (buffer, sampleRate);

        return buffer;
    }
}
