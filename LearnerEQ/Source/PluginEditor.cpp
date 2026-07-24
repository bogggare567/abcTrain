#include "PluginEditor.h"
#include "EQCoefficients.h"
#include "FrequencyGuide.h"
#include "VocalEqLesson.h"

namespace
{
    constexpr const char* defaultGuideText = "Drag a band's Freq knob to see what it does.";
}

LearnerEQEditor::LearnerEQEditor (LearnerEQProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      lessonController (p.apvts, buildVocalEqLesson())
{
    titleLabel.setText ("Learner EQ", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    guideLabel.setJustificationType (juce::Justification::centred);
    guideLabel.setFont (juce::Font (14.0f));
    guideLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    guideLabel.setText (defaultGuideText, juce::dontSendNotification);
    addAndMakeVisible (guideLabel);

    addAndMakeVisible (spectrum);

    for (int band = 0; band < LearnerEQProcessor::numBands; ++band)
    {
        auto& controls = bands[(size_t) band];

        controls.nameLabel.setText (EQCoefficients::nameForBand (band), juce::dontSendNotification);
        controls.nameLabel.setJustificationType (juce::Justification::centred);
        controls.nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (controls.nameLabel);

        for (auto* slider : { &controls.freqSlider, &controls.gainSlider, &controls.qSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::deepskyblue);
            addAndMakeVisible (slider);
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

    lessonButton.onClick = [this] { lessonController.showAndStart(); };
    addAndMakeVisible (lessonButton);

    addChildComponent (lessonController);
    lessonController.onClosed = [this] { resized(); };

    startTimerHz (30);
    setSize (760, 480);
}

LearnerEQEditor::~LearnerEQEditor()
{
    processor.setSpectrumAnalyser (nullptr);
}

void LearnerEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e24));
}

void LearnerEQEditor::resized()
{
    lessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (16);

    auto titleRow = area.removeFromTop (32);
    lessonButton.setBounds (titleRow.removeFromRight (80));
    titleLabel.setBounds (titleRow);

    guideLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    spectrum.setBounds (area.removeFromTop (220));
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
}
