#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "shared/ui/TouchScroll.h"

#include <array>
#include <functional>
#include <vector>

// The player's own page (ADR 061): what has been done, in numbers - the
// totals, Decibelo per family and how it has moved, what today held - and
// the way to support the project. Statistics the way lichess shows them:
// a few facts at the top, the rating history as lines, the families as
// rows.
//
// Two pages in one component: the profile, and "support the project"
// opened from it. On a phone it is a tab of its own and also the way to
// Settings and Achievements, which leave the bar there.
class ProfileScreenComponent : public juce::Component
{
public:
    struct Total { juce::String value, caption; juce::Colour colour; };

    struct FamilyRow
    {
        juce::String name, detail, rating, delta;
        juce::Colour colour;
        bool deltaUp = true;
    };

    struct Content
    {
        juce::String name, subtitle;
        std::array<Total, 3> totals;

        juce::String historyCaption, historyEmpty;
        juce::StringArray periods;   // week, month, all
        int period = 1;
        std::vector<std::array<int, 4>> history;   // one per day, oldest first; 0 = no rating
        std::array<juce::Colour, 4> familyColours;

        juce::String familiesCaption;
        std::vector<FamilyRow> families;

        juce::String activityCaption;
        juce::StringArray activity;

        // Rows that open other pages. Achievements and Settings only on a
        // phone, where they are not in the bar.
        bool showPageLinks = false;
        juce::String achievementsTitle, achievementsDetail;
        juce::String settingsTitle, settingsDetail;
        juce::String supportTitle, supportDetail;

        // The support page.
        juce::String backLabel, supportPageTitle, supportBody;
        bool donationAllowed = true;
        juce::String donateCaption, donateButton, donateNote;
        juce::String freeCaption;
        juce::StringArray freeItems;
    };

    ProfileScreenComponent();

    void setContent (Content);
    void showSupportPage (bool shouldShow);
    bool isShowingSupport() const noexcept { return supportShown; }

    std::function<void()> onAchievements, onSettings, onDonate;
    std::function<void (int)> onFreeItem;
    std::function<void (int)> onPeriodChosen;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Layout
    {
        // Profile
        juce::Rectangle<int> header, totals, historyCaption, history, familiesCaption, activityCaption;
        std::vector<juce::Rectangle<int>> periodChips;
        std::vector<juce::Rectangle<int>> families, activity;
        juce::Rectangle<int> achievementsRow, supportRow, settingsRow;

        // Support
        juce::Rectangle<int> back, title, body, donateCaption, donateButton, donateNote, freeCaption;
        std::vector<juce::Rectangle<int>> freeRows;

        int bottom = 0;
    };

    Layout layout() const;
    juce::Rectangle<int> column() const;
    void paintProfile (juce::Graphics&, const Layout&);
    void paintSupport (juce::Graphics&, const Layout&);
    void paintLinkRow (juce::Graphics&, juce::Rectangle<int>, const juce::String& title,
                       const juce::String& detail, juce::Colour accent);

    Content content;
    bool supportShown = false;
    float scroll = 0.0f;
    TouchScroll scroller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProfileScreenComponent)
};
