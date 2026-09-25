#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstring>
#include <functional>
#include <vector>

// The built-in training sounds that ship as code, not as files.
//
// Every sound here is synthesized from scratch on first launch (a few tenths
// of a second, once) and cached as a WAV next to the five embedded ones, so
// the library treats them exactly like any other clip. Nothing is sampled
// from anyone: the recipes are the textbook ones a synth programmer would
// use, which is also why they make good training material - each is built to
// put its energy where a real instrument does:
//
//   drums   - kicks with a pitch drop (the "click" and the "boom" are separate
//             bands, the thing an EQ student learns to find), snares as a
//             tuned body plus filtered noise, hats as metallic square
//             clusters, claps as noise bursts in a flam;
//   bass    - a sine sub, a plucked string (Karplus-Strong), a filtered saw;
//   keys    - an FM electric piano, a drawbar organ, an FM bell;
//   pads    - detuned saws through a slow filter, a formant "aah" choir;
//   loops   - whole grooves at common tempos, and a small full mix, because
//             a compressor or a reverb is learnt on music, not on one hit.
//
// Deterministic: a fixed seed per sound, so the same file comes out on every
// machine and a test can pin it. Mono; the exercises that need stereo make
// it themselves.
namespace BuiltInSynth
{
    struct Sound
    {
        const char* category;   // "Built-in Percussive" / "Built-in Sustained" / "Built-in Loops"
        const char* name;       // file name without extension
        std::function<juce::AudioBuffer<float> (double sampleRate)> render;
    };

    // Bumped when a recipe changes, so the cache is rewritten.
    constexpr int version = 2;

    namespace detail
    {
        constexpr double twoPi = 6.283185307179586;

        inline juce::AudioBuffer<float> buffer (double sr, double seconds)
        {
            juce::AudioBuffer<float> b (1, juce::jmax (1, (int) (sr * seconds)));
            b.clear();
            return b;
        }

        // One-pole low-pass, the simplest filter that sounds like one.
        struct OnePole
        {
            float z = 0.0f, a = 0.0f;
            void set (double hz, double sr) { a = (float) std::exp (-twoPi * hz / sr); }
            float lp (float x) { z = x * (1.0f - a) + z * a; return z; }
            float hp (float x) { return x - lp (x); }
        };

        // Chamberlin state-variable filter: resonant low/band/high-pass.
        struct Svf
        {
            float low = 0, band = 0, f = 0.1f, q = 1.0f;
            void set (double hz, double sr, double resonance)
            {
                f = (float) (2.0 * std::sin (juce::MathConstants<double>::pi * juce::jmin (hz, sr * 0.2) / sr));
                q = (float) (1.0 / juce::jmax (0.5, resonance));
            }
            float process (float x, int mode)   // 0 low, 1 band, 2 high
            {
                low += f * band;
                const auto high = x - low - q * band;
                band += f * high;
                return mode == 0 ? low : mode == 1 ? band : high;
            }
        };

        inline float env (double t, double attack, double decay)
        {
            if (t < 0.0) return 0.0f;
            if (t < attack) return (float) (t / attack);
            return (float) std::exp (-(t - attack) / decay);
        }

        inline void addAt (juce::AudioBuffer<float>& dst, const juce::AudioBuffer<float>& src, int at, float gain)
        {
            for (int i = 0; i < src.getNumSamples() && at + i < dst.getNumSamples(); ++i)
                if (at + i >= 0)
                    dst.getWritePointer (0)[at + i] += gain * src.getSample (0, i);
        }

        inline void normalise (juce::AudioBuffer<float>& b, float peak = 0.8f)
        {
            const auto m = b.getMagnitude (0, 0, b.getNumSamples());
            if (m > 1.0e-6f)
                b.applyGain (peak / m);

            // Five milliseconds of fade at both ends: a loop point is a click
            // otherwise, and a click is a transient the ear will hear as part
            // of the sound.
            const auto fade = juce::jmin (b.getNumSamples() / 4, 220);
            b.applyGainRamp (0, fade, 0.0f, 1.0f);
            b.applyGainRamp (b.getNumSamples() - fade, fade, 1.0f, 0.0f);
        }

        // ---- drums ----------------------------------------------------------

        inline juce::AudioBuffer<float> kick (double sr, double startHz, double endHz, double drop, double decay, double click, int seed)
        {
            auto b = buffer (sr, decay * 5.0 + 0.05);
            juce::Random r (seed);
            double phase = 0.0;
            OnePole clickFilter;
            clickFilter.set (3500.0, sr);

            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                const auto hz = endHz + (startHz - endHz) * std::exp (-t / drop);
                phase += twoPi * hz / sr;
                auto v = (float) std::sin (phase) * env (t, 0.0015, decay);
                v += (float) click * clickFilter.lp (r.nextFloat() * 2.0f - 1.0f) * env (t, 0.0005, 0.004);
                b.setSample (0, i, std::tanh (1.6f * v));
            }

            return b;
        }

        inline juce::AudioBuffer<float> snare (double sr, double bodyHz, double noiseDecay, double tone, int seed)
        {
            auto b = buffer (sr, 0.6);
            juce::Random r (seed);
            Svf noiseFilter;
            noiseFilter.set (3800.0, sr, 1.1);

            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                const auto body = std::sin (twoPi * bodyHz * t) + 0.5 * std::sin (twoPi * bodyHz * 1.51 * t);
                const auto n = noiseFilter.process (r.nextFloat() * 2.0f - 1.0f, 1);
                b.setSample (0, i, (float) (tone * body * env (t, 0.001, 0.06)) + 1.6f * n * env (t, 0.001, noiseDecay));
            }

            return b;
        }

        inline juce::AudioBuffer<float> hat (double sr, double decay, int seed)
        {
            auto b = buffer (sr, decay * 6.0 + 0.02);
            juce::Random r (seed);
            Svf hp;
            hp.set (7500.0, sr, 0.9);

            // Six detuned square waves at inharmonic ratios - the cymbal
            // recipe of the analogue drum machines.
            static const double ratios[] { 2.0, 3.0, 4.16, 5.43, 6.79, 8.21 };
            std::vector<double> phase (6, 0.0);

            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                double metal = 0.0;
                for (size_t k = 0; k < 6; ++k)
                {
                    phase[k] += ratios[k] * 317.0 / sr;
                    metal += (std::fmod (phase[k], 1.0) < 0.5 ? 1.0 : -1.0);
                }
                const auto x = (float) (metal / 6.0) * 0.6f + (r.nextFloat() * 2.0f - 1.0f) * 0.4f;
                b.setSample (0, i, hp.process (x, 2) * env (t, 0.0008, decay));
            }

            return b;
        }

        inline juce::AudioBuffer<float> clap (double sr, int seed)
        {
            auto b = buffer (sr, 0.5);
            juce::Random r (seed);
            Svf bp;
            bp.set (1300.0, sr, 1.4);

            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                // Three hands a few milliseconds apart, then the room.
                float e = 0.0f;
                for (auto offset : { 0.0, 0.011, 0.023 })
                    e = juce::jmax (e, env (t - offset, 0.0005, 0.007));
                e = juce::jmax (e, 0.55f * env (t - 0.031, 0.001, 0.09));
                b.setSample (0, i, bp.process (r.nextFloat() * 2.0f - 1.0f, 1) * e * 2.5f);
            }

            return b;
        }

        inline juce::AudioBuffer<float> tom (double sr, double hz, int seed)
        {
            auto b = kick (sr, hz * 1.6, hz, 0.03, 0.18, 0.15, seed);
            return b;
        }

        inline juce::AudioBuffer<float> shaker (double sr, int seed)
        {
            auto b = buffer (sr, 0.18);
            juce::Random r (seed);
            Svf bp;
            bp.set (6500.0, sr, 1.2);

            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                b.setSample (0, i, bp.process (r.nextFloat() * 2.0f - 1.0f, 1) * env (t, 0.02, 0.03));
            }

            return b;
        }

        // ---- tones ----------------------------------------------------------

        inline double midiHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

        inline juce::AudioBuffer<float> sub (double sr, double hz, double seconds)
        {
            auto b = buffer (sr, seconds);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                const auto v = std::sin (twoPi * hz * t) + 0.12 * std::sin (twoPi * 2.0 * hz * t);
                b.setSample (0, i, (float) v * env (t, 0.006, seconds * 0.6));
            }
            return b;
        }

        // Karplus-Strong: a burst of noise in a delay line the length of one
        // period, averaged on every pass - a plucked string, in six lines.
        inline juce::AudioBuffer<float> pluck (double sr, double hz, double seconds, double brightness, int seed)
        {
            auto b = buffer (sr, seconds);
            juce::Random r (seed);
            const auto period = juce::jmax (2, (int) std::round (sr / hz));
            std::vector<float> line ((size_t) period);
            OnePole shape;
            shape.set (juce::jlimit (200.0, 12000.0, brightness), sr);

            for (auto& s : line)
                s = shape.lp (r.nextFloat() * 2.0f - 1.0f);

            // The loop's averaging darkens a high string fast and a low one
            // slowly; a body filter on the output does what the guitar's
            // wood does and takes the fizz off the low strings.
            OnePole body;
            body.set (juce::jlimit (400.0, 9000.0, hz * 14.0), sr);

            size_t pos = 0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto next = (pos + 1) % line.size();
                const auto out = line[pos];
                line[pos] = 0.4985f * (line[pos] + line[next]);
                pos = next;
                b.setSample (0, i, body.lp (out));
            }
            return b;
        }

        inline juce::AudioBuffer<float> sawBass (double sr, double hz, double seconds, double cutoff)
        {
            auto b = buffer (sr, seconds);
            Svf f;
            double ph = 0.0, ph2 = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                ph = std::fmod (ph + hz / sr, 1.0);
                ph2 = std::fmod (ph2 + hz * 1.004 / sr, 1.0);
                f.set (cutoff * (0.35 + 2.5 * env (t, 0.002, 0.12)), sr, 2.2);
                const auto saw = (float) ((2.0 * ph - 1.0) + (2.0 * ph2 - 1.0)) * 0.5f;
                b.setSample (0, i, f.process (saw, 0) * env (t, 0.004, seconds * 0.7));
            }
            return b;
        }

        // Two-operator FM: the modulator's level falling with time is what
        // makes a Rhodes bark and then glow.
        inline juce::AudioBuffer<float> fm (double sr, double hz, double seconds, double ratio, double index, double indexDecay, double decay)
        {
            auto b = buffer (sr, seconds);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                const auto mod = index * std::exp (-t / indexDecay) * std::sin (twoPi * hz * ratio * t);
                b.setSample (0, i, (float) std::sin (twoPi * hz * t + mod) * env (t, 0.002, decay));
            }
            return b;
        }

        inline juce::AudioBuffer<float> chordOf (double sr, const std::vector<double>& notes, double seconds,
                                                 const std::function<juce::AudioBuffer<float> (double hz)>& voice)
        {
            auto b = buffer (sr, seconds);
            for (auto n : notes)
                addAt (b, voice (midiHz (n)), 0, 1.0f / (float) notes.size());
            return b;
        }

        inline juce::AudioBuffer<float> organ (double sr, double hz, double seconds)
        {
            auto b = buffer (sr, seconds);
            // Drawbars 8' 4' 2 2/3' 2': fundamental, octave, twelfth, fifteenth.
            static const double harmonics[] { 1.0, 2.0, 3.0, 4.0 };
            static const double levels[] { 1.0, 0.7, 0.5, 0.35 };
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                double v = 0.0;
                for (int k = 0; k < 4; ++k)
                    v += levels[k] * std::sin (twoPi * hz * harmonics[k] * t);
                const auto trem = 1.0 + 0.06 * std::sin (twoPi * 5.8 * t);
                b.setSample (0, i, (float) (v * trem / 2.5) * juce::jmin (1.0f, (float) (t / 0.01)));
            }
            return b;
        }

        inline juce::AudioBuffer<float> pad (double sr, const std::vector<double>& notes, double seconds, int seed)
        {
            auto b = buffer (sr, seconds);
            juce::Random r (seed);
            Svf f;

            struct Voice { double hz, phase; };
            std::vector<Voice> voices;
            for (auto n : notes)
                for (auto detune : { -0.11, -0.04, 0.03, 0.09 })
                    voices.push_back ({ midiHz (n + detune), r.nextDouble() });

            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                double v = 0.0;
                for (auto& voice : voices)
                {
                    voice.phase = std::fmod (voice.phase + voice.hz / sr, 1.0);
                    v += 2.0 * voice.phase - 1.0;
                }
                f.set (700.0 + 900.0 * (0.5 + 0.5 * std::sin (twoPi * 0.15 * t)), sr, 0.9);
                const auto rise = juce::jmin (1.0, t / 0.8) * juce::jmin (1.0, (seconds - t) / 0.8);
                b.setSample (0, i, f.process ((float) (v / voices.size()), 0) * (float) rise);
            }
            return b;
        }

        // A sung "aah": a glottal pulse through three formant resonators
        // (700, 1220, 2600 Hz) - the vowel lives in the formants, which is
        // exactly what an EQ student is trying to hear.
        inline juce::AudioBuffer<float> choir (double sr, const std::vector<double>& notes, double seconds)
        {
            auto b = buffer (sr, seconds);

            for (auto n : notes)
            {
                Svf f1, f2, f3;
                f1.set (700.0, sr, 6.0);
                f2.set (1220.0, sr, 8.0);
                f3.set (2600.0, sr, 10.0);
                const auto hz = midiHz (n);
                double phase = 0.0;

                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    const auto t = i / sr;
                    const auto vibrato = 1.0 + 0.006 * std::sin (twoPi * 5.2 * t);
                    phase = std::fmod (phase + hz * vibrato / sr, 1.0);
                    const auto pulse = (float) (phase < 0.1 ? std::sin (juce::MathConstants<double>::pi * phase / 0.1) : 0.0);
                    const auto v = f1.process (pulse, 1) + 0.6f * f2.process (pulse, 1) + 0.25f * f3.process (pulse, 1);
                    const auto rise = juce::jmin (1.0, t / 0.35) * juce::jmin (1.0, (seconds - t) / 0.5);
                    b.getWritePointer (0)[i] += v * (float) rise / (float) notes.size();
                }
            }

            return b;
        }

        inline juce::AudioBuffer<float> lead (double sr, double hz, double seconds)
        {
            auto b = buffer (sr, seconds);
            Svf f;
            double ph = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const auto t = i / sr;
                ph = std::fmod (ph + hz * (1.0 + 0.004 * std::sin (twoPi * 5.5 * t)) / sr, 1.0);
                f.set (2400.0, sr, 1.5);
                const auto sq = ph < 0.5 ? 1.0f : -1.0f;
                b.setSample (0, i, f.process (sq, 0) * env (t, 0.01, seconds * 0.8));
            }
            return b;
        }

        // ---- loops ----------------------------------------------------------

        // A step pattern: one character per sixteenth, 'x' a hit, 'o' a
        // softer one, '.' a rest.
        inline void sequence (juce::AudioBuffer<float>& dst, const juce::AudioBuffer<float>& hit, const char* pattern,
                              double bpm, double sr, float gain, int bars, double swing = 0.0)
        {
            const auto steps = (int) std::strlen (pattern);
            const auto stepSeconds = 60.0 / bpm / 4.0;

            for (int bar = 0; bar < bars; ++bar)
                for (int s = 0; s < steps; ++s)
                {
                    const auto c = pattern[s];
                    if (c == '.')
                        continue;
                    const auto t = (bar * steps + s) * stepSeconds + ((s % 2) == 1 ? swing * stepSeconds : 0.0);
                    addAt (dst, hit, (int) (t * sr), c == 'x' ? gain : gain * 0.55f);
                }
        }

        inline juce::AudioBuffer<float> loopBuffer (double sr, double bpm, int bars)
        {
            return buffer (sr, bars * 4.0 * 60.0 / bpm);
        }
    }

    inline std::vector<Sound> all()
    {
        using namespace detail;
        std::vector<Sound> s;
        const auto P = "Built-in Percussive";
        const auto S = "Built-in Sustained";
        const auto L = "Built-in Loops";

        const auto repeat = [] (const juce::AudioBuffer<float>& hit, double sr, double bpm, int beats)
        {
            auto b = buffer (sr, beats * 60.0 / bpm);
            for (int i = 0; i < beats; ++i)
                addAt (b, hit, (int) (i * 60.0 / bpm * sr), 1.0f);
            return b;
        };

        // Drums, each repeated on the beat so a compressor has something to
        // grab and release, and a reverb has gaps to ring into.
        s.push_back ({ P, "Kick 808",   [=] (double sr) { auto b = repeat (kick (sr, 120, 48, 0.06, 0.35, 0.2, 11), sr, 96, 8); normalise (b); return b; } });
        s.push_back ({ P, "Kick tight", [=] (double sr) { auto b = repeat (kick (sr, 180, 62, 0.02, 0.09, 0.8, 12), sr, 120, 8); normalise (b); return b; } });
        s.push_back ({ P, "Snare fat",  [=] (double sr) { auto b = repeat (snare (sr, 185, 0.16, 0.9, 13), sr, 90, 8); normalise (b); return b; } });
        s.push_back ({ P, "Snare crisp",[=] (double sr) { auto b = repeat (snare (sr, 240, 0.08, 0.5, 14), sr, 120, 8); normalise (b); return b; } });
        s.push_back ({ P, "Clap",       [=] (double sr) { auto b = repeat (clap (sr, 15), sr, 100, 8); normalise (b); return b; } });
        s.push_back ({ P, "Hat closed", [=] (double sr) { auto b = repeat (hat (sr, 0.035, 16), sr, 240, 16); normalise (b); return b; } });
        s.push_back ({ P, "Hat open",   [=] (double sr) { auto b = repeat (hat (sr, 0.22, 17), sr, 120, 8); normalise (b); return b; } });
        s.push_back ({ P, "Toms",       [=] (double sr)
        {
            auto b = buffer (sr, 4.0);
            const double hz[] { 180, 140, 110, 82 };
            for (int i = 0; i < 8; ++i)
                addAt (b, tom (sr, hz[i % 4], 18 + i), (int) (i * 0.5 * sr), 1.0f);
            normalise (b);
            return b;
        } });
        s.push_back ({ P, "Shaker",     [=] (double sr) { auto b = repeat (shaker (sr, 19), sr, 480, 32); normalise (b); return b; } });

        // Bass and tones: a four-note phrase, so frequency work has a moving
        // fundamental rather than one frozen note.
        const auto phrase = [] (double sr, double bpm, const std::vector<double>& notes,
                                const std::function<juce::AudioBuffer<float> (double hz)>& voice)
        {
            auto b = buffer (sr, notes.size() * 2.0 * 60.0 / bpm);
            for (size_t i = 0; i < notes.size(); ++i)
                addAt (b, voice (midiHz (notes[i])), (int) (i * 2.0 * 60.0 / bpm * sr), 1.0f);
            return b;
        };

        s.push_back ({ S, "Sub bass",   [=] (double sr) { auto b = phrase (sr, 90, { 33, 33, 36, 31 }, [sr] (double hz) { return sub (sr, hz, 1.2); }); normalise (b); return b; } });
        s.push_back ({ S, "Pick bass",  [=] (double sr) { auto b = phrase (sr, 110, { 40, 43, 45, 38, 40, 43, 47, 45 }, [sr] (double hz) { return pluck (sr, hz, 1.0, 3500, 21); }); normalise (b); return b; } });
        s.push_back ({ S, "Saw bass",   [=] (double sr) { auto b = phrase (sr, 124, { 36, 36, 39, 34, 36, 41, 39, 34 }, [sr] (double hz) { return sawBass (sr, hz, 0.9, 900); }); normalise (b); return b; } });
        s.push_back ({ S, "E-piano",    [=] (double sr)
        {
            const std::vector<std::vector<double>> chords { { 57, 60, 64, 67 }, { 53, 57, 60, 64 }, { 55, 59, 62, 65 }, { 52, 55, 59, 62 } };
            auto b = buffer (sr, 8.0);
            for (size_t c = 0; c < chords.size(); ++c)
                addAt (b, chordOf (sr, chords[c], 2.0, [sr] (double hz) { return fm (sr, hz, 2.0, 1.0, 2.2, 0.25, 1.1); }), (int) (c * 2.0 * sr), 1.0f);
            normalise (b);
            return b;
        } });
        s.push_back ({ S, "Organ",      [=] (double sr) { auto b = chordOf (sr, { 48, 55, 60, 64, 67 }, 6.0, [sr] (double hz) { return organ (sr, hz, 6.0); }); normalise (b); return b; } });
        s.push_back ({ S, "Strings pad",[=] (double sr) { auto b = pad (sr, { 45, 52, 57, 60, 64 }, 8.0, 22); normalise (b); return b; } });
        s.push_back ({ S, "Choir aah",  [=] (double sr) { auto b = choir (sr, { 57, 60, 64 }, 6.0); normalise (b); return b; } });
        s.push_back ({ S, "Guitar",     [=] (double sr)
        {
            // A strummed open chord: six strings a few milliseconds apart.
            auto b = buffer (sr, 6.0);
            const double notes[] { 40, 47, 52, 56, 59, 64 };
            for (int strum = 0; strum < 3; ++strum)
                for (int k = 0; k < 6; ++k)
                    addAt (b, pluck (sr, midiHz (notes[k]), 2.0, 6000, 30 + k + strum * 6), (int) ((strum * 2.0 + k * 0.012) * sr), 1.0f);
            normalise (b);
            return b;
        } });
        s.push_back ({ S, "Bell",       [=] (double sr) { auto b = phrase (sr, 80, { 72, 76, 79, 84 }, [sr] (double hz) { return fm (sr, hz, 1.4, 3.5, 3.0, 0.6, 0.9); }); normalise (b); return b; } });
        s.push_back ({ S, "Lead",       [=] (double sr) { auto b = phrase (sr, 120, { 69, 72, 76, 74, 72, 69, 67, 69 }, [sr] (double hz) { return lead (sr, hz, 0.9); }); normalise (b); return b; } });

        // Loops: two bars each, cut exactly on the bar so they loop without a seam.
        s.push_back ({ L, "Beat 90 hip-hop", [=] (double sr)
        {
            const double bpm = 90;
            auto b = loopBuffer (sr, bpm, 2);
            sequence (b, kick (sr, 110, 50, 0.05, 0.3, 0.3, 40), "x.....x...x.....", bpm, sr, 1.0f, 2);
            sequence (b, snare (sr, 190, 0.15, 0.8, 41), "....x.......x...", bpm, sr, 0.8f, 2);
            sequence (b, hat (sr, 0.04, 42), "x.x.x.x.x.x.x.xo", bpm, sr, 0.35f, 2, 0.12);
            normalise (b);
            return b;
        } });
        s.push_back ({ L, "Beat 120 house", [=] (double sr)
        {
            const double bpm = 124;
            auto b = loopBuffer (sr, bpm, 2);
            sequence (b, kick (sr, 170, 55, 0.025, 0.14, 0.6, 43), "x...x...x...x...", bpm, sr, 1.0f, 2);
            sequence (b, clap (sr, 44), "....x.......x...", bpm, sr, 0.6f, 2);
            sequence (b, hat (sr, 0.16, 45), "..x...x...x...x.", bpm, sr, 0.35f, 2);
            sequence (b, hat (sr, 0.03, 46), "x.x.x.x.x.x.x.x.", bpm, sr, 0.18f, 2);
            normalise (b);
            return b;
        } });
        s.push_back ({ L, "Beat 140 break", [=] (double sr)
        {
            const double bpm = 140;
            auto b = loopBuffer (sr, bpm, 2);
            sequence (b, kick (sr, 150, 52, 0.03, 0.18, 0.5, 47), "x.........x.....", bpm, sr, 1.0f, 2);
            sequence (b, snare (sr, 230, 0.1, 0.6, 48), "....x..o.o..x...", bpm, sr, 0.85f, 2);
            sequence (b, hat (sr, 0.03, 49), "xoxoxoxoxoxoxoxo", bpm, sr, 0.3f, 2);
            normalise (b);
            return b;
        } });
        s.push_back ({ L, "Groove 100 bass", [=] (double sr)
        {
            const double bpm = 100;
            auto b = loopBuffer (sr, bpm, 2);
            sequence (b, kick (sr, 130, 50, 0.04, 0.25, 0.4, 50), "x.....x.x.......", bpm, sr, 0.9f, 2);
            sequence (b, snare (sr, 200, 0.12, 0.7, 51), "....x.......x...", bpm, sr, 0.7f, 2);
            sequence (b, hat (sr, 0.04, 52), "x.x.x.x.x.x.x.x.", bpm, sr, 0.25f, 2, 0.1);
            const auto step = 60.0 / bpm / 4.0;
            const double line[] { 40, 0, 40, 43, 0, 45, 0, 43, 40, 0, 38, 0, 40, 43, 45, 47 };
            for (int bar = 0; bar < 2; ++bar)
                for (int i = 0; i < 16; ++i)
                    if (line[i] > 0)
                        addAt (b, pluck (sr, midiHz (line[i]), 0.35, 2500, 60 + i), (int) ((bar * 16 + i) * step * sr), 0.7f);
            normalise (b);
            return b;
        } });
        s.push_back ({ L, "Chord stabs 110", [=] (double sr)
        {
            const double bpm = 110;
            auto b = loopBuffer (sr, bpm, 2);
            const auto stab = [sr] (const std::vector<double>& notes)
            {
                return chordOf (sr, notes, 0.4, [sr] (double hz) { return sawBass (sr, hz, 0.4, 2600); });
            };
            const auto step = 60.0 / bpm / 4.0;
            const int hits[] { 0, 3, 6, 10, 16, 19, 22, 26 };
            for (int k = 0; k < 8; ++k)
                addAt (b, stab (k < 4 ? std::vector<double> { 57, 60, 64 } : std::vector<double> { 55, 59, 62 }), (int) (hits[k] * step * sr), 1.0f);
            sequence (b, kick (sr, 150, 52, 0.03, 0.15, 0.5, 70), "x...x...x...x...", bpm, sr, 0.8f, 2);
            normalise (b);
            return b;
        } });
        s.push_back ({ L, "Mini mix 120", [=] (double sr)
        {
            // Everything at once, at sensible levels: the one to learn a bus
            // compressor, a master EQ or a send reverb on.
            const double bpm = 120;
            auto b = loopBuffer (sr, bpm, 2);
            sequence (b, kick (sr, 150, 50, 0.03, 0.2, 0.5, 80), "x.......x.x.....", bpm, sr, 1.0f, 2);
            sequence (b, snare (sr, 200, 0.14, 0.7, 81), "....x.......x...", bpm, sr, 0.75f, 2);
            sequence (b, hat (sr, 0.035, 82), "x.xox.xox.xox.xo", bpm, sr, 0.28f, 2);
            const auto step = 60.0 / bpm / 4.0;
            const double bassLine[] { 36, 36, 43, 36, 34, 34, 41, 34 };
            for (int i = 0; i < 8; ++i)
                addAt (b, sawBass (sr, midiHz (bassLine[i]), step * 3.5, 700), (int) (i * 4 * step * sr), 0.55f);
            addAt (b, pad (sr, { 60, 63, 67 }, 2.0, 83), 0, 0.35f);
            addAt (b, pad (sr, { 58, 62, 65 }, 2.0, 84), (int) (16 * step * sr), 0.35f);
            normalise (b);
            return b;
        } });

        return s;
    }
}
