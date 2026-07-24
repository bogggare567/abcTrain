#include "PluginEditor.h"
#include "ParameterGuide.h"
#include "VocalCompressionLesson.h"
#include <array>

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
      lessonController (p.apvts, buildVocalCompressionLesson())
{
    titleLabel.setText ("Learner Comp", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    guideLabel.setJustificationType (juce::Justification::centred);
    guideLabel.setFont (juce::Font (13.0f));
    guideLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    guideLabel.setText (defaultGuideText, juce::dontSendNotification);
    addAndMakeVisible (guideLabel);

    addAndMakeVisible (spectrum);

    addAndMakeVisible (waveform);

    gainReductionLabel.setJustificationType (juce::Justification::centred);
    gainReductionLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    gainReductionLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible (gainReductionLabel);

    inputPeakLabel.setJustificationType (juce::Justification::centred);
    inputPeakLabel.setFont (juce::Font (13.0f));
    inputPeakLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (inputPeakLabel);

    outputPeakLabel.setJustificationType (juce::Justification::centred);
    outputPeakLabel.setFont (juce::Font (13.0f));
    outputPeakLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (outputPeakLabel);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        const auto& spec = knobSpecs[i];

        knob.nameLabel.setText (spec.label, juce::dontSendNotification);
        knob.nameLabel.setJustificationType (juce::Justification::centred);
        knob.nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (knob.nameLabel);

        knob.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::deepskyblue);
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
    bypassButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible (bypassButton);

    for (int i = 0; i < (int) CompressorGuide::presets.size(); ++i)
    {
        auto* button = presetButtons.add (new juce::TextButton (CompressorGuide::presets[(size_t) i].name));
        button->onClick = [this, i] { processor.applyPreset (i); };
        addAndMakeVisible (button);
    }

    processor.setWaveformDisplay (&waveform);
    processor.setSpectrumAnalyzer (&spectrum);

    lessonButton.onClick = [this] { lessonController.showAndStart(); };
    addAndMakeVisible (lessonButton);

    addChildComponent (lessonController);
    lessonController.onClosed = [this] { resized(); };

    startTimerHz (30);
    setSize (820, 780);
}

LearnerCompEditor::~LearnerCompEditor()
{
    processor.setWaveformDisplay (nullptr);
    processor.setSpectrumAnalyzer (nullptr);
}

void LearnerCompEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e24));
}

void LearnerCompEditor::resized()
{
    lessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (16);

    auto titleRow = area.removeFromTop (32);
    lessonButton.setBounds (titleRow.removeFromRight (80));
    titleRow.removeFromRight (8);
    bypassButton.setBounds (titleRow.removeFromRight (90));
    titleRow.removeFromRight (8);
    titleLabel.setBounds (titleRow);

    guideLabel.setBounds (area.removeFromTop (20));
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
