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
                        { "dryWet",   t ("knob.mix", "Mix"),             "%", 0 },
                        { "width",    t ("knob.width", "Width"),         "%", 0 } }, decimalPoint()),
      presets (presetNames (localisation))
{
    echogram.setStrings ({ t ("lp.echo.dry", "dry"),
                           t ("lp.echo.first", "first reflections after {{ms}} ms"),
                           t ("lp.echo.tail", "tail {{s}} s"),
                           t ("unit.s", "s") });
    echogram.setDecimalSeparator (decimalPoint());
    addAndMakeVisible (echogram);
    addAndMakeVisible (waveform);

    for (auto* label : { &inputPeakLabel, &outputPeakLabel })
    {
        label->setJustificationType (juce::Justification::centred);
        label->setFont (AbcTrainLookAndFeel::monoFont());
        addAndMakeVisible (*label);
    }

    // Type as four segments rather than a dropdown: the choice between
    // them is the first reverb decision, and it should be visible.
    typeChoice.setOptions ({ 0, 1, 2, 3 }, { t ("verb.type.room", "Room"), t ("verb.type.hall", "Hall"),
                                             t ("verb.type.plate", "Plate"), t ("verb.type.spring", "Spring") });
    typeChoice.onChange = [this] (int value)
    {
        if (auto* param = verbProcessor.apvts.getParameter (LearnerVerbProcessor::typeParamId))
            param->setValueNotifyingHost (param->convertTo0to1 ((float) value));

        presets.setActive (-1);
        repaint();
    };
    addAndMakeVisible (typeChoice);
    syncType();

    knobs.onDragStart = [this] (const juce::String& id) { showGuide (t ("guide.verb." + id, ReverbGuide::describe (id))); };
    knobs.onDragEnd = [this] (const juce::String&) { showGuide ({}); };
    knobs.onValueChange = [this] (const juce::String&)
    {
        if (! moduleScreen.isRunning() && presets.getActive() >= 0 && knobs.isMouseButtonDown (true))
            presets.setActive (-1);
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

    finishSetup (std::move (modules), 880, 880);
}

LearnerVerbEditor::~LearnerVerbEditor()
{
    verbProcessor.setWaveformDisplay (nullptr);
}

void LearnerVerbEditor::syncType()
{
    if (auto* raw = verbProcessor.apvts.getRawParameterValue (LearnerVerbProcessor::typeParamId))
    {
        const auto type = juce::roundToInt (raw->load());

        if (type != typeChoice.getValue())
        {
            typeChoice.setValue (type);
            repaint (thisIsTextArea);
        }
    }
}

void LearnerVerbEditor::paint (juce::Graphics& g)
{
    LearnerEditorBase::paint (g);

    const auto& theme = AbcTrainTheme::current();
    const auto caption = [&] (const juce::String& s, juce::Rectangle<int> r)
    {
        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (s), r.toFloat(),
                                              AbcTrainLookAndFeel::labelFont(), theme.textDim, 1.6f);
    };

    caption (t ("lp.type", "Type"), typeCaptionArea);
    caption (t ("lp.thisIs", "This is"), thisIsCaptionArea);

    static const char* keys[] = { "verb.desc.room", "verb.desc.hall", "verb.desc.plate", "verb.desc.spring" };
    static const char* fallbacks[] = {
        "A small hard room. A short tail, and the first reflections still heard one by one.",
        "A big space. A long, dense tail that arrives late and stays.",
        "A metal sheet, not a room: dense and bright from the first moment, no early reflections.",
        "A spring in a box: a metallic drip and boing, the sound of guitar amps and dub." };
    const auto type = juce::jlimit (0, 3, typeChoice.getValue());

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    g.drawFittedText (t (keys[type], fallbacks[type]), thisIsTextArea, juce::Justification::centredLeft, 2, 1.0f);
}

void LearnerVerbEditor::layoutAnalysis (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;

    auto meterRow = area.removeFromBottom (22);
    area.removeFromBottom (Spacing::small);

    const auto echoHeight = (area.getHeight() - Spacing::medium) * 62 / 100;
    echogram.setBounds (area.removeFromTop (echoHeight).reduced (1));
    area.removeFromTop (Spacing::medium);
    waveform.setBounds (area.reduced (1));

    inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 2));
    outputPeakLabel.setBounds (meterRow);
}

void LearnerVerbEditor::layoutControls (juce::Rectangle<int> area)
{
    // TYPE, a full-width bar, and one line under it saying what that
    // type is - the first reverb decision, stated before any knob.
    {
        auto row = area.removeFromTop (34);
        typeCaptionArea = row.removeFromLeft (96);
        typeChoice.setBounds (row);
    }

    area.removeFromTop (8);

    {
        auto row = area.removeFromTop (30);
        thisIsCaptionArea = row.removeFromLeft (96);
        thisIsTextArea = row;
    }

    area.removeFromTop (10);
    knobs.setBounds (area.removeFromTop (132));
    area.removeFromTop (8);
    presets.setBounds (area.removeFromTop (32));
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
}

void LearnerVerbEditor::themeChanged()
{
    const auto& theme = AbcTrainTheme::current();

    inputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    outputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    echogram.setAccentColour (accent);
    typeChoice.setAccent (accent);
    waveform.setAccentColour (accent);
    knobs.refreshColours();
    presets.setActive (presets.getActive());
}

void LearnerVerbEditor::tick()
{
    // The type can change from the host, a preset, or a module step.
    syncType();
    updateEchogram();

    inputPeakLabel.setText (peakText ("lp.in", "In", waveform.getInputPeak()), juce::dontSendNotification);
    outputPeakLabel.setText (peakText ("lp.out", "Out", waveform.getOutputPeak()), juce::dontSendNotification);
}
