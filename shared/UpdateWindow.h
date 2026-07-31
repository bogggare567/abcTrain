#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "UpdateChecker.h"
#include "InstalledPlugins.h"
#include <functional>
#include <vector>

// The update, as a window you can watch.
//
// The complaint this answers, in the user's words: "непонятно происходит
// обновление или нет" - you could not tell whether anything was happening.
// The progress was real and was reported through a callback that each
// editor routed into a *tooltip*: a line of text that appears next to the
// pointer, over a control nobody is hovering while a 40 MB download runs.
// Work with no visible surface is work that appears not to be happening.
//
// So it is a component now: what is being fetched, how far along it is in
// both a bar and megabytes, what is already installed on this machine and
// where, and what will happen when it finishes. It is a child of the
// editor rather than a native dialogue for the same reason every other
// panel here is - a plugin window inside a host is not a good place to
// start spawning OS windows, and this way it is themed like everything
// else.
//
// **What it deliberately does not claim.** No program can replace a plugin
// binary that a host currently has mapped into memory, and no plugin can
// restart the DAW hosting it. The standalone app *can* relaunch itself and
// does. Everywhere else the last word is "restart your DAW" - not a
// missing feature, but how loaded dynamic libraries work.
class UpdateWindow : public juce::Component,
                      private juce::Timer
{
public:
    struct Strings
    {
        juce::String title;          // "Update available"
        juce::String body;           // "Version {{latest}} is out - you have {{current}}."
        juce::String installedHere;  // "Found on this computer"
        juce::String nothingFound;   // no installed copy located
        juce::String noAsset;        // no installer for this platform
        juce::String install;        // "Download and install"
        juce::String later;
        juce::String cancel;
        juce::String openPage;
        juce::String downloading;    // "Downloading {{done}} of {{total}} MB"
        juce::String opening;        // "Starting the installer..."
        juce::String failed;
        juce::String finishedPlugin; // "...restart your DAW..."
        juce::String finishedApp;    // "...restarting..."
        juce::String versionUnknown;
    };

    UpdateWindow();
    ~UpdateWindow() override;

    void setStrings (Strings);

    // Shows the offer for this release. `runningStandalone` decides which
    // closing sentence is true, so it is asked for rather than guessed.
    void show (UpdateChecker::ReleaseInfo, bool runningStandalone);

    std::function<void()> onClosed;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override;

private:
    enum class Phase { offer, downloading, opening, done, failed };

    void timerCallback() override;
    void beginDownload();
    void finish (juce::File downloaded);
    void layoutButtons();
    juce::Rectangle<int> panelBounds() const;

    Strings text;
    UpdateChecker::ReleaseInfo release;
    std::vector<InstalledPlugins::Found> installed;

    Phase phase = Phase::offer;
    bool standalone = false;

    // Written by the download callback on the message thread, read by
    // paint on the same one - a plain float, not an atomic, because
    // UpdateChecker posts its progress back through callAsync.
    float progress = 0.0f;
    double bytesTotal = 0.0;
    juce::String status;

    // Eased entrance, matching every other panel in the product.
    float appear = 0.0f;

    juce::TextButton installButton, laterButton, cancelButton, doneButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateWindow)
};
