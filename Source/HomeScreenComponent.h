#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AppIcons.h"
#include <functional>
#include <vector>

// The screen you land on: every exercise at once, and what you have won.
//
// It used to be a scrolling list of tall description cards grouped into
// four named categories. Three things were wrong with that, and all three
// came from the same instinct to explain rather than to show:
//
//  - **It scrolled.** Nine exercises, four visible. Choosing between nine
//    things you cannot see at the same time is not choosing.
//  - **Every card carried a paragraph.** You read it once, on the first
//    day, and then it was nine paragraphs of furniture forever.
//  - **The category headings cost a row each** to say something the icons
//    and the ordering already say.
//
// Now: one flat grid of icon tiles, all nine on screen, no scrolling, no
// headings. The description moves to a hover tip - there when it is
// wanted, invisible when it is not. Underneath, achievements as a
// horizontal strip of badges, earned ones lit and locked ones dim, which
// is the one place in this app where seeing what you have *not* got yet
// is the entire point.
//
// A tile can be starred, which sorts it to the front. That is the whole
// "pick what interests you" idea, without an onboarding questionnaire
// whose answers go stale.
class HomeScreenComponent : public juce::Component,
                             private juce::Timer
{
public:
    struct CardInfo
    {
        int gameIndex = 0;
        juce::String name;
        juce::String benefit;          // hover tip only
        AppIcons::Icon icon = AppIcons::Icon::eq;

        // Per-exercise progression (see ProgressManager): the level this
        // exercise is at, how far through it, and whether the promotion
        // test is live right now.
        int level = 1;
        float levelProgress = 0.0f;
        bool promotionPending = false;
        int promotionStreak = 0;
        int promotionTestLength = 5;

        // "72% · 148" - accuracy and rounds played, small. The paragraph
        // of description moved to the hover tip; these two numbers are the
        // only thing about an exercise that is worth carrying every day.
        juce::String statsLine;

        // This exercise's family colour (see AbcTrainTheme::accentFor) -
        // resolved by the editor so this component keeps no opinion about
        // which exercise trains what.
        juce::Colour accent;

        bool isCurrent = false;
        bool isFavourite = false;
    };

    struct BadgeInfo
    {
        juce::String name;
        juce::String description;
        AppIcons::Icon icon = AppIcons::Icon::eq;

        // Difficulty tier, drawn as colour rather than as a different
        // glyph per achievement - bronze through to the near-unreachable
        // ones. Kept as a plain colour so this component needs no table.
        juce::Colour tint;

        bool earned = false;
        float progress = 0.0f;
    };

    HomeScreenComponent();
    ~HomeScreenComponent() override;

    void setCards (std::vector<CardInfo> newCards);
    void setBadges (std::vector<BadgeInfo> newBadges);
    void setBadgeStripCaption (juce::String caption);

    std::function<void (int gameIndex)> onGameChosen;
    std::function<void (int gameIndex, bool shouldBeFavourite)> onFavouriteToggled;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void rebuildLayout();

    void paintTile (juce::Graphics&, const CardInfo&, juce::Rectangle<int>, float hover);
    void paintBadgeStrip (juce::Graphics&);
    void paintHoverTip (juce::Graphics&);

    juce::Rectangle<int> starHitBox (juce::Rectangle<int> tile) const;
    int tileIndexAt (juce::Point<int>) const;
    int badgeIndexAt (juce::Point<int>) const;

    static constexpr int columns = 3;
    // What the tile's own contents add up to: 8 padding + 32 badge row +
    // 4 + 30 name + 14 stats + 4 track + 8 padding. Derived, not guessed -
    // at 76 the stats line and the progress track drew on top of each
    // other, which is exactly the kind of thing a rendered screenshot
    // shows in a second and no test ever will.
    static constexpr int tileHeight = 100;
    static constexpr int tileHeightCeiling = 112;
    static constexpr int badgeStripHeight = 78;
    static constexpr int badgeSize = 44;

    std::vector<CardInfo> cards;
    std::vector<BadgeInfo> badges;
    juce::String badgeStripCaption;

    std::vector<juce::Rectangle<int>> tileBounds;
    std::vector<float> hoverAmounts;
    int hoveredTile = -1;
    bool hoveredStar = false;
    int hoveredBadge = -1;

    juce::Rectangle<int> badgeStrip;
    float badgeScroll = 0.0f;
    float maxBadgeScroll = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeScreenComponent)
};
