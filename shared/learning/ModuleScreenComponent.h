#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/learning/TrainingModule.h"
#include "shared/learning/ModuleProgress.h"
#include "shared/audio/LessonAudioBed.h"
#include "shared/learning/PracticeAudioSource.h"
#include <functional>
#include <vector>

// The training panel inside a Learner plugin: the shelf of modules and
// walkthroughs, and the runner that teaches one and then checks it.
//
// **Where it sits.** The editor gives it exactly the analysis section -
// spectrum, waveform, meters - and never the knobs. While a module is
// explaining (demo, try it) the panel takes only the height its text needs,
// so the spectrum under it still shows what the knobs are doing. During the
// check and the result it covers the whole analysis section: every meter
// there - the output level, the gain reduction, the waveform - can answer
// the question for you, and a test of hearing that can be passed by
// reading a number is not one (ADR 037).
//
// **You answer with the plugin's own knob.** The hidden reference goes into
// the DSP through the processor's check override, past the knob, so the
// knob is yours throughout and its position is the answer.
//
// **A staircase, like the trainer.** Each module has a step 1..10 in
// ModuleProgress; the step sets the accept band, three passes in a row
// take one step narrower, a miss one step wider. The band is drawn on the
// scale in the knob's own units, and the result says the error and the
// band in those units too - "+2.1 dB against ±3 dB".
//
// **Walkthroughs are modules with no check** - a sequence of settings with
// a sentence each. They used to be a second, full-window overlay with its
// own buttons in English; now there is one panel and one set of words.
class ModuleScreenComponent : public juce::Component,
                               private juce::Timer
{
public:
    ModuleScreenComponent (juce::AudioProcessorValueTreeState&, ModuleProgress&,
                           PracticeAudioSource&,
                           std::function<void (const juce::String&, float)> setOverride,
                           std::function<void()> clearOverride);
    ~ModuleScreenComponent() override;

    // Modules first, then walkthroughs (definitions whose check has no
    // parameter), in the order given.
    void setModules (std::vector<TrainingModule::Definition>);

    struct Strings
    {
        juce::String match, reference, mine, submit;
        juce::String passed, notYet, itWas, youSaid, again, done;
        juce::String phaseWatch, phaseTry, phaseCheck, phaseResult;
        juce::String shelfTitle, shelfSubtitle, walkthroughs, walkthroughWhy;
        juce::String close, back, next, ready, finish;

        // Templates with {{placeholders}}.
        juce::String stepOf;          // "Step {{n}} of {{m}}"
        juce::String levelLine;       // "Step {{n}} · within {{tol}}"
        juce::String errorLine;       // "off by {{err}} - the band was {{tol}}"
        juce::String steppedUp;       // "Step {{from}} -> {{to}}: a narrower band"
        juce::String steppedDown;     // "Step {{from}} -> {{to}}: a wider band"
        juce::String toNextStep;      // "{{n}} more in a row for the next step"
        juce::String topStep;         // "The top step - this is as fine as it gets"
        juce::String newRecord;
        juce::String notTried = "Not tried";
        juce::String checkHint;                    // {{knob}}, {{submit}}
        juce::String stepsCount = "{{n}} steps";   // a walkthrough's length

        juce::String decimal = ".";   // "," in languages that write it so
        juce::String octaves = "oct";
    };

    void setStrings (Strings);

    // Module and walkthrough text, looked up by key ("mod.comp.attack.name",
    // ".why", ".try", ".step1" ...), falling back to the English in the
    // definition when a language has no entry. Unit suffixes likewise
    // ("unit.dB", "unit.ms").
    std::function<juce::String (const juce::String& key)> translate;

    void setAccentColour (juce::Colour);
    void prepare (double sampleRate);

    void openShelf();
    std::function<void()> onClosed;

    // After a module's first step is applied - Learner EQ selects the band
    // the check will be about, so the knobs under the panel are its knobs.
    std::function<void (const TrainingModule::Definition&)> onModuleOpened;

    // For tools/EditorSnapshots.
    void completeAnimation();
    void openCheckForSnapshot (int moduleIndex);
    void openResultForSnapshot (int moduleIndex, bool passed);

    bool isRunning() const noexcept { return isVisible() && phase != Phase::shelf; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Phase { shelf, demo, tryIt, check, result };

    void timerCallback() override;

    void openModule (int index);
    void goToPhase (Phase);
    void beginCheck();
    void submitAnswer();
    void closeModule();
    void closePanel();

    void applyStep (const LessonStep&);
    void setParameter (const juce::String& id, float value);
    float getParameter (const juce::String& id) const;
    void playBed (TrainingModule::Bed, int seed);
    void stopBed();

    void saveParameters();
    void restoreParameters();

    bool isWalkthrough (int index) const;
    int numModules() const;

    juce::Rectangle<int> panelBounds() const;
    juce::Rectangle<int> shelfRowBounds (int index) const;
    juce::Rectangle<int> shelfListBounds() const;
    int shelfColumns() const;
    int shelfContentHeight() const;
    void paintModuleCard (juce::Graphics&, int index, juce::Rectangle<int>);
    void paintWalkthroughCard (juce::Graphics&, int index, juce::Rectangle<int>);
    void layoutButtons();
    void paintShelf (juce::Graphics&, juce::Rectangle<int>);
    void paintRunner (juce::Graphics&, juce::Rectangle<int>);
    void paintCheckScale (juce::Graphics&, juce::Rectangle<int>);

    juce::String textFor (const TrainingModule::Definition&, const juce::String& field, const juce::String& fallback) const;
    juce::String stepText (const TrainingModule::Definition&, int step) const;
    juce::String unitSuffix (const TrainingModule::Check&) const;
    juce::String number (float value, int decimals) const;
    juce::String formatValue (float value) const;
    juce::String formatTolerance (const TrainingModule::Check&, int level) const;
    juce::String formatError (const TrainingModule::Check&, float target, float answer, int level) const;
    void refreshAuditionButtons();
    float currentKnobValue() const;

    const TrainingModule::Definition* currentModule() const;

    juce::AudioProcessorValueTreeState& apvts;
    ModuleProgress& progress;
    PracticeAudioSource& practiceSource;
    std::function<void (const juce::String&, float)> setOverride;
    std::function<void()> clearOverride;

    std::vector<TrainingModule::Definition> modules;
    Strings text;
    juce::Colour accent { 0xff5b8def };

    Phase phase = Phase::shelf;
    int moduleIndex = -1;
    int demoStep = 0;
    int hoveredRow = -1;
    float shelfScroll = 0.0f;

    // The check.
    int checkLevel = 1;
    float hiddenTarget = 0.0f;
    float playerValue = 0.0f;
    bool auditioningReference = true;
    ModuleProgress::Outcome lastOutcome;
    int levelBefore = 1;
    juce::Random random;

    std::vector<std::pair<juce::String, float>> savedParameters;

    double bedSampleRate = 44100.0;
    float appearAmount = 0.0f;

    juce::TextButton backButton, nextButton, readyButton, referenceButton,
                     mineButton, submitButton, againButton, doneButton, closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleScreenComponent)
};
