#include "PluginEditor.h"
#include "EQCoefficients.h"
#include "FrequencyGuide.h"
#include "VocalEqLesson.h"
#include "../../shared/Version.h"
#include <memory>

namespace
{
    constexpr const char* defaultGuideText = "Drag a band's Freq knob to see what it does.";
}

LearnerEQEditor::LearnerEQEditor (LearnerEQProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      lessonController (p.apvts, buildVocalEqLesson())
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("Learner EQ", juce::dontSendNotification);
    titleLabel.setFont (AbcTrainLookAndFeel::titleFont());
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    guideLabel.setJustificationType (juce::Justification::centred);
    guideLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
    guideLabel.setText (defaultGuideText, juce::dontSendNotification);
    addAndMakeVisible (guideLabel);

    addAndMakeVisible (spectrum);

    addAndMakeVisible (waveform);

    inputPeakLabel.setJustificationType (juce::Justification::centred);
    inputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    inputPeakLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
    addAndMakeVisible (inputPeakLabel);

    outputPeakLabel.setJustificationType (juce::Justification::centred);
    outputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    outputPeakLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
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
            guideLabel.setText (juce::String (EQCoefficients::nameForBand (band)) + ": "
                                     + FrequencyGuide::describe ((float) freqSlider.getValue()),
                                 juce::dontSendNotification);
            spectrum.setHighlightedBand (band);
        };

        freqSlider.onValueChange = [this, band, &freqSlider]
        {
            if (freqSlider.isMouseButtonDown())
                guideLabel.setText (juce::String (EQCoefficients::nameForBand (band)) + ": "
                                         + FrequencyGuide::describe ((float) freqSlider.getValue()),
                                     juce::dontSendNotification);
        };

        freqSlider.onDragEnd = [this]
        {
            guideLabel.setText (defaultGuideText, juce::dontSendNotification);
            spectrum.setHighlightedBand (-1);
        };
    }

    processor.setSpectrumAnalyser (&spectrum);
    processor.setWaveformDisplay (&waveform);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, LearnerEQProcessor::bypassParamId, bypassButton);
    addAndMakeVisible (bypassButton);

    lessonButton.onClick = [this] { lessonController.showAndStart(); };
    addAndMakeVisible (lessonButton);

    addChildComponent (lessonController);
    lessonController.onClosed = [this] { resized(); };

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

    startTimerHz (30);
    setSize (760, 728);
}

LearnerEQEditor::~LearnerEQEditor()
{
    processor.setSpectrumAnalyser (nullptr);
    processor.setWaveformDisplay (nullptr);
    setLookAndFeel (nullptr);
}

void LearnerEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));
}

void LearnerEQEditor::resized()
{
    lessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (16);

    auto titleRow = area.removeFromTop (32);
    lessonButton.setBounds (titleRow.removeFromRight (80));
    titleRow.removeFromRight (8);
    bypassButton.setBounds (titleRow.removeFromRight (90));
    titleRow.removeFromRight (8);
    updateButton.setBounds (titleRow.removeFromRight (80));
    titleRow.removeFromRight (8);
    titleLabel.setBounds (titleRow);

    guideLabel.setBounds (area.removeFromTop (52));
    area.removeFromTop (8);

    spectrum.setBounds (area.removeFromTop (220));
    area.removeFromTop (16);

    waveform.setBounds (area.removeFromTop (130));
    area.removeFromTop (8);

    auto meterRow = area.removeFromTop (20);
    inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 2));
    outputPeakLabel.setBounds (meterRow);
    area.removeFromTop (16);

    auto bandsArea = area;
    const auto columnWidth = bandsArea.getWidth() / LearnerEQProcessor::numBands;

    for (int band = 0; band < LearnerEQProcessor::numBands; ++band)
    {
        auto column = bandsArea.removeFromLeft (columnWidth).reduced (6);
        auto& controls = bands[(size_t) band];

        controls.nameLabel.setBounds (column.removeFromTop (20));
        controls.freqSlider.setBounds (column.removeFromTop (90));
        controls.gainSlider.setBounds (column.removeFromTop (90));
        controls.qSlider.setBounds (column.removeFromTop (90));
    }
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
