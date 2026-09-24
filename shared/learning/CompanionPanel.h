#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// The Learner's teaching layer in a window of its own (the author,
// 2026-09-24: "модули перекрывают обзор на то, что происходит в плагине").
//
// The plugin window is the processor: curve, meters, knobs, nothing on top
// of them. Everything that *explains* moves here, into a window you drag
// wherever it is out of the way - beside the plugin, onto a second monitor:
//
//   Modules / Lesson   the module shelf and runner; the instrument lesson
//                      (Learner EQ) - tabs along the top
//   Under the pointer  what the control or range under the mouse does,
//                      which used to be a strip at the foot of the plugin
//   Hearing today      in the app: this session, the week's dose, the next
//                      break, like a usage meter; in a DAW: time with the
//                      plugin open, since the plugin cannot know the dose
//
// The panel does not own the module screen or the lesson: the editor lends
// them and takes them back when the window closes, so closing the window
// returns the plugin to its all-in-one layout.
struct CompanionHearing
{
    bool fromApp = false;          // the app's HearingGuard is behind these
    int sessionMinutes = 0;
    double levelDbA = 0.0;         // 0 = silent or unknown
    double weekFraction = 0.0;     // 1.0 = the week's limit
    bool calibrated = false;
    int minutesUntilBreak = -1;    // -1 = no break reminders
};

class CompanionPanel : public juce::Component
{
public:
    CompanionPanel();

    struct Strings
    {
        juce::String modules { "Modules" }, lesson { "Lesson" };
        juce::String underPointer { "Under the pointer" };
        juce::String underPointerEmpty { "Point at a knob, a band or a range - what it does shows here." };
        juce::String hearing { "Hearing today" };
        juce::String session { "This session" };
        juce::String week { "Week's dose" };
        juce::String untilBreak { "Until a break" };
        juce::String minutes { "{{n}} min" };
        juce::String inMinutes { "in {{n}} min" };
        juce::String off { "off" };
        juce::String notCalibrated { "Calibrate in the app's Settings -> Hearing to see the dose." };
        juce::String pluginTime { "With this plugin open" };
        juce::String pluginNote { "The week's dose is counted by the abcTrain app." };
    };

    void setStrings (Strings);
    void setAccent (juce::Colour);

    // Lent by the editor. `lesson` may be null: that plugin has no lesson
    // beside its modules, and the tabs row shows only "Modules".
    void attach (juce::Component& modules, juce::Component* lesson);
    void detach();

    void showModules();
    void showLesson();

    void setGuide (const juce::String&);
    void setHearing (const CompanionHearing&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> tabsArea() const;
    juce::Rectangle<int> tabBounds (int index) const;
    juce::Rectangle<int> bodyArea() const;
    juce::Rectangle<int> guideArea() const;
    juce::Rectangle<int> hearingArea() const;
    void paintHearing (juce::Graphics&, juce::Rectangle<int>);

    juce::Component* modulesView = nullptr;
    juce::Component* lessonView = nullptr;
    bool lessonShown = false;

    Strings text;
    juce::Colour accent { 0xff5b9bd5 };
    juce::String guide;
    CompanionHearing hearing;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionPanel)
};

// A plain window around the panel, with the system's own title bar. Kept
// above the host while the host is the active application, and a normal
// window when it is not - so it never floats over the mail client.
class CompanionWindow : public juce::DocumentWindow
{
public:
    CompanionWindow (const juce::String& title, CompanionPanel&, juce::Colour background);

    std::function<void()> onCloseRequested;
    void closeButtonPressed() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionWindow)
};
