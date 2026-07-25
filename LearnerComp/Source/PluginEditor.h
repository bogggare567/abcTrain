#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "../../shared/WaveformDisplay.h"
#include "../../shared/SpectrumAnalyzer.h"
#include "../../shared/LessonController.h"
#include "../../shared/UpdateChecker.h"
#include "../../shared/AbcTrainLookAndFeel.h"
#include "../../shared/AbcTrainTheme.h"
#include "../../shared/AppIcons.h"
#include "../../shared/GuideTooltip.h"
#include "../../shared/GainReductionMeter.h"
#include <array>
#include <memory>

class LearnerCompEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit LearnerCompEditor (LearnerCompProcessor&);
    ~LearnerCompEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // Pushes the active palette into widgets that set their own colours,
    // which the LookAndFeel's colour scheme can't reach.
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

    LearnerCompProcessor& processor;

    juce::Label titleLabel;
    AppIconComponent pluginIcon;

    // Floating, blur-backed guide card - replaces the fixed text strip that
    // used to sit under the title. It appears over the visualisation only
    // while a knob is being dragged, and eases out afterwards.
    GuideTooltip guideTooltip;

    SpectrumAnalyzerComponent spectrum;
    WaveformDisplay waveform;

    // Arc meter with a gradient sweep and a glow that intensifies with
    // reduction, replacing the plain "GR: -3.2 dB" text readout.
    GainReductionMeter gainReductionMeter;

    juce::Label inputPeakLabel;
    juce::Label outputPeakLabel;

    std::array<KnobControl, 7> knobs;
    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    juce::OwnedArray<juce::TextButton> presetButtons;

    // Two lessons (see decisions/017) means a small picker instead of one
    // "Lesson" button - each LessonController owns its own MicroLesson, and
    // only one is ever visible/started at a time (see lessonSelector.onChange).
    juce::ComboBox lessonSelector;
    LessonController lessonController;
    LessonController busGlueLessonController;

    juce::TextButton updateButton { "Updates" };

    // Light/dark switch, persisted in the shared product-wide
    // PropertiesFile so all four plugins agree on the mode.
    juce::TextButton themeButton { "Light" };
    juce::PropertiesFile themeProperties;

    // Section backdrops, computed in resized() and drawn in paint().
    juce::Rectangle<int> analysisSection;
    juce::Rectangle<int> controlSection;

    // Product site link - see decisions/016.
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerCompEditor)
};
