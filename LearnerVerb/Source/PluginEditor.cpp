#include "PluginEditor.h"
#include "ReverbGuide.h"
#include "VocalSpaceLesson.h"
#include "../../shared/Version.h"
#include <array>

namespace
{
    struct KnobSpec
    {
        const char* paramId;
        const char* label;
    };

    const std::array<KnobSpec, 6> knobSpecs {{
        { "decay",    "Decay" },
        { "preDelay", "Pre-Delay" },
        { "size",     "Size" },
        { "damping",  "Damping" },
        { "dryWet",   "Dry/Wet" },
        { "width",    "Width" }
    }};

    constexpr const char* defaultGuideText = "Drag a control to see what it does.";
}

LearnerVerbEditor::LearnerVerbEditor (LearnerVerbProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      lessonController (p.apvts, buildVocalSpaceLesson())
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("Learner Verb", juce::dontSendNotification);
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

    typeSelector.addItem ("Room", 1);
    typeSelector.addItem ("Hall", 2);
    typeSelector.addItem ("Plate", 3);
    typeSelector.addItem ("Spring", 4);
    addAndMakeVisible (typeSelector);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, LearnerVerbProcessor::typeParamId, typeSelector);

    typeSelector.onChange = [this]
    {
        guideLabel.setText (ReverbGuide::describe ("type"), juce::dontSendNotification);
    };

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
            guideLabel.setText (ReverbGuide::describe (paramId), juce::dontSendNotification);
        };
        knob.slider.onDragEnd = [this]
        {
            guideLabel.setText (defaultGuideText, juce::dontSendNotification);
        };
    }

    for (int i = 0; i < (int) ReverbGuide::presets.size(); ++i)
    {
        auto* button = presetButtons.add (new juce::TextButton (ReverbGuide::presets[(size_t) i].name));
        button->onClick = [this, i] { processor.applyPreset (i); };
        addAndMakeVisible (button);
    }

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, LearnerVerbProcessor::bypassParamId, bypassButton);
    addAndMakeVisible (bypassButton);

    processor.setWaveformDisplay (&waveform);
    processor.setSpectrumAnalyzer (&spectrum);

    lessonButton.onClick = [this] { lessonController.showAndStart(); };
    addAndMakeVisible (lessonButton);

    addChildComponent (lessonController);
    lessonController.onClosed = [this] { resized(); };

    updateButton.onClick = [this]
    {
        juce::Component::SafePointer<juce::Component> safeThis (this);

        UpdateChecker::checkForUpdatesAsync (CurrentVersion::string, [safeThis] (bool foundNewer, UpdateChecker::ReleaseInfo release)
        {
            if (! foundNewer || safeThis == nullptr)
                return;

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
    };
    addAndMakeVisible (updateButton);

    startTimerHz (30);
    setSize (760, 748);
}

LearnerVerbEditor::~LearnerVerbEditor()
{
    processor.setWaveformDisplay (nullptr);
    processor.setSpectrumAnalyzer (nullptr);
    setLookAndFeel (nullptr);
}

void LearnerVerbEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));
}

void LearnerVerbEditor::resized()
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

    typeSelector.setBounds (area.removeFromTop (28).withSizeKeepingCentre (200, 24));
    area.removeFromTop (8);
    guideLabel.setBounds (area.removeFromTop (48));
    area.removeFromTop (8);

    spectrum.setBounds (area.removeFromTop (140));
    area.removeFromTop (8);

    waveform.setBounds (area.removeFromTop (150));
    area.removeFromTop (8);

    auto meterRow = area.removeFromTop (20);
    inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 2));
    outputPeakLabel.setBounds (meterRow);

    area.removeFromTop (8);

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
}

void LearnerVerbEditor::timerCallback()
{
    const auto sr = processor.getSampleRate();
    spectrum.setSampleRate (sr > 0.0 ? sr : 44100.0);

    inputPeakLabel.setText ("In: "
                                 + juce::String (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -60.0f), 1)
                                 + " dB",
                             juce::dontSendNotification);

    outputPeakLabel.setText ("Out: "
                                  + juce::String (juce::Decibels::gainToDecibels (waveform.getOutputPeak(), -60.0f), 1)
                                  + " dB",
                              juce::dontSendNotification);
}
