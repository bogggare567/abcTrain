#include "PluginEditor.h"
#include "CompressorModules.h"
#include "ParameterGuide.h"
#include "VocalCompressionLesson.h"
#include "BusGlueLesson.h"
#include "TransientLessons.h"

namespace
{
    LearnerEditorBase::Services servicesFor (LearnerCompProcessor& p)
    {
        return { p.apvts, LearnerCompProcessor::bypassParamId, p.getSharedProperties(),
                 p.getPracticeLibrary(), p.getPracticeSource(),
                 [&p] (const juce::String& id, float v) { p.setCheckOverride (id, v); },
                 [&p] { p.clearCheckOverride(); } };
    }

    juce::StringArray presetNames (const LocalisationManager& loc)
    {
        juce::StringArray names;

        for (size_t i = 0; i < CompressorGuide::presets.size(); ++i)
        {
            const auto key = "preset.comp." + juce::String ((int) i) + ".name";
            const auto text = loc.getText (key);
            names.add (text == key ? juce::String (CompressorGuide::presets[i].name) : text);
        }

        return names;
    }
}

LearnerCompEditor::LearnerCompEditor (LearnerCompProcessor& p)
    : LearnerEditorBase (p, servicesFor (p),
                         { "ABC Learner Comp", AbcTrainTheme::Family::dynamics,
                           AppIcons::Icon::learnerComp, "lp.compressor" }),
      compProcessor (p),
      knobs (p.apvts, { { "threshold", t ("knob.threshold", "Threshold"), " " + t ("unit.dB", "dB"), 0 },
                        { "ratio",     t ("knob.ratio", "Ratio"),         ":1", 1 },
                        { "attack",    t ("knob.attack", "Attack"),       " " + t ("unit.ms", "ms"), 1 },
                        { "release",   t ("knob.release", "Release"),     " " + t ("unit.ms", "ms"), 0 },
                        { "knee",      t ("knob.knee", "Knee"),           " " + t ("unit.dB", "dB"), 0 },
                        { "makeup",    t ("knob.makeup", "Makeup"),       " " + t ("unit.dB", "dB"), 1 },
                        { "dryWet",    t ("knob.mix", "Mix"),             "%", 0 } }, decimalPoint()),
      presets (presetNames (localisation))
{
    transferCurve.setStrings ({ t ("lp.curveIn", "in, dB"), t ("lp.curveOut", "out, dB") });
    addAndMakeVisible (transferCurve);
    addAndMakeVisible (waveform);
    gainReductionMeter.setUnits (t ("unit.dB", "dB"), decimalPoint());
    addAndMakeVisible (gainReductionMeter);

    for (auto* label : { &inputPeakLabel, &outputPeakLabel })
    {
        label->setJustificationType (juce::Justification::centred);
        label->setFont (AbcTrainLookAndFeel::monoFont());
        addAndMakeVisible (*label);
    }

    knobs.onDragStart = [this] (const juce::String& id)
    {
        showGuide (t ("guide.comp." + id, CompressorGuide::describe (id)));
    };
    knobs.onDragEnd = [this] (const juce::String&) { showGuide ({}); };
    knobs.onValueChange = [this] (const juce::String&)
    {
        // A preset is a claim about every knob; once one moves it is false.
        if (! moduleScreen.isRunning() && presets.getActive() >= 0 && knobs.isMouseButtonDown (true))
            presets.setActive (-1);
    };
    addAndMakeVisible (knobs);

    presets.onChosen = [this] (int i)
    {
        compProcessor.applyPreset (i);
        const auto what = t ("preset.comp." + juce::String (i) + ".what", CompressorGuide::presets[(size_t) i].what);
        showGuide (what, 9000);
    };
    addAndMakeVisible (presets);

    compProcessor.setWaveformDisplay (&waveform);

    auto modules = CompressorModules::all();
    for (auto& w : CompressorModules::walkthroughs())
        modules.push_back (std::move (w));

    finishSetup (std::move (modules), 900, 830);
}

void LearnerCompEditor::updateTransferCurve()
{
    // The knobs, never a check's hidden reference (that reaches only the
    // DSP, and the module panel covers this view while it plays).
    const auto value = [this] (const char* id) { return compProcessor.apvts.getRawParameterValue (id)->load(); };
    transferCurve.setParameters (value ("threshold"), value ("ratio"), value ("knee"), value ("makeup"));
}

LearnerCompEditor::~LearnerCompEditor()
{
    compProcessor.setWaveformDisplay (nullptr);
}

void LearnerCompEditor::layoutAnalysis (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;

    auto meterRow = area.removeFromBottom (48);
    area.removeFromBottom (Spacing::medium);

    // The transfer curve square on the left - level in against level out
    // wants equal axes - and the waveform, which shows the same thing
    // happening in time, beside it.
    const auto curveSide = juce::jmin (area.getHeight(), area.getWidth() * 2 / 5);
    transferCurve.setBounds (area.removeFromLeft (curveSide).reduced (1));
    area.removeFromLeft (Spacing::medium);
    waveform.setBounds (area.reduced (1));

    // In and out on the ends, the gain-reduction bar between them.
    inputPeakLabel.setBounds (meterRow.removeFromLeft (170));
    outputPeakLabel.setBounds (meterRow.removeFromRight (170));
    gainReductionMeter.setBounds (meterRow.reduced (Spacing::medium, 6));
}

void LearnerCompEditor::layoutControls (juce::Rectangle<int> area)
{
    knobs.setBounds (area.removeFromTop (knobRowHeight()));
    area.removeFromTop (rowGap());
    presets.setBounds (area.removeFromTop (presetRowHeight()));
}

void LearnerCompEditor::themeChanged()
{
    const auto& theme = AbcTrainTheme::current();

    inputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    outputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    transferCurve.setAccentColour (accent);
    waveform.setAccentColour (accent);
    knobs.refreshColours();
    presets.setActive (presets.getActive());
}

void LearnerCompEditor::tick()
{
    updateTransferCurve();
    transferCurve.setInputLevel (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -100.0f));

    gainReductionMeter.setGainReductionDb (waveform.getCurrentHighlightAmount());

    inputPeakLabel.setText (peakText ("lp.in", "In", waveform.getInputPeak()), juce::dontSendNotification);
    outputPeakLabel.setText (peakText ("lp.out", "Out", waveform.getOutputPeak()), juce::dontSendNotification);
}
