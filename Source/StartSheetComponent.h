#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "shared/ui/AppIcons.h"

#include <array>
#include <functional>

// What an exercise asks before it starts (ADR 061): which mode, this time.
//
// The mode used to be three pills under the scale, set once and then
// carried along - so a Blitz chosen yesterday came back as a countdown the
// next time somebody only wanted to practise. Asked at the door instead,
// with last time's answer already chosen, it costs one tap when nothing
// changes and saves the surprise when something should.
//
// The sheet also says what the exercise is, where the staircase has got to
// and what it will be played on - the three things worth knowing before
// pressing Start. On a phone it rises from the bottom edge; on a wider
// window it is a card in the middle.
class StartSheetComponent : public juce::Component
{
public:
    struct Mode
    {
        juce::String name;      // "Practice"
        juce::String detail;    // "No limits · 66 rounds last time"
        bool locked = false;    // shown, dimmed; pressing it says why
    };

    struct Content
    {
        juce::String title, family, description;
        AppIcons::Icon icon = AppIcons::Icon::eq;
        juce::Colour accent;

        juce::String levelCaption, levelText;
        int levelSteps = 0, levelOf = 10;

        juce::String chooseCaption;
        std::array<Mode, 3> modes;

        juce::String soundCaption, soundValue;
        juce::String startText;
    };

    StartSheetComponent();

    // Opens with `mode` (0 practice, 1 survival, 2 blitz) chosen.
    void show (Content, int mode);
    void dismiss();
    bool isShowing() const noexcept { return isVisible(); }

    std::function<void (int mode)> onStart;
    std::function<void()> onSound;           // the sound row
    std::function<void()> onDismissed;       // closed without starting
    std::function<void (int mode)> onLockedPressed;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    bool asSheet() const { return getWidth() < 600; }

    // A phone on its side: too short for one column, so the exercise on
    // the left and the choice on the right.
    bool twoColumns() const { return ! asSheet() && getHeight() < 560; }
    juce::Rectangle<int> startBounds() const;
    juce::Rectangle<int> panelBounds() const;
    int contentHeight (int width) const;

    struct Rows
    {
        juce::Rectangle<int> header, description, level, choose, sound;
        std::array<juce::Rectangle<int>, 3> modes;
    };
    Rows rowsIn (juce::Rectangle<int> panel) const;

    Content content;
    int selected = 0;
    juce::TextButton startButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StartSheetComponent)
};
