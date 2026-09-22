#include "PluginEditor.h"
#include "shared/dsp/EQCoefficients.h"
#include "FrequencyGuide.h"
#include "FrequencyZones.h"
#include "EQModules.h"
#include "VocalEqLesson.h"
#include "FindResonanceLesson.h"
#include "HighPassLesson.h"

namespace
{
    LearnerEditorBase::Services servicesFor (LearnerEQProcessor& p)
    {
        return { p.apvts, LearnerEQProcessor::bypassParamId, p.getSharedProperties(),
                 p.getPracticeLibrary(), p.getPracticeSource(),
                 [&p] (const juce::String& id, float v) { p.setCheckOverride (id, v); },
                 [&p] { p.clearCheckOverride(); } };
    }

    const char* typeKey (EQCoefficients::BandType type)
    {
        switch (type)
        {
            case EQCoefficients::BandType::bell:      return "eq.type.bell";
            case EQCoefficients::BandType::lowShelf:  return "eq.type.lowShelf";
            case EQCoefficients::BandType::highShelf: return "eq.type.highShelf";
            case EQCoefficients::BandType::highPass:  return "eq.type.highPass";
            case EQCoefficients::BandType::lowPass:   return "eq.type.lowPass";
            case EQCoefficients::BandType::notch:     return "eq.type.notch";
        }

        return "eq.type.bell";
    }

    const char* zoneKey (const FrequencyZones::Zone& zone)
    {
        return zone.name;
    }
}

LearnerEQEditor::LearnerEQEditor (LearnerEQProcessor& p)
    : LearnerEditorBase (p, servicesFor (p),
                         { "ABC Learner EQ", AbcTrainTheme::Family::frequency,
                           AppIcons::Icon::learnerEQ, "lp.bands" }),
      eqProcessor (p)
{
    {
        juce::StringArray names;

        for (const auto& zone : FrequencyZones::all)
            names.add (t (juce::String ("zone.") + zone.name + ".name", zone.name));

        spectrum.setZoneNames (names);
    }

    addAndMakeVisible (spectrum);
    addAndMakeVisible (waveform);

    for (auto* label : { &inputPeakLabel, &outputPeakLabel })
    {
        label->setJustificationType (juce::Justification::centred);
        label->setFont (AbcTrainLookAndFeel::monoFont());
        addAndMakeVisible (*label);
    }

    // ---- the curve is the instrument ----
    spectrum.onBandSelected = [this] (int band) { selectBand (band); };

    spectrum.onBandMoved = [this] (int band, float freqHz, float gainDb)
    {
        writeParameter (LearnerEQProcessor::freqParamId (band), freqHz);

        // A pass filter or a notch has no gain to move.
        if (EQCoefficients::usesGain (eqProcessor.getBandType (band)))
            writeParameter (LearnerEQProcessor::gainParamId (band), gainDb);

        showGuide (typeName (eqProcessor.getBandType (band)) + " - "
                   + t ("guide.eq.freq." + juce::String ((int) FrequencyGuide::rangeIndexFor (freqHz)),
                        FrequencyGuide::describe (freqHz)));
        pushSelectedBandToControls();
    };

    spectrum.onBandQChanged = [this] (int band, float q)
    {
        writeParameter (LearnerEQProcessor::qParamId (band), q);
        pushSelectedBandToControls();
    };

    spectrum.onBandAdded = [this] (float freqHz, float gainDb)
    {
        // Below 45 Hz a new band is almost always meant to be a high-pass.
        const auto type = freqHz < 45.0f ? EQCoefficients::BandType::highPass : EQCoefficients::BandType::bell;
        const auto added = eqProcessor.addBand (freqHz, gainDb, type);

        if (added >= 0)
            selectBand (added);
    };

    spectrum.onBandRemoved = [this] (int band)
    {
        eqProcessor.removeBand (band);

        if (selectedBand == band)
            selectBand (-1);
    };

    spectrum.onPointerMoved = [this] { refreshZoneLabel(); };

    // ---- the selected band ----
    {
        std::vector<int> values;
        juce::StringArray labels;

        for (int type = 0; type < EQCoefficients::numTypes; ++type)
        {
            const auto bandType = EQCoefficients::typeFromIndex (type);
            values.push_back (type);
            labels.add (t (juce::String (typeKey (bandType)) + ".short", EQCoefficients::nameForType (bandType)));
        }

        typeChoice.setOptions (values, labels);
    }

    typeChoice.onChange = [this] (int value)
    {
        if (selectedBand >= 0)
        {
            writeParameter (LearnerEQProcessor::typeParamId (selectedBand), (float) value);
            pushSelectedBandToControls();
        }
    };
    addAndMakeVisible (typeChoice);

    freqSlider.setRange (20.0, 20000.0);
    freqSlider.setSkewFactor (0.3);
    gainSlider.setRange (-18.0, 18.0, 0.1);
    qSlider.setRange (0.1, 18.0, 0.01);
    qSlider.setSkewFactor (0.4);

    // Values in this language's units and decimals, and kHz above 1000 -
    // "12000 Hz" is a number you have to count the zeros of.
    freqSlider.textFromValueFunction = [this] (double hz) { return formatFrequency (hz); };
    freqSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        const auto v = text.replace (",", ".").retainCharacters ("0123456789.").getDoubleValue();
        return text.containsIgnoreCase ("k") || text.contains (juce::CharPointer_UTF8 ("\xd0\xba")) ? v * 1000.0 : v;
    };
    gainSlider.textFromValueFunction = [this] (double db)
    {
        return (db > 0.05 ? "+" : "") + formatNumber (db, 1) + " " + t ("unit.dB", "dB");
    };
    gainSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.replace (",", ".").retainCharacters ("-0123456789.").getDoubleValue();
    };
    qSlider.textFromValueFunction = [this] (double q) { return formatNumber (q, 2); };
    qSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return text.replace (",", ".").retainCharacters ("0123456789.").getDoubleValue();
    };

    freqSlider.onValueChange = [this]
    {
        if (selectedBand < 0)
            return;

        writeParameter (LearnerEQProcessor::freqParamId (selectedBand), (float) freqSlider.getValue());

        if (freqSlider.isMouseButtonDown() && ! moduleScreen.isRunning())
            showGuide (t ("guide.eq.freq." + juce::String ((int) FrequencyGuide::rangeIndexFor ((float) freqSlider.getValue())),
                          FrequencyGuide::describe ((float) freqSlider.getValue())));
    };
    freqSlider.onDragEnd = [this] { showGuide ({}); };

    gainSlider.onValueChange = [this]
    {
        if (selectedBand >= 0)
            writeParameter (LearnerEQProcessor::gainParamId (selectedBand), (float) gainSlider.getValue());
    };

    qSlider.onValueChange = [this]
    {
        if (selectedBand >= 0)
            writeParameter (LearnerEQProcessor::qParamId (selectedBand), (float) qSlider.getValue());
    };

    for (auto* slider : { &freqSlider, &gainSlider, &qSlider })
    {
        addAndMakeVisible (slider);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 20);
        slider->updateText();
    }

    for (int i = 0; i < LearnerEQProcessor::maxBands; ++i)
    {
        bandChips[(size_t) i].setButtonText (juce::String (i + 1));
        bandChips[(size_t) i].onClick = [this, i] { selectBand (i); };
        addChildComponent (bandChips[(size_t) i]);
    }

    addBandChip.setTooltip (t ("eq.addBand", "Add a band at 1 kHz"));
    addBandChip.onClick = [this]
    {
        const auto added = eqProcessor.addBand (1000.0f, 0.0f, EQCoefficients::BandType::bell);

        if (added >= 0)
            selectBand (added);
    };
    addAndMakeVisible (addBandChip);

    bandLabel.setJustificationType (juce::Justification::centredRight);
    bandLabel.setFont (AbcTrainLookAndFeel::labelFont());
    addAndMakeVisible (bandLabel);

    zoneLabel.setJustificationType (juce::Justification::centredLeft);
    zoneLabel.setFont (AbcTrainLookAndFeel::bodyFont());
    addAndMakeVisible (zoneLabel);

    zonesButton.setButtonText (t ("eq.zones", "Zones"));
    zonesButton.setClickingTogglesState (true);
    zonesButton.setToggleState (true, juce::dontSendNotification);
    AbcTrainLookAndFeel::makePrimary (zonesButton, true);
    zonesButton.onClick = [this]
    {
        spectrum.setZonesVisible (zonesButton.getToggleState());
        AbcTrainLookAndFeel::makePrimary (zonesButton, zonesButton.getToggleState());
    };
    addAndMakeVisible (zonesButton);

    refreshZoneLabel();
    selectBand (0);
    pushBandsToDisplay();

    eqProcessor.setSpectrumAnalyser (&spectrum);
    eqProcessor.setWaveformDisplay (&waveform);

    // Every module and lesson is about band 1; the knobs under the panel
    // must be band 1's when it opens.
    moduleScreen.onModuleOpened = [this] (const TrainingModule::Definition&)
    {
        pushBandsToDisplay();
        selectBand (0);
    };

    auto modules = EQModules::all();
    for (auto& w : EQModules::walkthroughs())
        modules.push_back (std::move (w));

    finishSetup (std::move (modules), 880, 800);
}

LearnerEQEditor::~LearnerEQEditor()
{
    eqProcessor.setSpectrumAnalyser (nullptr);
    eqProcessor.setWaveformDisplay (nullptr);
}

juce::String LearnerEQEditor::formatFrequency (double hz) const
{
    if (hz >= 999.5)
        return formatNumber (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " " + t ("unit.kHz", "kHz");

    return juce::String (juce::roundToInt (hz)) + " " + t ("unit.Hz", "Hz");
}

void LearnerEQEditor::refreshBandChips()
{
    auto row = chipRow;
    auto count = 0;

    for (int i = 0; i < LearnerEQProcessor::maxBands; ++i)
    {
        auto& chip = bandChips[(size_t) i];
        const auto on = eqProcessor.isBandOn (i);

        if (on && row.getWidth() >= 34)
        {
            chip.setBounds (row.removeFromLeft (34));
            row.removeFromLeft (2);
            ++count;
        }

        chip.setVisible (on);

        if (AbcTrainLookAndFeel::isPrimary (chip) != (i == selectedBand))
            AbcTrainLookAndFeel::makePrimary (chip, i == selectedBand);
    }

    addBandChip.setBounds (row.removeFromLeft (34));
    addBandChip.setEnabled (count < LearnerEQProcessor::maxBands);
}

juce::String LearnerEQEditor::typeName (EQCoefficients::BandType type) const
{
    return t (typeKey (type), EQCoefficients::nameForType (type));
}

void LearnerEQEditor::layoutAnalysis (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;

    auto meterRow = area.removeFromBottom (24);
    area.removeFromBottom (Spacing::small);

    // The curve is the instrument and gets most of the height.
    const auto spectrumHeight = (area.getHeight() - Spacing::medium) * 64 / 100;
    spectrum.setBounds (area.removeFromTop (spectrumHeight).reduced (1));
    area.removeFromTop (Spacing::medium);
    waveform.setBounds (area.reduced (1));

    inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 2));
    outputPeakLabel.setBounds (meterRow);
}

void LearnerEQEditor::layoutControls (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;

    auto zoneRow = area.removeFromTop (24);
    zonesButton.setBounds (zoneRow.removeFromRight (96).withSizeKeepingCentre (96, 24));
    zoneRow.removeFromRight (Spacing::small);
    zoneLabel.setBounds (zoneRow);

    area.removeFromTop (rowGap());

    auto typeRow = area.removeFromTop (30);
    bandLabel.setBounds (typeRow.removeFromRight (typeRow.getWidth() > 860 ? 110 : 0));
    typeRow.removeFromRight (Spacing::small);
    chipRow = typeRow.removeFromRight (juce::jmin (typeRow.getWidth() / 3, 9 * 36));
    typeRow.removeFromRight (Spacing::medium);
    refreshBandChips();
    typeChoice.setBounds (typeRow.removeFromLeft (juce::jmin (typeRow.getWidth(), juce::jmax (typeChoice.getPreferredWidth(), 520))));

    area.removeFromTop (rowGap());

    const auto width = area.getWidth() / 3;
    juce::Slider* sliders[] { &freqSlider, &gainSlider, &qSlider };

    for (int i = 0; i < 3; ++i)
    {
        auto column = area.removeFromLeft (width).reduced (Spacing::tight, 0);
        knobCaptions[(size_t) i] = column.removeFromTop (20);
        sliders[i]->setBounds (column);
    }
}

void LearnerEQEditor::paintOverChildren (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const juce::String captions[] { t ("eq.freq", "Frequency"), t ("eq.gain", "Gain"), t ("eq.q", "Q") };

    for (int i = 0; i < 3; ++i)
        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (captions[i]), knobCaptions[(size_t) i].toFloat(),
                                              AbcTrainLookAndFeel::labelFont(), theme.textDim, 1.3f,
                                              juce::Justification::centred);

    LearnerEditorBase::paintOverChildren (g);
}

void LearnerEQEditor::themeChanged()
{
    const auto& theme = AbcTrainTheme::current();

    inputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    outputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    bandLabel.setColour (juce::Label::textColourId, theme.textDim);

    for (auto* slider : { &freqSlider, &gainSlider, &qSlider })
    {
        slider->setColour (juce::Slider::textBoxTextColourId, theme.textBright);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    spectrum.setAccentColour (accent);
    waveform.setAccentColour (accent);
    typeChoice.setAccent (accent);
    refreshZoneLabel();
}

void LearnerEQEditor::tick()
{
    pushBandsToDisplay();

    // A band can go away without this editor doing it - host automation,
    // a preset load, a lesson - so the selection is re-checked every frame.
    if (selectedBand >= 0 && ! eqProcessor.isBandOn (selectedBand))
        selectBand (-1);

    pushSelectedBandToControls();
    refreshBandChips();

    inputPeakLabel.setText (peakText ("lp.in", "In", waveform.getInputPeak()), juce::dontSendNotification);
    outputPeakLabel.setText (peakText ("lp.out", "Out", waveform.getOutputPeak()), juce::dontSendNotification);
}

void LearnerEQEditor::writeParameter (const juce::String& id, float value)
{
    if (auto* parameter = eqProcessor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void LearnerEQEditor::selectBand (int band)
{
    selectedBand = (band >= 0 && eqProcessor.isBandOn (band)) ? band : -1;
    spectrum.setSelectedBand (selectedBand);
    pushSelectedBandToControls();
    refreshBandChips();
}

void LearnerEQEditor::pushSelectedBandToControls()
{
    const auto hasBand = selectedBand >= 0;

    typeChoice.setEnabled (hasBand);
    freqSlider.setEnabled (hasBand);
    qSlider.setEnabled (hasBand);

    if (! hasBand)
    {
        gainSlider.setEnabled (false);
        bandLabel.setText (localisation.getText ("eq.noBand"), juce::dontSendNotification);
        return;
    }

    const auto type = eqProcessor.getBandType (selectedBand);

    bandLabel.setText (localisation.getText ("eq.band", { { "number", juce::String (selectedBand + 1) } }),
                       juce::dontSendNotification);

    // Gain is greyed for the shapes that have none.
    gainSlider.setEnabled (EQCoefficients::usesGain (type));
    typeChoice.setValue ((int) type);

    // A mirror of the parameters, never echoed back.
    freqSlider.setValue (eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (selectedBand))->load(), juce::dontSendNotification);
    gainSlider.setValue (eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (selectedBand))->load(), juce::dontSendNotification);
    qSlider.setValue (eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::qParamId (selectedBand))->load(), juce::dontSendNotification);
}

void LearnerEQEditor::refreshZoneLabel()
{
    const auto freq = spectrum.getPointerFrequency();

    if (freq < 0.0f)
    {
        zoneLabel.setText (localisation.getText ("eq.zoneHint"), juce::dontSendNotification);
        zoneLabel.setColour (juce::Label::textColourId, AbcTrainTheme::current().textDim);
        return;
    }

    const auto& zone = FrequencyZones::zoneFor (freq);
    const auto key = juce::String ("zone.") + zoneKey (zone);

    zoneLabel.setText (t (key + ".name", zone.name) + " - " + t (key + ".feels", zone.feels)
                           + "   " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "   "
                           + formatFrequency (freq),
                       juce::dontSendNotification);
    zoneLabel.setColour (juce::Label::textColourId, AbcTrainTheme::current().text);
}

void LearnerEQEditor::pushBandsToDisplay()
{
    std::vector<SpectrumAnalyserComponent::Band> active;
    active.reserve (LearnerEQProcessor::maxBands);

    for (int band = 0; band < LearnerEQProcessor::maxBands; ++band)
    {
        if (! eqProcessor.isBandOn (band))
            continue;

        SpectrumAnalyserComponent::Band entry;
        entry.index = band;
        entry.type = eqProcessor.getBandType (band);
        entry.freqHz = eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (band))->load();
        entry.gainDb = eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (band))->load();
        entry.q = eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::qParamId (band))->load();
        active.push_back (entry);
    }

    const auto sr = eqProcessor.getSampleRate();
    spectrum.setEQState (sr > 0.0 ? sr : 44100.0, std::move (active));
}
