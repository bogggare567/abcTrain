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
// The check works the same way, and for a better reason than it used to.
// **You answer by turning the plugin's own knob** - not a slider this panel
// invented, which was a second answering mechanic in a product that already
// had one and made the plugins feel unlike the trainer. So the knobs must
// stay reachable during the check too; only the analysis section is covered,
// which is also what stops the answer being readable off the spectrum.
//
// The hidden reference is applied through Processor::setCheckOverride rather
// than by writing the parameter, because the knob is bound to that parameter
// and would otherwise move to the answer and give it away.
class ModuleScreenComponent : public juce::Component,
                               private juce::Timer
{
public:
    // The two callbacks are how the panel reaches the processor's check
    // override without knowing which processor it is - the alternative was
    // a template, for two call sites.
    ModuleScreenComponent (juce::AudioProcessorValueTreeState&, ModuleProgress&,
                           PracticeAudioSource&,
                           std::function<void (const juce::String&, float)> setOverride,
                           std::function<void()> clearOverride);
    ~ModuleScreenComponent() override;

    void setModules (std::vector<TrainingModule::Definition>);

    // The plugin's existing multi-knob lessons, listed under the modules
    // rather than behind a second dropdown of their own. A module is one
    // knob; a walkthrough is a whole workflow. Two entry points for "teach
    // me something" in one title row was the confusion this removes.
    void setWalkthroughs (juce::StringArray names);

    // Every caption, localised by the caller. Shared code keeps no
    // LocalisationManager, same as UpdatePrompt.
    struct Strings
    {
        juce::String match, reference, mine, submit, turnKnob;
        juce::String passed, notYet, itWas, youSaid, again, done;
        juce::String phaseWatch, phaseTry, phaseCheck, phaseResult;
        juce::String shelfTitle, shelfSubtitle, walkthroughs, walkthroughWhy;
        juce::String clips, pickCategory, close, back, next, ready;
    };

    void setStrings (Strings);
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
    float currentKnobValue() const;

    const TrainingModule::Definition* currentModule() const;

    juce::AudioProcessorValueTreeState& apvts;
    ModuleProgress& progress;
    PracticeAudioSource& practiceSource;
    std::function<void (const juce::String&, float)> setOverride;
    std::function<void()> clearOverride;

    std::vector<TrainingModule::Definition> modules;
    juce::StringArray walkthroughs;
    Strings text;
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

    // Every rendered bed is kept alive for this panel's whole lifetime
    // rather than freed when the next one is made.
    //
    // This was a real use-after-free on the audio thread: `bed` was a
    // single buffer, playBed() reassigned it, and AudioBuffer's assignment
    // frees the old allocation - the one whose address the processor was
    // still reading through setOverrideBuffer, in the middle of a block.
    // Freeing it under the audio thread is exactly the shape of crash that
    // shows up as "the plugin dies while I am using it" and never in a
    // test. Same fix, and the same accepted memory tradeoff, as
    // ReferenceAudioLibrary::loadedBuffers.
    juce::OwnedArray<juce::AudioBuffer<float>> renderedBeds;

    // 0 = panel over the analysis area, 1 = panel over everything.
    float expansion = 0.0f;
    float expansionTarget = 0.0f;
    float appearAmount = 0.0f;

    juce::TextButton backButton { "Back" }, nextButton { "Next" },
                     readyButton { "I'm ready" }, referenceButton { "Reference" },
                     mineButton { "Mine" }, submitButton { "Submit" },
                     againButton { "Try again" }, doneButton { "Done" },
                     closeButton { "Close" };


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleScreenComponent)
};
