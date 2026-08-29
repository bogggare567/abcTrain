#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AppIcons.h"
#include <functional>
#include <vector>

// What happened in the run that just ended, and what to do next.
//
// Before this, a Survival run ending meant the lives counter hit zero and
// a "Start again" button appeared. Everything the run had produced - how
// many you got right, whether that beat your record, whether your aim was
// improving - was either gone or buried in a corner label. The moment a
// run ends is the one moment a player is actually asking "how did I do",
// and it was the moment the app said least.
//
// **Only the player's own numbers appear here.** No percentiles, no
// comparison against other people: there is no server, and a fabricated
// "better than 80% of players" would be a lie. What it can honestly say
// is how this run compares to *your* best, and where this exercise sits
// among the four skills - which is also the more useful question, because
// it answers "what should I play next".
class RunResultsComponent : public juce::Component,
                             private juce::Timer
{
public:
    struct SkillStanding
    {
        juce::String name;
        AppIcons::Icon icon = AppIcons::Icon::eq;
        int level = 1;
        float levelProgress = 0.0f;
        bool isCurrent = false;
    };

    struct Summary
    {
        juce::String exerciseName;
        juce::String modeName;

        int score = 0;
        int rounds = 0;
        int bestStreakThisRun = 0;

        // The record before this run, so "personal best" can be an event
        // rather than a number nobody can interpret.
        int previousBest = 0;
        bool isNewBest = false;

        // Accuracy across the run, 0..1. Shown next to the lifetime figure
        // for the same exercise, which is the only comparison that means
        // anything without a server.
        float runAccuracy = 0.0f;
        float lifetimeAccuracy = 0.0f;

        // Where the four skill families stand, so the answer to "what
        // next" is on screen rather than requiring a trip home.
        std::vector<SkillStanding> skills;

        // Where this exercise's misses land, in its own division of its
        // subject - the seven named ranges for a frequency exercise, the
        // five reverb types for that one, and so on. See
        // Game::getNumSkillBuckets.
        //
        // This is the only part of a results screen that changes what
        // somebody does tomorrow. Score and accuracy report a run that is
        // already over; "almost every miss was between 500 Hz and 4 kHz"
        // is an instruction. Lifetime rather than this-run, because one
        // run of a dozen rounds cannot tell a weakness from bad luck.
        struct MissBucket
        {
            juce::String label;
            int attempts = 0;
            int misses = 0;

            float missRate() const noexcept
            {
                return attempts > 0 ? (float) misses / (float) attempts : 0.0f;
            }
        };

        std::vector<MissBucket> buckets;

        // Written by the editor, which has the localised strings; empty
        // when there is not enough data to say anything honest.
        juce::String missVerdict;
    };

    RunResultsComponent();
    ~RunResultsComponent() override;

    void show (Summary);

    // Jumps the arrival and the count-up to their end state, for
    // tools/EditorSnapshots - which never pumps a message loop, so a
    // screen whose resting state is "every number still at zero" renders
    // as a screen full of zeroes. Same seam, same reason, as
    // SupportScreenComponent::completeReveal.
    void completeAnimation();

    // Localised captions, pushed in like every other view here.
    void setStrings (juce::String title, juce::String again, juce::String home,
                     juce::String scoreLabel, juce::String accuracyLabel,
                     juce::String streakLabel, juce::String bestLabel,
                     juce::String newBestLabel, juce::String whereYouStand);

    std::function<void()> onPlayAgain;
    std::function<void()> onGoHome;

    // "Try the other one." A run ending is the moment somebody is most
    // willing to change how they play - they have just found out how the
    // last way went - and until now the screen offered only the same mode
    // again or the way out. Takes a SessionManager::Mode as an int so this
    // component keeps knowing nothing about the session.
    std::function<void (int mode)> onModeChosen;
    void setModeOffer (juce::String caption, juce::StringArray modeNames, int currentMode);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::Rectangle<int> cardBounds() const;
    void paintStat (juce::Graphics&, juce::Rectangle<int>, const juce::String& caption,
                    const juce::String& value, juce::Colour valueColour);

    Summary summary;

    // Eased 0..1 arrival, and a separate one for the numbers counting up.
    // The count-up is the whole point of the screen having any motion at
    // all: a score that lands instantly is a fact, a score that arrives is
    // a result.
    float appearAmount = 0.0f;
    float countAmount = 0.0f;

    juce::String titleText, againText, homeText;
    juce::String scoreCaption, accuracyCaption, streakCaption, bestCaption;
    juce::String newBestText, whereYouStandText;

    juce::TextButton againButton, homeButton;

    // At most two: the modes that are not the one just played.
    juce::OwnedArray<juce::TextButton> modeButtons;
    juce::String modeCaption;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RunResultsComponent)
};
