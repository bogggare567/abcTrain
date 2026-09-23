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
      presets()
{
    presets.setCaption (t ("lp.startFrom", "Start from"));
    presets.setItems (presetNames (localisation));
    waveform.setGainReductionTrace (t ("lp.gainReduction", "Gain reduction"),
                                    t ("lp.inOutLegendComp", "In (grey) / out (orange)"));
    transferCurve.setStrings ({ t ("lp.curveIn", "in, dB"), t ("lp.curveOut", "out, dB") });
    addAndMakeVisible (transferCurve);
    addAndMakeVisible (waveform);
    gainReductionMeter.setUnits (t ("unit.dB", "dB"), decimalPoint());
    addAndMakeVisible (gainReductionMeter);

    knobs.onDragStart = [this] (const juce::String& id)
    {
        showGuide (t ("guide.comp." + id, CompressorGuide::describe (id)));
    };
    knobs.onDragEnd = [this] (const juce::String&) { showGuide ({}); };
    knobs.onValueChange = [this] (const juce::String&)
    {
        // A preset is a claim about every knob; once one moves it is false.
        if (! moduleScreen.isRunning() && presets.getChosen() >= 0 && knobs.isMouseButtonDown (true))
            presets.setChosen (-1);
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

    // The transfer curve square on the left - level in against level out
    // wants equal axes - the waveform with the gain-reduction line beside
    // it, and a standing GR meter at the end.
    const auto curveSide = juce::jmin (area.getHeight(), area.getWidth() * 3 / 10);
    transferCurve.setBounds (area.removeFromLeft (curveSide));
    area.removeFromLeft (Spacing::medium);

    gainReductionMeter.setBounds (area.removeFromRight (56).reduced (0, Spacing::large));
    area.removeFromRight (Spacing::medium);
    waveform.setBounds (area);
}

void LearnerCompEditor::layoutControls (juce::Rectangle<int> area)
{
    presets.setBounds (area.removeFromBottom (controlsFooterHeight()).withTrimmedLeft (0).translated (-AbcTrainTheme::Spacing::medium, AbcTrainTheme::Spacing::medium));
    area.removeFromBottom (2 * rowGap());
    knobs.setBounds (area.withTrimmedTop (rowGap()));
}

void LearnerCompEditor::themeChanged()
{
    transferCurve.setAccentColour (accent);
    waveform.setAccentColour (accent);
    presets.setAccent (accent);
    knobs.refreshColours();
}

void LearnerCompEditor::tick()
{
    updateTransferCurve();
    transferCurve.setInputLevel (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -100.0f));

    gainReductionMeter.setGainReductionDb (waveform.getCurrentHighlightAmount());

}
