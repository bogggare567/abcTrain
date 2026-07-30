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
#include "../../shared/PracticeSourceSelector.h"
#include "../../shared/i18n/LocalisationManager.h"
#include <array>
#include <memory>

class LearnerEQEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit LearnerEQEditor (LearnerEQProcessor&);
    ~LearnerEQEditor() override;

    void paint (juce::Graphics&) override;

    // The bypass veil has to go over the spectrum and waveform, which are
    // child components - paint() runs underneath them.
    void paintOverChildren (juce::Graphics&) override;
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
        std::array<juce::Label, 3> knobLabels;   // Freq / Gain / Q
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment, gainAttachment, qAttachment;
    };

    // Declared first so it's constructed before, and destroyed after,
    // every other Component below - see the class comment on
    // AbcTrainLookAndFeel.
    AbcTrainLookAndFeel lookAndFeel;


    LearnerEQProcessor& processor;

    // The three Learner plugins were English-only while the trainer had
    // twelve languages - a seam the update dialogue made obvious. Reads
    // the same product-wide preference the trainer writes.
    //
    // Declared *after* `processor` deliberately: members initialise in
    // declaration order, and the first attempt put this above it, so
    // getSharedProperties() ran on a reference that did not exist yet.
    LocalisationManager localisation { processor.getSharedProperties() };
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

    // Icon buttons rather than 76px and 62px of text for two controls
    // pressed once a session - the same treatment EarTrainer's title row
    // already had (see decisions/022).
    IconButton updateButton { AppIcons::Icon::download };

    // Light/dark switch, persisted product-wide.
    IconButton themeButton { AppIcons::Icon::sun };

    // What this plugin listens to when there is no host feeding it -
    // see shared/PracticeSourceSelector.h.
    PracticeSourceSelector practiceSelector { processor.getPracticeLibrary(),
                                              processor.getPracticeSource(),
                                              processor.getSharedProperties(),
                                              [this] { return processor.getSampleRate(); } };
    juce::PropertiesFile themeProperties;

    // This plugin's family colour (see AbcTrainTheme::accentFor). Held as
    // a member because the light and dark variants differ, so it has to be
    // recomputed and re-pushed on every theme change, not just once.
    juce::Colour accent;

    // Eased 0..1 "how bypassed does this look". Driven by the editor's
    // existing 30 Hz timer rather than a second one: bypass used to change
    // nothing on screen at all, so you could not tell by looking whether
    // you were hearing the plugin.
    float bypassVeil = 0.0f;

    // Section backdrops, computed in resized() and drawn in paint().
    juce::Rectangle<int> analysisSection;
    juce::Rectangle<int> controlSection;

    // Product site link - see decisions/016.
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEQEditor)
};
