#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AppIcons.h"
#include <functional>
#include <vector>

// "Choose training" screen: a grid of cards, one per exercise, replacing
// the plain ComboBox list the game selector used to be.
//
// A dropdown of nine names told a player nothing about what any of them
// would do for their ears, or how they were doing at it. Each card now
// carries the exercise's icon, its name, one line on the skill it builds,
// and that player's own record on it - so picking a training is an
// informed choice rather than a shot in the dark.
//
// Deliberately knows nothing about Game/GameManager/ProgressManager: the
// editor hands it a plain vector of CardInfo and gets back an index. That
// keeps it testable-by-inspection and means a future non-EarTrainer
// consumer (or a reordered game list) needs no changes here.
class GamePickerComponent : public juce::Component,
                             private juce::Timer
{
public:
    struct CardInfo
    {
        juce::String name;
        juce::String benefit;      // "what this training gives you"
        juce::String statsLine;    // the player's own record, already formatted
        AppIcons::Icon icon = AppIcons::Icon::eq;
        bool isCurrent = false;
    };

    GamePickerComponent();
    ~GamePickerComponent() override;

    void setCards (std::vector<CardInfo> newCards);
    void setHeading (const juce::String& newHeading) { heading = newHeading; repaint(); }

    // Fired with the chosen card's index. The editor is responsible for
    // hiding this component - it doesn't hide itself, so a caller can keep
    // it open (e.g. to show the newly-selected card's highlighted state).
    std::function<void (int)> onGameChosen;
    std::function<void()> onClosed;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> boundsForCard (int index) const;
    int cardIndexAt (juce::Point<int> position) const;
    void paintCard (juce::Graphics&, int index, juce::Rectangle<float> bounds);

    static constexpr int columns = 3;
    static constexpr int cardHeight = 132;

    std::vector<CardInfo> cards;
    // Per-card eased hover amount, parallel to `cards`. A card lifts and
    // brightens under the pointer, on the same easing the LookAndFeel's
    // buttons use - see shared/WidgetStateRegistry for why this state has
    // to live somewhere rather than being read from a boolean each paint.
    std::vector<float> hoverAmounts;
    int hoveredIndex = -1;

    juce::String heading { "Choose training" };
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GamePickerComponent)
};
