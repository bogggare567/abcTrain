#include "shared/audio/InstrumentLabel.h"

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace InstrumentLabel
{
    namespace
    {
        struct Entry
        {
            Instrument instrument;
            const char* id;
            const char* folder;
            Family family;
        };

        constexpr std::array<Entry, numInstruments> entries {{
            { Instrument::kick,       "kick",       "Kick",       Family::drums },
            { Instrument::snare,      "snare",      "Snare",      Family::drums },
            { Instrument::hihat,      "hihat",      "Hi-hat",     Family::drums },
            { Instrument::cymbals,    "cymbals",    "Cymbals",    Family::drums },
            { Instrument::toms,       "toms",       "Toms",       Family::drums },
            { Instrument::percussion, "percussion", "Percussion", Family::drums },
            { Instrument::bass,       "bass",       "Bass",       Family::tonal },
            { Instrument::guitar,     "guitar",     "Guitar",     Family::tonal },
            { Instrument::keys,       "keys",       "Keys",       Family::tonal },
            { Instrument::vocal,      "vocal",      "Vocals",     Family::tonal },
            { Instrument::wind,       "wind",       "Wind",       Family::tonal },
            // "Full Mix", not "Full mix": the character sorter used this
            // exact name, and an older import's mixes should land in the
            // same folder as a new one's.
            { Instrument::mix,        "mix",        "Full Mix",   Family::mix },
            { Instrument::other,      "other",      "Other",      Family::other },
        }};

        const Entry& entryOf (Instrument instrument) noexcept
        {
            return entries[(size_t) instrument];
        }

        // ---- names --------------------------------------------------------

        // Lower-case Latin and Cyrillic without the C library: towlower
        // depends on the process locale, and in the default "C" locale on
        // macOS it leaves "БОЧКА" alone.
        juce::juce_wchar lower (juce::juce_wchar c) noexcept
        {
            if (c >= 'A' && c <= 'Z')          return c + 32;
            if (c >= 0x410 && c <= 0x42F)      return c + 0x20;   // А-Я
            if (c == 0x401)                    return 0x451;      // Ё
            return c;
        }

        bool isWordLetter (juce::juce_wchar c) noexcept
        {
            return (c >= 'a' && c <= 'z') || (c >= 0x430 && c <= 0x44F) || c == 0x451;
        }

        bool isDigit (juce::juce_wchar c) noexcept { return c >= '0' && c <= '9'; }

        juce::String lowered (const juce::String& text)
        {
            juce::String out;
            out.preallocateBytes (text.getNumBytesAsUTF8() + 4);

            for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
                out += juce::String::charToString (lower (*p));

            return out;
        }

        // Letters apart from digits: "Tom2" is "tom" and "2", "OH_L" is
        // "oh" and "l" - the same split as prepare_audio.tokens_of.
        juce::StringArray tokensOf (const juce::String& text)
        {
            juce::StringArray tokens;
            juce::String current;
            auto currentIsDigit = false;

            const auto flush = [&]
            {
                if (current.isNotEmpty())
                    tokens.add (current);
                current.clear();
            };

            for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
            {
                const auto c = lower (*p);
                const auto letter = isWordLetter (c), digit = isDigit (c);

                if (! letter && ! digit)
                {
                    flush();
                    continue;
                }

                if (current.isNotEmpty() && digit != currentIsDigit)
                    flush();

                current += juce::String::charToString (c);
                currentIsDigit = digit;
            }

            flush();
            return tokens;
        }

        struct Keywords
        {
            Instrument instrument;
            const char* words;   // space-separated; "*" at the end = prefix
        };

        // Copy of prepare_audio.KEYWORDS - order matters: the first match
        // wins, so drums are tried before "bass" ("bass drum" is a kick).
        const Keywords keywordTable[] = {
            { Instrument::kick,       "kick* kik bd bassdrum 808 кик* бочк* бочка" },
            { Instrument::snare,      "snare* sn snr sd rim малый малого рабочий снейр* снэр*" },
            { Instrument::hihat,      "hh hat hats hihat* хэт* хет хеты" },
            { Instrument::cymbals,    "oh ohl ohr overhead* cym* ride crash тарел* оверхед* райд крэш" },
            { Instrument::toms,       "tom toms ftom rtom htom mtom floortom том томы томов" },
            { Instrument::percussion, "perc* loop loops shaker* tamb* conga* bongo* clap* перк* шейкер* луп лупы хлоп*" },
            { Instrument::bass,       "bass basses sub бас басы басс" },
            { Instrument::guitar,     "gtr* guitar* гитар*" },
            { Instrument::keys,       "keys key piano* pno rhodes organ* synth* syn pad pads omnisphere vital ana serum massive "
                                      "клавиш* пиано фортепиано синт* орган* пэд*" },
            { Instrument::vocal,      "vox vocal* voc voice* bv bvs вокал* голос* бэк*" },
            { Instrument::wind,       "pipe* flute* sax* trumpet* horn* brass whistle* дудк* дудка флейт* сакс* труба трубы свирел*" },
            { Instrument::mix,        "mix mixdown master* микс* мастер* сведение" },
        };

        // Two-word names, checked before splitting into words.
        struct Phrase { const char* joined; Instrument instrument; };

        const Phrase phraseTable[] = {
            { "bassdrum", Instrument::kick },      { "hihat", Instrument::hihat },
            { "хайхэт", Instrument::hihat },       { "хайхет", Instrument::hihat },
            { "floortom", Instrument::toms },      { "drumloop", Instrument::percussion },
            { "fullmix", Instrument::mix },        { "малыйбарабан", Instrument::snare },
            { "басбарабан", Instrument::kick },
        };

        // ---- measurements -------------------------------------------------

        float ramp (float x, float lo, float hi) noexcept
        {
            return juce::jlimit (0.0f, 1.0f, (x - lo) / (hi - lo));
        }

        float medianOf (std::vector<float> values)
        {
            if (values.empty())
                return 0.0f;

            const auto middle = values.begin() + (std::ptrdiff_t) (values.size() / 2);
            std::nth_element (values.begin(), middle, values.end());

            if (values.size() % 2 == 1)
                return *middle;

            const auto upper = *middle;
            const auto lowerHalfMax = *std::max_element (values.begin(), middle);
            return 0.5f * (upper + lowerHalfMax);
        }

        std::vector<float> hann (int size)
        {
            // numpy.hanning: symmetric, zero at both ends.
            std::vector<float> w ((size_t) size);
            for (int i = 0; i < size; ++i)
                w[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (size - 1));
            return w;
        }

        // AudioSliceAnalyzer's summary, the part the character sorter uses:
        // centroid, three bands by magnitude, onsets a second, how much of
        // the time it is loud.
        void appSummary (const std::vector<float>& mono, double rate, Features& f)
        {
            constexpr int order = 10, size = 1 << order, hop = size / 2;

            if ((int) mono.size() < size)
                return;

            juce::dsp::FFT fft (order);
            const auto window = hann (size);
            std::vector<float> buffer ((size_t) size * 2);
            std::vector<float> previous ((size_t) size / 2, 0.0f);
            std::vector<float> flux;

            double total = 0.0, weighted = 0.0, bass = 0.0, mid = 0.0, high = 0.0;

            for (int start = 0; start + size <= (int) mono.size(); start += hop)
            {
                std::fill (buffer.begin(), buffer.end(), 0.0f);
                for (int i = 0; i < size; ++i)
                    buffer[(size_t) i] = mono[(size_t) (start + i)] * window[(size_t) i];

                fft.performFrequencyOnlyForwardTransform (buffer.data(), true);

                auto frameFlux = 0.0f;

                for (int k = 1; k < size / 2; ++k)
                {
                    const auto m = buffer[(size_t) k];
                    const auto hz = (double) k * rate / size;

                    total += m;
                    weighted += m * hz;
                    (hz < 250.0 ? bass : hz < 4000.0 ? mid : high) += m;

                    frameFlux += juce::jmax (0.0f, m - previous[(size_t) k]);
                    previous[(size_t) k] = m;
                }

                flux.push_back (frameFlux);
            }

            if (total <= 0.0)
                return;

            f.centroidHz = (float) (weighted / total);
            f.bassFraction = (float) (bass / total);
            f.midFraction = (float) (mid / total);
            f.highFraction = (float) (high / total);

            const auto threshold = juce::jmax (medianOf (flux) * 2.2f,
                                               *std::max_element (flux.begin(), flux.end()) * 0.15f);
            auto onsets = 0;
            auto armed = true;

            for (auto value : flux)
            {
                if (armed && value > threshold)       { ++onsets; armed = false; }
                else if (value < threshold * 0.6f)    { armed = true; }
            }

            f.onsetsPerSecond = (float) ((double) onsets / ((double) mono.size() / rate));

            const auto frame = juce::jmax (64, (int) (rate * 0.02));
            double sum = 0.0;
            for (auto s : mono)
                sum += (double) s * s;

            const auto overall = std::sqrt (sum / (double) mono.size());
            const auto count = (int) mono.size() / frame;

            if (overall > 1.0e-9 && count > 0)
            {
                auto loud = 0;

                for (int c = 0; c < count; ++c)
                {
                    double e = 0.0;
                    for (int i = 0; i < frame; ++i)
                    {
                        const auto s = mono[(size_t) (c * frame + i)];
                        e += (double) s * s;
                    }
                    if (std::sqrt (e / frame) > overall * 0.35)
                        ++loud;
                }

                f.sustainFraction = (float) loud / (float) count;
            }
        }

        // The finer part: bands by power, noisiness, pitch, crest.
        void timbre (const std::vector<float>& mono, double rate, Features& f)
        {
            constexpr int order = 12, size = 1 << order, hop = size / 2, bins = size / 2 + 1;

            if ((int) mono.size() < size)
                return;

            juce::dsp::FFT fft (order);
            const auto window = hann (size);
            std::vector<float> buffer ((size_t) size * 2);

            std::vector<std::vector<float>> frames;
            std::vector<double> energies;
            double total = 0.0, sub = 0.0, lowMid = 0.0, air = 0.0;

            for (int start = 0; start + size <= (int) mono.size(); start += hop)
            {
                std::fill (buffer.begin(), buffer.end(), 0.0f);
                for (int i = 0; i < size; ++i)
                    buffer[(size_t) i] = mono[(size_t) (start + i)] * window[(size_t) i];

                fft.performRealOnlyForwardTransform (buffer.data(), true);

                std::vector<float> power ((size_t) bins);
                double energy = 0.0;

                for (int k = 0; k < bins; ++k)
                {
                    const auto re = buffer[(size_t) (2 * k)], im = buffer[(size_t) (2 * k + 1)];
                    const auto p = re * re + im * im;
                    power[(size_t) k] = p;
                    energy += p;

                    if (k == 0)
                        continue;

                    const auto hz = (double) k * rate / size;
                    total += p;
                    if (hz > 20.0 && hz < 120.0)          sub += p;
                    else if (hz >= 120.0 && hz < 400.0)   lowMid += p;
                    if (hz >= 6000.0)                     air += p;
                }

                frames.push_back (std::move (power));
                energies.push_back (energy);
            }

            if (total <= 0.0)
                return;

            f.subFraction = (float) (sub / total);
            f.lowMidFraction = (float) (lowMid / total);
            f.airFraction = (float) (air / total);

            const auto loudest = *std::max_element (energies.begin(), energies.end());
            const auto bandLo = (int) std::ceil (60.0 * size / rate);
            const auto bandHi = juce::jmin (bins - 1, (int) std::floor (juce::jmin (16000.0, rate / 2.0) * size / rate));
            const auto lagLo = (int) (rate / 1000.0);
            const auto lagHi = juce::jmin ((int) (rate / 50.0), size / 4);

            std::vector<float> flatness, pitch;

            for (size_t n = 0; n < frames.size(); ++n)
            {
                if (energies[n] <= loudest * 1.0e-4)
                    continue;   // only frames within 40 dB of the loudest

                const auto& power = frames[n];

                double logSum = 0.0, linSum = 0.0;
                for (int k = bandLo; k <= bandHi; ++k)
                {
                    const auto p = (double) power[(size_t) k] + 1.0e-20;
                    logSum += std::log (p);
                    linSum += p;
                }
                const auto count = (double) juce::jmax (1, bandHi - bandLo + 1);
                flatness.push_back ((float) (std::exp (logSum / count) / (linSum / count)));

                // Autocorrelation through the power spectrum: a note with a
                // pitch repeats strongly at its period, noise does not.
                std::fill (buffer.begin(), buffer.end(), 0.0f);
                for (int k = 0; k < bins; ++k)
                    buffer[(size_t) (2 * k)] = power[(size_t) k];

                fft.performRealOnlyInverseTransform (buffer.data());

                const auto zero = buffer[0];
                if (zero <= 1.0e-20f || lagHi <= lagLo)
                    continue;

                auto best = -1.0f;
                for (int lag = lagLo; lag < lagHi; ++lag)
                    best = juce::jmax (best, buffer[(size_t) lag] / zero / (1.0f - (float) lag / (float) size));

                pitch.push_back (best);
            }

            f.flatness = medianOf (flatness);
            f.pitchedness = medianOf (pitch);

            auto peak = 0.0f;
            double sum = 0.0;
            for (auto s : mono)
            {
                peak = juce::jmax (peak, std::abs (s));
                sum += (double) s * s;
            }
            const auto rms = std::sqrt (sum / (double) mono.size());
            f.crestDb = rms > 1.0e-9 && peak > 0.0f ? (float) (20.0 * std::log10 (peak / rms)) : 0.0f;
        }

        using Scores = std::array<float, numInstruments>;

        // prepare_audio.audio_scores. Fuzzy rules, not a model: each is a
        // physical property you can check in the numbers. Melodic
        // instruments are capped below minimumConfidence on purpose - by
        // these measurements alone a guitar, keys and a voice cannot be
        // told apart, only with a name.
        Scores scoresFor (const Features& f)
        {
            const auto percussive = ramp (0.85f - f.sustainFraction, 0.05f, 0.4f) * ramp (f.crestDb, 8.0f, 14.0f);
            const auto broad = juce::jmin (1.0f, f.bassFraction / 0.15f, f.midFraction / 0.25f, f.highFraction / 0.10f);
            const auto melodic = ramp (f.pitchedness, 0.5f, 0.8f) * ramp (f.midFraction, 0.4f, 0.65f)
                               * ramp (f.sustainFraction, 0.3f, 0.6f) * (1.0f - ramp (f.subFraction, 0.3f, 0.5f));
            Scores s {};
            s[(size_t) Instrument::kick] = percussive * ramp (f.subFraction, 0.35f, 0.65f) * (1.0f - ramp (f.airFraction, 0.2f, 0.4f));
            s[(size_t) Instrument::snare] = percussive * ramp (f.midFraction, 0.35f, 0.6f) * ramp (f.flatness, 0.02f, 0.15f)
                                            * (1.0f - ramp (f.subFraction, 0.25f, 0.5f)) * ramp (f.centroidHz, 600.0f, 1500.0f)
                                            * (1.0f - ramp (f.centroidHz, 5000.0f, 9000.0f));
            s[(size_t) Instrument::hihat] = ramp (f.airFraction, 0.45f, 0.75f) * ramp (f.centroidHz, 4000.0f, 7000.0f)
                                            * juce::jmax (percussive, ramp (f.onsetsPerSecond, 3.0f, 6.0f))
                                            * (1.0f - 0.6f * ramp (f.width(), 0.15f, 0.4f));
            s[(size_t) Instrument::cymbals] = ramp (f.highFraction, 0.3f, 0.55f)
                                              * juce::jmax (ramp (f.sustainFraction, 0.45f, 0.8f), ramp (f.width(), 0.1f, 0.35f))
                                              * (1.0f - ramp (f.pitchedness, 0.7f, 0.9f));
            s[(size_t) Instrument::toms] = percussive * ramp (f.lowMidFraction, 0.35f, 0.6f) * (1.0f - ramp (f.subFraction, 0.4f, 0.6f))
                                           * (1.0f - ramp (f.onsetsPerSecond, 3.0f, 5.0f));
            s[(size_t) Instrument::percussion] = percussive * ramp (f.onsetsPerSecond, 2.5f, 4.0f) * broad * 0.8f;
            s[(size_t) Instrument::bass] = ramp (f.bassFraction, 0.5f, 0.75f) * ramp (f.sustainFraction, 0.3f, 0.6f)
                                           * ramp (f.pitchedness, 0.5f, 0.8f);
            s[(size_t) Instrument::mix] = broad * ramp (f.sustainFraction, 0.6f, 0.85f) * (0.5f + 0.5f * ramp (f.width(), 0.03f, 0.2f)) * 0.8f;
            s[(size_t) Instrument::vocal] = melodic * (0.5f + 0.5f * ramp (f.stereoCorrelation, 0.8f, 0.97f)) * 0.55f;
            s[(size_t) Instrument::keys] = melodic * (0.4f + 0.6f * ramp (f.width(), 0.05f, 0.3f)) * 0.55f;
            s[(size_t) Instrument::guitar] = melodic * 0.45f;
            s[(size_t) Instrument::wind] = melodic * ramp (f.pitchedness, 0.85f, 0.97f) * 0.5f;
            s[(size_t) Instrument::other] = 0.0f;
            return s;
        }

        // Pairs that are not a quarrel between name and sound: a kick close
        // mic that sounds like an 808, an overhead that sounds like
        // percussion; and by ear a guitar from keys is not a quarrel either.
        bool compatible (Instrument a, Instrument b)
        {
            if (a == b || a == Instrument::mix || b == Instrument::mix)
                return true;

            const auto fa = familyOf (a), fb = familyOf (b);

            if ((a == Instrument::percussion || b == Instrument::percussion) && fa == fb)
                return true;

            if (fa == Family::tonal && fb == Family::tonal && a != Instrument::bass && b != Instrument::bass)
                return true;

            const auto pair = [&] (Instrument x, Instrument y)
            {
                return (a == x && b == y) || (a == y && b == x);
            };

            return pair (Instrument::cymbals, Instrument::hihat) || pair (Instrument::kick, Instrument::toms)
                || pair (Instrument::snare, Instrument::toms) || pair (Instrument::kick, Instrument::bass);
        }
    }

    Family familyOf (Instrument i) noexcept           { return entryOf (i).family; }
    const char* idOf (Instrument i) noexcept           { return entryOf (i).id; }
    const char* folderNameFor (Instrument i) noexcept  { return entryOf (i).folder; }

    const char* nameKeyFor (Instrument i) noexcept
    {
        static const std::array<juce::String, numInstruments> keys = []
        {
            std::array<juce::String, numInstruments> k;
            for (size_t n = 0; n < entries.size(); ++n)
                k[n] = juce::String ("sounds.instrument.") + entries[n].id;
            return k;
        }();

        return keys[(size_t) i].toRawUTF8();
    }

    std::optional<Instrument> fromId (const juce::String& id)
    {
        for (const auto& e : entries)
            if (id.equalsIgnoreCase (e.id))
                return e.instrument;
        return std::nullopt;
    }

    std::optional<Instrument> fromFolderName (const juce::String& folderName)
    {
        for (const auto& e : entries)
            if (folderName == e.folder)
                return e.instrument;
        return std::nullopt;
    }

    std::optional<NameHit> fromName (const juce::String& fileName)
    {
        const auto text = lowered (fileName);

        // Phrases first, on the name with separators removed.
        const auto joined = text.removeCharacters (" -_.");

        for (const auto& phrase : phraseTable)
            if (joined.contains (juce::String::fromUTF8 (phrase.joined)))
                return NameHit { phrase.instrument, juce::String::fromUTF8 (phrase.joined) };

        const auto tokens = tokensOf (text);

        for (const auto& token : tokens)
            for (const auto& row : keywordTable)
            {
                const auto words = juce::StringArray::fromTokens (juce::String::fromUTF8 (row.words), " ", {});

                for (const auto& word : words)
                {
                    if (word.endsWithChar ('*'))
                    {
                        const auto stem = word.dropLastCharacters (1);
                        if (token.length() >= stem.length() && token.startsWith (stem))
                            return NameHit { row.instrument, token };
                    }
                    else if (token == word)
                    {
                        return NameHit { row.instrument, token };
                    }
                }
            }

        return std::nullopt;
    }

    Features measure (const juce::AudioBuffer<float>& audio, int start, int numSamples, double sampleRate)
    {
        Features f;
        const auto channels = audio.getNumChannels();
        start = juce::jlimit (0, audio.getNumSamples(), start);
        numSamples = juce::jlimit (0, audio.getNumSamples() - start, numSamples);

        if (channels <= 0 || numSamples <= 0 || sampleRate <= 0.0)
            return f;

        std::vector<float> mono ((size_t) numSamples, 0.0f);
        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* data = audio.getReadPointer (ch, start);
            for (int i = 0; i < numSamples; ++i)
                mono[(size_t) i] += data[i] / (float) channels;
        }

        appSummary (mono, sampleRate, f);
        timbre (mono, sampleRate, f);

        if (channels >= 2)
        {
            const auto* l = audio.getReadPointer (0, start);
            const auto* r = audio.getReadPointer (1, start);
            double lr = 0.0, ll = 0.0, rr = 0.0;

            for (int i = 0; i < numSamples; ++i)
            {
                lr += (double) l[i] * r[i];
                ll += (double) l[i] * l[i];
                rr += (double) r[i] * r[i];
            }

            const auto denominator = std::sqrt (ll * rr);
            // Silence is "no data", not "wide".
            f.stereoCorrelation = denominator > 1.0e-9 ? (float) (lr / denominator) : 1.0f;
        }

        return f;
    }

    Features median (const std::vector<Features>& items)
    {
        Features m;

        if (items.empty())
            return m;

        const auto of = [&] (float Features::* field)
        {
            std::vector<float> v;
            for (const auto& f : items)
                v.push_back (f.*field);
            return medianOf (std::move (v));
        };

        m.centroidHz = of (&Features::centroidHz);
        m.onsetsPerSecond = of (&Features::onsetsPerSecond);
        m.sustainFraction = of (&Features::sustainFraction);
        m.bassFraction = of (&Features::bassFraction);
        m.midFraction = of (&Features::midFraction);
        m.highFraction = of (&Features::highFraction);
        m.subFraction = of (&Features::subFraction);
        m.lowMidFraction = of (&Features::lowMidFraction);
        m.airFraction = of (&Features::airFraction);
        m.flatness = of (&Features::flatness);
        m.pitchedness = of (&Features::pitchedness);
        m.crestDb = of (&Features::crestDb);
        m.stereoCorrelation = of (&Features::stereoCorrelation);
        return m;
    }

    Verdict decide (const std::optional<NameHit>& name, const Features& features, bool songShaped)
    {
        const auto scores = scoresFor (features);

        // Best, and how far ahead of the runner-up: "sounds like this and
        // like that" is not confidence.
        auto best = 0, second = -1;
        for (int i = 1; i < numInstruments; ++i)
            if (scores[(size_t) i] > scores[(size_t) best])
                best = i;
        for (int i = 0; i < numInstruments; ++i)
            if (i != best && (second < 0 || scores[(size_t) i] > scores[(size_t) second]))
                second = i;

        const auto top = scores[(size_t) best];
        const auto margin = std::sqrt (juce::jlimit (0.0f, 1.0f, (top - scores[(size_t) second]) / 0.3f));
        const auto confidence = top * margin;
        const auto heard = (Instrument) best;

        Verdict v;
        v.bestGuess = heard;

        const auto heardText = juce::String ("sounds like ") + idOf (heard) + " (" + juce::String (confidence, 2) + ")";

        if (name.has_value())
        {
            const auto named = name->instrument;
            const auto said = "name \"" + name->word + "\" -> " + idOf (named);

            if (named == Instrument::mix)
            {
                v.instrument = Instrument::mix;
                v.confidence = 0.9f;
                v.reason = said;
                return v;
            }

            const auto contradicts = confidence >= 0.7f && scores[(size_t) named] < 0.15f && ! compatible (named, heard);

            if (contradicts)
            {
                v.instrument = Instrument::other;
                v.confidence = confidence;
                v.reason = said + ", but " + heardText + " - they disagree";
                return v;
            }

            v.instrument = named;
            v.confidence = heard == named && confidence >= minimumConfidence
                             ? 1.0f - (1.0f - 0.75f) * (1.0f - confidence)
                             : 0.75f;
            v.reason = said + "; " + heardText;
            return v;
        }

        // A single song-length file whose loudest claim is a drum is still
        // most likely a song: one bright passage is not a hi-hat stem.
        if (confidence >= minimumConfidence && ! (songShaped && familyOf (heard) == Family::drums))
        {
            v.instrument = heard;
            v.confidence = confidence;
            v.reason = heardText;
            return v;
        }

        if (songShaped && (heard == Instrument::mix || features.width() >= 0.15f
                           || scores[(size_t) Instrument::mix] >= 0.2f))
        {
            v.instrument = Instrument::mix;
            v.confidence = 0.7f;
            v.reason = "a song-length file, name says nothing; " + heardText;
            return v;
        }

        v.instrument = Instrument::other;
        v.confidence = confidence;
        v.reason = "name says nothing; " + heardText + " - not sure enough";
        return v;
    }
}
