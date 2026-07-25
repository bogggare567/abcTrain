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
        learnerEQ, learnerComp, learnerVerb
    };

    // Normalised to a 24x24 box - callers scale via Path::scaleToFit().
    juce::Path getPath (Icon icon);

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
    void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds, juce::Colour colour);
}

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
