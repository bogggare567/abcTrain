#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AppIcons.h"
#include <functional>
#include <vector>

// The whole shelf: every achievement, earned and not, with what each one
// asks for and how close it is.
//
// **The locked ones are the point.** It would be easy to hide them behind
// question marks and call it mystery; that would throw away what this
// screen is actually for. The unearned list is a *map of the subject* -
// it says that spotting a plate and spotting a long hall are two
// different abilities, that accuracy and endurance are scored separately,
// that there is such a thing as being good at delay times. Someone who
// reads it comes away knowing more about what there is to learn than they
// did before, whether or not they ever earn a single one.
//
// See docs/user-journey.md for why this screen exists at all rather than
// only the home-screen strip.
class AchievementsScreenComponent : public juce::Component,
                                     private juce::Timer
{
public:
    struct Entry
    {
        juce::String name;
        juce::String description;
        AppIcons::Icon icon = AppIcons::Icon::eq;
        juce::Colour tint;
        juce::String tierName;
        bool earned = false;
        float progress = 0.0f;
    };

    AchievementsScreenComponent();
    ~AchievementsScreenComponent() override;

    void setEntries (std::vector<Entry>);
    void setStrings (juce::String title, juce::String subtitle, juce::String close);

    std::function<void()> onClosed;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    juce::Rectangle<int> cardBounds() const;
    juce::Rectangle<int> listBounds() const;
    void paintEntry (juce::Graphics&, const Entry&, juce::Rectangle<int>, float hover);

    std::vector<Entry> entries;
    std::vector<float> hoverAmounts;
    int hoveredRow = -1;

    float scrollOffset = 0.0f;
    float maxScroll = 0.0f;

    juce::String titleText, subtitleText;
    juce::TextButton closeButton;

    static constexpr int rowHeight = 54;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AchievementsScreenComponent)
};
