#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AppIcons.h"
#include "shared/ui/Dimming.h"

#include <array>
#include <functional>

// The command bar across the top: the mark, where you are, where else you
// can go, and how you are doing.
//
// It replaces the left rail from the previous pass, and the swap is the
// design's call rather than a change of mind. The rail solved the right
// problem - navigation had been a strip of unlabelled icons along the
// bottom, which is where a *status* bar goes - but it solved it by
// spending 156px of a 840px window on five words. Across the top the same
// five words cost 59px of height, and the content gets the whole width
// back, which is what makes four exercise cards fit on a row where three
// used to.
//
// Rows are drawn rather than built from TextButtons: a nav tab is a label
// and a selected state, and routing that through
// LookAndFeel::drawButtonBackground would give it the same bordered well
// every other button has - which is the thing that made the interface read
// as a grid of cells in the first place. The selected tab is a solid
// accent block with the page colour as its text, which is the one place in
// this design where a filled rectangle means "you are here".
class TopNavComponent : public juce::Component,
                         private juce::Timer
{
public:
    // Order is the order they appear. Kept as an enum rather than indices
    // so a caller cannot quietly mean the wrong tab.
    // Profile is last: on a desktop it is one more tab, on a phone one of
    // the four left in the bar (ADR 061).
    enum class Item { trainings, studio, achievements, sounds, settings, live, profile };
    static constexpr int numItems = 7;

    // Live can be switched off in Settings; a hidden tab takes no room.
    void setItemHidden (Item, bool shouldBeHidden);

    TopNavComponent();
    ~TopNavComponent() override;

    void setLabels (juce::StringArray itemLabels, juce::String streakCaption);

    void setActiveItem (Item item);
    Item getActiveItem() const noexcept { return active; }

    // streak: consecutive days. The level moved out of here and into the
    // focus band below, where it has room to be a number rather than a
    // sentence.
    void setStatus (int streakDays);

    // Today's practice against the day's dose (t02), already worded by the
    // editor ("today 7/15 min"); drawn after the streak. Empty hides it.
    void setToday (juce::String text);

    // This week's hearing dose as a fraction of the limit (ADR 036), drawn
    // as ten segments left of the streak. Hidden when `show` is false -
    // hearing protection off, or nothing measured yet.
    void setHearing (float weeklyFraction, bool show, juce::String caption);

    std::function<void (Item)> onItemChosen;

    // Where the editor should put the controls it already owns. The bar
    // deliberately does not own them: the update button alone has fifteen
    // call sites tied to the check-for-updates flow, and moving ownership
    // to re-house a widget is how a layout change turns into a behaviour
    // change. The bar paints; the editor positions.
    juce::Rectangle<int> getVolumeSlot() const;
    juce::Rectangle<int> getThemeSlot() const;
    juce::Rectangle<int> getUpdateSlot() const;
    juce::Rectangle<int> getSizeSlot() const;
    juce::Rectangle<int> getLanguageSlot() const;

    static constexpr int preferredHeight = 59;

    // Phones (ADR 060): the same items as a tab bar along the bottom edge -
    // an icon over a short word, equal widths, the way every phone app
    // navigates. A row of words across the top does not fit 390 points
    // and sits where a thumb does not reach. Achievements leave the bar
    // here (five tabs is the most a phone bar holds); the home screen's
    // badge strip still opens them. The bar's own height is compactHeight;
    // anything below it in its bounds is the home indicator's area, kept
    // clear and painted as background.
    //
    // A phone on its side has height to spare at neither edge: the same
    // tabs stand in a column down the left side instead (railWidth wide,
    // after `leadingInset`, the notch's side of the screen).
    void setCompact (bool shouldBeCompact, bool asColumn = false, int leadingInset = 0);
    bool isCompact() const noexcept { return compact; }
    bool isColumn() const noexcept { return compact && column; }
    static constexpr int compactHeight = 56;
    static constexpr int railWidth = 76;

    void paint (juce::Graphics&) override;

    // Shown dimmed while a run locks the bar - drawn through
    // Dimming::paint rather than Component::setAlpha, which on iOS made
    // CoreGraphics composite a layer the size of the window every frame.
    void setDimmed (float alpha)
    {
        if (! juce::approximatelyEqual (dimmed, alpha))
        {
            dimmed = alpha;
            repaint();
        }
    }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // The tabs are laid out from their own measured text, so a language
    // whose word for "Achievements" is twice as long still fits instead of
    // being clipped by a fixed column width.
    juce::Rectangle<int> tabBounds (int index) const;
    int tabAt (juce::Point<int>) const;
    int lastTabRight() const;
    juce::Rectangle<int> rightCell (int slotFromRight) const;
    juce::String streakCaption() const;
    bool shownInBar (int index) const noexcept;
    void paintCompact (juce::Graphics&);
    void paintContent (juce::Graphics&);
    float dimmed = 1.0f;

    bool compact = false, column = false;
    int columnInset = 0;

    juce::StringArray labels;
    juce::String streakTemplate;
    juce::String todayText;
    float hearingFraction = 0.0f;
    bool hearingShown = false;
    juce::String hearingCaption;
    Item active = Item::trainings;
    std::array<bool, numItems> hidden {};

    int hovered = -1;

    // One eased value per tab. The hover has to *travel* rather than
    // switch: an instant highlight is what makes a row of tabs feel like a
    // table of cells, and the whole point of this bar is that it should
    // not.
    std::array<float, numItems> glow {};

    int streakDays = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopNavComponent)
};
