#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LessonRunner.h"
#include "LessonLibrary.h"
#include <functional>

// The Studio's lessons window (ADR 052, the approved "Окно уроков"
// mockup): courses on the left, one lesson on the right, read a step at a
// time while the Studio plugin behind it turns its own knobs.
//
// It is a window of its own rather than a page because the point is to
// watch the plugin while reading: a page would cover the very knobs the
// lesson is moving.
class LessonsPanel : public juce::Component
{
public:
    struct Host
    {
        // Shows the Studio on this plugin (0 EQ, 1 Comp, 2 Verb) and
        // returns its processor; the lesson turns its knobs.
        std::function<juce::AudioProcessor& (int effect)> showEffect;

        // The editor on show in the Studio, for @highlight (may be null).
        std::function<juce::AudioProcessorEditor*()> currentEditor;

        std::function<juce::String (const juce::String& key)> text;
        std::function<juce::String (const juce::String& key, const std::map<juce::String, juce::String>&)> textWith;
        juce::String language;       // "ru", "en", ...

        juce::PropertiesFile* properties = nullptr;   // which lessons are done

        // The app's look: a window of its own does not inherit it from the
        // editor, and without it the buttons came out in stock JUCE.
        juce::LookAndFeel* lookAndFeel = nullptr;
    };

    explicit LessonsPanel (Host);
    ~LessonsPanel() override;

    void openLesson (const juce::String& id);   // also for snapshots
    void goToStep (int index);
    void closeLesson();
    void finishGlide() { runner.finish(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr int railWidth = 330;
    static constexpr const char* doneKey = "lessonsDone";

private:
    class Rail;

    bool isDone (const juce::String& id) const;
    void markDone (const juce::String& id);
    void applyStep();
    void applyMaterial (const juce::String& material);
    void setHighlight (juce::Range<float>);
    int effectIndex() const;

    juce::Rectangle<int> readerBounds() const;
    juce::Rectangle<int> stepListBounds() const;
    int stepRowAt (juce::Point<int>) const;

    Host host;
    LessonRunner runner;
    std::vector<LessonLibrary::Course> courses;

    const LessonFile::Lesson* lesson = nullptr;
    int step = 0;
    int hoveredStep = -1;
    juce::String playingMaterial;

    std::unique_ptr<Rail> rail;
    juce::Viewport railView;
    juce::TextButton backButton, nextButton, coursesButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LessonsPanel)
};

// The panel in an ordinary desktop window, closed with its close button.
class LessonsWindow : public juce::DocumentWindow
{
public:
    LessonsWindow (const juce::String& title, LessonsPanel::Host);
    void closeButtonPressed() override { setVisible (false); if (onClose) onClose(); }

    LessonsPanel& panel() { return *content; }
    std::function<void()> onClose;

private:
    LessonsPanel* content = nullptr;
};
