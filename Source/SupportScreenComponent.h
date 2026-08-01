#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/i18n/LocalisationManager.h"
#include <array>
#include <functional>

// The welcome screen: what the name means, and the two ways to help.
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

    // Offered only on a first run, and only offered. The decline is the
    // same size as the accept: a walkthrough somebody feels cornered into
    // is one that teaches them to dismiss things unread.
    std::function<void()> onTourRequested;
    void setTourOffer (juce::String question, juce::String accept, juce::String decline);

    // Re-reads every string and restarts the reveal. Called on a language
    // change, and whenever the screen becomes visible.
    void refresh();

    // Jumps the reveal to its end state.
    //
    // Exists for tools/EditorSnapshots, which deliberately never pumps a
    // message loop - so no Timer fires and every eased value is captured
    // at rest. That is right for hover and for the bypass veil, and wrong
    // for the one animation whose *rest* state is "nothing on screen yet":
    // without this the contact sheet showed this screen with the three
    // words missing entirely, which is worse than no picture at all.
    //
    // The same shape as ProgressManager::registerAnswer - a small public
    // seam that exists because the thing that needs to observe this
    // cannot drive it the normal way.
    void completeReveal();

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;

    // Draws "abcTrain" with the a, b and c tinted their family colours, so
    // the name and the three words below are visibly the same idea.
    void paintWordmark (juce::Graphics&, juce::Rectangle<float>);

    // The block's own bounds, vertically centred inside whatever it is
    // given. Both paint() and resized() start from this, so they cannot
    // disagree about where anything is.
    juce::Rectangle<int> contentArea (juce::Rectangle<int>) const;

    LocalisationManager& localisation;

    juce::Image appIcon;

    // 0..1 per word, staggered. Each eases in and rises a few pixels - the
    // same motion the guide card uses, so the app has one idea of how
    // things arrive rather than three.
    std::array<float, 3> wordReveal { { 0.0f, 0.0f, 0.0f } };

    // The three letters keep bouncing after they have arrived, each a third
    // of a cycle behind the last, so the mark reads as three things rather
    // than one word. It never stops: this screen is the front door and a
    // door with something alive on it is one people do not mind opening.
    double bouncePhase = 0.0;
    double elapsedMs = 0.0;

    juce::TextButton donateButton, starButton, continueButton;
    juce::TextButton tourButton, noTourButton;
    juce::String tourQuestion;
    juce::Rectangle<int> tourQuestionBounds;

    // Where the bouncing wordmark last painted, so the steady-state timer
    // can repaint that strip alone instead of the whole window.
    juce::Rectangle<int> wordmarkRepaintArea;

    // Seconds, for the background instruments. Its own clock, so the
    // wordmark's one-shot sweep can finish and stop while this carries on.
    //
    // **Started mid-scene, not at zero.** Each scene cross-fades in over
    // its first 1.4 seconds, so a clock starting at zero means the screen
    // opens on a second and a half of nothing - which is exactly the
    // emptiness the background exists to fix, arriving at the worst
    // possible moment.
    double ambientPhase = 3.2;
    // One quiet line, every launch. Everything here is a listening test
    // against a small difference, and laptop speakers simply do not
    // reproduce the bottom two octaves or the top one - on those, several
    // exercises are answerable only by guessing. Better to say so once
    // than to let somebody conclude their ears are the problem.
    juce::Label headphoneNote;

    juce::HyperlinkButton repoLink { "github.com/bogggare567/abcTrain",
                                      juce::URL ("https://github.com/bogggare567/abcTrain") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SupportScreenComponent)
};
