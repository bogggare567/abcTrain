#include "PluginEditor.h"
#include "ParameterGuide.h"
#include "VocalCompressionLesson.h"
#include "BusGlueLesson.h"
#include "../../shared/Version.h"
#include <array>
#include <memory>

namespace
{
    struct KnobSpec
    {
        const char* paramId;
        const char* label;
    };

    const std::array<KnobSpec, 7> knobSpecs {{
        { "threshold", "Threshold" },
        { "ratio",     "Ratio" },
        { "attack",    "Attack" },
        { "release",   "Release" },
        { "knee",      "Knee" },
        { "makeup",    "Makeup" },
        { "dryWet",    "Dry/Wet" }
    }};

    constexpr const char* defaultGuideText = "Drag a knob to see what it does.";
}

LearnerCompEditor::LearnerCompEditor (LearnerCompProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      lessonController (p.apvts, buildVocalCompressionLesson()),
      busGlueLessonController (p.apvts, buildBusGlueLesson())
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("Learner Comp", juce::dontSendNotification);
    titleLabel.setFont (AbcTrainLookAndFeel::titleFont());
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    pluginIcon.setIcon (AppIcons::Icon::learnerComp);
    addAndMakeVisible (pluginIcon);

    guideLabel.setJustificationType (juce::Justification::centred);
    guideLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
    guideLabel.setText (defaultGuideText, juce::dontSendNotification);
    addAndMakeVisible (guideLabel);

    addAndMakeVisible (spectrum);

    addAndMakeVisible (waveform);

    gainReductionLabel.setJustificationType (juce::Justification::centred);
    gainReductionLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    gainReductionLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd98c5f));
    addAndMakeVisible (gainReductionLabel);

    inputPeakLabel.setJustificationType (juce::Justification::centred);
    inputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    inputPeakLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
    addAndMakeVisible (inputPeakLabel);

    outputPeakLabel.setJustificationType (juce::Justification::centred);
    outputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    outputPeakLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
    addAndMakeVisible (outputPeakLabel);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        const auto& spec = knobSpecs[i];

        knob.nameLabel.setText (spec.label, juce::dontSendNotification);
        knob.nameLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (knob.nameLabel);

        addAndMakeVisible (knob.slider);

        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, spec.paramId, knob.slider);

        const juce::String paramId (spec.paramId);
        knob.slider.onDragStart = [this, paramId]
        {
            guideLabel.setText (CompressorGuide::describe (paramId), juce::dontSendNotification);
        };
        knob.slider.onDragEnd = [this]
        {
            guideLabel.setText (defaultGuideText, juce::dontSendNotification);
        };
    }

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, LearnerCompProcessor::bypassParamId, bypassButton);
    addAndMakeVisible (bypassButton);

    for (int i = 0; i < (int) CompressorGuide::presets.size(); ++i)
    {
        auto* button = presetButtons.add (new juce::TextButton (CompressorGuide::presets[(size_t) i].name));
        button->onClick = [this, i] { processor.applyPreset (i); };
        addAndMakeVisible (button);
    }

    processor.setWaveformDisplay (&waveform);
    processor.setSpectrumAnalyzer (&spectrum);

    lessonSelector.addItem ("Vocal Compression", 1);
    lessonSelector.addItem ("Bus Glue Compression", 2);
    lessonSelector.onChange = [this]
    {
        // Two lessons, one picker (see decisions/017) - only the selected
        // one is ever shown/started; the other stays hidden.
        if (lessonSelector.getSelectedId() == 1)
            lessonController.showAndStart();
        else if (lessonSelector.getSelectedId() == 2)
            busGlueLessonController.showAndStart();
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
        juce::Component::SafePointer<LearnerCompEditor> safeThis (this);
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

    soundkorbLink.setFont (AbcTrainLookAndFeel::monoFont(), false, juce::Justification::centredRight);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xff5b9bd5));
    addAndMakeVisible (soundkorbLink);

    // Added last, after every other child, so a shown lesson overlay
    // actually covers the title-row buttons/link instead of them poking
    // through on top of it - the same z-order fix as decisions/015's
    // Training Sounds overlay and decisions/016's soundkorb.ru link.
    addChildComponent (lessonController);
    lessonController.onClosed = [this] { resized(); };

    addChildComponent (busGlueLessonController);
    busGlueLessonController.onClosed = [this] { resized(); };

    startTimerHz (30);
    setSize (820, 808);
}

LearnerCompEditor::~LearnerCompEditor()
{
    processor.setWaveformDisplay (nullptr);
    processor.setSpectrumAnalyzer (nullptr);
    setLookAndFeel (nullptr);
}

void LearnerCompEditor::paint (juce::Graphics& g)
{
    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());
}

void LearnerCompEditor::resized()
{
    lessonController.setBounds (getLocalBounds());
    busGlueLessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (16);

    auto titleRow = area.removeFromTop (32);
    lessonSelector.setBounds (titleRow.removeFromRight (150));
    titleRow.removeFromRight (8);
    bypassButton.setBounds (titleRow.removeFromRight (90));
    titleRow.removeFromRight (8);
    updateButton.setBounds (titleRow.removeFromRight (80));
    titleRow.removeFromRight (8);
    pluginIcon.setBounds (titleRow.removeFromLeft (28));
    titleRow.removeFromLeft (4);
    titleLabel.setBounds (titleRow);

    guideLabel.setBounds (area.removeFromTop (48));
    area.removeFromTop (8);

    spectrum.setBounds (area.removeFromTop (140));
    area.removeFromTop (8);

    waveform.setBounds (area.removeFromTop (160));
    area.removeFromTop (8);

    auto meterRow = area.removeFromTop (28);
    inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 3));
    gainReductionLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 2));
    outputPeakLabel.setBounds (meterRow);

    area.removeFromTop (16);

    auto knobRow = area.removeFromTop (110);
    const auto knobWidth = knobRow.getWidth() / (int) knobs.size();
    for (auto& knob : knobs)
    {
        auto column = knobRow.removeFromLeft (knobWidth).reduced (4);
        knob.nameLabel.setBounds (column.removeFromTop (18));
        knob.slider.setBounds (column);
    }

    area.removeFromTop (16);

    auto bottomRow = area.removeFromTop (32);
    const auto presetWidth = bottomRow.getWidth() / juce::jmax (1, presetButtons.size());
    for (auto* button : presetButtons)
        button->setBounds (bottomRow.removeFromLeft (presetWidth).reduced (4, 0));

    area.removeFromTop (8);
    soundkorbLink.setBounds (area.removeFromTop (18).removeFromRight (130));
}

void LearnerCompEditor::timerCallback()
{
    const auto sr = processor.getSampleRate();
    spectrum.setSampleRate (sr > 0.0 ? sr : 44100.0);

    gainReductionLabel.setText ("GR: " + juce::String (waveform.getCurrentHighlightAmount(), 1) + " dB",
                                 juce::dontSendNotification);

    inputPeakLabel.setText ("In: "
                                 + juce::String (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -60.0f), 1)
                                 + " dB",
                             juce::dontSendNotification);

    outputPeakLabel.setText ("Out: "
                                  + juce::String (juce::Decibels::gainToDecibels (waveform.getOutputPeak(), -60.0f), 1)
                                  + " dB",
                              juce::dontSendNotification);
}
