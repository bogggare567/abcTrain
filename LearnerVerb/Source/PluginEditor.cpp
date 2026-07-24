#include "PluginEditor.h"
#include "ReverbGuide.h"
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
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setText ("Learner Verb", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    guideLabel.setJustificationType (juce::Justification::centred);
    guideLabel.setFont (juce::Font (13.0f));
    guideLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    guideLabel.setText (defaultGuideText, juce::dontSendNotification);
    addAndMakeVisible (guideLabel);

    addAndMakeVisible (waveform);

    inputPeakLabel.setJustificationType (juce::Justification::centred);
    inputPeakLabel.setFont (juce::Font (13.0f));
    inputPeakLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (inputPeakLabel);

    outputPeakLabel.setJustificationType (juce::Justification::centred);
    outputPeakLabel.setFont (juce::Font (13.0f));
    outputPeakLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
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
        knob.nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (knob.nameLabel);

        knob.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::mediumpurple);
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

    processor.setWaveformDisplay (&waveform);

    startTimerHz (30);
    setSize (760, 560);
}

LearnerVerbEditor::~LearnerVerbEditor()
{
    processor.setWaveformDisplay (nullptr);
}

void LearnerVerbEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e24));
}

void LearnerVerbEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (32));
    typeSelector.setBounds (area.removeFromTop (28).withSizeKeepingCentre (200, 24));
    area.removeFromTop (8);
    guideLabel.setBounds (area.removeFromTop (20));
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
    inputPeakLabel.setText ("In: "
                                 + juce::String (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -60.0f), 1)
                                 + " dB",
                             juce::dontSendNotification);

    outputPeakLabel.setText ("Out: "
                                  + juce::String (juce::Decibels::gainToDecibels (waveform.getOutputPeak(), -60.0f), 1)
                                  + " dB",
                              juce::dontSendNotification);
}
