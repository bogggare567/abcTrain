#include "PluginEditor.h"
#include "EQCoefficients.h"
#include "FrequencyGuide.h"
#include "VocalEqLesson.h"
#include "FindResonanceLesson.h"
#include "../../shared/Version.h"
#include "../../shared/i18n/LocalisationManager.h"
#include <memory>

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
    lookAndFeel.refreshFromTheme();

    setLookAndFeel (&lookAndFeel);

    // Drawn by paint() with letter-spacing rather than via the Label.
    titleLabel.setText ("Learner EQ", juce::dontSendNotification);
    titleLabel.setVisible (false);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

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

        for (auto* slider : { &controls.freqSlider, &controls.gainSlider, &controls.qSlider })
            addAndMakeVisible (slider);

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
        updateButton.setButtonText ("Checking...");

        UpdateChecker::checkForUpdatesAsync (CurrentVersion::string, [safeThis, handled] (bool foundNewer, UpdateChecker::ReleaseInfo release)
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);

            if (! foundNewer)
            {
                safeThis->updateButton.setButtonText ("Up to date");
                juce::Timer::callAfterDelay (2500, [safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->updateButton.setButtonText ("Updates");
                });
                return;
            }

            safeThis->updateButton.setButtonText ("Updates");

            const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
                juce::MessageBoxIconType::InfoIcon,
                "Update Available",
                "Version " + release.tagName + " is available - you're on " + juce::String (CurrentVersion::string) + ".",
                "Open Release Page", "Later",
                safeThis.getComponent());

            juce::AlertWindow::showAsync (options, [release] (int result)
            {
                // makeOptionsOkCancel adds two buttons; per AlertWindow's
                // documented N-button result mapping, button[0] ("Open
                // Release Page") returns 1, button[1] ("Later") returns 0.
                if (result == 1)
                    juce::URL (release.htmlUrl).launchInDefaultBrowser();
            });
        });

        juce::Timer::callAfterDelay (6000, [safeThis, handled]
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);
            safeThis->updateButton.setButtonText ("Couldn't check");

            juce::Timer::callAfterDelay (2500, [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->updateButton.setButtonText ("Updates");
            });
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
    lessonController.onClosed = [this] { resized(); };

    addChildComponent (resonanceLessonController);
    resonanceLessonController.onClosed = [this] { resized(); };

    startTimerHz (30);
    // Taller for the section panels' own padding/captions; the guide text
    // no longer needs a permanent strip (it floats on demand instead).
    setSize (790, 742);

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

    themeButton.setButtonText (theme.mode == AbcTrainTheme::Mode::light ? "Dark" : "Light");
    repaint();
}

void LearnerEQEditor::toggleTheme()
{
    const auto newMode = AbcTrainTheme::getMode() == AbcTrainTheme::Mode::light
                             ? AbcTrainTheme::Mode::dark
                             : AbcTrainTheme::Mode::light;

    AbcTrainTheme::setMode (newMode);
    themeProperties.setValue (themeModeKey, newMode == AbcTrainTheme::Mode::light ? "light" : "dark");

    lookAndFeel.refreshFromTheme();
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

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

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

void LearnerEQEditor::resized()
{
    using namespace AbcTrainTheme;

    lessonController.setBounds (getLocalBounds());
    resonanceLessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (Spacing::large);

    auto titleRow = area.removeFromTop (32);
    lessonSelector.setBounds (titleRow.removeFromRight (156));
    titleRow.removeFromRight (Spacing::small);
    themeButton.setBounds (titleRow.removeFromRight (62));
    titleRow.removeFromRight (Spacing::small);
    updateButton.setBounds (titleRow.removeFromRight (76));
    titleRow.removeFromRight (Spacing::small);
    bypassButton.setBounds (titleRow.removeFromRight (96));
    pluginIcon.setBounds (titleRow.removeFromLeft (28));

    area.removeFromTop (Spacing::section);

    // --- analysis section: response curve + spectrum, waveform, peaks ---
    analysisSection = area.removeFromTop (432);
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
    controlSection = area.removeFromTop (198);
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
