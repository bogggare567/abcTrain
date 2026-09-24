#include "PluginEditor.h"
#include "shared/dsp/EQCoefficients.h"
#include "FrequencyGuide.h"
#include "FrequencyZones.h"
#include "EQModules.h"
#include "VocalEqLesson.h"
#include "FindResonanceLesson.h"
#include "HighPassLesson.h"
#include "InstrumentMaps.h"

namespace
{
    // A filter's response, drawn small: flat line through the middle, the
    // shape where the filter acts. Gain types bulge up (the way they are
    // usually shown), cuts go down.
    juce::Path filterShapeIcon (EQCoefficients::BandType type, juce::Rectangle<float> box)
    {
        using T = EQCoefficients::BandType;
        const auto b = box.reduced (box.getWidth() * 0.16f, box.getHeight() * 0.26f);
        const auto l = b.getX(), r = b.getRight(), w = b.getWidth();
        const auto mid = b.getCentreY(), top = b.getY(), bottom = b.getBottom();
        juce::Path p;

        switch (type)
        {
            case T::bell:
                p.startNewSubPath (l, mid);
                p.lineTo (l + w * 0.22f, mid);
                p.cubicTo (l + w * 0.38f, mid, l + w * 0.40f, top, l + w * 0.5f, top);
                p.cubicTo (l + w * 0.60f, top, l + w * 0.62f, mid, l + w * 0.78f, mid);
                p.lineTo (r, mid);
                break;
            case T::lowShelf:
                p.startNewSubPath (l, top);
                p.lineTo (l + w * 0.28f, top);
                p.cubicTo (l + w * 0.45f, top, l + w * 0.45f, mid, l + w * 0.62f, mid);
                p.lineTo (r, mid);
                break;
            case T::highShelf:
                p.startNewSubPath (l, mid);
                p.lineTo (l + w * 0.38f, mid);
                p.cubicTo (l + w * 0.55f, mid, l + w * 0.55f, top, l + w * 0.72f, top);
                p.lineTo (r, top);
                break;
            case T::highPass:
                p.startNewSubPath (l + w * 0.05f, bottom);
                p.cubicTo (l + w * 0.30f, bottom - (bottom - mid) * 0.2f, l + w * 0.30f, mid, l + w * 0.55f, mid);
                p.lineTo (r, mid);
                break;
            case T::lowPass:
                p.startNewSubPath (l, mid);
                p.lineTo (l + w * 0.45f, mid);
                p.cubicTo (l + w * 0.70f, mid, l + w * 0.70f, bottom - (bottom - mid) * 0.2f, l + w * 0.95f, bottom);
                break;
            case T::notch:
                p.startNewSubPath (l, mid);
                p.lineTo (l + w * 0.40f, mid);
                p.cubicTo (l + w * 0.47f, mid, l + w * 0.48f, bottom, l + w * 0.5f, bottom);
                p.cubicTo (l + w * 0.52f, bottom, l + w * 0.53f, mid, l + w * 0.60f, mid);
                p.lineTo (r, mid);
                break;
        }

        return p;
    }

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
    addAndMakeVisible (lessonPanel);

    // Instrument first: the map of a kick is not the map of a voice, and
    // choosing one is what makes the zones and the lesson specific.
    {
        juce::StringArray names { t ("eq.inst.general", "General") };

        for (const auto& inst : InstrumentMaps::all())
            names.add (t (juce::String ("eq.inst.") + inst.key, inst.english));

        instrumentChips.setCaption (t ("eq.instrument", "Instrument"));
        instrumentChips.setItems (names);
        instrumentChips.onChosen = [this] (int index) { chooseInstrument (index - 1); };
        addAndMakeVisible (instrumentChips);
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

        juce::ignoreUnused (values);
        typeChips.setItems (labels);

        // The shape of each filter rather than its name: an engineer reads
        // a bell, a shelf and a slope faster than the words (the author,
        // 2026-09-24). The name stays as the tooltip.
        typeChips.setIconPainter ([] (juce::Graphics& g, int index, juce::Rectangle<float> box, juce::Colour colour)
        {
            g.setColour (colour);
            g.strokePath (filterShapeIcon (EQCoefficients::typeFromIndex (index), box),
                          juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }, 46);
    }

    typeChips.onHovered = [this] (int value)
    {
        const auto type = EQCoefficients::typeFromIndex (value);
        showGuide (typeName (type) + " - " + t (juce::String ("guide.eq.type.") + juce::String (value),
                                                 juce::String (EQCoefficients::nameForType (type))));
    };

    typeChips.onChosen = [this] (int value)
    {
        if (selectedBand >= 0)
        {
            writeParameter (LearnerEQProcessor::typeParamId (selectedBand), (float) value);
            pushSelectedBandToControls();
        }
    };
    addAndMakeVisible (typeChips);

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

    chooseInstrument (services.libraryProperties.getIntValue ("eqInstrument", 0) - 1);
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

        // Each chip in its band's colour - the same colour as its dot on
        // the curve - filled when it is the one the knobs are turning.
        const auto colour = SpectrumAnalyserComponent::colourForBand (i);
        const auto selected = i == selectedBand;
        chip.setColour (juce::TextButton::buttonColourId, selected ? colour : colour.withAlpha (0.12f));
        chip.setColour (juce::TextButton::textColourOffId, selected ? AbcTrainTheme::current().windowBackground : colour);
    }

    addBandChip.setBounds (row.removeFromLeft (34));
    addBandChip.setEnabled (count < LearnerEQProcessor::maxBands);
}

juce::String LearnerEQEditor::typeName (EQCoefficients::BandType type) const
{
    return t (typeKey (type), EQCoefficients::nameForType (type));
}

void LearnerEQEditor::layoutToolbar (juce::Rectangle<int> area)
{
    placeWithMaterial (instrumentChips, area);
}

void LearnerEQEditor::layoutAnalysis (juce::Rectangle<int> area)
{
    // The curve is the instrument and gets most of the width; the lesson
    // sits beside it, never over it - or, with the companion window open,
    // is in that window and the curve takes the whole width.
    if (lessonInCompanion())
    {
        spectrum.setBounds (area);
        return;
    }

    const auto panelWidth = juce::jlimit (240, 360, area.getWidth() * 3 / 10);
    lessonPanel.setBounds (area.removeFromRight (panelWidth));
    area.removeFromRight (AbcTrainTheme::Spacing::medium);
    spectrum.setBounds (area);
}

void LearnerEQEditor::layoutControls (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;

    // Left: which band, its shape, the bands. Right: its three knobs.
    auto left = area.removeFromLeft (area.getWidth() * 55 / 100);
    area.removeFromLeft (Spacing::medium);

    bandCaptionArea = left.removeFromTop (18);
    left.removeFromTop (rowGap());
    typeChips.setBounds (left.removeFromTop (30));
    left.removeFromTop (rowGap());
    chipRow = left.removeFromTop (30);
    refreshBandChips();

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

    const auto caption = selectedBand >= 0 ? localisation.getText ("eq.band", { { "number", juce::String (selectedBand + 1) } })
                                                 + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + t ("eq.selected", "selected")
                                           : localisation.getText ("eq.noBand");
    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (caption), bandCaptionArea.toFloat(),
                                          AbcTrainLookAndFeel::microFont(), theme.textDim, 1.4f);

    LearnerEditorBase::paintOverChildren (g);
}

void LearnerEQEditor::chooseInstrument (int index)
{
    const auto& instruments = InstrumentMaps::all();
    instrument = juce::isPositiveAndBelow (index, (int) instruments.size()) ? index : -1;
    instrumentChips.setChosen (instrument + 1);

    services.libraryProperties.setValue ("eqInstrument", instrument + 1);
    services.libraryProperties.saveIfNeeded();

    std::vector<SpectrumAnalyserComponent::CustomZone> zones;

    if (instrument >= 0)
    {
        const auto& theme = AbcTrainTheme::current();

        for (const auto& z : instruments[(size_t) instrument].zones)
        {
            SpectrumAnalyserComponent::CustomZone zone;
            zone.lowHz = z.lowHz;
            zone.highHz = z.highHz;
            zone.name = t (juce::String ("eq.zone.") + z.key, z.english);
            zone.colour = z.kind == InstrumentMaps::ZoneKind::problem ? theme.negative
                        : z.kind == InstrumentMaps::ZoneKind::edge ? theme.positive
                                                                   : accent;
            zones.push_back (zone);
        }
    }

    spectrum.setCustomZones (std::move (zones));
    refreshLesson();
}

void LearnerEQEditor::refreshLesson()
{
    if (instrument < 0)
    {
        // No instrument: the general map, and what is under the pointer.
        const auto freq = spectrum.getPointerFrequency();
        juce::String title = t ("eq.general.title", "Where things live"), body = localisation.getText ("eq.zoneHint");

        if (freq > 0.0f)
        {
            const auto& zone = FrequencyZones::zoneFor (freq);
            const auto key = juce::String ("zone.") + zoneKey (zone);
            title = t (key + ".name", zone.name) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + formatFrequency (freq);
            body = t (key + ".feels", zone.feels);
        }

        lessonPanel.setContent (t ("eq.general.caption", "The general map"), title, {}, body,
                                t ("eq.general.foot", "Choose an instrument above for its own map and a lesson."));
        return;
    }

    const auto& inst = InstrumentMaps::all()[(size_t) instrument];
    const auto prefix = juce::String ("eq.lesson.") + inst.key + ".";

    std::vector<InstrumentMaps::BandState> bands;

    for (int band = 0; band < LearnerEQProcessor::maxBands; ++band)
        if (eqProcessor.isBandOn (band))
            bands.push_back ({ eqProcessor.getBandType (band),
                               eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (band))->load(),
                               eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (band))->load() });

    std::vector<LessonPanel::Step> steps;
    auto currentGiven = false;

    for (const auto& step : inst.steps)
    {
        LessonPanel::Step s;
        s.text = t (prefix + step.key, step.english);

        if (InstrumentMaps::isDone (step, bands))
            s.state = LessonPanel::State::done;
        else if (! currentGiven)
        {
            s.state = LessonPanel::State::current;
            currentGiven = true;
        }

        steps.push_back (s);
    }

    const auto name = t (juce::String ("eq.inst.") + inst.key, inst.english);
    lessonPanel.setContent (t ("eq.lesson.caption", "Lesson") + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + name,
                            t (prefix + "title", inst.lessonTitle), std::move (steps), {},
                            t ("eq.lesson.foot", "The coloured zones are where each part usually lives; the room and the mic move them."));
}

void LearnerEQEditor::themeChanged()
{
    const auto& theme = AbcTrainTheme::current();

    for (auto* slider : { &freqSlider, &gainSlider, &qSlider })
    {
        slider->setColour (juce::Slider::textBoxTextColourId, theme.textBright);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    spectrum.setAccentColour (accent);
    waveform.setAccentColour (accent);
    typeChips.setAccent (accent);
    instrumentChips.setAccent (accent);
    lessonPanel.setAccentColour (accent);
    chooseInstrument (instrument);
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
    refreshLesson();
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
    repaint (bandCaptionArea);
}

void LearnerEQEditor::pushSelectedBandToControls()
{
    const auto hasBand = selectedBand >= 0;

    typeChips.setEnabled (hasBand);
    freqSlider.setEnabled (hasBand);
    qSlider.setEnabled (hasBand);

    if (! hasBand)
    {
        gainSlider.setEnabled (false);
        typeChips.setChosen (-1);
        repaint (bandCaptionArea);
        return;
    }

    const auto type = eqProcessor.getBandType (selectedBand);

    // Gain is greyed for the shapes that have none.
    gainSlider.setEnabled (EQCoefficients::usesGain (type));

    if (typeChips.getChosen() != (int) type)
    {
        typeChips.setChosen ((int) type);
        repaint (bandCaptionArea);
    }

    // A mirror of the parameters, never echoed back.
    freqSlider.setValue (eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (selectedBand))->load(), juce::dontSendNotification);
    gainSlider.setValue (eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (selectedBand))->load(), juce::dontSendNotification);
    qSlider.setValue (eqProcessor.apvts.getRawParameterValue (LearnerEQProcessor::qParamId (selectedBand))->load(), juce::dontSendNotification);
}

void LearnerEQEditor::refreshZoneLabel()
{
    // The pointer's zone now reads in the lesson panel (general map only).
    if (instrument < 0)
        refreshLesson();
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
