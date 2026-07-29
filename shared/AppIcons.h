#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Simple, thin-line vector icons (Path-based, no external asset/SVG
// dependency - drawn programmatically, scaled to fit whatever size a
// caller needs via juce::Path::scaleToFit). Covers the 9 EarTrainer games
// and the 3 Learner plugins, matching the "icons for every game/plugin"
// part of a UI-overhaul request; the full styleguide/mockup/professional
// icon-pack scope of that request is a real design job (Figma, a hired
// designer) this codebase can't produce on its own - see
// decisions/016-icons-and-site-link.md.
namespace AppIcons
{
    enum class Icon
    {
        eq, compression, reverb, pan, delay, distortion, stereoWidth, gain, frequencyRange,
        learnerEQ, learnerComp, learnerVerb,

        // Interface icons, for the compact title-row buttons that replaced
        // a row of wide text buttons.
        sound, download, sun, moon, home, scope,

        // A five-pointed star, for achievements that belong to no single
        // exercise. The interface glyphs were all wrong here: `scope`
        // shrunk to badge size reads as a "no entry" sign.
        award,

        // A gear, for the settings screen. Distinct from `sun`, which the
        // theme toggle already owns.
        settings,

        // A checklist, for the training modules. Reads as a list of things
        // to get through, which is what a module shelf is - a book or a cap
        // would say "reading", and none of this is reading.
        modules
    };

    // Normalised to a 24x24 box - callers scale via Path::scaleToFit().
    juce::Path getPath (Icon icon, float animation = 0.0f);

    // Maps a Game::getName() English string (see the gameI18nKeys table in
    // Source/PluginEditor.cpp) to its icon; returns Icon::eq for anything
    // unrecognised (e.g. a future game not yet added here) rather than
    // asserting, same "missing entry falls back gracefully" precedent as
    // translateGameName()'s i18n lookup.
    Icon iconForGameName (const juce::String& englishName);

    // Draws `icon` centred and scaled (preserving aspect ratio) into
    // `bounds`, stroked in `colour`. A free function rather than a
    // Component, so it can be used directly inside an existing
    // Component::paint() (e.g. a title row) without adding a child.
    // `animation` 0..1 makes the glyph do the thing it depicts: the
    // compression chevrons close on the line, the reverb arcs travel
    // outward, the pan needle sweeps, the range bracket slides to another
    // span. Not decoration - a mark that moves the way its subject moves
    // is the cheapest explanation of the subject there is, and these are
    // exercises whose names ("stereo width", "the range") mean nothing yet
    // to the person reading them.
    void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds, juce::Colour colour,
               float animation = 0.0f);

    // The same glyph on a rounded coloured plate, for the places where an
    // icon has to be *recognised* rather than merely read - the home
    // screen's nine tiles and the achievement badges. `strength` 0..1 is
    // how lit it is: a locked achievement draws the same badge dim.
    void drawBadged (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds,
                     juce::Colour colour, float strength = 1.0f, float animation = 0.0f);

    // How far this glyph turns when the pointer arrives on it, in degrees.
    //
    // Not decoration and not applied uniformly. A gear turns because a
    // gear turns - the motion says what the object is, and a thing that
    // moves the way you expect it to move is the whole of what "tactile"
    // means on a screen. A star turns 72 degrees, exactly one point, so it
    // lands back on itself. Glyphs that mean something directional - the
    // download arrow, the sun and moon - stay put, because rotating them
    // would say something false about them. 0 for most icons is correct,
    // not unfinished.
    float hoverSpinDegrees (Icon) noexcept;
}

// A square button that is just an icon. Replaces the row of wide text
// buttons that used to eat most of every title row - "Training Sounds",
// "Updates", "Light" and so on - which were loud out of all proportion to
// how often they're pressed.
//
// It animates on two axes, both eased through the LookAndFeel's shared
// WidgetStateRegistry so the feel matches every other control: the icon
// lifts and brightens under the pointer, and *changing the icon* cross-
// fades rather than cutting - which is what makes the theme toggle read
// as one control changing state instead of two different buttons
// swapping places.
class IconButton : public juce::Button,
                    private juce::Timer
{
public:
    explicit IconButton (AppIcons::Icon initialIcon);
    ~IconButton() override;

    // Cross-fades to the new glyph. Setting the icon it already shows is
    // a no-op, so this is safe to call from a refresh that runs often.
    void setIcon (AppIcons::Icon newIcon);
    AppIcons::Icon getIcon() const noexcept { return icon; }

    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

private:
    void timerCallback() override;

    AppIcons::Icon icon;
    AppIcons::Icon previousIcon;
    float morph = 1.0f;    // 0 = fully previous, 1 = fully current

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconButton)
};

// Lightweight Component wrapper around AppIcons::draw(), for spots that
// want the icon as its own addAndMakeVisible()'d child (e.g. next to
// EarTrainer's game selector) rather than drawn inline in a paint().
class AppIconComponent : public juce::Component
{
public:
    void setIcon (AppIcons::Icon newIcon) noexcept
    {
        icon = newIcon;
        repaint();
    }

    void setIconColour (juce::Colour newColour) noexcept
    {
        colour = newColour;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        AppIcons::draw (g, icon, getLocalBounds().toFloat().reduced (2.0f), colour);
    }

private:
    AppIcons::Icon icon = AppIcons::Icon::eq;
    juce::Colour colour { 0xff5b9bd5 };
};
