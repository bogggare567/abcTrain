#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../shared/AbcTrainTheme.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AppIcons.h"

// The left rail: where you are, where else you can go, and how you are
// doing - permanently on screen.
//
// It replaces a strip of six unlabelled icons and two dropdowns along the
// bottom of the window. The bottom of a window is where a *status* bar
// goes; putting navigation there is a shape people stopped meeting around
// the time they stopped meeting Windows 95, which is exactly why the app
// "doesn't look like anything else" and why it was possible to get lost in
// it. Every application anyone uses daily puts its navigation down the
// left, so that is where a person looks first.
//
// Two things earn their place here besides navigation:
//
//   - The level and streak, which were a sentence ("Level 4 - 51 / 100 to
//     level 5") that had to be read and parsed. As a bar that is always
//     there, it is seen rather than read.
//   - Volume and theme, because they are about the listener rather than
//     about the exercise, and they are the two you reach for mid-round.
//
// Rows are drawn rather than built from TextButtons: a nav row is an icon,
// a label and a selected state, and routing that through
// LookAndFeel::drawButtonBackground would have given it the same bordered
// well every other button has - which is the thing that made the interface
// read as a grid of cells in the first place.
class SideRailComponent : public juce::Component,
                           private juce::Timer
{
public:
    // Order is the order they appear. Kept as an enum rather than indices
    // so a caller cannot quietly mean the wrong row.
    enum class Item { trainings, achievements, sounds, settings };
    static constexpr int numItems = 4;

    SideRailComponent();
    ~SideRailComponent() override;

    void setLabels (juce::StringArray itemLabels, juce::String levelCaption,
                     juce::String streakCaption);

    void setActiveItem (Item item);
    Item getActiveItem() const noexcept { return active; }

    // level: what they are on. progress: 0..1 toward the next. streak: days.
    void setStatus (int level, float progress, int streakDays);

    std::function<void (Item)> onItemChosen;

    // Where the editor should put the controls it already owns. The rail
    // deliberately does not own them: the update button alone has fifteen
    // call sites tied to the check-for-updates flow, and moving ownership
    // to re-house a widget is how a layout change turns into a behaviour
    // change. The rail paints; the editor positions.
    juce::Rectangle<int> getVolumeSlot() const;
    juce::Rectangle<int> getThemeSlot() const;
    juce::Rectangle<int> getUpdateSlot() const;

    static constexpr int preferredWidth = 156;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> rowBounds (int index) const;
    int rowAt (juce::Point<int>) const;

    juce::StringArray labels;
    juce::String levelText, streakText;
    Item active = Item::trainings;

    int hovered = -1;

    // One eased value per row. The hover has to *travel* rather than
    // switch: an instant highlight is what makes a list feel like a table
    // of cells, and the whole point of this rail is that it should not.
    std::array<float, numItems> glow {};

    int level = 1;
    float levelProgress = 0.0f;
    float shownProgress = 0.0f;   // eased, so a level-up is visible as motion
    int streakDays = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SideRailComponent)
};
