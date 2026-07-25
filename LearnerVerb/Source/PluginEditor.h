#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../../shared/WaveformDisplay.h"
#include "../../shared/SpectrumAnalyzer.h"
#include "../../shared/LessonController.h"
#include "../../shared/UpdateChecker.h"
#include "../../shared/AbcTrainLookAndFeel.h"
#include "../../shared/AbcTrainTheme.h"
#include "../../shared/GuideTooltip.h"
#include "../../shared/AppIcons.h"
#include <array>
#include <memory>

class LearnerVerbEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit LearnerVerbEditor (LearnerVerbProcessor&);
    ~LearnerVerbEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // Pushes the active palette into widgets that colour themselves.
    void applyTheme();
    void toggleTheme();

    struct KnobControl
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // Declared first so it's constructed before, and destroyed after,
    // every other Component below - see the class comment on
    // AbcTrainLookAndFeel.
    AbcTrainLookAndFeel lookAndFeel;

    LearnerVerbProcessor& processor;

    juce::Label titleLabel;
    AppIconComponent pluginIcon;
    // Floating, blur-backed guide card, shown only while a control is
    // being dragged - replaces the permanent text strip.
    GuideTooltip guideTooltip;
    SpectrumAnalyzerComponent spectrum;
    WaveformDisplay waveform;
    juce::Label inputPeakLabel;
    juce::Label outputPeakLabel;

    juce::ComboBox typeSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;

    std::array<KnobControl, 6> knobs;

    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::OwnedArray<juce::TextButton> presetButtons;

    // Two lessons (see decisions/017) means a small picker instead of one
    // "Lesson" button - each LessonController owns its own MicroLesson, and
    // only one is ever visible/started at a time (see lessonSelector.onChange).
    juce::ComboBox lessonSelector;
    LessonController lessonController;
    LessonController tailLessonController;

    juce::TextButton updateButton { "Updates" };

    // Light/dark switch, persisted product-wide.
    juce::TextButton themeButton { "Light" };
    juce::PropertiesFile themeProperties;

    // Section backdrops, computed in resized() and drawn in paint().
    juce::Rectangle<int> analysisSection;
    juce::Rectangle<int> controlSection;

    // Product site link - see decisions/016.
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerVerbEditor)
};
