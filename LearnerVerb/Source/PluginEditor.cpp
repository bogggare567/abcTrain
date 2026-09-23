#include "PluginEditor.h"
#include "ReverbModules.h"
#include "ReverbGuide.h"
#include "VocalSpaceLesson.h"
#include "BrightVsDarkTailLesson.h"
#include "SpaceLessons.h"

namespace
{
    LearnerEditorBase::Services servicesFor (LearnerVerbProcessor& p)
    {
        return { p.apvts, LearnerVerbProcessor::bypassParamId, p.getSharedProperties(),
                 p.getPracticeLibrary(), p.getPracticeSource(),
                 [&p] (const juce::String& id, float v) { p.setCheckOverride (id, v); },
                 [&p] { p.clearCheckOverride(); } };
    }

    juce::StringArray presetNames (const LocalisationManager& loc)
    {
        juce::StringArray names;

        for (size_t i = 0; i < ReverbGuide::presets.size(); ++i)
        {
            const auto key = "preset.verb." + juce::String ((int) i) + ".name";
            const auto text = loc.getText (key);
            names.add (text == key ? juce::String (ReverbGuide::presets[i].name) : text);
        }

        return names;
    }
}

LearnerVerbEditor::LearnerVerbEditor (LearnerVerbProcessor& p)
    : LearnerEditorBase (p, servicesFor (p),
                         { "ABC Learner Verb", AbcTrainTheme::Family::space,
                           AppIcons::Icon::learnerVerb, "lp.reverb" }),
      verbProcessor (p),
      knobs (p.apvts, { { "decay",    t ("knob.decay", "Decay"),         " " + t ("unit.s", "s"), 2 },
                        { "preDelay", t ("knob.preDelay", "Pre-delay"),  " " + t ("unit.ms", "ms"), 0 },
                        { "size",     t ("knob.size", "Size"),           "%", 0 },
                        { "damping",  t ("knob.damping", "Damping"),     "%", 0 },
                        { "width",    t ("knob.width", "Width"),         "%", 0 },
                        { "dryWet",   t ("knob.mix", "Mix"),             "%", 0 } }, decimalPoint())
{
    room.setStrings ({ t ("lp.room.caption", "The room these knobs describe"),
                       t ("lp.room.source", "source"), t ("lp.room.you", "you"),
                       t ("lp.room.plate", "Not a room: a metal plate"),
                       t ("lp.room.spring", "Not a room: a spring in a box"),
                       t ("unit.m", "m") });
    addAndMakeVisible (room);

    echogram.setStrings ({ t ("lp.echo.dry", "dry"),
                           t ("lp.echo.first", "first reflections after {{ms}} ms"),
                           t ("lp.echo.tail", "tail {{s}} s"),
                           t ("unit.s", "s") });
    echogram.setDecimalSeparator (decimalPoint());
    addAndMakeVisible (echogram);

    // Type first, as chips: the choice between a room, a hall, a plate and
    // a spring is the first reverb decision, and it should be visible.
    typeChips.setCaption (t ("lp.type", "Type"));
    typeChips.setItems ({ t ("verb.type.room", "Room"), t ("verb.type.hall", "Hall"),
                          t ("verb.type.plate", "Plate"), t ("verb.type.spring", "Spring") });
    typeChips.onChosen = [this] (int value)
    {
        if (auto* param = verbProcessor.apvts.getParameter (LearnerVerbProcessor::typeParamId))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) value));

        presets.setChosen (-1);
        syncType();
    };
    addAndMakeVisible (typeChips);

    presets.setCaption (t ("lp.startFrom", "Start from"));
    presets.setItems (presetNames (localisation));
    syncType();

    knobs.onDragStart = [this] (const juce::String& id) { showGuide (t ("guide.verb." + id, ReverbGuide::describe (id))); };
    knobs.onDragEnd = [this] (const juce::String&) { showGuide ({}); };
    knobs.onValueChange = [this] (const juce::String&)
    {
        if (! moduleScreen.isRunning() && presets.getChosen() >= 0 && knobs.isMouseButtonDown (true))
            presets.setChosen (-1);
    };
    addAndMakeVisible (knobs);

    presets.onChosen = [this] (int i)
    {
        verbProcessor.applyPreset (i);
        syncType();
        showGuide (t ("preset.verb." + juce::String (i) + ".what", ReverbGuide::presets[(size_t) i].what), 9000);
    };
    addAndMakeVisible (presets);

    verbProcessor.setWaveformDisplay (&waveform);

    auto modules = ReverbModules::all();
    for (auto& w : ReverbModules::walkthroughs())
        modules.push_back (std::move (w));

    finishSetup (std::move (modules), 880, 800);
}

LearnerVerbEditor::~LearnerVerbEditor()
{
    verbProcessor.setWaveformDisplay (nullptr);
}

juce::String LearnerVerbEditor::typeDescription (int type) const
{
    static const char* keys[] = { "verb.desc.room", "verb.desc.hall", "verb.desc.plate", "verb.desc.spring" };
    static const char* fallbacks[] = {
        "A small hard room. A short tail, and the first reflections still heard one by one.",
        "A big space. A long, dense tail that arrives late and stays.",
        "A metal sheet, not a room: dense and bright from the first moment, no early reflections.",
        "A spring in a box: a metallic drip and boing, the sound of guitar amps and dub." };
    const auto index = juce::jlimit (0, 3, type);
    return t (keys[index], fallbacks[index]);
}

void LearnerVerbEditor::syncType()
{
    if (auto* raw = verbProcessor.apvts.getRawParameterValue (LearnerVerbProcessor::typeParamId))
    {
        const auto type = juce::roundToInt (raw->load());

        if (type != shownType)
        {
            shownType = type;
            typeChips.setChosen (type);
            room.setDescription (typeDescription (type));
        }
    }
}

void LearnerVerbEditor::layoutToolbar (juce::Rectangle<int> area)
{
    placeWithMaterial (typeChips, area);
}

void LearnerVerbEditor::layoutAnalysis (juce::Rectangle<int> area)
{
    // The room on the left, what it does to one click on the right:
    // the place, then the measurement of the place.
    auto left = area.removeFromLeft ((area.getWidth() - AbcTrainTheme::Spacing::medium) / 2);
    area.removeFromLeft (AbcTrainTheme::Spacing::medium);
    room.setBounds (left);
    echogram.setBounds (area);
}

void LearnerVerbEditor::layoutControls (juce::Rectangle<int> area)
{
    presets.setBounds (area.removeFromBottom (controlsFooterHeight()).translated (-AbcTrainTheme::Spacing::medium, AbcTrainTheme::Spacing::medium));
    area.removeFromBottom (2 * rowGap());
    knobs.setBounds (area.withTrimmedTop (rowGap()));
}

void LearnerVerbEditor::updateEchogram (bool immediately)
{
    // The knobs' own values - never the hidden reference of a check, which
    // reaches only the DSP (and the module panel covers this view then).
    const auto value = [this] (const char* id) { return verbProcessor.apvts.getRawParameterValue (id)->load(); };

    ReverbMeasure::Setting s;
    s.type = static_cast<ReverbEngine::Type> (juce::jlimit (0, 3, juce::roundToInt (value (LearnerVerbProcessor::typeParamId))));
    s.decaySeconds = value ("decay");
    s.preDelayMs = value ("preDelay");
    s.size = value ("size") / 100.0f;
    s.damping = value ("damping") / 100.0f;
    echogram.update (s, immediately);

    room.setRoom ((int) s.type, s.size, s.preDelayMs, s.damping);
}

void LearnerVerbEditor::updateNotes()
{
    // What each number means as a place. Words, not a second readout:
    // the value is already under the knob.
    const auto value = [this] (const char* id) { return verbProcessor.apvts.getRawParameterValue (id)->load(); };
    const auto type = juce::jlimit (0, 3, juce::roundToInt (value (LearnerVerbProcessor::typeParamId)));
    const auto size = value ("size") / 100.0f;
    const auto isRoom = type <= 1;
    const auto m = " " + t ("unit.m", "m");
    const auto approx = juce::String (juce::CharPointer_UTF8 ("\xe2\x89\x88 "));
    const auto times = juce::String (juce::CharPointer_UTF8 (" \xc3\x97 "));

    const auto rt = echogram.getMeasuredRt60();
    knobs.setNote ("decay", rt > 0.0 ? t ("lp.note.rt60", "RT60 measured {{s}} s").replace ("{{s}}", formatNumber (rt, 1)) : juce::String());

    // Sound travels about 0.343 m per millisecond.
    knobs.setNote ("preDelay", t ("lp.note.preDelay", "{{m}} m more path to the first wall")
                                   .replace ("{{m}}", approx + formatNumber (value ("preDelay") * 0.343, 1)));

    knobs.setNote ("size", isRoom ? t ("lp.note.size", "room {{dims}}")
                                        .replace ("{{dims}}", approx + juce::String (juce::roundToInt (RoomView::lengthFor (type, size)))
                                                              + times + juce::String (juce::roundToInt (RoomView::widthFor (type, size)))
                                                              + times + juce::String (juce::roundToInt (RoomView::heightFor (type, size))) + m)
                                  : t ("lp.note.sizeOther", "how long the metal rings"));

    const auto damping = value ("damping");
    const auto material = damping < 34.0f ? t ("lp.note.brick", "brick and glass")
                        : damping < 67.0f ? t ("lp.note.wood", "wood")
                                          : t ("lp.note.curtains", "curtains and people");
    knobs.setNote ("damping", t ("lp.note.walls", "walls: {{what}}").replace ("{{what}}", material));
    knobs.setNote ("width", t ("lp.note.width", "mono to wide"));
    knobs.setNote ("dryWet", t ("lp.note.mix", "how much room against the dry"));
}

void LearnerVerbEditor::themeChanged()
{
    echogram.setAccentColour (accent);
    room.setAccentColour (accent);
    typeChips.setAccent (accent);
    presets.setAccent (accent);
    waveform.setAccentColour (accent);
    knobs.refreshColours();
}

void LearnerVerbEditor::tick()
{
    // The type can change from the host, a preset, or a module step.
    syncType();
    updateEchogram();
    updateNotes();
}
