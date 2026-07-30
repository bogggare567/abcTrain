#include "PluginEditor.h"
#include "../../shared/UpdatePrompt.h"
#include "EQCoefficients.h"
#include "FrequencyGuide.h"
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

    for (int band = 0; band < LearnerEQProcessor::numBands; ++band)
    {
        auto& controls = bands[(size_t) band];

        controls.nameLabel.setText (EQCoefficients::nameForBand (band), juce::dontSendNotification);
        controls.nameLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (controls.nameLabel);

        // Three identical knobs per band with only the band's name above
        // them: nothing on screen said which was frequency, which was gain
        // and which was Q. The numbers underneath don't answer it either -
        // 0.70 could be a Q or a gain. Obvious in a rendered editor,
        // invisible to every test.
        const char* const knobCaptions[] = { "Freq", "Gain", "Q" };

        for (int knob = 0; knob < 3; ++knob)
        {
            auto& caption = controls.knobLabels[(size_t) knob];
            caption.setText (knobCaptions[knob], juce::dontSendNotification);
            caption.setJustificationType (juce::Justification::centred);
            caption.setFont (AbcTrainLookAndFeel::captionFont());
            addAndMakeVisible (caption);
        }

        for (auto* slider : { &controls.freqSlider, &controls.gainSlider, &controls.qSlider })
        {
            addAndMakeVisible (slider);

            // See the same call in LearnerComp - and note the order: this
            // must follow addAndMakeVisible, or the slider has no parent
            // and resolves back to JUCE's default LookAndFeel.
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
        }

        controls.freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, LearnerEQProcessor::freqParamId (band), controls.freqSlider);
        controls.gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, LearnerEQProcessor::gainParamId (band), controls.gainSlider);
        controls.qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, LearnerEQProcessor::qParamId (band), controls.qSlider);

        auto& freqSlider = controls.freqSlider;

        freqSlider.onDragStart = [this, band, &freqSlider]
        {
            guideTooltip.setText (juce::String (EQCoefficients::nameForBand (band)) + ": "
                                       + FrequencyGuide::describe ((float) freqSlider.getValue()));
            spectrum.setHighlightedBand (band);
        };

        freqSlider.onValueChange = [this, band, &freqSlider]
        {
            if (freqSlider.isMouseButtonDown())
                guideTooltip.setText (juce::String (EQCoefficients::nameForBand (band)) + ": "
                                           + FrequencyGuide::describe ((float) freqSlider.getValue()));
        };

        freqSlider.onDragEnd = [this]
        {
            guideTooltip.setText ({});
            spectrum.setHighlightedBand (-1);
        };
    }

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

    for (auto& controls : bands)
        controls.nameLabel.setColour (juce::Label::textColourId, theme.textDim);

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

    // --- band section: one column of freq/gain/Q per band ---
    controlSection = area.removeFromTop (212);   // +14 for the Freq/Gain/Q captions
    {
        auto inner = controlSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        const auto columnWidth = inner.getWidth() / LearnerEQProcessor::numBands;
        for (int band = 0; band < LearnerEQProcessor::numBands; ++band)
        {
            auto column = inner.removeFromLeft (columnWidth).reduced (Spacing::tight, 0);
            auto& controls = bands[(size_t) band];

            controls.nameLabel.setBounds (column.removeFromTop (18));

            // Three knobs side by side per band rather than stacked: the
            // stacked layout needed 270px of height per column, which is
            // what forced the window so tall and left the bands cramped.
            const auto knobWidth = column.getWidth() / 3;

            auto captionRow = column.removeFromTop (14);
            for (auto& caption : controls.knobLabels)
                caption.setBounds (captionRow.removeFromLeft (knobWidth));

            controls.freqSlider.setBounds (column.removeFromLeft (knobWidth));
            controls.gainSlider.setBounds (column.removeFromLeft (knobWidth));
            controls.qSlider.setBounds (column);
        }
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

    std::array<float, 4> freqs {}, gains {}, qs {};

    for (int band = 0; band < LearnerEQProcessor::numBands; ++band)
    {
        freqs[(size_t) band] = processor.apvts.getRawParameterValue (LearnerEQProcessor::freqParamId (band))->load();
        gains[(size_t) band] = processor.apvts.getRawParameterValue (LearnerEQProcessor::gainParamId (band))->load();
        qs[(size_t) band] = processor.apvts.getRawParameterValue (LearnerEQProcessor::qParamId (band))->load();
    }

    const auto sr = processor.getSampleRate();
    spectrum.setEQState (sr > 0.0 ? sr : 44100.0, freqs, gains, qs);

    inputPeakLabel.setText ("In: "
                                 + juce::String (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -60.0f), 1)
                                 + " dB",
                             juce::dontSendNotification);

    outputPeakLabel.setText ("Out: "
                                  + juce::String (juce::Decibels::gainToDecibels (waveform.getOutputPeak(), -60.0f), 1)
                                  + " dB",
                              juce::dontSendNotification);
}
