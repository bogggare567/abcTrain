#include "OwnPluginsPanel.h"
#include "KnobGlossary.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/audio/BuiltInSynth.h"
#include <algorithm>
#include <map>

namespace
{
    constexpr int listWidth = 300;
    constexpr int rowHeight = 40;
    constexpr int bandHeight = 250;   // the steps' band under the plugin's window: one height for both

    using LnF = AbcTrainLookAndFeel;
}

//==============================================================================
void OwnPluginsPanel::LockCover::paint (juce::Graphics& g)
{
    // Opaque: a plugin's window is a native view and nothing JUCE draws can
    // sit on top of it, so during a round the window is hidden and this
    // stands in its place. The page's own colour, not a black well.
    const auto& theme = AbcTrainTheme::current();
    g.fillAll (theme.panelBackground);
    g.setColour (theme.outline);
    g.drawRect (getLocalBounds());
    g.setColour (theme.text);
    g.setFont (LnF::bodyFont());
    LnF::fitText (g, message, getLocalBounds().reduced (24), juce::Justification::centred, true);
}

//==============================================================================
juce::String OwnPluginsPanel::Entry::formatsText() const
{
    juce::StringArray f;
    for (const auto& v : variants)
        f.addIfNotAlreadyThere (v.format);
    return f.joinIntoString (" ");
}

OwnPluginsPanel::Category OwnPluginsPanel::categoryFromName (const juce::String& name)
{
    // By the words plugins are named with. A guess, but a good one: a
    // plugin is named for what it does far more often than not, and the
    // big vendors' product lines are listed by their own names.
    const auto n = " " + name.toLowerCase().replaceCharacters ("_-.", "   ") + " ";
    const auto any = [&n] (std::initializer_list<const char*> words)
    {
        for (const auto* w : words)
            if (n.contains (w))
                return true;
        return false;
    };

    if (any ({ "synth", "piano", "organ", "musicdevice", "sampler", "drum machine", "kontakt", "serum", "vital" }))
        return instrument;
    if (any ({ " eq", "eq ", "pro q", "equal", "filter", "gullfoss", "soothe", "curve", "tilt", "pultec", " q4", " q3" }))
        return eq;
    if (any ({ "comp", "pro c", "limit", "pro l", "glue", "clip", "gate", "expander", "de ess", "deess", "pro ds",
               "pro mb", "multiband", "transient", "maxim", "leveler", "rider", "pro g", "dynamic", "1176", "la2a", "la 2a" }))
        return dynamics;
    if (any ({ "verb", "room", "hall", "plate", "pro r", "delay", "echo", "timeless", "space", "dearvr", "stereo",
               "width", "imager", "panner", "spatial", "chamber", "shimmer" }))
        return space;
    if (any ({ "sat", "tape", "tube", "drive", "distort", "decapitator", "crush", "cassette", "dirt", "warm", "fuzz",
               " amp", "revive", "exciter", "trash", "heat", "color", "colour", "lofi", "lo fi" }))
        return saturation;
    if (any ({ "chorus", "flang", "phase", "tremolo", "vibrato", "rotary", "ensemble", "pitch", "shift", "micro",
               "doubler", "modul" }))
        return modulation;
    return other;
}

juce::String OwnPluginsPanel::vendorFromName (const juce::String& name)
{
    // The vendors whose prefix is not their name.
    static const std::pair<const char*, const char*> prefixes[] {
        { "bx_", "Brainworx" }, { "kHs ", "Kilohearts" }, { "FabFilter", "FabFilter" }, { "Valhalla", "Valhalla DSP" },
        { "dearVR", "Dear Reality" }, { "Pro-", "FabFilter" }, { "Waves", "Waves" }, { "iZotope", "iZotope" },
        { "Ozone", "iZotope" }, { "Neutron", "iZotope" }, { "RX ", "iZotope" }, { "Soundtoys", "Soundtoys" },
        { "UAD", "Universal Audio" }, { "Arturia", "Arturia" }, { "Softube", "Softube" }, { "TDR", "Tokyo Dawn Labs" },
        { "DLS", "Apple" }, { "Melda", "MeldaProduction" },
    };

    for (const auto& [prefix, vendor] : prefixes)
        if (name.startsWith (prefix))
            return vendor;

    // Otherwise the first word, which is the vendor often enough
    // ("Cassette" alone stays itself).
    return name.upToFirstOccurrenceOf (" ", false, false);
}

void OwnPluginsPanel::readVst3Info (const juce::File& bundle, juce::String& vendor, Category& category)
{
    // A VST3 built with a recent SDK carries its vendor and categories in a
    // small JSON file inside the bundle: read without loading the plugin.
    const auto info = bundle.getChildFile ("Contents/Resources/moduleinfo.json");
    if (! info.existsAsFile())
        return;

    // The file may have trailing commas (JSON5); drop them before parsing.
    auto textOfFile = info.loadFileAsString();
    for (auto close : { "}", "]" })
        for (int guard = 0; guard < 64; ++guard)
        {
            const auto before = textOfFile;
            textOfFile = textOfFile.replace (juce::String (",\n") + close, juce::String ("\n") + close)
                                   .replace (juce::String (", ") + close, close)
                                   .replace (juce::String (",") + close, close);
            if (textOfFile == before)
                break;
        }

    const auto json = juce::JSON::parse (textOfFile);
    if (! json.isObject())
        return;

    const auto v = json.getProperty ("Factory Info", {}).getProperty ("Vendor", {}).toString();
    if (v.isNotEmpty())
        vendor = v;

    if (auto* classes = json.getProperty ("Classes", {}).getArray())
        for (const auto& c : *classes)
            if (auto* subs = c.getProperty ("Sub Categories", {}).getArray())
            {
                juce::StringArray s;
                for (const auto& x : *subs)
                    s.add (x.toString());

                if (s.contains ("Instrument"))     { category = instrument; return; }
                if (s.contains ("EQ"))             { category = eq; return; }
                if (s.contains ("Dynamics"))       { category = dynamics; return; }
                if (s.contains ("Reverb") || s.contains ("Delay") || s.contains ("Spatial")) { category = space; return; }
                if (s.contains ("Distortion"))     { category = saturation; return; }
                if (s.contains ("Modulation") || s.contains ("Pitch Shift")) { category = modulation; return; }
            }
}

//==============================================================================
OwnPluginsPanel::OwnPluginsPanel (OwnPluginPlayer& p) : player (p)
{
   #if JUCE_PLUGINHOST_VST3 || JUCE_PLUGINHOST_AU
    juce::addDefaultFormatsToManager (formats);
   #endif

    searchBox.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible (searchBox);

    sortChoice.setValue (0);
    sortChoice.setUppercase (false);
    sortChoice.onChange = [this] (int) { applyFilter(); };
    addAndMakeVisible (sortChoice);

    scanButton.onClick = [this] { scan(); };
    addAndMakeVisible (scanButton);

    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    addChildComponent (lockCover);

    knobViewport.setViewedComponent (&knobHolder, false);
    knobViewport.setScrollBarsShown (true, false);
    addChildComponent (knobViewport);

    optionCount.setOptions ({ 2, 3, 5 }, { "2", "3", "5" });
    optionCount.setValue (3);
    addChildComponent (optionCount);

    modeChoice.setValue (0);
    modeChoice.onChange = [this] (int)
    {
        // Leaving a round half-way is allowed; it just ends.
        restoreDemoKnob (false);
        roundOn = answered = false;
        checking = false;
        stopTimer();
        setListen (OwnPluginPlayer::Listen::a);
        learnPlaying = -1.0f;
        rebuildKnobToggles();   // the knobs heard so far are the ones asked about
        refreshRoundUi();
        resized();
        repaint();
    };
    addChildComponent (modeChoice);

    for (auto* b : { &listenA, &listenB, &listenDry })
    {
        b->setClickingTogglesState (false);
        addChildComponent (*b);
    }
    listenA.onClick = [this] { setListen (OwnPluginPlayer::Listen::a); };
    listenB.onClick = [this] { setListen (OwnPluginPlayer::Listen::b); };
    listenDry.onClick = [this] { setListen (OwnPluginPlayer::Listen::dry); };

    startButton.onClick = [this] { startRound(); };
    addChildComponent (startButton);

    for (int i = 0; i < 5; ++i)
    {
        auto* b = optionButtons.add (new juce::TextButton());
        b->onClick = [this, i] { answer (i); };
        addChildComponent (b);
    }

    // Getting to know the knobs.
    knobList.setRowHeight (32);
    knobList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addChildComponent (knobList);
    learnLow.onClick = [this] { playLearnValue (learnLowValue); };
    learnHigh.onClick = [this] { playLearnValue (learnHighValue); };
    learnMine.onClick = [this] { restoreDemoKnob (true); learnPlaying = -1.0f; refreshRoundUi(); };
    learnSweep.onClick = [this]
    {
        // The whole travel and back to where it was: what the knob does,
        // heard as one movement.
        if (learnKnobs.empty() || player.getRig() == nullptr)
            return;
        const auto index = learnKnobs[(size_t) learnSelected].index;
        if (demoIndex != index)
        {
            restoreDemoKnob (false);
            demoIndex = index;
            demoSaved = player.getRig()->a->getParameters()[index]->getValue();
        }
        learnKnobs[(size_t) learnSelected].heard = true;
        motion = { index, 0.0f, 1.0f, juce::Time::getMillisecondCounterHiRes(), 5000.0, true };
        if (auto* p = player.getRig()->a->getParameters()[index])
            p->setValueNotifyingHost (0.0f);
        learnPlaying = 2.0f;   // "sweeping"
        setListen (OwnPluginPlayer::Listen::a);
        startTimerHz (60);
        knobList.repaint();
        refreshRoundUi();
    };
    learnNext.onClick = [this] { selectLearnKnob (learnSelected + 1 < (int) learnKnobs.size() ? learnSelected + 1 : 0); };
    for (auto* b : { &learnLow, &learnHigh, &learnMine, &learnNext, &learnSweep })
        addChildComponent (b);
    LnF::makePrimary (learnNext, true);

    setStrings (text);
}

OwnPluginsPanel::~OwnPluginsPanel()
{
    releasePlugin();
}

void OwnPluginsPanel::setStrings (Strings s)
{
    text = std::move (s);
    searchBox.setTextToShowWhenEmpty (text.search, AbcTrainTheme::current().textDim);
    sortChoice.setOptions ({ 0, 1, 2 }, { text.sortPurpose, text.sortName, text.sortVendor });
    modeChoice.setOptions ({ 0, 1 }, { text.learn, text.guess });
    scanButton.setButtonText (text.scan);
    listenA.setButtonText (text.yours);
    listenB.setButtonText (text.hidden);
    listenDry.setButtonText (text.dry);
    learnMine.setButtonText (text.mine);
    learnNext.setButtonText (text.nextKnob);
    learnSweep.setButtonText (text.sweep);
    lockCover.message = text.locked;
    applyFilter();
    refreshRoundUi();
    repaint();
}

juce::String OwnPluginsPanel::fill (juce::String t, const juce::String& key, const juce::String& value) const
{
    return t.replace ("{{" + key + "}}", value);
}

//==============================================================================
void OwnPluginsPanel::scanIfNeeded()
{
    if (! scanned)
        scan();
}

void OwnPluginsPanel::scan()
{
    // Names only: nothing is loaded while listing, so a plugin that would
    // crash on load cannot take the list down with it.
    std::map<juce::String, Entry> byName;

    for (auto* format : formats.getFormats())
    {
        for (const auto& id : format->searchPathsForPlugins (format->getDefaultLocationsToSearch(), true, false))
        {
            const auto fmt = format->getName() == "AudioUnit" ? juce::String ("AU") : format->getName();
            auto name = format->getNameOfPluginFromIdentifier (id);
            // VST3 answers with the bundle's path on macOS: the name is the
            // bundle's file name.
            if (name.containsChar ('/') || name.containsChar ('\\'))
                name = juce::File (name).getFileNameWithoutExtension();

            // Our own trainer is not something to train on.
            if (name.containsIgnoreCase ("Ear Trainer") || name.startsWith ("ABC Learner"))
                continue;

            // Instruments and MIDI effects play nothing on a loop of drums.
            if (fmt == "AU" && (id.contains ("/aumu") || id.contains ("/augn") || id.contains ("/aumi")
                                || id.contains ("Synths/") || id.contains ("Generators/") || id.contains ("MIDI")))
                continue;

            auto& e = byName[name.toLowerCase()];
            if (e.name.isEmpty())
            {
                e.name = name;
                e.vendor = vendorFromName (name);
                e.category = categoryFromName (name);
            }

            if (fmt == "VST3")
            {
                readVst3Info (juce::File (id), e.vendor, e.category);
                e.variants.insert (e.variants.begin(), { fmt, id });   // VST3 first: loaded first
            }
            else
            {
                e.variants.push_back ({ fmt, id });
            }
        }
    }

    all.clear();
    for (auto& [key, e] : byName)
        if (e.category != instrument)
            all.push_back (e);

    scanned = true;
    status = all.empty() ? text.none : text.found.replace ("{{n}}", juce::String ((int) all.size()));
    applyFilter();
}

void OwnPluginsPanel::applyFilter()
{
    const auto q = searchBox.getText().trim();
    std::vector<int> matching;
    for (int i = 0; i < (int) all.size(); ++i)
        if (q.isEmpty() || all[(size_t) i].name.containsIgnoreCase (q) || all[(size_t) i].vendor.containsIgnoreCase (q))
            matching.push_back (i);

    const auto sort = sortChoice.getValue();
    std::sort (matching.begin(), matching.end(), [this, sort] (int a, int b)
    {
        const auto& x = all[(size_t) a];
        const auto& y = all[(size_t) b];
        if (sort == 0 && x.category != y.category) return x.category < y.category;
        if (sort == 2 && x.vendor != y.vendor)     return x.vendor.compareNatural (y.vendor) < 0;
        return x.name.compareNatural (y.name) < 0;
    });

    rows.clear();
    juce::String lastHeader;
    for (auto i : matching)
    {
        const auto& e = all[(size_t) i];
        const auto header = sort == 0 ? text.categories[juce::jlimit (0, text.categories.size() - 1, (int) e.category)]
                          : sort == 2 ? e.vendor : juce::String();
        if (header.isNotEmpty() && header != lastHeader)
        {
            rows.push_back ({ true, header, -1 });
            lastHeader = header;
        }
        rows.push_back ({ false, {}, i });
    }

    list.updateContent();
    list.repaint();
}

int OwnPluginsPanel::getNumRows()
{
    return (int) rows.size();
}

void OwnPluginsPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int) rows.size())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto& r0 = rows[(size_t) row];
    auto r = juce::Rectangle<int> (0, 0, width, height);

    if (r0.header)
    {
        // A group's caption: small, spaced, the accent's colour.
        g.setColour (theme.accent);
        g.setFont (LnF::microFont());
        LnF::fitText (g, r0.caption.toUpperCase(), r.reduced (12, 0).withTrimmedTop (height / 3), juce::Justification::centredLeft, true);
        return;
    }

    const auto& e = all[(size_t) r0.entry];

    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.25f));
        g.fillRect (r);
    }

    g.setColour (theme.divider);
    g.fillRect (r.removeFromBottom (1));

    // The formats it has, small at the right: "VST3 AU".
    auto badge = r.removeFromRight (78).reduced (6, 12);
    g.setColour (theme.textDim);
    g.setFont (LnF::microFont());
    LnF::fitText (g, e.formatsText(), badge, juce::Justification::centredRight, false);

    // The name, and the vendor under it when sorted by name.
    auto textArea = r.reduced (12, 3);
    g.setColour (selected ? theme.textBright : theme.text);
    g.setFont (LnF::bodyFont());

    if (sortChoice.getValue() == 1)
    {
        LnF::fitText (g, e.name, textArea.removeFromTop (textArea.getHeight() * 3 / 5), juce::Justification::bottomLeft, true);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, e.vendor, textArea, juce::Justification::topLeft, true);
    }
    else
    {
        LnF::fitText (g, e.name, textArea, juce::Justification::centredLeft, true);
    }
}

void OwnPluginsPanel::selectedRowsChanged (int lastRowSelected)
{
    if (frozen || lastRowSelected < 0 || lastRowSelected >= (int) rows.size())
        return;

    const auto& r = rows[(size_t) lastRowSelected];
    if (r.header)
    {
        list.deselectRow (lastRowSelected);
        return;
    }

    load (all[(size_t) r.entry]);
}

//==============================================================================
juce::AudioBuffer<float> OwnPluginsPanel::practiceLoop (double sampleRate)
{
    // The first built-in groove: a compressor or a reverb is learnt on
    // music, not on one hit (BuiltInSynth). Rendered here, at the device
    // rate, so the player never resamples.
    for (const auto& s : BuiltInSynth::all())
        if (juce::String (s.category) == "Built-in Loops")
        {
            auto mono = s.render (sampleRate);
            juce::AudioBuffer<float> stereo (2, mono.getNumSamples());
            const auto rms = mono.getRMSLevel (0, 0, mono.getNumSamples());
            const auto gain = rms > 1.0e-5f ? juce::Decibels::decibelsToGain (-18.0f) / rms : 1.0f;
            for (int ch = 0; ch < 2; ++ch)
                stereo.copyFromWithRamp (ch, 0, mono.getReadPointer (0), mono.getNumSamples(), gain, gain);
            return stereo;
        }

    return {};
}

std::unique_ptr<OwnPluginPlayer::Rig> OwnPluginsPanel::buildRig (const Variant& v, juce::String& error)
{
    juce::AudioPluginFormat* format = nullptr;
    for (auto* f : formats.getFormats())
        if ((f->getName() == "AudioUnit" ? juce::String ("AU") : f->getName()) == v.format)
            format = f;

    if (format == nullptr)
    {
        error = "no format";
        return {};
    }

    juce::OwnedArray<juce::PluginDescription> types;
    format->findAllTypesForFile (types, v.id);
    if (types.isEmpty())
    {
        error = "not a plugin";
        return {};
    }

    const auto rate = player.getSampleRate();
    const auto block = player.getBlockSize();
    auto rig = std::make_unique<OwnPluginPlayer::Rig>();
    rig->a = formats.createPluginInstance (*types[0], rate, block, error);
    if (rig->a == nullptr)
        return {};
    rig->b = formats.createPluginInstance (*types[0], rate, block, error);
    if (rig->b == nullptr)
        return {};

    for (auto* inst : { rig->a.get(), rig->b.get() })
    {
        inst->disableNonMainBuses();   // the sidechain stays silent
        inst->setRateAndBufferSizeDetails (rate, block);
        inst->prepareToPlay (rate, block);
    }

    rig->loop = practiceLoop (rate);
    return rig;
}

void OwnPluginsPanel::releasePlugin()
{
    restoreDemoKnob (false);
    motion = {};
    roundOn = answered = false;

    if (pluginWindow != nullptr)
    {
        pluginWindow->clearContentComponent();
        pluginWindow.reset();
    }

    if (pluginEditor != nullptr)
    {
        if (auto* p = pluginEditor->getAudioProcessor())
            p->editorBeingDeleted (pluginEditor.get());
        pluginEditor.reset();
    }

    player.setRig (nullptr);
    silentKnobs.clear();
    checking = false;
    stopTimer();
    knobs.clear();
    knobToggles.clear();
    learnKnobs.clear();
    knobList.updateContent();
    learnPlaying = -1.0f;
    current = {};
    currentFormat = {};
    refreshRoundUi();
}

void OwnPluginsPanel::load (const Entry& e)
{
    if (e.name == current.name && player.getRig() != nullptr)
        return;

    releasePlugin();
    status = fill (text.loading, "name", e.name);
    repaint();

    // VST3 first (an AU of bx_glue crashed as two instances on the
    // author's Mac), then whatever else the plugin comes as.
    juce::String error;
    std::unique_ptr<OwnPluginPlayer::Rig> rig;
    for (const auto& v : e.variants)
    {
        rig = buildRig (v, error);
        if (rig != nullptr)
        {
            currentFormat = v.format;
            break;
        }
    }

    if (rig == nullptr)
    {
        status = fill (fill (text.failed, "name", e.name), "why", error);
        repaint();
        return;
    }

    current = e;

    // The knobs, as the host reads them.
    knobs.clear();
    const auto& params = rig->a->getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        auto* p = params[i];
        OwnPluginRound::Knob k;
        k.index = i;
        k.name = p->getName (64);
        k.label = p->getLabel();
        k.steps = p->getNumSteps();
        k.discrete = p->isDiscrete();
        k.boolean = p->isBoolean();
        k.meta = p->isMetaParameter();
        k.automatable = p->isAutomatable();
        knobs.push_back (k);
    }

    // A's own window: the real one. Created before the rig goes to the
    // audio thread, from the message thread, as a host does.
    if (rig->a->hasEditor())
        pluginEditor.reset (rig->a->createEditorAndMakeActive());

    player.setListen (OwnPluginPlayer::Listen::a);
    player.setRig (std::move (rig));
    status = text.pick;

    // A new plugin starts at getting to know it.
    modeChoice.setValue (0, juce::dontSendNotification);
    rebuildLearnKnobs();
    rebuildKnobToggles();
    selectLearnKnob (0);
    resized();          // editorFrame for this window size
    placeEditor();
    refreshRoundUi();
    repaint();
}

void OwnPluginsPanel::placeEditor()
{
    if (pluginEditor == nullptr)
        return;

    // A plugin draws at its own size, and its window is a native view that
    // cannot be scaled from outside (the first version tried, and Gullfoss
    // spilled over half the page). If it fits the frame it lives in the
    // page; if not, in a window of its own beside the app.
    const auto fits = pluginEditor->getWidth() <= editorFrame.getWidth()
                   && pluginEditor->getHeight() <= editorFrame.getHeight();

    if (fits)
    {
        if (pluginWindow != nullptr)
        {
            pluginWindow->clearContentComponent();
            pluginWindow.reset();
        }
        addAndMakeVisible (*pluginEditor);
        pluginEditor->setTopLeftPosition (editorFrame.getCentreX() - pluginEditor->getWidth() / 2, editorFrame.getY());
        return;
    }

    if (pluginWindow == nullptr)
    {
        struct PluginWindow : public juce::DocumentWindow
        {
            explicit PluginWindow (const juce::String& name)
                : juce::DocumentWindow (name, AbcTrainTheme::current().windowBackground, juce::DocumentWindow::closeButton) {}
            void closeButtonPressed() override { setVisible (false); }
        };

        removeChildComponent (pluginEditor.get());
        pluginWindow = std::make_unique<PluginWindow> (current.name);
        pluginWindow->setUsingNativeTitleBar (true);
        pluginWindow->setContentNonOwned (pluginEditor.get(), true);

        // Beside the app's window if the screen has room, otherwise over it.
        if (auto* top = getTopLevelComponent())
        {
            auto at = top->getScreenBounds();
            pluginWindow->setTopLeftPosition (at.getRight() + 8, at.getY());
            if (const auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (at))
                if (pluginWindow->getRight() > d->userBounds.getRight())
                    pluginWindow->setTopLeftPosition (juce::jmax (d->userBounds.getX(), d->userBounds.getRight() - pluginWindow->getWidth()), at.getY() + 60);
        }
    }

    pluginWindow->setVisible (true);
    pluginWindow->toFront (false);
}

void OwnPluginsPanel::showPluginWindow (bool shouldShow)
{
    if (pluginWindow != nullptr)
        pluginWindow->setVisible (shouldShow);
    else if (pluginEditor != nullptr)
        pluginEditor->setVisible (shouldShow);
}

//==============================================================================
// Getting to know the knobs

void OwnPluginsPanel::rebuildLearnKnobs()
{
    learnKnobs.clear();
    std::vector<LearnKnob> unknown;

    std::vector<int> copies;
    const auto distinct = distinctKnobs (&copies);

    for (size_t i = 0; i < distinct.size(); ++i)
    {
        const auto& k = distinct[i];
        LearnKnob lk { k.index, k.name, KnobGlossary::keyFor (k.name), false, copies[i] };
        (lk.key.isNotEmpty() ? learnKnobs : unknown).push_back (lk);
    }

    std::stable_sort (learnKnobs.begin(), learnKnobs.end(), [] (const LearnKnob& a, const LearnKnob& b)
                      { return KnobGlossary::orderOf (a.key) < KnobGlossary::orderOf (b.key); });
    learnKnobs.insert (learnKnobs.end(), unknown.begin(), unknown.end());
    learnSelected = 0;
    knobList.updateContent();
}

int OwnPluginsPanel::knownCount() const
{
    int n = 0;
    for (const auto& k : learnKnobs)
        n += k.key.isNotEmpty() ? 1 : 0;
    return n;
}

std::vector<OwnPluginRound::Knob> OwnPluginsPanel::distinctKnobs (std::vector<int>* copiesOut) const
{
    // "Band 7 Frequency" and "Band 12 Frequency" are one kind of knob: the
    // digits make the difference. One of each kind, preferably from a band
    // the player's setting uses ("Band 3 Used" on), so the knob moves
    // something that is actually sounding.
    const auto pattern = [] (const juce::String& name)
    {
        juce::String out;
        bool inDigits = false;
        for (auto c : name)
        {
            const auto digit = juce::CharacterFunctions::isDigit (c);
            if (digit && ! inDigits) out << '#';
            if (! digit) out << c;
            inDigits = digit;
        }
        return out.toLowerCase().trim();
    };
    const auto bandOf = [] (const juce::String& name)
    {
        const auto at = name.indexOfAnyOf ("0123456789");
        return at < 0 ? -1 : name.substring (at).getIntValue();
    };

    std::map<int, bool> bandInUse;
    if (auto* rig = player.getRig())
        for (const auto& k : knobs)
        {
            const auto lower = k.name.toLowerCase();
            const auto band = bandOf (k.name);
            if (band >= 0 && (lower.contains ("used") || lower.contains ("enabled") || lower.contains ("active")
                              || lower.endsWith (" on")))
                if (auto* p = rig->a->getParameters()[k.index])
                    bandInUse[band] = bandInUse[band] || p->getValue() >= 0.5f;
        }

    std::vector<juce::String> order;
    std::map<juce::String, std::vector<OwnPluginRound::Knob>> groups;
    for (const auto& k : knobs)
    {
        if (! OwnPluginRound::isTrainable (k))
            continue;
        const auto key = pattern (k.name);
        if (groups.find (key) == groups.end())
            order.push_back (key);
        groups[key].push_back (k);
    }

    std::vector<OwnPluginRound::Knob> out;
    for (const auto& key : order)
    {
        const auto& members = groups[key];
        auto chosen = members.front();
        if (members.size() > 1 && ! bandInUse.empty())
            for (const auto& m : members)
            {
                const auto it = bandInUse.find (bandOf (m.name));
                if (it != bandInUse.end() && it->second)
                {
                    chosen = m;
                    break;
                }
            }
        out.push_back (chosen);
        if (copiesOut != nullptr)
            copiesOut->push_back ((int) members.size());
    }
    return out;
}

void OwnPluginsPanel::moveKnob (int index, float target, double lengthMs)
{
    auto* rig = player.getRig();
    if (rig == nullptr)
        return;
    auto* p = rig->a->getParameters()[index];
    if (p == nullptr)
        return;
    motion = { index, p->getValue(), target, juce::Time::getMillisecondCounterHiRes(), lengthMs, false };
    startTimerHz (60);
}

void OwnPluginsPanel::stepMotion()
{
    auto* rig = player.getRig();
    if (rig == nullptr || motion.index < 0)
    {
        motion = {};
        return;
    }

    const auto t = juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - motion.startMs) / juce::jmax (1.0, motion.lengthMs));
    float v;
    if (motion.sweep)
    {
        // Up over the first 70 %, back down to the player's value after.
        v = t < 0.7 ? (float) (t / 0.7) : juce::jmap ((float) ((t - 0.7) / 0.3), 1.0f, demoSaved);
    }
    else
    {
        const auto eased = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * t));
        v = motion.from + (motion.to - motion.from) * eased;
    }

    if (auto* p = rig->a->getParameters()[motion.index])
        p->setValueNotifyingHost (v);

    if (t >= 1.0)
    {
        if (motion.sweep)
        {
            learnPlaying = -1.0f;
            refreshRoundUi();
        }
        motion = {};
        if (! checking)
            stopTimer();
    }
}

void OwnPluginsPanel::restoreDemoKnob (bool animated)
{
    if (demoIndex < 0)
        return;

    if (animated)
        moveKnob (demoIndex, demoSaved, 400.0);
    else if (auto* rig = player.getRig())
        if (auto* p = rig->a->getParameters()[demoIndex])
        {
            motion = {};
            p->setValueNotifyingHost (demoSaved);
        }

    if (! animated)
        demoIndex = -1;
    else
        juce::Timer::callAfterDelay (450, [safe = juce::Component::SafePointer<OwnPluginsPanel> (this)]
        {
            if (safe != nullptr && safe->motion.index < 0)
                safe->demoIndex = -1;
        });
}

void OwnPluginsPanel::selectLearnKnob (int i)
{
    restoreDemoKnob (false);
    learnSelected = juce::jlimit (0, juce::jmax (0, (int) learnKnobs.size() - 1), i);
    learnPlaying = -1.0f;
    setListen (OwnPluginPlayer::Listen::a);
    knobList.selectRow (learnSelected, false, true);
    knobList.scrollToEnsureRowIsOnscreen (learnSelected);
    refreshRoundUi();
    repaint();
}

juce::String OwnPluginsPanel::learnValueText (float normalised) const
{
    auto* rig = player.getRig();
    if (rig == nullptr || learnKnobs.empty())
        return juce::String (juce::roundToInt (normalised * 100.0f)) + "%";

    const auto index = learnKnobs[(size_t) learnSelected].index;
    if (auto* p = rig->a->getParameters()[index])
    {
        auto t = p->getText (normalised, 24).trim();
        const auto unit = p->getLabel().trim();
        if (unit.isNotEmpty() && ! t.endsWithIgnoreCase (unit))
            t << " " << unit;
        return t;
    }
    return {};
}

void OwnPluginsPanel::playLearnValue (float normalised)
{
    auto* rig = player.getRig();
    if (rig == nullptr || learnKnobs.empty())
        return;

    auto& lk = learnKnobs[(size_t) learnSelected];

    // The knob turns in the plugin's own window (A), as a lesson turns a
    // Learner's: the player sees which knob it is and hears it move. A's
    // own value is kept and goes back with "Your setting" or the next card.
    if (demoIndex != lk.index)
    {
        restoreDemoKnob (false);
        demoIndex = lk.index;
        if (auto* p = rig->a->getParameters()[lk.index])
            demoSaved = p->getValue();
    }

    moveKnob (lk.index, normalised, 450.0);
    lk.heard = true;
    learnPlaying = normalised;
    setListen (OwnPluginPlayer::Listen::a);
    knobList.repaint();
    refreshRoundUi();
    repaint();
}

int OwnPluginsPanel::KnobListModel::getNumRows()
{
    return (int) owner.learnKnobs.size();
}

void OwnPluginsPanel::KnobListModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= (int) owner.learnKnobs.size())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto& k = owner.learnKnobs[(size_t) row];
    auto r = juce::Rectangle<int> (0, 0, width, height);

    // The line between the described knobs and the rest.
    if (k.key.isEmpty() && (row == 0 || owner.learnKnobs[(size_t) row - 1].key.isNotEmpty()))
    {
        g.setColour (theme.divider);
        g.fillRect (r.getX(), 0, width, 1);
    }

    if (selected)
    {
        g.setColour (theme.accent.withAlpha (0.22f));
        g.fillRect (r);
    }

    // A tick once heard.
    auto mark = r.removeFromLeft (24);
    if (k.heard)
    {
        g.setColour (theme.positive);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, juce::String::fromUTF8 ("\xe2\x9c\x93"), mark, juce::Justification::centred, false);
    }

    g.setColour (k.key.isEmpty() ? theme.textDim : selected ? theme.textBright : theme.text);
    g.setFont (LnF::bodyFont());
    LnF::fitText (g, k.name, r.reduced (4, 0), juce::Justification::centredLeft, true);
}

void OwnPluginsPanel::KnobListModel::selectedRowsChanged (int row)
{
    if (row >= 0 && row != owner.learnSelected)
        owner.selectLearnKnob (row);
}

//==============================================================================
// Guessing

void OwnPluginsPanel::rebuildKnobToggles()
{
    // Ticked: the described knobs the player has listened to; if none yet,
    // the first four described ones. The rest are there, unticked, for
    // whoever wants them.
    std::vector<int> heard, known;
    for (const auto& lk : learnKnobs)
    {
        if (lk.key.isNotEmpty())
            known.push_back (lk.index);
        if (lk.heard)
            heard.push_back (lk.index);
    }
    if (heard.empty())
        for (size_t i = 0; i < known.size() && i < 4; ++i)
            heard.push_back (known[i]);

    knobToggles.clear();
    const auto add = [this, &heard] (const OwnPluginRound::Knob& k)
    {
        auto* t = knobToggles.add (new juce::ToggleButton (k.name));
        t->setComponentID (juce::String (k.index));
        t->setToggleState (std::find (heard.begin(), heard.end(), k.index) != heard.end(), juce::dontSendNotification);
        knobHolder.addAndMakeVisible (t);
    };

    // In the order of getting to know them; one of each kind of band knob.
    for (const auto& lk : learnKnobs)
        for (const auto& k : knobs)
            if (k.index == lk.index)
                add (k);
}

void OwnPluginsPanel::startRound()
{
    auto* rig = player.getRig();
    if (rig == nullptr)
        return;

    restoreDemoKnob (false);

    // The player's setting may have changed since the last round: B starts
    // from A's state every time, then only the round's knob moves.
    juce::MemoryBlock state;
    rig->a->getStateInformation (state);
    rig->b->setStateInformation (state.getData(), (int) state.getSize());

    std::vector<OwnPluginRound::Knob> ticked;
    for (auto* t : knobToggles)
        if (t->getToggleState())
            for (const auto& k : knobs)
                if (k.index == t->getComponentID().getIntValue())
                    ticked.push_back (k);

    std::vector<float> values;
    for (auto* p : rig->a->getParameters())
        values.push_back (p->getValue());

    const auto& params = rig->a->getParameters();
    round = OwnPluginRound::make (ticked, values, optionCount.getValue(), random,
                                  [&params] (int knob, float v) { return params[knob]->getText (v, 32).trim(); },
                                  silentKnobs);

    if (round.knob < 0)
    {
        status = text.noKnobs;
        roundOn = false;
        refreshRoundUi();
        repaint();
        return;
    }

    if (auto* p = rig->b->getParameters()[round.knob])
        p->setValueNotifyingHost (round.options[(size_t) round.correct]);

    // Listen before asking: if B sounds the same as A on this loop, the
    // round is dropped and another knob taken (timerCallback).
    // Two stages: 0.4 s for the change to settle (a state copy resets
    // the plugin's filters, which differs from A for a moment), then the
    // difference is measured afresh for 0.6 s.
    settleUntilMs = juce::Time::getMillisecondCounterHiRes() + 400.0;
    checkUntilMs = settleUntilMs + 600.0;
    settled = false;
    checking = true;
    startTimer (50);

    for (const auto& k : knobs)
        if (k.index == round.knob)
            player.setLoudnessMatch (! OwnPluginRound::isLevelKnob (k));

    roundOn = true;
    answered = false;
    answeredIndex = -1;
    verdict = {};
    status = {};
    setListen (OwnPluginPlayer::Listen::b);
    refreshRoundUi();
    resized();
    repaint();
}

void OwnPluginsPanel::timerCallback()
{
    if (motion.index >= 0)
    {
        stepMotion();
        if (! checking)
            return;
    }

    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (checking && ! settled && now >= settleUntilMs)
    {
        player.resetDifference();
        settled = true;
    }

    if (! checking || now < checkUntilMs)
        return;

    checking = false;
    if (motion.index < 0)
        stopTimer();

    auto levelKnob = false;
    for (const auto& k : knobs)
        if (k.index == round.knob)
            levelKnob = OwnPluginRound::isLevelKnob (k);

    const auto silent = player.getDifferenceDb() < OwnPluginRound::inaudibleBelowDb;
    const auto tell = ! levelKnob && std::abs (player.getLevelGapDb()) > OwnPluginRound::unmatchableAboveDb;

    if (roundOn && ! answered && (silent || tell))
    {
        silentKnobs.push_back (round.knob);
        const auto skipped = round.knobName;
        startRound();
        if (round.knob >= 0)
            status = fill (text.silent, "knob", skipped);
        repaint();
    }
}

void OwnPluginsPanel::answer (int option)
{
    if (! roundOn || answered || option < 0 || option >= (int) round.options.size())
        return;

    answered = true;
    answeredIndex = option;
    ++roundCount;

    const auto ok = option == round.correct;
    if (ok)
        ++rightCount;

    const auto truth = round.optionTexts[round.correct];
    verdict = ok ? fill (text.right, "value", truth)
                 : fill (fill (text.wrong, "value", truth), "answer", round.optionTexts[option]);

    refreshRoundUi();
    repaint();
}

void OwnPluginsPanel::setListen (OwnPluginPlayer::Listen l)
{
    player.setListen (l);
    listenA.setToggleState (l == OwnPluginPlayer::Listen::a, juce::dontSendNotification);
    listenB.setToggleState (l == OwnPluginPlayer::Listen::b, juce::dontSendNotification);
    listenDry.setToggleState (l == OwnPluginPlayer::Listen::dry, juce::dontSendNotification);
}

void OwnPluginsPanel::refreshRoundUi()
{
    const auto loaded = player.getRig() != nullptr || ! knobs.empty();
    const auto learn = loaded && learning();
    const auto guessing = loaded && ! learning();

    modeChoice.setVisible (loaded);

    // Step 1.
    knobList.setVisible (learn);
    const auto any = learn && ! learnKnobs.empty();
    learnLow.setVisible (any);
    learnHigh.setVisible (any);
    learnMine.setVisible (any);
    learnNext.setVisible (any);
    learnSweep.setVisible (any);
    if (any)
    {
        learnLow.setButtonText (fill (text.low, "v", learnValueText (learnLowValue)));
        learnHigh.setButtonText (fill (text.high, "v", learnValueText (learnHighValue)));
        LnF::makePrimary (learnLow, learnPlaying >= 0.0f && learnPlaying < 0.5f);
        LnF::makePrimary (learnHigh, learnPlaying >= 0.5f && learnPlaying <= 1.0f);
        LnF::makePrimary (learnSweep, learnPlaying > 1.0f);
        LnF::makePrimary (learnMine, learnPlaying < 0.0f);
    }

    // Step 2.
    for (auto* b : { &listenA, &listenB, &listenDry, &startButton })
        b->setVisible (guessing);
    optionCount.setVisible (guessing && ! roundOn);
    knobViewport.setVisible (guessing && ! roundOn);

    startButton.setButtonText (roundOn ? text.next : text.start);
    startButton.setEnabled (! roundOn || answered);

    // The plugin's window stays during a round (the author: "после начала
    // раунда он пропадает"). It shows A - the player's own setting - so it
    // gives nothing of B away; turning A's knobs to match B by ear is the
    // exercise done another way, not a cheat.
    showPluginWindow (true);
    // Over the frame: "locked" during a round, or, when the plugin lives in
    // its own window beside the app, where it went.
    lockCover.setVisible (pluginWindow != nullptr);
    lockCover.message = text.ownWindow;
    if (lockCover.isVisible())
        lockCover.toFront (false);

    const auto& theme = AbcTrainTheme::current();
    for (int i = 0; i < optionButtons.size(); ++i)
    {
        auto* b = optionButtons[i];
        const auto exists = guessing && roundOn && i < (int) round.options.size();
        b->setVisible (exists);
        if (! exists)
            continue;

        b->setButtonText (round.optionTexts[i]);
        auto colour = theme.widgetBackground;
        if (answered && i == round.correct)
            colour = theme.positive.withAlpha (0.55f);
        else if (answered && i == answeredIndex)
            colour = theme.negative.withAlpha (0.55f);
        b->setColour (juce::TextButton::buttonColourId, colour);
        b->setEnabled (true);   // answer() ignores a second press; disabled text was unreadable on the verdict colours
    }
}

//==============================================================================
void OwnPluginsPanel::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    LnF::paintPanelBackground (g, getLocalBounds().toFloat());

    // The list's rail.
    auto rail = getLocalBounds().removeFromLeft (listWidth);
    g.setColour (theme.panelBackground.withAlpha (0.6f));
    g.fillRect (rail);
    g.setColour (theme.divider);
    g.fillRect (rail.removeFromRight (1));

    g.setColour (theme.textDim);
    g.setFont (LnF::microFont());
    LnF::fitText (g, all.empty() && ! scanned ? text.scanning : status.isNotEmpty() && current.name.isEmpty() ? status : juce::String(),
                  juce::Rectangle<int> (12, getHeight() - 30, listWidth - 24, 24), juce::Justification::centredLeft, true);

    // Header: the plugin's name, its vendor and format, and the page's state.
    if (! headerArea.isEmpty())
    {
        auto h = headerArea;
        if (modeChoice.isVisible())
            h.removeFromRight (modeChoice.getWidth() + 16);
        auto top = h.removeFromTop (30);
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont());
        const auto title = current.name.isNotEmpty() ? current.name : text.title;
        const auto tw = juce::jmin (top.getWidth(), juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title)) + 8);
        LnF::fitText (g, title, top.removeFromLeft (tw), juce::Justification::centredLeft, true);
        if (current.name.isNotEmpty())
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, current.vendor + juce::String::fromUTF8 ("  \xc2\xb7  ") + currentFormat, top.withTrimmedLeft (12),
                          juce::Justification::centredLeft, true);
        }

        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        const auto line = current.name.isEmpty() ? text.pick
                        : status.isNotEmpty() && status != text.pick ? status
                        : learning() ? text.learnIntro : text.guessIntro;
        LnF::fitText (g, line, h, juce::Justification::centredLeft, true);
    }

    // The editor's frame where the plugin draws nothing yet.
    if (! editorFrame.isEmpty() && pluginEditor == nullptr)
    {
        g.setColour (theme.outline);
        g.drawRect (editorFrame);
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, current.name.isNotEmpty() ? current.name : text.pick, editorFrame.reduced (20), juce::Justification::centred, true);
    }

    if (roundArea.isEmpty() || (player.getRig() == nullptr && knobs.empty()))
        return;

    // ---- step 1: the card of the chosen knob ----
    if (learning())
    {
        g.setColour (theme.divider);
        g.fillRect (roundArea.getX(), roundArea.getY(), roundArea.getWidth(), 1);

        auto progress = roundArea.withHeight (26).withTrimmedTop (4);
        int heardKnown = 0;
        for (const auto& k : learnKnobs)
            heardKnown += k.heard && k.key.isNotEmpty() ? 1 : 0;
        g.setColour (theme.textDim);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.heard.replace ("{{n}}", juce::String (heardKnown)).replace ("{{m}}", juce::String (knownCount())),
                      progress, juce::Justification::centredRight, true);

        if (learnKnobs.empty())
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, text.noKnobs, cardArea, juce::Justification::centredLeft, true);
            return;
        }

        const auto& k = learnKnobs[(size_t) learnSelected];
        auto c = cardArea;
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (24.0f));
        LnF::fitText (g, k.name, c.removeFromTop (34), juce::Justification::centredLeft, true);

        if (k.key.isEmpty() || text.glossary == nullptr)
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::bodyFont());
            LnF::fitLines (g, text.noDescription, c.removeFromTop (48), juce::Justification::topLeft, 2, 0.9f);
        }
        else
        {
            g.setColour (theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitLines (g, text.glossary (k.key + ".what"), c.removeFromTop (48), juce::Justification::topLeft, 2, 0.9f);
            c.removeFromTop (4);
            g.setColour (theme.accent);
            g.setFont (LnF::labelFont());
            LnF::fitLines (g, fill (text.listenFor, "what", text.glossary (k.key + ".listen")), c.removeFromTop (40),
                           juce::Justification::topLeft, 2, 0.9f);
        }

        // Pro-Q's 24 identical bands: said once, and which one is shown.
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        if (k.copies > 1)
            LnF::fitText (g, text.copies.replace ("{{n}}", juce::String (k.copies)).replace ("{{name}}", k.name),
                          c.removeFromTop (20), juce::Justification::centredLeft, true);
        if (pluginEditor != nullptr || frozen)
            LnF::fitText (g, text.watch, c.removeFromTop (20), juce::Justification::centredLeft, true);
        return;
    }

    // ---- step 2: the round: question, what to listen for, tally, verdict ----
    {
        auto r = roundArea;
        auto top = r.removeFromTop (28);

        if (roundOn)
        {
            g.setColour (theme.textBright);
            g.setFont (LnF::headingFont());
            LnF::fitText (g, fill (text.question, "knob", round.knobName), top.removeFromLeft (top.getWidth() * 2 / 3),
                          juce::Justification::centredLeft, true);

            // What this knob changes, under the question: the hint the
            // familiarisation taught, so the ear knows where to go.
            const auto key = KnobGlossary::keyFor (round.knobName);
            if (key.isNotEmpty() && text.glossary != nullptr)
            {
                g.setColour (theme.accent);
                g.setFont (LnF::labelFont());
                LnF::fitText (g, fill (text.listenFor, "what", text.glossary (key + ".listen")),
                              roundArea.withTrimmedTop (28).withHeight (18), juce::Justification::centredLeft, true);
            }
        }
        else if (knobViewport.isVisible())
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitText (g, LnF::toCaps (text.knobs), top.removeFromLeft (top.getWidth() / 2), juce::Justification::centredLeft, true);
            LnF::fitText (g, LnF::toCaps (text.options), top.withTrimmedRight (170), juce::Justification::centredRight, true);
        }

        if (roundCount > 0)
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, text.tally.replace ("{{n}}", juce::String (rightCount)).replace ("{{m}}", juce::String (roundCount)),
                          roundArea.withHeight (28), juce::Justification::centredRight, false);
        }

        if (verdict.isNotEmpty())
        {
            g.setColour (answeredIndex == round.correct ? theme.positive : theme.negative);
            g.setFont (LnF::headingFont());
            LnF::fitText (g, verdict, roundArea.withTrimmedTop (roundArea.getHeight() - 30).withTrimmedRight (200),
                          juce::Justification::centredLeft, true);
        }
    }
}

void OwnPluginsPanel::resized()
{
    using namespace AbcTrainTheme;
    auto area = getLocalBounds();

    // Left: search, how to sort, the list, scan.
    {
        auto left = area.removeFromLeft (listWidth).reduced (Spacing::medium);
        searchBox.setBounds (left.removeFromTop (32));
        left.removeFromTop (Spacing::small);
        sortChoice.setBounds (left.removeFromTop (30));
        left.removeFromTop (Spacing::small);
        left.removeFromBottom (30);   // the status line
        scanButton.setBounds (left.removeFromBottom (32));
        left.removeFromBottom (Spacing::small);
        list.setBounds (left);
    }

    auto right = area.reduced (Spacing::large, Spacing::medium);
    headerArea = right.removeFromTop (56);
    modeChoice.setBounds (headerArea.withTrimmedLeft (headerArea.getWidth() - juce::jmin (headerArea.getWidth() / 2, juce::jmax (360, modeChoice.getPreferredWidth())))
                              .removeFromTop (34));
    right.removeFromTop (Spacing::small);

    // The steps take one fixed band at the bottom - the same in both, with
    // or without a round on, so nothing moves (the stable window rule).
    roundArea = right.removeFromBottom (bandHeight);
    right.removeFromBottom (Spacing::medium);

    // The plugin's window gets the rest, at its own size if it fits.
    editorFrame = right;
    lockCover.setBounds (right);
    if (pluginEditor != nullptr && pluginWindow == nullptr && pluginEditor->getParentComponent() == this)
        pluginEditor->setTopLeftPosition (right.getCentreX() - pluginEditor->getWidth() / 2, right.getY());

    // ---- step 1 ----
    {
        auto band = roundArea.withTrimmedTop (30);
        knobList.setBounds (band.removeFromLeft (juce::jmin (280, band.getWidth() / 3)));
        band.removeFromLeft (Spacing::large);
        auto buttons = band.removeFromBottom (44);
        cardArea = band.withTrimmedBottom (Spacing::small);
        const auto nextWidth = juce::jmin (230, buttons.getWidth() / 4);
        const auto w = (buttons.getWidth() - nextWidth - 4 * Spacing::small) / 4;
        for (auto* b : { &learnLow, &learnHigh, &learnSweep, &learnMine })
        {
            b->setBounds (buttons.removeFromLeft (w));
            buttons.removeFromLeft (Spacing::small);
        }
        learnNext.setBounds (buttons);
    }

    // ---- step 2 ----
    auto r = roundArea;
    r.removeFromTop (28 + 18 + Spacing::small);

    auto listenRow = r.removeFromTop (44);
    startButton.setBounds (listenRow.removeFromRight (170));
    listenRow.removeFromRight (Spacing::medium);
    const auto third = juce::jmin (200, listenRow.getWidth() / 3);
    listenA.setBounds (listenRow.removeFromLeft (third));
    listenRow.removeFromLeft (Spacing::hairline);
    listenB.setBounds (listenRow.removeFromLeft (third));
    listenRow.removeFromLeft (Spacing::hairline);
    listenDry.setBounds (listenRow.removeFromLeft (juce::jmin (120, listenRow.getWidth())));
    r.removeFromTop (Spacing::medium);

    // Either the knob list and the option count (between rounds) or the
    // options (during one): the same band.
    auto band = r.removeFromTop (juce::jmin (r.getHeight() - 34, 100));
    optionCount.setBounds (band.removeFromRight (170).withHeight (34));
    band.removeFromRight (Spacing::medium);
    knobViewport.setBounds (band);
    {
        const auto cols = juce::jmax (1, band.getWidth() / 200);
        const auto rowsN = (knobToggles.size() + cols - 1) / cols;
        knobHolder.setSize (band.getWidth() - 10, juce::jmax (band.getHeight(), rowsN * 30));
        for (int i = 0; i < knobToggles.size(); ++i)
            knobToggles[i]->setBounds ((i % cols) * 200, (i / cols) * 30, 196, 28);
    }

    const auto n = juce::jmax (1, (int) round.options.size());
    auto optionsRow = roundArea.withTrimmedTop (28 + 18 + Spacing::small + 44 + Spacing::medium).withHeight (64);
    const auto w = (optionsRow.getWidth() - (n - 1) * Spacing::small) / n;
    for (int i = 0; i < optionButtons.size(); ++i)
    {
        optionButtons[i]->setBounds (optionsRow.removeFromLeft (w));
        optionsRow.removeFromLeft (Spacing::small);
    }
}

//==============================================================================
void OwnPluginsPanel::showForSnapshot (bool learningStep)
{
    // What the page looks like with plugins on the list and a round on
    // bx_glue's Attack - names and texts as PluginProbe read them on the
    // author's Mac (2026-10-23), no plugin loaded.
    const auto entry = [] (const char* name, const char* vendor, Category c, std::initializer_list<const char*> formatsList)
    {
        Entry e;
        e.name = name;
        e.vendor = vendor;
        e.category = c;
        for (const auto* f : formatsList)
            e.variants.push_back ({ f, name });
        return e;
    };

    all = { entry ("bx_clipper", "Brainworx", dynamics, { "VST3", "AU" }), entry ("bx_glue", "Brainworx", dynamics, { "VST3", "AU" }),
            entry ("bx_limiter True Peak", "Brainworx", dynamics, { "VST3" }), entry ("FabFilter Pro-C 3", "FabFilter", dynamics, { "VST3", "AU" }),
            entry ("FabFilter Pro-Q 4", "FabFilter", eq, { "VST3", "AU" }), entry ("Gullfoss", "Soundtheory", eq, { "VST3", "AU" }),
            entry ("kHs Compressor", "Kilohearts", dynamics, { "AU" }), entry ("ValhallaRoom", "Valhalla DSP", space, { "VST3", "AU" }),
            entry ("FabFilter Pro-R 2", "FabFilter", space, { "VST3" }), entry ("Cassette", "Cassette", saturation, { "VST3", "AU" }),
            entry ("dxRevive Pro", "Accentize", other, { "AU" }) };
    frozen = true;
    scanned = true;
    status = text.found.replace ("{{n}}", juce::String ((int) all.size()));
    applyFilter();

    for (int i = 0; i < (int) rows.size(); ++i)
        if (! rows[(size_t) i].header && all[(size_t) rows[(size_t) i].entry].name == "bx_glue")
            list.selectRow (i, true, true);   // does not load: no rig in a snapshot

    for (const auto& e : all)
        if (e.name == "bx_glue")
            current = e;
    currentFormat = "VST3";

    for (auto name : { "Input Gain", "XL Saturation", "Threshold", "Ratio", "Attack", "Release", "Band Tilt", "Mix", "Character" })
    {
        OwnPluginRound::Knob k;
        k.index = (int) knobs.size();
        k.name = name;
        knobs.push_back (k);
    }
    rebuildLearnKnobs();
    learnKnobs[0].heard = learnKnobs[1].heard = true;
    rebuildKnobToggles();

    if (learningStep)
    {
        modeChoice.setValue (0, juce::dontSendNotification);
        learnSelected = 2;   // Attack
        knobList.selectRow (2, false, true);
        learnPlaying = learnHighValue;
        refreshRoundUi();
        learnLow.setButtonText (fill (text.low, "v", "0.3 ms"));
        learnHigh.setButtonText (fill (text.high, "v", "30 ms"));
        resized();
        return;
    }

    modeChoice.setValue (1, juce::dontSendNotification);
    round.knob = 4;
    round.knobName = "Attack";
    round.options = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f };
    round.optionTexts = { "0.1", "0.3", "1", "3", "10" };
    round.correct = 3;
    roundOn = true;
    answered = true;
    answeredIndex = 2;
    roundCount = 6;
    rightCount = 4;
    verdict = fill (fill (text.wrong, "value", "3"), "answer", "1");
    refreshRoundUi();
    for (auto* b : { &listenA, &listenB, &listenDry, &startButton })
        b->setVisible (true);
    setListen (OwnPluginPlayer::Listen::b);
    resized();
}

bool OwnPluginsPanel::handleHotkey (const juce::String& id)
{
    if (learning() && ! learnKnobs.empty())
    {
        if (id == "own.a")    { learnMine.triggerClick(); return true; }
        if (id == "own.b")    { learnHigh.triggerClick(); return true; }
        if (id == "own.dry")  { learnLow.triggerClick();  return true; }
        if (id == "own.next") { learnNext.triggerClick(); return true; }
        return false;
    }

    if (id == "own.a")   { if (listenA.isEnabled())   listenA.triggerClick();   return true; }
    if (id == "own.b")   { if (listenB.isEnabled())   listenB.triggerClick();   return true; }
    if (id == "own.dry") { if (listenDry.isEnabled()) listenDry.triggerClick(); return true; }
    if (id == "own.next"){ if (startButton.isEnabled() && startButton.isVisible()) startButton.triggerClick(); return true; }
    return false;
}
