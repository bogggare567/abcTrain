#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "UpdateChecker.h"
#include <functional>

// What happens after "a newer version exists".
//
// Three versions of this have existed and the first two were both fair to
// complain about. It began as a dialogue whose only button opened a web
// page - a notification with homework: find the right file among six
// assets, download it, find it again in Finder. Then it downloaded the
// file and revealed it, which removed the hunting but still ended in
// "now go and do the rest yourself".
//
// It now asks once and then does the whole thing: downloads this
// platform's installer and launches it. What it cannot do, on any
// platform, is replace a plugin binary that a host currently has loaded -
// so the last word is always "restart your DAW". That is not a missing
// feature, it is how loaded dynamic libraries work.
//
// A separate file from UpdateChecker on purpose: ADR 007 keeps
// UpdateChecker.h free of any GUI dependency so its parsing and comparison
// stay testable without a message loop. This is the GUI half.
namespace UpdatePrompt
{
    // Every line of text, already localised by the caller. Pushed in rather
    // than looked up here, the same shape as every other view in this
    // project - shared code has no LocalisationManager of its own.
    struct Strings
    {
        juce::String title;         // "Update available"
        juce::String body;          // "Version {{latest}} is out - you have {{current}}."
        juce::String offerInstall;  // "Download and install {{file}} ({{size}} MB)?"
        juce::String noAsset;       // no installer for this platform in that release
        juce::String updateNow;
        juce::String later;
        juce::String openPage;
        juce::String downloading;   // "Downloading {{percent}}%"
        juce::String opening;
        juce::String failed;
        juce::String installed;     // "...restart your DAW..."
        juce::String devBuild;      // shown instead of a raw "0.0.0-dev+sha..." string
    };

    void offer (const UpdateChecker::ReleaseInfo& release, const Strings&,
                juce::Component* parent, std::function<void (juce::String)> say);

    // Runs a downloaded installer. Exposed for the same reason
    // UpdateChecker's parsers are: it is the one piece with per-platform
    // behaviour worth reading on its own.
    //
    //  - macOS: mounts the .dmg and opens the .pkg inside it, so what the
    //    person sees is Apple's own installer already pointed at our
    //    package rather than a disk-image window to rummage through.
    //  - Windows: runs the -setup.exe. Inno Setup asks for elevation
    //    itself, which is the right place for that question.
    //  - Linux: reveals the .tar.gz. Auto-running a shell script fetched
    //    from the internet is not something this will do on a machine
    //    where the person did not ask for exactly that.
    //
    // Returns false if it could not start anything, in which case the
    // caller should fall back to revealing the file.
    bool launchInstaller (const juce::File& downloaded);
}
