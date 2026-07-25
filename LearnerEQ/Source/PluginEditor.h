#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "SpectrumAnalyser.h"
#include "../../shared/WaveformDisplay.h"
#include "../../shared/LessonController.h"
#include "../../shared/UpdateChecker.h"
#include "../../shared/AbcTrainLookAndFeel.h"
#include "../../shared/AbcTrainTheme.h"
#include "../../shared/GuideTooltip.h"
#include "../../shared/AppIcons.h"
#include <array>
#include <memory>

class LearnerEQEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit LearnerEQEditor (LearnerEQProcessor&);
    ~LearnerEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // Pushes the active palette into widgets that colour themselves.
    void applyTheme();
    void toggleTheme();

    struct BandControls
    {
        juce::Slider freqSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Slider qSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment, gainAttachment, qAttachment;
    };

    // Declared first so it's constructed before, and destroyed after,
    // every other Component below - see the class comment on
    // AbcTrainLookAndFeel.
    AbcTrainLookAndFeel lookAndFeel;

    LearnerEQProcessor& processor;
    SpectrumAnalyserComponent spectrum;
    juce::Label titleLabel;
    AppIconComponent pluginIcon;
    // Floating, blur-backed guide card, shown only while a band's
    // frequency knob is being dragged - replaces the permanent strip.
    GuideTooltip guideTooltip;
    WaveformDisplay waveform;
    juce::Label inputPeakLabel;
    juce::Label outputPeakLabel;
    std::array<BandControls, LearnerEQProcessor::numBands> bands;

    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    // Two lessons (see decisions/017) means a small picker instead of one
    // "Lesson" button - each LessonController owns its own MicroLesson, and
    // only one is ever visible/started at a time (see lessonSelector.onChange).
    juce::ComboBox lessonSelector;
    LessonController lessonController;
    LessonController resonanceLessonController;

    juce::TextButton updateButton { "Updates" };

    // Light/dark switch, persisted product-wide.
    juce::TextButton themeButton { "Light" };
    juce::PropertiesFile themeProperties;

    // Section backdrops, computed in resized() and drawn in paint().
    juce::Rectangle<int> analysisSection;
    juce::Rectangle<int> controlSection;

    // Product site link - see decisions/016.
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEQEditor)
};
