#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Shared dark theme for all four plugins, replacing the stock JUCE
// default look. Deliberately basic for this pass, per the explicit
// iterative-scope decision: solid colours and clean rounded shapes, no
// hover-fade timers, press-scale springs, or gradient-filled curves yet -
// see decisions/009-look-and-feel.md for what was scoped down and why.
//
// One instance per editor (not a shared static): each editor constructs
// its own AbcTrainLookAndFeel member, calls setLookAndFeel(&laf) in its
// constructor and setLookAndFeel(nullptr) in its destructor, and declares
// the member *first* in its class so it's constructed before - and
// destroyed after - every child Component that might still reference it
// during teardown.
class AbcTrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AbcTrainLookAndFeel();

    // Font sizes referenced directly by editors for one-off overrides
    // (title labels, monospace meter readouts) that a LookAndFeel default
    // can't express, since those depend on each editor's own layout.
    static constexpr float titleFontHeight = 22.0f;
    static constexpr float bodyFontHeight = 14.0f;
    static constexpr float monoFontHeight = 16.0f;

    static juce::Font titleFont();
    static juce::Font monoFont();

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};
