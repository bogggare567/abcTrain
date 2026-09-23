#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/i18n/LocalisationManager.h"
#include <array>
#include <functional>

// The welcome screen: what the name means, what is inside, and - on a
// second step - where an account will fit once Live exists.
//
// **Two steps, and the second one is honest.** Step one says what the app
// holds: Trainings, the Studio (the Learners running inside the app, ADR
// 041) and Live, marked "soon". Step two shows the sign-in that Live will
// use - Telegram or a six-digit code by e-mail - greyed out and labelled
// "coming with Live", with "Continue without an account" as the one
// primary action. Nothing on it connects anywhere: the installed app stays
// offline (CLAUDE.md), and a sign-in form that did something would break
// that. It is there so the account arrives later as a known thing rather
// than a surprise.
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
//     inside the binary where it takes minutes to remove.
//
// So it asks, once, clearly, and gets out of the way.
//
// What it also does now is answer a question the app was leaving open:
// the product is called abcTrain and the window said "Ear Trainer", with
// nothing anywhere explaining either. The three letters stand for the
// three things these exercises actually train - **ambiance, balance,
// clarity** - and they arrive one at a time over about a second, each in
// the colour of the family it names. It is the only animation in the app
// that exists purely to be looked at, and it is on the one screen a
// player sees exactly once.
class SupportScreenComponent : public juce::Component,
                                private juce::Timer
{
public:
    explicit SupportScreenComponent (LocalisationManager&);
    ~SupportScreenComponent() override;

    std::function<void()> onDismissed;

    // Offered only on a first run, and only offered. The decline ("Next")
    // is right beside it: a walkthrough somebody feels cornered into is one
    // that teaches them to dismiss things unread.
    std::function<void()> onTourRequested;
    void setTourOffer (juce::String question, juce::String accept, juce::String decline);

    // Re-reads every string and restarts the reveal. Called on a language
    // change, and whenever the screen becomes visible (which also returns
    // it to step one).
    void refresh();

    // 0 = what is inside, 1 = account.
    void showStep (int);
    int getStep() const noexcept { return step; }

    // Jumps the reveal to its end state, for tools/EditorSnapshots, which
    // never pumps a message loop - without it a still frame shows the
    // screen before anything has arrived.
    void completeReveal();

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void layout();
    void paintInside (juce::Graphics&);
    void paintAccount (juce::Graphics&);

    // "abcTrain" with a, b and c in their family colours, left-aligned.
    void paintWordmark (juce::Graphics&, juce::Rectangle<float>);

    LocalisationManager& localisation;

    int step = 0;

    // 0..1 per item, staggered: the three words under the wordmark and the
    // three cards arrive together, one pair at a time.
    std::array<float, 3> wordReveal { { 0.0f, 0.0f, 0.0f } };
    double sweepPhase = 0.0;
    double elapsedMs = 0.0;

    // Seconds, for the background instruments. Started mid-scene - each
    // scene cross-fades in over its first 1.4 s, so zero opens on nothing.
    double ambientPhase = 3.2;

    // Step one.
    juce::TextButton tourButton, nextButton;
    juce::HyperlinkButton donateLink, starLink;
    bool tourOffered = false;

    // Step two. The sign-in controls are real components, disabled, so
    // they look exactly like what will be there - not a picture of it.
    juce::TextButton backButton, continueButton;
    juce::TextButton telegramButton, sendCodeButton;
    juce::TextEditor emailField;

    // Laid out once in layout(), read by paint(), so the two cannot drift.
    struct Layout
    {
        juce::Rectangle<int> wordmark, words, tagline, stepLabel, note;
        std::array<juce::Rectangle<int>, 3> cards;
        juce::Rectangle<int> heading, body, offline, panel, signIn, orLabel, emailNote, privacy;
    } lay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SupportScreenComponent)
};
