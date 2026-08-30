#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../shared/AbcTrainTheme.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AppIcons.h"

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
    enum class Item { trainings, achievements, sounds, settings };
    static constexpr int numItems = 4;

    TopNavComponent();
    ~TopNavComponent() override;

    void setLabels (juce::StringArray itemLabels, juce::String streakCaption);

    void setActiveItem (Item item);
    Item getActiveItem() const noexcept { return active; }

    // streak: consecutive days. The level moved out of here and into the
    // focus band below, where it has room to be a number rather than a
    // sentence.
    void setStatus (int streakDays);

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

    void paint (juce::Graphics&) override;
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
    juce::Rectangle<int> rightCell (int slotFromRight) const;
    juce::String streakCaption() const;

    juce::StringArray labels;
    juce::String streakTemplate;
    Item active = Item::trainings;

    int hovered = -1;

    // One eased value per tab. The hover has to *travel* rather than
    // switch: an instant highlight is what makes a row of tabs feel like a
    // table of cells, and the whole point of this bar is that it should
    // not.
    std::array<float, numItems> glow {};

    int streakDays = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopNavComponent)
};
