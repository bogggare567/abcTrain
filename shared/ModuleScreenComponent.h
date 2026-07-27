#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TrainingModule.h"
#include "ModuleProgress.h"
#include "LessonAudioBed.h"
#include "PracticeAudioSource.h"
#include <functional>
#include <vector>

// The training-module panel inside a Learner plugin.
//
// **It deliberately does not cover the whole editor.** During the
// demonstration and the try-it-yourself step it sits over the analysis
// section only, so the knobs stay visible and reachable underneath - a
// lesson about a knob you cannot touch is a slideshow. Everything outside
// the painted panel falls through to whatever is beneath it, via hitTest.
//
// During the *check* it takes the whole area instead, and that is not a
// layout convenience: with the spectrum and the knobs on screen the answer
// is readable off the display, and a check you can see the answer to is not
// a check. The panel grows into place rather than jumping, so where it went
// is legible.
class ModuleScreenComponent : public juce::Component,
                               private juce::Timer
{
public:
    ModuleScreenComponent (juce::AudioProcessorValueTreeState&, ModuleProgress&,
                           PracticeAudioSource&);
    ~ModuleScreenComponent() override;

    void setModules (std::vector<TrainingModule::Definition>);

    // The plugin's existing multi-knob lessons, listed under the modules
    // rather than behind a second dropdown of their own. A module is one
    // knob; a walkthrough is a whole workflow. Two entry points for "teach
    // me something" in one title row was the confusion this removes.
    void setWalkthroughs (juce::StringArray names);
    std::function<void (int)> onWalkthroughSelected;
    void setAccentColour (juce::Colour);

    // Rendered at the host's rate, so the beds are in tune with the plugin.
    void prepare (double sampleRate);

    // Opens on the list of modules.
    void openShelf();

    std::function<void()> onClosed;

    // Jumps every eased value to its end state, for tools/EditorSnapshots -
    // which never pumps a message loop. Same seam, same reason, as
    // RunResultsComponent::completeAnimation.
    void completeAnimation();

    // Snapshot seam: opens straight into a module's check, which is the
    // state worth photographing.
    void openCheckForSnapshot (int moduleIndex);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Phase { shelf, demo, tryIt, check, result };

    void timerCallback() override;

    void openModule (int index);
    void goToPhase (Phase);
    void beginCheck();
    void submitAnswer();
    void closeModule();

    void applyStep (const LessonStep&);
    void setParameter (const juce::String& id, float value);
    float getParameter (const juce::String& id) const;
    void playBed (TrainingModule::Bed, int seed);
    void stopBed();

    juce::Rectangle<int> panelBounds() const;
    juce::Rectangle<int> shelfRowBounds (int index) const;
    juce::Rectangle<int> checkBandBounds() const;
    int numShelfRows() const { return (int) modules.size() + walkthroughs.size(); }
    void paintShelf (juce::Graphics&, juce::Rectangle<int>);
    void paintRunner (juce::Graphics&, juce::Rectangle<int>);
    void layoutButtons();
    juce::String formatValue (float value) const;
    static juce::String describeTolerance (const TrainingModule::Check&, int tier);
    void refreshAuditionButtons();

    const TrainingModule::Definition* currentModule() const;

    juce::AudioProcessorValueTreeState& apvts;
    ModuleProgress& progress;
    PracticeAudioSource& practiceSource;

    std::vector<TrainingModule::Definition> modules;
    juce::StringArray walkthroughs;
    juce::Colour accent { 0xff7f77dd };

    Phase phase = Phase::shelf;
    int moduleIndex = -1;
    int demoStep = 0;
    int hoveredRow = -1;

    // The check.
    int checkTier = 1;
    float hiddenTarget = 0.0f;
    float playerValue = 0.0f;
    bool auditioningReference = true;
    bool lastAttemptPassed = false;
    float lastAttemptQuality = 0.0f;
    juce::Random random;

    // What the knobs said before the module touched them, so leaving puts
    // the plugin back the way it was found. A teaching screen that silently
    // rewrites your settings is a teaching screen you stop opening.
    std::vector<std::pair<juce::String, float>> savedParameters;
    void saveParameters();
    void restoreParameters();

    double bedSampleRate = 44100.0;
    juce::AudioBuffer<float> bed;

    // 0 = panel over the analysis area, 1 = panel over everything.
    float expansion = 0.0f;
    float expansionTarget = 0.0f;
    float appearAmount = 0.0f;

    juce::TextButton backButton { "Back" }, nextButton { "Next" },
                     readyButton { "I'm ready" }, referenceButton { "Reference" },
                     mineButton { "Mine" }, submitButton { "Submit" },
                     againButton { "Try again" }, doneButton { "Done" },
                     closeButton { "Close" };

    juce::Slider answerSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleScreenComponent)
};
