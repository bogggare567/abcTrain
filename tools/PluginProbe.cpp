// PluginProbe - stage 0 of "Свой плагин" (docs/decisions/064, replica/recon.md).
//
// Answers the one question everything else depends on: can abcTrain drive a
// third-party plugin reliably? For every plugin whose name matches the filter
// it loads the plugin as a host would, then writes a JSON report with
//
//   - the parameter list: name, units label, steps, switch/discrete flags and
//     the plugin's own text at 33 knob positions (the "position -> text" table
//     from discussion #44);
//   - latency and tail;
//   - render repeatability: the same input rendered twice, difference in dB;
//   - state restore: save state, scramble every parameter, restore, and
//     compare both the parameter values and a fresh render with the original;
//   - unexpected silence (a demo / unactivated plugin muting itself);
//   - a first sweep per continuous parameter: 9 positions, pink noise, the
//     change in level and in three bands (low / mid / high) and in stereo
//     correlation. That is the raw material for the knob-role rules and the
//     perceptron; it is not a role guess yet.
//
// The plugin under test is a black box: nothing here trusts what it says
// about itself beyond the names; the numbers come from renders.
//
// Usage:
//   PluginProbe [filter] [outDir] [--format VST3|AU] [--positions N]
//   PluginProbe bx_ ~/Desktop/work/abcTrain/_work/out/probe
//
// Plugins are loaded in this process, one at a time. A plugin that crashes
// takes the tool with it: the report written so far stays on disk and the
// crashed plugin is the one named in "probing.txt". Out-of-process scanning
// comes with the in-app host (stage 1).

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdio>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr int renderSeconds = 3;

//==============================================================================
// Test signal: stereo pink noise, fixed seed, so every render sees the same input.
juce::AudioBuffer<float> makePinkNoise (int numSamples, int seed)
{
    juce::AudioBuffer<float> b (2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Random r (seed + ch * 7919);
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0; // Paul Kellet's filter
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float w = r.nextFloat() * 2.0f - 1.0f;
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            d[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.06f; // about -18 dBFS RMS
            b6 = w * 0.115926f;
        }
    }
    return b;
}

// Runs the input through the plugin block by block. The plugin is reset first
// so tails from the previous render do not leak in.
juce::AudioBuffer<float> render (juce::AudioPluginInstance& p, const juce::AudioBuffer<float>& in)
{
    p.reset();
    const int chans = juce::jmax (p.getTotalNumInputChannels(), p.getTotalNumOutputChannels(), 2);
    juce::AudioBuffer<float> out (2, in.getNumSamples());
    juce::AudioBuffer<float> block (chans, blockSize);
    juce::MidiBuffer midi;

    for (int pos = 0; pos < in.getNumSamples(); pos += blockSize)
    {
        const int n = juce::jmin (blockSize, in.getNumSamples() - pos);
        block.clear();
        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom (ch, 0, in, ch, pos, n);
        juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), chans, n);
        p.processBlock (view, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, view, juce::jmin (ch, chans - 1), 0, n);
    }
    return out;
}

double rmsDb (const juce::AudioBuffer<float>& b, int skip)
{
    double sum = 0; long n = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = skip; i < b.getNumSamples(); ++i) { const double v = b.getSample (ch, i); sum += v * v; ++n; }
    return n == 0 ? -200.0 : 10.0 * std::log10 (sum / (double) n + 1e-20);
}

// Level of (a - b) relative to a, in dB: -inf when identical.
double differenceDb (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int skip)
{
    juce::AudioBuffer<float> d (a);
    for (int ch = 0; ch < d.getNumChannels(); ++ch)
        d.addFrom (ch, 0, b, ch, 0, d.getNumSamples(), -1.0f);
    return rmsDb (d, skip) - rmsDb (a, skip);
}

struct Bands { double low = 0, mid = 0, high = 0, correlation = 0; };

// Average power in three bands (< 250 Hz, 250 Hz - 4 kHz, > 4 kHz) over the
// mid signal, plus L/R correlation. Coarse on purpose: stage 0 only needs to
// see which knobs move what.
Bands measure (const juce::AudioBuffer<float>& b, int skip)
{
    constexpr int order = 12, size = 1 << order;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> win (size, juce::dsp::WindowingFunction<float>::hann);
    std::vector<float> data (2 * size);
    double pl = 0, pm = 0, ph = 0; int frames = 0;

    for (int pos = skip; pos + size <= b.getNumSamples(); pos += size / 2, ++frames)
    {
        for (int i = 0; i < size; ++i)
            data[(size_t) i] = 0.5f * (b.getSample (0, pos + i) + b.getSample (1, pos + i));
        std::fill (data.begin() + size, data.end(), 0.0f);
        win.multiplyWithWindowingTable (data.data(), size);
        fft.performFrequencyOnlyForwardTransform (data.data());
        for (int k = 1; k < size / 2; ++k)
        {
            const double f = k * sampleRate / size, p = (double) data[(size_t) k] * data[(size_t) k];
            (f < 250.0 ? pl : f < 4000.0 ? pm : ph) += p;
        }
    }

    double sl = 0, sr = 0, slr = 0;
    for (int i = skip; i < b.getNumSamples(); ++i)
    {
        const double l = b.getSample (0, i), r = b.getSample (1, i);
        sl += l * l; sr += r * r; slr += l * r;
    }

    const auto db = [frames] (double p) { return 10.0 * std::log10 (p / juce::jmax (1, frames) + 1e-20); };
    return { db (pl), db (pm), db (ph), slr / std::sqrt (sl * sr + 1e-20) };
}

juce::var bandsVar (const Bands& x, const Bands& ref)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("lowDb", juce::roundToInt ((x.low - ref.low) * 10) / 10.0);
    o->setProperty ("midDb", juce::roundToInt ((x.mid - ref.mid) * 10) / 10.0);
    o->setProperty ("highDb", juce::roundToInt ((x.high - ref.high) * 10) / 10.0);
    o->setProperty ("correlation", juce::roundToInt (x.correlation * 100) / 100.0);
    return o;
}

std::vector<float> values (juce::AudioPluginInstance& p)
{
    std::vector<float> v;
    for (auto* prm : p.getParameters()) v.push_back (prm->getValue());
    return v;
}

void setValues (juce::AudioPluginInstance& p, const std::vector<float>& v)
{
    auto& prms = p.getParameters();
    for (size_t i = 0; i < v.size() && i < (size_t) prms.size(); ++i)
        prms[(int) i]->setValueNotifyingHost (v[i]);
}

//==============================================================================
juce::var probePlugin (juce::AudioPluginInstance& p, const juce::PluginDescription& d, int positions)
{
    auto* report = new juce::DynamicObject();
    report->setProperty ("name", d.name);
    report->setProperty ("vendor", d.manufacturerName);
    report->setProperty ("format", d.pluginFormatName);
    report->setProperty ("version", d.version);
    report->setProperty ("file", d.fileOrIdentifier);

    p.disableNonMainBuses();   // sidechain inputs off: we feed the main input only
    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
    report->setProperty ("latencySamples", p.getLatencySamples());
    report->setProperty ("tailSeconds", p.getTailLengthSeconds());

    const int skip = (int) sampleRate / 2 + p.getLatencySamples();   // settle time
    const auto input = makePinkNoise ((int) sampleRate * renderSeconds, 1234);

    // 1. Repeatability and silence.
    const auto first = render (p, input), second = render (p, input);
    const double outDb = rmsDb (first, skip), inDb = rmsDb (input, skip);
    report->setProperty ("inputRmsDb", juce::roundToInt (inDb * 10) / 10.0);
    report->setProperty ("outputRmsDb", juce::roundToInt (outDb * 10) / 10.0);
    report->setProperty ("repeatDifferenceDb", juce::roundToInt (juce::jmax (-200.0, differenceDb (first, second, skip)) * 10) / 10.0);
    report->setProperty ("silent", outDb < inDb - 40.0);

    // Silence somewhere in the middle of the render (demo plugins mute in bursts).
    int silentBlocks = 0;
    for (int pos = skip; pos + blockSize <= first.getNumSamples(); pos += blockSize)
        if (first.getRMSLevel (0, pos, blockSize) < 1.0e-5f && input.getRMSLevel (0, pos, blockSize) > 1.0e-3f)
            ++silentBlocks;
    report->setProperty ("silentBlocks", silentBlocks);

    // 2. Parameters and the position -> text table.
    auto& prms = p.getParameters();
    const auto original = values (p);
    juce::Array<juce::var> params;
    for (int i = 0; i < prms.size(); ++i)
    {
        auto* prm = prms[i];
        auto* o = new juce::DynamicObject();
        o->setProperty ("index", i);
        o->setProperty ("name", prm->getName (64));
        o->setProperty ("label", prm->getLabel());
        o->setProperty ("steps", prm->getNumSteps());
        o->setProperty ("discrete", prm->isDiscrete());
        o->setProperty ("boolean", prm->isBoolean());
        o->setProperty ("automatable", prm->isAutomatable());
        o->setProperty ("meta", prm->isMetaParameter());
        o->setProperty ("value", prm->getValue());
        if (auto* hp = dynamic_cast<juce::HostedAudioProcessorParameter*> (prm))
            o->setProperty ("id", hp->getParameterID());
        juce::Array<juce::var> texts;
        for (int k = 0; k < 33; ++k)
            texts.add (prm->getText ((float) k / 32.0f, 64));
        o->setProperty ("texts", texts);
        params.add (o);
    }

    // 3. State restore: save, scramble, restore, compare values and sound.
    juce::MemoryBlock state;
    p.getStateInformation (state);
    juce::Random rnd (99);
    for (auto* prm : prms) prm->setValueNotifyingHost (rnd.nextFloat());
    p.setStateInformation (state.getData(), (int) state.getSize());
    const auto restored = values (p);
    int mismatched = 0;
    juce::StringArray mismatchNames;
    for (size_t i = 0; i < original.size(); ++i)
        if (std::abs (original[i] - restored[i]) > 1.0e-4f)
        {
            ++mismatched;
            mismatchNames.add (prms[(int) i]->getName (32));
        }
    const auto afterRestore = render (p, input);
    auto* restore = new juce::DynamicObject();
    restore->setProperty ("stateBytes", (int) state.getSize());
    restore->setProperty ("parametersRestored", (int) original.size() - mismatched);
    restore->setProperty ("parametersTotal", (int) original.size());
    restore->setProperty ("mismatched", mismatchNames.joinIntoString (", "));
    restore->setProperty ("soundDifferenceDb", juce::roundToInt (juce::jmax (-200.0, differenceDb (first, afterRestore, skip)) * 10) / 10.0);
    report->setProperty ("restore", restore);
    setValues (p, original);   // whatever the plugin did, put the values back by hand too

    // 4. Sweep: every continuous parameter over N positions, others untouched.
    const auto ref = measure (first, skip);
    for (int i = 0; i < prms.size(); ++i)
    {
        auto* prm = prms[i];
        auto* o = params.getReference (i).getDynamicObject();
        if (prm->isBoolean() || prm->isMetaParameter() || ! prm->isAutomatable())
            continue;

        const int n = prm->isDiscrete() && prm->getNumSteps() > 1 ? juce::jmin (positions, prm->getNumSteps()) : positions;
        juce::Array<juce::var> sweep;
        double maxChange = 0.0;
        for (int k = 0; k < n; ++k)
        {
            const float v = n == 1 ? 0.0f : (float) k / (float) (n - 1);
            prm->setValueNotifyingHost (v);
            const auto out = render (p, input);
            const auto m = measure (out, skip);
            auto pt = bandsVar (m, ref);
            pt.getDynamicObject()->setProperty ("position", v);
            pt.getDynamicObject()->setProperty ("text", prm->getText (v, 64));
            pt.getDynamicObject()->setProperty ("levelDb", juce::roundToInt ((rmsDb (out, skip) - outDb) * 10) / 10.0);
            maxChange = juce::jmax (maxChange, std::abs (m.low - ref.low), std::abs (m.mid - ref.mid), std::abs (m.high - ref.high));
            sweep.add (pt);
        }
        prm->setValueNotifyingHost (original[(size_t) i]);
        o->setProperty ("sweep", sweep);
        o->setProperty ("maxBandChangeDb", juce::roundToInt (maxChange * 10) / 10.0);
        o->setProperty ("audibleOnPinkNoise", maxChange > 0.5);   // first cut; the real test is the perceptual scale
    }

    report->setProperty ("parameters", params);
    p.releaseResources();
    return report;
}

juce::String safeFileName (const juce::String& s)
{
    return s.replaceCharacters (" /\\:*?\"<>|", "__________");
}
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::String filter = argc > 1 ? juce::String (argv[1]) : juce::String ("bx_");
    juce::File outDir = argc > 2 ? juce::File (juce::String (argv[2])) : juce::File::getCurrentWorkingDirectory().getChildFile ("probe");
    juce::String onlyFormat = "VST3";
    int positions = 9;
    for (int i = 3; i + 1 < argc; ++i)
    {
        if (juce::String (argv[i]) == "--format") onlyFormat = argv[i + 1];
        if (juce::String (argv[i]) == "--positions") positions = juce::jlimit (2, 33, juce::String (argv[i + 1]).getIntValue());
    }
    if (onlyFormat == "AU") onlyFormat = "AudioUnit";
    outDir.createDirectory();

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    juce::String summary = "# PluginProbe\n\nfilter `" + filter + "`, format " + onlyFormat + ", " + juce::String (positions) + " positions\n\n"
                           "| plugin | params | latency | repeat dB | restore | restore sound dB | silent | load |\n|---|---|---|---|---|---|---|---|\n";
    int found = 0;

    for (auto* format : formats.getFormats())
    {
        if (onlyFormat.isNotEmpty() && format->getName() != onlyFormat)
            continue;

        const auto ids = format->searchPathsForPlugins (format->getDefaultLocationsToSearch(), true, false);
        for (const auto& id : ids)
        {
            const auto shortName = format->getNameOfPluginFromIdentifier (id);
            if (! shortName.containsIgnoreCase (filter))
                continue;

            juce::OwnedArray<juce::PluginDescription> types;
            format->findAllTypesForFile (types, id);
            for (auto* d : types)
            {
                ++found;
                std::printf ("probing %s (%s)\n", d->name.toRawUTF8(), d->pluginFormatName.toRawUTF8());
                std::fflush (stdout);
                outDir.getChildFile ("probing.txt").replaceWithText (d->name + "\n" + d->fileOrIdentifier + "\n");

                juce::String error;
                auto instance = formats.createPluginInstance (*d, sampleRate, blockSize, error);
                if (instance == nullptr)
                {
                    summary << "| " << d->name << " | - | - | - | - | - | - | " << error << " |\n";
                    continue;
                }

                const auto report = probePlugin (*instance, *d, positions);
                outDir.getChildFile (safeFileName (d->name) + "." + d->pluginFormatName + ".json")
                    .replaceWithText (juce::JSON::toString (report, false));

                const auto* r = report.getDynamicObject();
                const auto* rs = r->getProperty ("restore").getDynamicObject();
                summary << "| " << d->name
                        << " | " << r->getProperty ("parameters").size()
                        << " | " << r->getProperty ("latencySamples").toString()
                        << " | " << r->getProperty ("repeatDifferenceDb").toString()
                        << " | " << rs->getProperty ("parametersRestored").toString() << "/" << rs->getProperty ("parametersTotal").toString()
                        << " | " << rs->getProperty ("soundDifferenceDb").toString()
                        << " | " << (r->getProperty ("silent") ? "yes" : (int) r->getProperty ("silentBlocks") > 0 ? "bursts" : "no")
                        << " | ok |\n";
                instance.reset();
            }
        }
    }

    if (found == 0)
        summary << "\nNo plugin matched. Searched: " << formats.getFormat (0)->getDefaultLocationsToSearch().toString() << "\n";

    outDir.getChildFile ("probing.txt").deleteFile();
    outDir.getChildFile ("report.md").replaceWithText (summary);
    std::printf ("%s\n", summary.toRawUTF8());
    return 0;
}
