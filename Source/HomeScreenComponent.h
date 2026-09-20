#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AppIcons.h"
#include <functional>
#include <vector>

// The screen you land on: every exercise as **one row**, and on each row
// the one number that says where you are - your threshold, in that
// exercise's own units (ADR 035).
//
// It used to be a grid of nine cards, each saying "LEVEL 1", with a strip
// of achievement badges underneath. Nine identical boxes answer "what is
// there", which is interesting on the first day and never again; the
// question somebody arriving actually has is "where am I, and what should
// I work on", and a column of thresholds with a ruler beside each answers
// that in one look.
//
// A row reads left to right the way a channel strip does:
//   star · icon · name (and its record in words) · the ruler · the number
// The ruler's fill is the record (it never drops), a white tick is where
// the staircase stands today, and a dashed line is the next rung of the
// ladder - so "how far to the next name" is a distance you can see.
//
// Achievements are no longer here: they have their own page, and showing
// them twice in two shapes is what made both copies unreadable.
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

        // The staircase (see ProgressManager): where it stands now, and
        // the highest step ever held.
        int level = 1;
        int bestLevel = 1;

        // Both already worded by the editor, in the player's language:
        // "±0,35 oct" / "Room and Chamber", and "Working ear".
        juce::String levelText;
        juce::String rankText;

        // A number in units is set in the mono face like a meter reading;
        // a pair of names is words, and mono made them read as code.
        bool levelIsNumber = true;

        // "72% correct · 148 rounds", or "not started yet".
        juce::String statsLine;

        // This exercise's family colour (see AbcTrainTheme::accentFor).
        juce::Colour accent;

        bool isCurrent = false;
        bool isFavourite = false;

        // Cards arrive already grouped: a new section starts wherever the
        // title changes.
        juce::String sectionTitle;
        juce::String sectionSubtitle;   // the family's English name, non-English UIs only
        juce::String sectionCount;      // "4 exercises"

        // The exercise's English name under the translated one, in the
        // eleven languages that are not English.
        juce::String englishName;
    };

    // Kept so the editor's wiring compiles unchanged; the home screen no
    // longer draws them (see the class comment).
    struct BadgeInfo
    {
        juce::String name;
        juce::String description;
        AppIcons::Icon icon = AppIcons::Icon::eq;
        juce::Colour tint;
        bool earned = false;
        float progress = 0.0f;
    };

    HomeScreenComponent();
    ~HomeScreenComponent() override;

    void setCards (std::vector<CardInfo> newCards);
    void setBadges (std::vector<BadgeInfo>) {}
    void setBadgeStripCaption (juce::String, juce::String, juce::String) {}
    void setLevelCaption (juce::String) {}

    std::function<void (int gameIndex)> onGameChosen;
    std::function<void (int gameIndex, bool shouldBeFavourite)> onFavouriteToggled;
    std::function<void()> onBadgeStripClicked;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Section
    {
        juce::String title, subtitle, count;
        juce::Colour accent;
        juce::Rectangle<int> header;
    };

    std::vector<Section> sections;
    std::vector<CardInfo> cards;
    std::vector<juce::Rectangle<int>> rowBounds;
    std::vector<float> hoverAmounts;
    int hoveredRow = -1;

    // Scrolls only when even the shortest rows do not fit (a small laptop
    // window, ADR 038); the fade at the cut edge says there is more.
    float scroll = 0.0f, maxScroll = 0.0f;
    bool hoveredStar = false;

    void timerCallback() override;
    void rebuildLayout();

    void paintRow (juce::Graphics&, const CardInfo&, juce::Rectangle<int>, float hover);
    void paintSectionHeader (juce::Graphics&, const Section&);
    void paintRuler (juce::Graphics&, const CardInfo&, juce::Rectangle<float>);

    juce::Rectangle<int> starHitBox (juce::Rectangle<int> row) const;
    int rowIndexAt (juce::Point<int>) const;

    static constexpr int headerHeight = 30;
    static constexpr int sectionGap = 10;
    static constexpr int minRowHeight = 36;
    static constexpr int maxRowHeight = 58;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeScreenComponent)
};
