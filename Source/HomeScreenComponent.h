#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AppIcons.h"
#include <functional>
#include <vector>

// The screen you land on. Replaces the "one flat panel with everything on
// it" the editor used to be, and absorbs the training picker that briefly
// lived as an overlay beside it.
//
// Three jobs, in the order they matter:
//  1. Say where you are - level, streak, and the day's challenge.
//  2. Let you pick what to train, grouped by the skill it builds rather
//     than listed in the order the games happen to be registered in. Nine
//     flat entries is a list; four labelled groups is a map.
//  3. Get out of the way - one Start button, and picking a card starts it.
//
// A training can be starred, which pins it to a "Your focus" group above
// everything else. That's the whole "choose what you're interested in"
// idea: rather than an onboarding questionnaire whose answers go stale,
// the shortlist is edited in place, whenever, and persists.
class HomeScreenComponent : public juce::Component,
                             private juce::Timer
{
public:
    struct CardInfo
    {
        int gameIndex = 0;
        juce::String name;
        juce::String benefit;
        juce::String statsLine;
        AppIcons::Icon icon = AppIcons::Icon::eq;
        bool isCurrent = false;
        bool isFavourite = false;
    };

    // A labelled run of cards. Sections are built by the editor so this
    // component needs no opinion about which game trains what.
    struct Section
    {
        juce::String title;
        std::vector<CardInfo> cards;
    };

    HomeScreenComponent();
    ~HomeScreenComponent() override;

    void setSections (std::vector<Section> newSections);

    // Total height the laid-out sections need. The editor puts this
    // component inside a Viewport and sizes it to this, so the catalogue
    // can grow past the window rather than being clipped by it - which is
    // what happened the moment a fourth category existed.
    int getContentHeight() const noexcept { return contentHeight; }
    void setHeader (juce::String title, juce::String subtitle);

    // Chosen training, and star toggles.
    std::function<void (int gameIndex)> onGameChosen;
    std::function<void (int gameIndex, bool shouldBeFavourite)> onFavouriteToggled;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // Flattened layout: section headings and cards resolved to absolute
    // bounds in one pass, so painting and hit-testing can never disagree
    // about where anything is.
    struct LaidOutCard
    {
        juce::Rectangle<int> bounds;
        int sectionIndex = 0;
        int cardIndex = 0;
    };

    struct LaidOutSection
    {
        juce::Rectangle<int> titleBounds;
        int sectionIndex = 0;
    };

    void rebuildLayout();
    void paintCard (juce::Graphics&, const CardInfo&, juce::Rectangle<float>, float hover);
    juce::Rectangle<float> starBoundsFor (juce::Rectangle<float> cardBounds) const;
    int cardAt (juce::Point<int>) const;

    static constexpr int columns = 3;
    static constexpr int cardHeight = 118;
    static constexpr int sectionTitleHeight = 26;

    std::vector<Section> sections;
    std::vector<LaidOutCard> laidOutCards;
    std::vector<LaidOutSection> laidOutSections;

    // Eased hover per laid-out card, parallel to laidOutCards.
    std::vector<float> hoverAmounts;
    int hoveredCard = -1;
    bool hoveredStar = false;

    juce::String headerTitle, headerSubtitle;
    int contentHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeScreenComponent)
};
