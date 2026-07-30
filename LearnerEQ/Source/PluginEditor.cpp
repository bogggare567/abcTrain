#include "PluginEditor.h"
#include "../../shared/UpdatePrompt.h"
#include "EQCoefficients.h"
#include "FrequencyGuide.h"
#include "FrequencyZones.h"
#include "VocalEqLesson.h"
#include "FindResonanceLesson.h"
#include "../../shared/Version.h"
#include "../../shared/i18n/LocalisationManager.h"
#include <memory>

namespace
{
    // Every string the update prompt needs, pulled from this editor's own
    // LocalisationManager. The dialogue used to be hardcoded English on a
    // Russian interface, which is exactly the kind of seam that says
    // "this part was bolted on".
    UpdatePrompt::Strings updateStrings (const LocalisationManager& loc)
    {
        UpdatePrompt::Strings s;
        s.title        = loc.getText ("update.title");
        s.body         = loc.getText ("update.body");
        s.offerInstall = loc.getText ("update.offerInstall");
        s.noAsset      = loc.getText ("update.noAsset");
        s.updateNow    = loc.getText ("update.now");
        s.later        = loc.getText ("update.later");
        s.openPage     = loc.getText ("update.openPage");
        s.downloading  = loc.getText ("update.downloading");
        s.opening      = loc.getText ("update.opening");
        s.failed       = loc.getText ("update.failed");
        s.installed    = loc.getText ("update.installed");
        s.devBuild     = loc.getText ("update.devBuild");
        return s;
    }
}

namespace
{
    constexpr const char* themeModeKey = "themeMode";
}

LearnerEQEditor::LearnerEQEditor (LearnerEQProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      lessonController (p.apvts, buildVocalEqLesson()),
      resonanceLessonController (p.apvts, buildFindResonanceLesson()),
      // Same shared "abcTrain" settings folder the language preference
      // uses, so light/dark is one product-wide choice.
      themeProperties (LocalisationManager::makeDefaultOptions())
{

    AbcTrainTheme::setMode (themeProperties.getValue (themeModeKey, "dark") == "light"
                                ? AbcTrainTheme::Mode::light
                                : AbcTrainTheme::Mode::dark);
    accent = AbcTrainTheme::accentFor (AbcTrainTheme::Family::frequency);
    lookAndFeel.refreshFromTheme (accent);

    setLookAndFeel (&lookAndFeel);

    // Drawn by paint() with letter-spacing rather than via the Label.
    titleLabel.setText ("ABC Learner EQ", juce::dontSendNotification);
    titleLabel.setVisible (false);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

    addAndMakeVisible (practiceSelector);

    pluginIcon.setIcon (AppIcons::Icon::learnerEQ);
    addAndMakeVisible (pluginIcon);


    addAndMakeVisible (spectrum);

    addAndMakeVisible (waveform);

    inputPeakLabel.setJustificationType (juce::Justification::centred);
    inputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (inputPeakLabel);

    outputPeakLabel.setJustificationType (juce::Justification::centred);
    outputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (outputPeakLabel);

    // ---- the curve is the instrument ----
    spectrum.onBandSelected = [this] (int band) { selectBand (band); };

    spectrum.onBandMoved = [this] (int band, float freqHz, float gainDb)
    {
        writeParameter (LearnerEQProcessor::freqParamId (band), freqHz);

        // A pass filter or a notch has no gain to move, so vertical drag
        // does nothing for them rather than writing a value the shape
        // ignores. A control that looks like it works and does not is
        // worse than one that visibly does not.
        if (EQCoefficients::usesGain (processor.getBandType (band)))
            writeParameter (LearnerEQProcessor::gainParamId (band), gainDb);

        guideTooltip.setText (juce::String (EQCoefficients::nameForType (processor.getBandType (band)))
                                   + " - " + FrequencyGuide::describe (freqHz));
        pushSelectedBandToControls();
    };

    spectrum.onBandQChanged = [this] (int band, float q)
    {
        writeParameter (LearnerEQProcessor::qParamId (band), q);
        pushSelectedBandToControls();
    };

    spectrum.onBandAdded = [this] (float freqHz, float gainDb)
    {
        // Below 45 Hz a new band is almost always meant to be a high-pass
        // - that is what anyone reaches for down there - so offering a
        // bell first would make the common case the two-step one.
        const auto type = freqHz < 45.0f ? EQCoefficients::BandType::highPass
                                         : EQCoefficients::BandType::bell;

        const auto added = processor.addBand (freqHz, gainDb, type);

        if (added >= 0)
            selectBand (added);
    };

    spectrum.onBandRemoved = [this] (int band)
    {
        processor.removeBand (band);

        if (selectedBand == band)
            selectBand (-1);
    };

    spectrum.onPointerMoved = [this] { refreshZoneLabel(); };

    // ---- the selected band's exact numbers ----
    for (int type = 0; type < EQCoefficients::numTypes; ++type)
        typeSelector.addItem (EQCoefficients::nameForType (EQCoefficients::typeFromIndex (type)), type + 1);

    typeSelector.onChange = [this]
    {
        if (selectedBand < 0)
            return;

        writeParameter (LearnerEQProcessor::typeParamId (selectedBand),
                        (float) (typeSelector.getSelectedId() - 1));
        pushSelectedBandToControls();
    };

    addAndMakeVisible (typeSelector);

    freqSlider.setRange (20.0, 20000.0);
    freqSlider.setSkewFactorFromMidPoint (1000.0);
    gainSlider.setRange (-18.0, 18.0, 0.1);
    qSlider.setRange (0.1, 18.0, 0.01);
    qSlider.setSkewFactorFromMidPoint (1.2);

    // Without this the frequency box read "999.9999390". A slider prints
    // its raw double unless told otherwise, and the number under a knob is
    // the one thing on this panel that has to be exact *and* readable.
    freqSlider.setNumDecimalPlacesToDisplay (0);
    freqSlider.setTextValueSuffix (" Hz");
    gainSlider.setNumDecimalPlacesToDisplay (1);
    gainSlider.setTextValueSuffix (" dB");
    qSlider.setNumDecimalPlacesToDisplay (2);

    freqSlider.onValueChange = [this]
    {
        if (selectedBand < 0)
            return;

        writeParameter (LearnerEQProcessor::freqParamId (selectedBand), (float) freqSlider.getValue());

        if (freqSlider.isMouseButtonDown())
            guideTooltip.setText (FrequencyGuide::describe ((float) freqSlider.getValue()));
    };

    freqSlider.onDragEnd = [this] { guideTooltip.setText ({}); };

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
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 18);
    }

    for (auto* label : { &typeLabel, &freqLabel, &gainLabel, &qLabel })
    {
        label->setJustificationType (juce::Justification::centred);
        label->setFont (AbcTrainLookAndFeel::captionFont());
        addAndMakeVisible (label);
    }

    freqLabel.setText (localisation.getText ("eq.freq"), juce::dontSendNotification);
    gainLabel.setText (localisation.getText ("eq.gain"), juce::dontSendNotification);
    qLabel.setText (localisation.getText ("eq.q"), juce::dontSendNotification);
    refreshZoneLabel();

    zoneLabel.setJustificationType (juce::Justification::centredLeft);
    zoneLabel.setFont (AbcTrainLookAndFeel::bodyFont());
    addAndMakeVisible (zoneLabel);

    zonesButton.setClickingTogglesState (true);
    zonesButton.setToggleState (true, juce::dontSendNotification);
    zonesButton.onClick = [this] { spectrum.setZonesVisible (zonesButton.getToggleState()); };
    addAndMakeVisible (zonesButton);

    selectBand (0);

    // The display's band list arrives on the 30 Hz timer, so without this
    // the curve and its nodes are empty for the first frame after the
    // window opens - and permanently empty anywhere the message loop is
    // not pumped, which is how tools/EditorSnapshots first showed a
    // node-less curve.
    pushBandsToDisplay();

    processor.setSpectrumAnalyser (&spectrum);
    processor.setWaveformDisplay (&waveform);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, LearnerEQProcessor::bypassParamId, bypassButton);
    addAndMakeVisible (bypassButton);

    lessonSelector.addItem ("Vocal EQ Basics", 1);
    lessonSelector.addItem ("Find & Fix a Resonance", 2);
    lessonSelector.onChange = [this]
    {
        // Two lessons, one picker (see decisions/017) - only the selected
        // one is ever shown/started; the other stays hidden.
        if (lessonSelector.getSelectedId() == 1)
            lessonController.showAndStart();
        else if (lessonSelector.getSelectedId() == 2)
            resonanceLessonController.showAndStart();
    };
    // Nothing is selected until a lesson is picked, and an unlabelled
    // empty dropdown in the title row was the result - caught by rendering
    // the editor (tools/EditorSnapshots). Re-cleared on close below, so
    // picking the same lesson twice in a row still starts it.
    lessonSelector.setTextWhenNothingSelected ("Lessons");
    addAndMakeVisible (lessonSelector);

    updateButton.onClick = [this]
    {
        // See CLAUDE.md's Update-checking section: without this,
        // clicking "Updates" gave no visible reaction at all whenever no
        // newer release was found (or this repo simply had no releases
        // yet), since checkForUpdatesAsync's callback deliberately never
        // fires on failure. Now every click gets some visible outcome:
        // "Checking...", then the update prompt, a brief "Up to date",
        // or - if nothing came back within a few seconds - "Couldn't
        // check".
        juce::Component::SafePointer<LearnerEQEditor> safeThis (this);
        auto handled = std::make_shared<bool> (false);

        updateButton.setEnabled (false);
        // The button is now an icon with no room for text, so the outcome
        // goes where there is room: the guide card that already floats in
        // over the visualisation. ADR 014's rule still holds - every click
        // gets a visible outcome - it just gets a better-looking one.
        guideTooltip.setText ("Checking for updates...");

        UpdateChecker::checkForUpdatesAsync (CurrentVersion::string, [safeThis, handled] (bool foundNewer, UpdateChecker::ReleaseInfo release)
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);

            if (! foundNewer)
            {
                safeThis->guideTooltip.setText ("You're on the latest version ("
                                                 + juce::String (CurrentVersion::string) + ").", 4000);
                return;
            }

            safeThis->guideTooltip.setText ({});

            UpdatePrompt::offer (release, updateStrings (safeThis->localisation),
                                  safeThis.getComponent(),
                                  [safeThis] (juce::String text)
                                  {
                                      if (safeThis != nullptr)
                                          safeThis->guideTooltip.setText (text, 8000);
                                  });
        });

        juce::Timer::callAfterDelay (6000, [safeThis, handled]
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);
            safeThis->guideTooltip.setText ("Couldn't reach the update server. "
                                             "Check your connection and try again.", 5000);
        });
    };
    addAndMakeVisible (updateButton);

    soundkorbLink.setFont (AbcTrainLookAndFeel::monoFont().withHeight (13.0f), false,
                            juce::Justification::centredRight);
    addAndMakeVisible (soundkorbLink);

    // After the controls (so it floats above the visualisation it covers)
    // but before the lesson overlays, which must stay on top of everything.
    addAndMakeVisible (guideTooltip);

    // Added last, after every other child, so a shown lesson overlay
    // actually covers the title-row buttons/link instead of them poking
    // through on top of it - the same z-order fix as decisions/015's
    // Training Sounds overlay and decisions/016's soundkorb.ru link.
    addChildComponent (lessonController);
    lessonController.onClosed = [this]
    {
        // Back to "Lessons" so the same one can be started again.
        lessonSelector.setSelectedId (0, juce::dontSendNotification);
        resized();
    };

    addChildComponent (resonanceLessonController);
    resonanceLessonController.onClosed = [this]
    {
        // Back to "Lessons" so the same one can be started again.
        lessonSelector.setSelectedId (0, juce::dontSendNotification);
        resized();
    };

    startTimerHz (30);
    // Taller for the section panels' own padding/captions; the guide text
    // no longer needs a permanent strip (it floats on demand instead).
    // 20 + 32 + 28 + 432 + 12 + 212 + 20, derived from resized().
    // Resizable, with a floor that keeps the layout honest rather than
    // letting somebody squeeze it into nonsense, and a ceiling so the
    // knobs do not end up an inch across on a 5K display.
    setResizable (true, true);
    setResizeLimits (647, 589, 1264, 1134);
    getConstrainer()->setFixedAspectRatio (0.0);

    setSize (790, 756);

    applyTheme();
}

void LearnerEQEditor::applyTheme()
{
    const auto& theme = AbcTrainTheme::current();

    inputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    outputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, theme.accent);

    for (auto* label : { &typeLabel, &freqLabel, &gainLabel, &qLabel })
        label->setColour (juce::Label::textColourId, theme.textDim);

    refreshZoneLabel();

    // The glyph cross-fades rather than cutting, so the toggle reads as
    // one control changing state (see IconButton).
    themeButton.setIcon (theme.mode == AbcTrainTheme::Mode::light ? AppIcons::Icon::moon
                                                                  : AppIcons::Icon::sun);

    spectrum.setAccentColour (accent);
    waveform.setAccentColour (accent);
    pluginIcon.setIconColour (accent);
    repaint();
}

void LearnerEQEditor::toggleTheme()
{
    const auto newMode = AbcTrainTheme::getMode() == AbcTrainTheme::Mode::light
                             ? AbcTrainTheme::Mode::dark
                             : AbcTrainTheme::Mode::light;

    AbcTrainTheme::setMode (newMode);
    themeProperties.setValue (themeModeKey, newMode == AbcTrainTheme::Mode::light ? "light" : "dark");

    // accentFor() returns a different value per mode, so it has to be
    // asked again rather than reused from construction.
    accent = AbcTrainTheme::accentFor (AbcTrainTheme::Family::frequency);
    lookAndFeel.refreshFromTheme (accent);
    applyTheme();

    for (auto* child : getChildren())
        child->repaint();
}

LearnerEQEditor::~LearnerEQEditor()
{
    processor.setSpectrumAnalyser (nullptr);
    processor.setWaveformDisplay (nullptr);
    setLookAndFeel (nullptr);
}

void LearnerEQEditor::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    // Each plugin's own room, the same idea EarTrainer applies per
    // exercise - and the same four family colours, so the trainer and the
    // plugin that teaches the same skill read as one subject.
    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat(), accent);

    AbcTrainLookAndFeel::paintSectionPanel (g, analysisSection.toFloat(), "Analysis");
    AbcTrainLookAndFeel::paintSectionPanel (g, controlSection.toFloat(), "Bands");

    AbcTrainLookAndFeel::paintDisplayWell (g, spectrum.getBounds().toFloat().expanded (1.0f));
    AbcTrainLookAndFeel::paintDisplayWell (g, waveform.getBounds().toFloat().expanded (1.0f));

    AbcTrainLookAndFeel::drawTrackedText (
        g, titleLabel.getText(),
        juce::Rectangle<float> (52.0f, (float) AbcTrainTheme::Spacing::medium,
                                 (float) getWidth() * 0.4f, 32.0f),
        AbcTrainLookAndFeel::titleFont(), theme.textBright, 1.8f,
        juce::Justification::centredLeft);
}

void LearnerEQEditor::paintOverChildren (juce::Graphics& g)
{
    if (bypassVeil <= 0.004f || analysisSection.isEmpty())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (bypassVeil);
    const auto area = analysisSection.toFloat().reduced (AbcTrainTheme::Spacing::medium);

    // Desaturating the analysis rather than hiding it: you still want to
    // see the signal going past, you just need to be able to tell at a
    // glance that nothing is being done to it.
    g.setColour (theme.windowBackground.withAlpha (0.62f * eased));
    g.fillRoundedRectangle (area, AbcTrainTheme::Radius::well);

    AbcTrainLookAndFeel::drawTrackedText (g, "BYPASSED", area.withHeight (20.0f)
                                                              .withY (area.getCentreY() - 10.0f),
                                           AbcTrainLookAndFeel::captionFont(),
                                           theme.textDim.withAlpha (eased), 3.0f,
                                           juce::Justification::centred);
}

void LearnerEQEditor::resized()
{
    using namespace AbcTrainTheme;

    lessonController.setBounds (getLocalBounds());
    resonanceLessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (Spacing::large);

    auto titleRow = area.removeFromTop (32);
    lessonSelector.setBounds (titleRow.removeFromRight (156));
    titleRow.removeFromRight (Spacing::small);
    themeButton.setBounds (titleRow.removeFromRight (30).withSizeKeepingCentre (30, 30));
    titleRow.removeFromRight (Spacing::tight);
    updateButton.setBounds (titleRow.removeFromRight (30).withSizeKeepingCentre (30, 30));
    titleRow.removeFromRight (Spacing::small);
    bypassButton.setBounds (titleRow.removeFromRight (96));
    titleRow.removeFromRight (Spacing::small);
    practiceSelector.setBounds (titleRow.removeFromRight (practiceSelector.getPreferredWidth())
                                    .withSizeKeepingCentre (practiceSelector.getPreferredWidth(), 24));
    pluginIcon.setBounds (titleRow.removeFromLeft (28));

    area.removeFromTop (Spacing::section);

    // --- analysis section: response curve + spectrum, waveform, peaks ---
    // Everything below the analysis has a fixed height on purpose: a
    // rotary that grows is a rotary that stops matching its neighbours,
    // and a preset chip does not get more readable for being taller.
    const auto controlsHeight = 212 + Spacing::medium + 18 + Spacing::small;

    // Everything left over, but never less than what the rows inside it
    // actually ask for.
    //
    // The floor is not a guess: removeFromTop *clamps* to the height
    // available instead of overflowing, so a floor one pixel short does not
    // produce a scrollbar or a warning - it silently shrinks whatever is
    // last inside, which here is the gain-reduction meter. That is the same
    // fault ADR 023 recorded, and this reintroduced it by setting the floor
    // below the content height.
    analysisSection = area.removeFromTop (juce::jmax (432, area.getHeight() - controlsHeight));
    {
        auto inner = analysisSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        spectrum.setBounds (inner.removeFromTop (215).reduced (1));
        inner.removeFromTop (Spacing::medium);
        waveform.setBounds (inner.removeFromTop (124).reduced (1));
        inner.removeFromTop (Spacing::small);

        auto meterRow = inner.removeFromTop (20);
        inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 2));
        outputPeakLabel.setBounds (meterRow);
    }

    area.removeFromTop (Spacing::medium);

    // --- one row for the selected band: type, then its three numbers ---
    // Twelve knobs became four controls that follow your selection. The
    // curve says *where*; this says exactly what.
    controlSection = area.removeFromTop (150);
    {
        auto inner = controlSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        auto zoneRow = inner.removeFromTop (22);
        zonesButton.setBounds (zoneRow.removeFromRight (78).withSizeKeepingCentre (78, 22));
        zoneRow.removeFromRight (Spacing::small);
        zoneLabel.setBounds (zoneRow);

        inner.removeFromTop (Spacing::small);

        const auto columnWidth = inner.getWidth() / 4;

        auto typeColumn = inner.removeFromLeft (columnWidth).reduced (Spacing::small, 0);
        typeLabel.setBounds (typeColumn.removeFromTop (14));
        typeSelector.setBounds (typeColumn.removeFromTop (28));

        const auto place = [&inner, columnWidth] (juce::Label& caption, juce::Slider& slider)
        {
            auto column = inner.removeFromLeft (columnWidth).reduced (Spacing::tight, 0);
            caption.setBounds (column.removeFromTop (14));
            slider.setBounds (column);
        };

        place (freqLabel, freqSlider);
        place (gainLabel, gainSlider);
        place (qLabel, qSlider);
    }

    soundkorbLink.setBounds (area.removeFromBottom (18).removeFromRight (130));

    // The guide card floats over the lower part of the analysis section:
    // close to the knob being dragged, without covering the curve itself.
    guideTooltip.setBounds (analysisSection.reduced (Spacing::large, 0)
                                            .withHeight (70)
                                            .withY (analysisSection.getBottom() - 82));
}

void LearnerEQEditor::timerCallback()
{
    // Bypass used to change nothing on screen, so the only way to know
    // whether you were hearing the plugin was to look at the checkbox.
    // Eased on this timer rather than a second one - 30 Hz over ~260 ms is
    // eight frames, plenty for a fade.
    {
        const auto target = processor.apvts.getRawParameterValue (LearnerEQProcessor::bypassParamId)->load() > 0.5f
                                ? 1.0f : 0.0f;

        if (! juce::approximatelyEqual (bypassVeil, target))
        {
            const auto step = (float) (1000.0 / 30.0 / AbcTrainTheme::Duration::release);

            bypassVeil = std::abs (target - bypassVeil) <= step
                             ? target
                             : bypassVeil + (target > bypassVeil ? step : -step);
            repaint();
        }
    }

    pushBandsToDisplay();

    // A band can go away without this editor doing it - a host automating
    // the On parameter, or a preset load - so the selection is re-checked
    // every frame rather than only when this editor changes it.
    if (selectedBand >= 0 && ! processor.isBandOn (selectedBand))
        selectBand (-1);

    pushSelectedBandToControls();


    inputPeakLabel.setText ("In: "
                                 + juce::String (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -60.0f), 1)
                                 + " dB",
                             juce::dontSendNotification);

    outputPeakLabel.setText ("Out: "
                                  + juce::String (juce::Decibels::gainToDecibels (waveform.getOutputPeak(), -60.0f), 1)
                                  + " dB",
                              juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// The selected band
//
// One set of controls follows the selection instead of one set per band
// owning its own. That rules out APVTS attachments, which bind to a single
// parameter for their lifetime - so values are pushed in on the editor's
// timer and written back through the parameter object, which is what keeps
// host automation, undo and the "someone else moved it" case working.
// ---------------------------------------------------------------------------

void LearnerEQEditor::writeParameter (const juce::String& id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void LearnerEQEditor::selectBand (int band)
{
    selectedBand = (band >= 0 && processor.isBandOn (band)) ? band : -1;
    spectrum.setSelectedBand (selectedBand);
    pushSelectedBandToControls();
}

void LearnerEQEditor::pushSelectedBandToControls()
{
    const auto hasBand = selectedBand >= 0;

    typeSelector.setEnabled (hasBand);
    freqSlider.setEnabled (hasBand);
    qSlider.setEnabled (hasBand);

    if (! hasBand)
    {
        gainSlider.setEnabled (false);
        typeLabel.setText (localisation.getText ("eq.noBand"), juce::dontSendNotification);
        return;
    }

    const auto type = processor.getBandType (selectedBand);

    typeLabel.setText (localisation.getText ("eq.band",
                                              { { "number", juce::String (selectedBand + 1) } }),
                        juce::dontSendNotification);

    // Gain is greyed for the shapes that have none. A pass filter cuts by
    // its slope, not by an amount, and a gain control on it would be a
    // control that lies.
    gainSlider.setEnabled (EQCoefficients::usesGain (type));

    typeSelector.setSelectedId ((int) type + 1, juce::dontSendNotification);

    // dontSendNotification throughout: these are a mirror of the
    // parameters, and echoing them straight back would be a write loop
    // that fights whatever the user is currently dragging.
    freqSlider.setValue (processor.apvts.getRawParameterValue (
        LearnerEQProcessor::freqParamId (selectedBand))->load(), juce::dontSendNotification);
    gainSlider.setValue (processor.apvts.getRawParameterValue (
        LearnerEQProcessor::gainParamId (selectedBand))->load(), juce::dontSendNotification);
    qSlider.setValue (processor.apvts.getRawParameterValue (
        LearnerEQProcessor::qParamId (selectedBand))->load(), juce::dontSendNotification);
}

void LearnerEQEditor::refreshZoneLabel()
{
    const auto freq = spectrum.getPointerFrequency();

    if (freq < 0.0f)
    {
        // The hint for the gesture, when there is nothing to report. The
        // surface has no other affordance saying you can make a band.
        zoneLabel.setText (localisation.getText ("eq.zoneHint"), juce::dontSendNotification);
        zoneLabel.setColour (juce::Label::textColourId, AbcTrainTheme::current().textDim);
        return;
    }

    const auto& zone = FrequencyZones::zoneFor (freq);

    zoneLabel.setText (juce::String (zone.name) + " - " + zone.feels
                           + "   ·   " + juce::String (juce::roundToInt (freq)) + " Hz",
                        juce::dontSendNotification);
    zoneLabel.setColour (juce::Label::textColourId, AbcTrainTheme::current().text);
}

void LearnerEQEditor::pushBandsToDisplay()
{
    // Only the bands that are on. The display draws exactly what the DSP
    // runs, so the curve can never show a band the audio does not have.
    std::vector<SpectrumAnalyserComponent::Band> active;
    active.reserve (LearnerEQProcessor::maxBands);

    for (int band = 0; band < LearnerEQProcessor::maxBands; ++band)
    {
        if (! processor.isBandOn (band))
            continue;

        SpectrumAnalyserComponent::Band entry;
        entry.index = band;
        entry.type = processor.getBandType (band);
        entry.freqHz = processor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (band))->load();
        entry.gainDb = processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (band))->load();
        entry.q = processor.apvts.getRawParameterValue (LearnerEQProcessor::qParamId (band))->load();
        active.push_back (entry);
    }

    const auto sr = processor.getSampleRate();
    spectrum.setEQState (sr > 0.0 ? sr : 44100.0, std::move (active));
}
