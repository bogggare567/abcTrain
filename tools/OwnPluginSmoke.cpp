// OwnPluginSmoke - «Свой плагин» end to end without a window (ADR 065).
//
// Loads a real plugin twice, builds the same rig the Studio page builds,
// makes rounds with OwnPluginRound and runs OwnPluginPlayer as the audio
// thread would. For each round it checks what the page promises:
//
//   - A (the player's setting) is not changed by the round;
//   - B differs from A by the round's knob - audibly, unless the knob is
//     one the plugin does not apply to this sound;
//   - with loudness matching on, B's level ends up close to A's;
//   - switching A -> B -> A is click-free (no sample jump larger than the
//     signal itself makes);
//   - the player's setting is restored exactly at the end.
//
// Usage: OwnPluginSmoke <plugin path or name filter> [rounds]
//   OwnPluginSmoke bx_glue 12
//   OwnPluginSmoke build/LearnerEQ_artefacts/Release/VST3/ABC\ Learner\ EQ.vst3

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/OwnPlugins/OwnPluginPlayer.h"
#include "../Source/OwnPlugins/OwnPluginRound.h"
#include <cstdio>

namespace
{
constexpr double rate = 48000.0;
constexpr int block = 512;

juce::AudioBuffer<float> loop()
{
    // Drums-like test loop: pink-ish noise bursts and a low sine, 2 s.
    juce::AudioBuffer<float> b (2, (int) rate * 2);
    juce::Random r (5);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const auto beat = std::fmod ((double) i / rate, 0.5);
        const auto env = (float) std::exp (-beat * 18.0);
        const auto s = 0.25f * env * (r.nextFloat() * 2.0f - 1.0f)
                     + 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 70.0 * i / rate) * (float) std::exp (-beat * 6.0);
        b.setSample (0, i, s);
        b.setSample (1, i, s);
    }
    return b;
}

float rmsDb (const juce::AudioBuffer<float>& b)
{
    return juce::Decibels::gainToDecibels (juce::jmax (b.getRMSLevel (0, 0, b.getNumSamples()), 1.0e-9f));
}

juce::AudioBuffer<float> run (OwnPluginPlayer& p, OwnPluginPlayer::Listen l, int samples)
{
    p.setListen (l);
    juce::AudioBuffer<float> out (2, samples);
    for (int pos = 0; pos < samples; pos += block)
    {
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, juce::jmin (block, samples - pos));
        p.process (view);
    }
    return out;
}

float maxJump (const juce::AudioBuffer<float>& b)
{
    float m = 0.0f;
    for (int i = 1; i < b.getNumSamples(); ++i)
        m = juce::jmax (m, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
    return m;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf ("usage: OwnPluginSmoke <path or name> [rounds]\n");
        return 2;
    }

    const juce::String target (argv[1]);
    const int rounds = argc > 2 ? juce::String (argv[2]).getIntValue() : 8;
    // --format VST3 | AudioUnit: which one when a plugin comes in both
    // (bx_glue does). Default VST3, then AU.
    juce::String onlyFormat;
    for (int i = 3; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--format")
            onlyFormat = juce::String (argv[i + 1]) == "AU" ? juce::String ("AudioUnit") : juce::String (argv[i + 1]);
    const auto step = [] (const char* what) { std::printf ("  .. %s\n", what); std::fflush (stdout); };

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    std::unique_ptr<juce::PluginDescription> desc;
    juce::Array<juce::AudioPluginFormat*> order;
    for (auto* f : formats.getFormats())
        if (onlyFormat.isEmpty() || f->getName() == onlyFormat)
            order.add (f);
    std::stable_sort (order.begin(), order.end(), [] (juce::AudioPluginFormat* x, juce::AudioPluginFormat* y)
                      { return x->getName() == "VST3" && y->getName() != "VST3"; });

    step ("scanning");
    for (auto* f : order)
    {
        juce::StringArray ids;
        if (juce::File::isAbsolutePath (target) || juce::File::getCurrentWorkingDirectory().getChildFile (target).exists())
            ids.add (juce::File::getCurrentWorkingDirectory().getChildFile (target).getFullPathName());
        else
            for (const auto& id : f->searchPathsForPlugins (f->getDefaultLocationsToSearch(), true, false))
                if (f->getNameOfPluginFromIdentifier (id).containsIgnoreCase (target))
                    ids.add (id);

        for (const auto& id : ids)
        {
            juce::OwnedArray<juce::PluginDescription> types;
            f->findAllTypesForFile (types, id);
            if (! types.isEmpty() && desc == nullptr)
                desc = std::make_unique<juce::PluginDescription> (*types[0]);
        }
    }

    if (desc == nullptr)
    {
        std::printf ("FAIL: no plugin matches %s\n", target.toRawUTF8());
        return 1;
    }

    std::printf ("plugin: %s (%s)\n", desc->name.toRawUTF8(), desc->pluginFormatName.toRawUTF8());

    std::fflush (stdout);
    OwnPluginPlayer player;
    player.prepare (rate, block);

    juce::String error;
    auto rig = std::make_unique<OwnPluginPlayer::Rig>();
    step ("loading instance A");
    rig->a = formats.createPluginInstance (*desc, rate, block, error);
    step ("loading instance B");
    rig->b = formats.createPluginInstance (*desc, rate, block, error);
    step ("preparing");
    if (rig->a == nullptr || rig->b == nullptr)
    {
        std::printf ("FAIL: load: %s\n", error.toRawUTF8());
        return 1;
    }
    for (auto* i : { rig->a.get(), rig->b.get() })
    {
        i->disableNonMainBuses();
        i->setRateAndBufferSizeDetails (rate, block);
        i->prepareToPlay (rate, block);
    }
    rig->loop = loop();

    auto* a = rig->a.get();
    auto* b = rig->b.get();
    std::vector<float> before;
    for (auto* p : a->getParameters()) before.push_back (p->getValue());

    std::vector<OwnPluginRound::Knob> knobs;
    for (int i = 0; i < a->getParameters().size(); ++i)
    {
        auto* p = a->getParameters()[i];
        OwnPluginRound::Knob k;
        k.index = i; k.name = p->getName (64); k.label = p->getLabel(); k.steps = p->getNumSteps();
        k.discrete = p->isDiscrete(); k.boolean = p->isBoolean(); k.meta = p->isMetaParameter(); k.automatable = p->isAutomatable();
        knobs.push_back (k);
    }

    step ("playing");
    player.setRig (std::move (rig));
    juce::Random random (11);
    int failures = 0, audible = 0, made = 0, skippedCount = 0;
    std::vector<int> silent;

    for (int n = 0, tries = 0; n < rounds && tries < rounds * 6; ++tries)
    {
        juce::MemoryBlock state;
        a->getStateInformation (state);
        b->setStateInformation (state.getData(), (int) state.getSize());

        const auto& pa = a->getParameters();
        auto r = OwnPluginRound::make (knobs, before, 3, random, [&pa] (int k, float v) { return pa[k]->getText (v, 32).trim(); }, silent);
        if (r.knob < 0)
        {
            if (made == 0 && skippedCount == 0)
                for (const auto& k : knobs)
                    if (k.index < 40)
                        std::printf ("   knob %2d '%s' label '%s' steps %d%s%s%s -> %s\n", k.index, k.name.toRawUTF8(), k.label.toRawUTF8(), k.steps,
                                     k.boolean ? " bool" : "", k.meta ? " meta" : "", k.automatable ? "" : " not-automatable",
                                     OwnPluginRound::isTrainable (k) ? "trainable" : "excluded");
            std::printf ("%s: no audible knob left after %d skipped\n", made > 0 ? "note" : "FAIL", skippedCount);
            if (made == 0)
                return 1;
            break;
        }

        b->getParameters()[r.knob]->setValueNotifyingHost (r.options[(size_t) r.correct]);
        bool level = false;
        for (const auto& k : knobs) if (k.index == r.knob) level = OwnPluginRound::isLevelKnob (k);
        player.setLoudnessMatch (! level);

        // The page's check, in two stages, as on the page: let the change settle (a state copy
        // resets filters, which differs for a moment), then measure.
        run (player, OwnPluginPlayer::Listen::a, (int) (0.4 * rate));
        player.resetDifference();
        run (player, OwnPluginPlayer::Listen::a, (int) (0.6 * rate));
        if (! level && std::abs (player.getLevelGapDb()) > OwnPluginRound::unmatchableAboveDb)
        {
            std::printf ("skip:    %-24s B=%-10s level %+.1f dB - more than the match can take back\n", r.knobName.toRawUTF8(),
                         r.optionTexts[r.correct].toRawUTF8(), player.getLevelGapDb());
            silent.push_back (r.knob);
            ++skippedCount;
            continue;
        }

        if (player.getDifferenceDb() < OwnPluginRound::inaudibleBelowDb)
        {
            std::printf ("skip:    %-24s B=%-10s %.1f dB - same as A with this setting\n", r.knobName.toRawUTF8(),
                         r.optionTexts[r.correct].toRawUTF8(), player.getDifferenceDb());
            silent.push_back (r.knob);
            ++skippedCount;
            continue;
        }
        ++made;
        ++n;

        // Settle (the loudness follower spans the whole 2 s loop), then
        // measure each side over exactly one loop.
        run (player, OwnPluginPlayer::Listen::a, (int) (2.5 * rate));
        const auto outA = run (player, OwnPluginPlayer::Listen::a, (int) (2.0 * rate));
        run (player, OwnPluginPlayer::Listen::b, (int) (12.0 * rate));
        const auto outB = run (player, OwnPluginPlayer::Listen::b, (int) (2.0 * rate));

        juce::AudioBuffer<float> diff (outA);
        for (int ch = 0; ch < 2; ++ch) diff.addFrom (ch, 0, outB, ch, 0, diff.getNumSamples(), -1.0f);
        const auto diffDb = rmsDb (diff) - rmsDb (outA);
        const auto levelGap = std::abs (rmsDb (outA) - rmsDb (outB));
        const auto heard = diffDb > -40.0f;
        audible += heard ? 1 : 0;

        // A switch in the middle of a buffer: the jump must stay of the
        // signal's own size.
        const auto sw1 = run (player, OwnPluginPlayer::Listen::a, block * 4);
        const auto sw2 = run (player, OwnPluginPlayer::Listen::b, block * 4);
        const auto jump = juce::jmax (maxJump (sw1), maxJump (sw2));
        const auto ownJump = juce::jmax (maxJump (outA), maxJump (outB));
        const auto clicky = jump > ownJump * 1.5f + 1.0e-3f;

        const auto matched = level || ! heard || levelGap < 1.5f;
        std::printf ("round %d: %-24s B=%-10s diff %6.1f dB  level gap %4.1f dB%s%s%s\n", n, r.knobName.toRawUTF8(),
                     r.optionTexts[r.correct].toRawUTF8(), diffDb, levelGap, level ? " (level knob, no match)" : "",
                     matched ? "" : "  LEVEL TELL", clicky ? "  CLICK" : "");
        failures += (matched ? 0 : 1) + (clicky ? 1 : 0);
    }

    // A was never touched.
    int moved = 0;
    for (int i = 0; i < a->getParameters().size(); ++i)
        if (std::abs (a->getParameters()[i]->getValue() - before[(size_t) i]) > 1.0e-5f)
            ++moved;
    if (moved > 0)
    {
        std::printf ("FAIL: %d of A's knobs moved during the rounds\n", moved);
        ++failures;
    }

    player.setRig (nullptr);
    std::printf ("%s: %d rounds, %d audible, %d knobs skipped as inaudible, %d problems\n",
                 failures == 0 ? "OK" : "FAIL", made, audible, skippedCount, failures);
    return failures == 0 ? 0 : 1;
}
