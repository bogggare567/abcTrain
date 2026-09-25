#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SeminarHost.h"
#include "shared/ui/AbcTrainLookAndFeel.h"

// What the hall sees: a window meant for the projector or a second screen.
//
// Lobby   - a QR code as large as the screen allows, the address typed out
//           beside it, the room code, and the names as people come in.
// Round   - the question, which version is playing (A clean / B processed),
//           how many have answered, and a small QR for latecomers.
// Answer  - the answer, how the room voted (a bar per choice, or the votes
//           as dots on the scale with the accept band), and how many were
//           right.
// End     - the table.
//
// It opens on the second display, full screen, when there is one; on a
// single laptop it is an ordinary window (F toggles full screen). Keys work
// in it, so a presenter with a clicker can run the room from here: Space -
// show the answer / next, A and B - the version, S - stop the sound.
class ProjectorView : public juce::Component,
                      private juce::Timer
{
public:
    struct Strings
    {
        juce::String scan { "Scan, or open in the phone's browser:" };
        juce::String roomCode { "Room code" };
        juce::String personalCodes { "Personal codes are on your cards" };
        juce::String joined { "In the room: {{n}}" };
        juce::String waitingForStart { "Waiting for the presenter to start" };
        juce::String round { "Round {{n}} of {{m}}" };
        juce::String answered { "Answered: {{n}} of {{m}}" };
        juce::String playingClean { "A - clean" }, playingProcessed { "B - processed" }, silent { "Sound off" };
        juce::String theAnswer { "The answer" };
        juce::String rightCount { "Right: {{n}} of {{m}}" };
        juce::String noVotes { "Nobody answered this one." };
        juce::String finished { "Results" };
        juce::String points { "{{n}}" };
        juce::String lateJoin { "Late? Scan to join" };
        juce::String keys { "Space - answer / next  |  A / B - version  |  S - stop  |  F - full screen" };
        juce::String showAnswer { "Show the answer" }, next { "Next" }, again { "Again" }, start { "Start" };
        juce::String fullScreen { "Full screen" };
    };

    ProjectorView (SeminarHost&, std::function<juce::String()> address);
    ~ProjectorView() override;

    void setStrings (Strings);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    std::function<void()> onToggleFullScreen;
    std::function<void()> onChanged;     // the host changed something from here

    // The address phones open, with or without the room code in it.
    juce::String joinUrl() const;

private:
    void timerCallback() override;
    void refreshButtons();
    void primaryAction();

    void paintLobby (juce::Graphics&, juce::Rectangle<int>, const LocalRoom::Snapshot&);
    void paintRound (juce::Graphics&, juce::Rectangle<int>, const LocalRoom::Snapshot&);
    void paintAnswer (juce::Graphics&, juce::Rectangle<int>, const LocalRoom::Snapshot&);
    void paintFinished (juce::Graphics&, juce::Rectangle<int>, const LocalRoom::Snapshot&);
    void paintQr (juce::Graphics&, juce::Rectangle<float>) const;

    // Its own look: a window of its own does not inherit the editor's.
    AbcTrainLookAndFeel lookAndFeel;
    SeminarHost& host;
    std::function<juce::String()> address;
    Strings text;
    juce::TextButton cleanButton, processedButton, primaryButton, fullButton;
    juce::String lastUrl;
    std::unique_ptr<class QrCode> qr;
    int lastPaintKey = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectorView)
};

class ProjectorWindow : public juce::DocumentWindow
{
public:
    ProjectorWindow (SeminarHost&, std::function<juce::String()> address, const ProjectorView::Strings&);

    void closeButtonPressed() override;
    std::function<void()> onClosed;

    ProjectorView& getView() noexcept { return *view; }

    // Puts the window on the display that is not the main one, full screen,
    // when there is such a display.
    void placeOnBestDisplay();

private:
    ProjectorView* view = nullptr;
};
