#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/ui/SegmentedChoice.h"
#include "OwnPluginPlayer.h"
#include "OwnPluginRound.h"
#include "OwnLesson.h"
#include <functional>
#include <memory>
#include <vector>

/**
    «Свой плагин» - training on the player's installed plugins (ADR 065,
    reworked in ADR 067 after the author's verdict: "непонятно, чему что
    учит").

    Left: the plugins on this computer, found without loading any of them,
    grouped by what they are for (equalisers, dynamics, space, saturation,
    modulation, other) or sorted by name or vendor. One row per plugin: its
    AU and VST3 are one plugin, and the VST3 is the one loaded.

    Right, for the chosen plugin: its own window above, and below it three
    steps.

    0. A lesson (ADR 068). The course lessons written for "your own
       plugin" (lessons/own, plugin: any) that this plugin has the knobs
       for: text by steps, and the knobs travel in the plugin's own window
       to values in its own units. The player's setting comes back when the
       lesson ends or is left.

    1. Getting to know it. Every knob whose name the dictionary knows
       (KnobGlossary) gets a card: what it does, what to listen for, and
       two buttons that play the knob turned down and turned up against
       the player's own setting. Knobs it does not know are listed apart:
       "no description - turn it yourself".

    2. Guess. B is a hidden copy of the player's setting with one knob
       moved; the options are that knob's values in the plugin's own
       words. Pressing an option is the answer (no Submit button, as in the
       trainer). The knobs asked about are the known ones the player has
       listened to, unless he ticks others.

    The player's setting is instance A and is never touched: B is a copy,
    and a lesson or a knob card that turns A's knobs puts them back.
    Leaving the page drops both; A's setting is kept in a file per plugin
    and comes back the next time the plugin is opened.
*/
class OwnPluginsPanel : public juce::Component,
                        private juce::ListBoxModel,
                        private juce::Timer
{
public:
    struct Strings
    {
        juce::String title { "Your plugins" };
        juce::String search { "Find a plugin" };
        juce::String scan { "Look for plugins" };
        juce::String scanning { "Looking for plugins..." };
        juce::String found { "{{n}} [[plugin|plugins]] on this computer" };
        juce::String none { "No VST3 or AU plugins found in the standard folders." };
        juce::String pick { "Pick a plugin on the left. Set it the way you like on the practice loop - that is your setting, A." };
        juce::String loading { "Loading {{name}}..." };
        juce::String failed { "{{name}} did not load: {{why}}" };
        juce::String knobs { "Knobs in the round" };
        juce::String noKnobs { "No knob of this plugin can be trained: none of them moves the sound." };
        juce::String options { "Options" };
        juce::String yours { "A - your setting" };
        juce::String hidden { "B - hidden" };
        juce::String dry { "Dry" };
        juce::String start { "Start a round" };
        juce::String next { "Next round" };
        juce::String question { "What is {{knob}} in B?" };
        juce::String right { "Right: {{value}}" };
        juce::String wrong { "It was {{value}} - you chose {{answer}}" };
        juce::String tally { "{{n}} of {{m}} right" };
        juce::String locked { "The plugin's knobs are locked while the round is on" };
        juce::String desktopOnly { "Your own plugins are trained on a computer: the plugins live there." };
        juce::String ownWindow { "The plugin's window is bigger than this page: it is open beside the app" };
        juce::String silent { "{{knob}} does not change this loop with your setting - skipped" };

        // ADR 067: groups, getting to know the knobs, then guessing.
        juce::String sortPurpose { "By purpose" }, sortName { "A-Z" }, sortVendor { "Vendor" };
        juce::StringArray categories { "Equalisers", "Dynamics", "Space", "Saturation", "Modulation", "Other" };
        juce::String learn { "1 - Get to know it" }, guess { "2 - Guess" };
        juce::String learnIntro { "What each knob does and how it sounds. Then guess it by ear." };
        juce::String low { "Less: {{v}}" }, high { "More: {{v}}" }, mine { "Your setting" }, nextKnob { "Next knob" };
        juce::String listenFor { "Listen for: {{what}}" };
        juce::String noDescription { "No description for this one: turn it in the plugin and listen." };
        juce::String unknownCaption { "Without a description" };
        juce::String heard { "{{n}} of {{m}} known knobs heard" };
        juce::String guessIntro { "B is your setting with one knob moved. Which value is it?" };
        juce::String sweep { "Sweep the knob" };
        juce::String copies { "There are {{n}} knobs like this - one per band. Shown here: {{name}}." };
        juce::String watch { "Watch the knob move in the plugin's window." };

        // ADR 068: course lessons on the player's own plugin.
        juce::String lessonsStep { "1 · Lessons" };
        juce::String lessonIntro { "A lesson turns this plugin's own knobs and says what to listen for." };
        juce::String noLessons { "No lesson fits this plugin yet: the lessons need knobs it does not have. Get to know the knobs one by one in the next step." };
        juce::String lessonMeta { "{{n}} [[step|steps]] · {{m}} min" };
        juce::String lessonStart { "Start the lesson" };
        juce::String lessonBack { "Back" };
        juce::String lessonNext { "Next" };
        juce::String lessonDone { "Done" };
        juce::String lessonDry { "Bypass" };
        juce::String lessonStepOf { "Step {{n}} of {{m}}" };
        juce::String lessonSources { "Sources: {{s}}" };
        juce::String lessonFinished { "Lesson done. Your setting is back." };
        juce::String lessonReach { "This plugin cannot reach every value of the step: the knob is at the nearest." };
        juce::String language { "en" };   // a lesson's text is Russian or English (lessons/FORMAT.md)

        // own.g.<key>.what / .listen, by the dictionary's key.
        std::function<juce::String (const juce::String& key)> glossary;
    };

    explicit OwnPluginsPanel (OwnPluginPlayer&);

    // Which course lessons are done: the same list the Studio's lessons
    // keep (LessonsPanel::doneKey), so a lesson is done once, wherever.
    std::function<bool (const juce::String& lessonId)> isLessonDone;
    std::function<void (const juce::String& lessonId)> markLessonDone;
    ~OwnPluginsPanel() override;

    void setStrings (Strings);
    void scanIfNeeded();
    void releasePlugin();   // drops both instances (leaving the page)

    void paint (juce::Graphics&) override;
    void resized() override;

    // Hotkeys own.a / own.b / own.dry / own.next. False: not handled here.
    bool handleHotkey (const juce::String& actionId);

    // EditorSnapshots: a list and a round (or the knob cards), without any
    // plugin installed.
    void showForSnapshot (bool learningStep = false);
    void showLessonForSnapshot();

    // ---- the list (public for the tests) ----
    enum Category { eq = 0, dynamics, space, saturation, modulation, other, instrument };
    static Category categoryFromName (const juce::String& name);
    static juce::String vendorFromName (const juce::String& name);

private:
    struct Variant { juce::String format, id; };
    struct Entry
    {
        juce::String name, vendor;
        Category category = other;
        std::vector<Variant> variants;   // VST3 first
        juce::String formatsText() const;
    };
    struct Row { bool header = false; juce::String caption; int entry = -1; };

    // ListBoxModel (the plugin list)
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void scan();
    void applyFilter();
    void load (const Entry&);
    void rebuildKnobToggles();
    void rebuildLearnKnobs();
    void startRound();
    void timerCallback() override;   // drops a round nobody can hear
    void answer (int option);
    void setListen (OwnPluginPlayer::Listen);
    void refreshRoundUi();
    juce::String fill (juce::String text, const juce::String& key, const juce::String& value) const;
    std::unique_ptr<OwnPluginPlayer::Rig> buildRig (const Variant&, juce::String& error);
    static juce::AudioBuffer<float> practiceLoop (double sampleRate);
    static juce::File settingFile (const juce::String& name, const juce::String& format);
    static void readVst3Info (const juce::File& bundle, juce::String& vendor, Category& category);

    OwnPluginPlayer& player;
    Strings text;

    juce::AudioPluginFormatManager formats;
    std::vector<Entry> all;
    std::vector<Row> rows;
    bool scanned = false;
    bool frozen = false;   // a snapshot: selecting a row loads nothing
    juce::String status;

    juce::TextEditor searchBox;
    SegmentedChoice sortChoice;
    juce::TextButton scanButton;
    juce::ListBox list { "plugins", this };

    Entry current;
    juce::String currentFormat;
    std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;
    std::unique_ptr<juce::DocumentWindow> pluginWindow;   // when the plugin is bigger than the frame
    void placeEditor();
    void showPluginWindow (bool);
    juce::Rectangle<int> editorFrame, roundArea, headerArea, cardArea;

    struct LockCover : public juce::Component
    {
        juce::String message;
        void paint (juce::Graphics&) override;
    };
    LockCover lockCover;

    std::vector<OwnPluginRound::Knob> knobs;
    juce::OwnedArray<juce::ToggleButton> knobToggles;
    juce::Viewport knobViewport;
    juce::Component knobHolder;

    SegmentedChoice optionCount, modeChoice;
    juce::TextButton listenA, listenB, listenDry, startButton;
    juce::OwnedArray<juce::TextButton> optionButtons;

    // ---- getting to know the knobs ----
    struct LearnKnob { int index = -1; juce::String name, key; bool heard = false; int copies = 1; };
    std::vector<LearnKnob> learnKnobs;   // known ones first (in KnobGlossary order), then the unknown
    int learnSelected = 0;
    enum Step { lessonStepId = 0, knobsStepId = 1, guessStepId = 2 };
    bool learning() const { return modeChoice.getValue() == knobsStepId; }
    bool lessonMode() const { return modeChoice.getValue() == lessonStepId; }
    int knownCount() const;
    struct KnobListModel : public juce::ListBoxModel
    {
        explicit KnobListModel (OwnPluginsPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
        void selectedRowsChanged (int) override;
        OwnPluginsPanel& owner;
    };
    KnobListModel knobListModel { *this };
    juce::ListBox knobList { "knobs", &knobListModel };
    juce::TextButton learnLow, learnHigh, learnMine, learnNext, learnSweep;

    // The knob is turned in the plugin's own window, the way a lesson turns
    // a Learner's: on A, so the player sees which knob it is and where it
    // goes. A's value is kept and put back when the card is left.
    int demoIndex = -1;
    float demoSaved = 0.0f;
    struct Motion { int index = -1; float from = 0.0f, to = 0.0f; double startMs = 0.0, lengthMs = 0.0; bool sweep = false; } motion;
    void moveKnob (int index, float target, double lengthMs);
    void restoreDemoKnob (bool animated);
    void stepMotion();

    // Knobs that do the same job in every band (Pro-Q's 24 bands): one of
    // each kind, from a band that is in use.
    std::vector<OwnPluginRound::Knob> distinctKnobs (std::vector<int>* copiesOut = nullptr) const;
    void selectLearnKnob (int);
    void playLearnValue (float normalised);   // B = A with the knob here
    juce::String learnValueText (float normalised) const;
    static constexpr float learnLowValue = 0.15f, learnHighValue = 0.85f;
    float learnPlaying = -1.0f;               // the value B plays, or -1 (A)

    // ---- a course lesson on this plugin (ADR 068) ----
    std::vector<const LessonFile::Lesson*> lessonsHere;   // the ones this plugin has the knobs for
    OwnLesson::KnobMap knobMap;                           // glossary key -> parameter index
    int lessonSelected = 0;
    int lessonStep = -1;                                  // -1: chosen, not started
    std::map<int, float> lessonMine;                      // A as it was when the lesson began
    bool lessonReachProblem = false;
    bool lessonJustFinished = false;
    struct Glide { int index = -1; float from = 0.0f, to = 0.0f; };
    std::vector<Glide> lessonGlides;
    double lessonGlideStartMs = 0.0;
    static constexpr double lessonGlideMs = 700.0;        // LessonRunner::glideMs: the Studio's lessons move at this pace

    struct LessonListModel : public juce::ListBoxModel
    {
        explicit LessonListModel (OwnPluginsPanel& o) : owner (o) {}
        int getNumRows() override;
        void paintListBoxItem (int, juce::Graphics&, int, int, bool) override;
        void selectedRowsChanged (int) override;
        OwnPluginsPanel& owner;
    };
    LessonListModel lessonListModel { *this };
    juce::ListBox lessonList { "lessons", &lessonListModel };
    juce::TextButton lessonBack, lessonNext, lessonDry;

    void rebuildLessons();
    void selectLesson (int);
    void showLessonStep (int);
    void endLesson (bool animated);                       // A back to the player's setting
    void stepLessonGlides();
    const LessonFile::Lesson* chosenLesson() const;
    bool lessonDone (const juce::String& id) const { return isLessonDone != nullptr && isLessonDone (id); }

    OwnPluginRound::Round round;
    bool roundOn = false, answered = false;
    int answeredIndex = -1, rightCount = 0, roundCount = 0;
    juce::String verdict;
    juce::Random random;
    std::vector<int> silentKnobs;   // inaudible with this setting on this loop
    double settleUntilMs = 0.0, checkUntilMs = 0.0;
    bool settled = false;
    bool checking = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OwnPluginsPanel)
};
