#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../shared/UpdateWindow.h"
#include "PluginProcessor.h"
#include "../../shared/WaveformDisplay.h"
#include "../../shared/SpectrumAnalyzer.h"
#include "../../shared/LessonController.h"
#include "../../shared/ModuleScreenComponent.h"
#include "../../shared/ModuleProgress.h"
#include "../../shared/UpdateChecker.h"
#include "../../shared/AbcTrainLookAndFeel.h"
#include "../../shared/AbcTrainTheme.h"
#include "../../shared/AppIcons.h"
#include "../../shared/PracticeSourceSelector.h"
#include "../../shared/i18n/LocalisationManager.h"
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

    // The bypass veil has to go over the spectrum and waveform, which are
    // child components - paint() runs underneath them.
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // Snapshot seams for tools/EditorSnapshots.
    void openModuleShelfForSnapshot() { moduleScreen.openShelf(); moduleScreen.completeAnimation(); }
    void openModuleCheckForSnapshot() { moduleScreen.openCheckForSnapshot (2); }

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

    // The three Learner plugins were English-only while the trainer had
    // twelve languages - a seam the update dialogue made obvious. Reads
    // the same product-wide preference the trainer writes.
    //
    // Declared *after* `processor` deliberately: members initialise in
    // declaration order, and the first attempt put this above it, so
    // getSharedProperties() ran on a reference that did not exist yet.
    LocalisationManager localisation { processor.getSharedProperties() };

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

    // Which preset the knobs currently *are*, or -1. A row of identical
    // buttons says "here are four things you may press"; one filled chip
    // among three outlines says "you are here, and here is what else there
    // is" - which is the question a preset row actually answers. Cleared
    // the moment a knob moves, because after that the answer is no longer
    // true.
    int activePreset = -1;
    void refreshPresetChips();

public:
    // Snapshot seam: a screenshot of every knob at its default is a
    // screenshot of a plugin nobody has used yet.
    void applyPresetForSnapshot (int index)
    {
        processor.applyPreset (index);
        activePreset = index;
        refreshPresetChips();
    }

private:

    // Two lessons (see decisions/017) means a small picker instead of one
    // "Lesson" button - each LessonController owns its own MicroLesson, and
    // only one is ever visible/started at a time (see lessonSelector.onChange).
    // Replaced the "Lessons" combo box. A module is one knob, taught then
    // checked; the two walkthroughs above are listed inside the same shelf,
    // so there is one door marked "teach me something" instead of two.
    IconButton modulesButton { AppIcons::Icon::modules };
    ModuleProgress moduleProgress { processor.getSharedProperties() };
    ModuleScreenComponent moduleScreen { processor.apvts, moduleProgress,
                                          processor.getPracticeSource(),
                                          [this] (const juce::String& id, float value)
                                          { processor.setCheckOverride (id, value); },
                                          [this] { processor.clearCheckOverride(); } };
    LessonController lessonController;
    LessonController busGlueLessonController;

    // The two that explain the knobs rather than walking a workflow: what
    // attack actually does to a transient, and where pumping comes from.
    // The per-knob modules teach you to *hit* a value; these teach you why
    // the value is what it is.
    LessonController attackLessonController;
    LessonController releaseLessonController;

    // Icon buttons rather than 76px and 62px of text for two controls
    // pressed once a session - the same treatment EarTrainer's title row
    // already had (see decisions/022).
    IconButton updateButton { AppIcons::Icon::download };

    // Light/dark switch, persisted in the shared product-wide
    // PropertiesFile so all four plugins agree on the mode.
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

    // The update, as something you can watch. Added last of all the
    // children so it paints over everything, including the lesson
    // overlays - an update is the one thing that should not be behind
    // anything.
    UpdateWindow updateWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerCompEditor)
};
