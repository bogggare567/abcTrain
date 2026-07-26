#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// Shown once, on first launch, before the home screen: what this project
// is, and the two ways to help it exist.
//
// **It does not gate anything.** Every button leads onward, and
// "Continue" is always there. Two reasons, both practical rather than
// principled:
//
//  1. Trading software access for a GitHub star is against GitHub's
//     Acceptable Use Policies, which explicitly prohibit incentivised
//     stars as inauthentic engagement. A repo doing it risks being
//     flagged - the opposite of the visibility it was meant to buy.
//  2. It wouldn't work anyway. A plugin cannot verify a star without the
//     user authenticating against GitHub, and any client-side check ships
//     inside the binary where it takes minutes to remove. The earlier
//     "free licence for stargazers" idea was dropped for exactly this
//     (see docs/roadmap.md).
//
// So it asks, once, clearly, and gets out of the way. An ask that people
// can refuse is the only kind worth making in a project that is free
// anyway.
class SupportScreenComponent : public juce::Component
{
public:
    SupportScreenComponent();

    std::function<void()> onDismissed;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton donateButton, starButton, continueButton;
    juce::HyperlinkButton repoLink { "github.com/bogggare567/abcTrain",
                                      juce::URL ("https://github.com/bogggare567/abcTrain") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SupportScreenComponent)
};
