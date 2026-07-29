#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "UpdateChecker.h"
#include <functional>

// What happens after "a newer version exists".
//
// It used to be a dialogue whose only button opened a web page. That is a
// notification with homework attached: find the right file among six
// assets, download it, find it again in Finder. Reported, correctly, as
// not being an update mechanism at all.
//
// Now the dialogue offers to fetch this platform's installer directly, and
// hands back the file. It stops there: it does not run it. Installing a
// plugin writes into a shared system folder, needs admin rights, and would
// be doing it while the host has that very plugin loaded - an updater that
// did that silently is one that can pull the floor out from under a
// session in progress.
//
// A separate file from UpdateChecker on purpose: ADR 007 keeps
// UpdateChecker.h free of any GUI dependency so its parsing and comparison
// stay unit-testable without a message loop. This half is the GUI half.
namespace UpdatePrompt
{
    // `say` is how this reports progress and outcomes - each editor has its
    // own place for a line of text, so none is assumed here.
    void offer (const UpdateChecker::ReleaseInfo& release, juce::Component* parent,
                std::function<void (juce::String)> say);
}
