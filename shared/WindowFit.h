#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdlib>

// How big a window may open on the display it is about to appear on.
//
// Every editor used to open at its design size, and that design size was
// also its floor: 1180 x 880 for the trainer, which on a 13-inch laptop
// (about 800 usable points of height once the menu bar, the dock and the
// window's own title bar are gone) meant the smallest setting "barely fit"
// and nothing smaller existed. Now the design size is the size a window
// *prefers*, and it opens at whatever part of it the screen can actually
// show - down to a floor each editor's layout is built to work at.
namespace WindowFit
{
    // The part of the screen a window may use: the display under the mouse
    // (where a new window appears) or the primary one, minus the dock and
    // menu bar (that is what userArea already excludes) and a margin for
    // the title bar a standalone window adds.
    inline juce::Rectangle<int> usableArea()
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* display = displays.getDisplayForPoint (juce::Desktop::getMousePosition());

        if (display == nullptr)
            display = displays.getPrimaryDisplay();

        if (display == nullptr)
            return { 0, 0, 1440, 900 };

        return display->userArea.withTrimmedTop (36).reduced (12, 12);
    }

    // The tools that render a window to a picture (tools/EditorSnapshots,
    // tools/ClickMap) run on a virtual display of whatever size, and must
    // always see the design size.
    inline bool fittingDisabled()
    {
        return std::getenv ("ABC_DESIGN_SIZE") != nullptr;
    }

    // The largest size up to `design` that fits on screen at `scale`,
    // never smaller than `minimum`.
    inline juce::Point<int> fit (juce::Point<int> design, juce::Point<int> minimum, float scale = 1.0f)
    {
        if (fittingDisabled())
            return design;

        const auto area = usableArea();
        const auto w = (int) ((float) area.getWidth() / scale);
        const auto h = (int) ((float) area.getHeight() / scale);

        return { juce::jlimit (minimum.x, juce::jmax (minimum.x, design.x), w),
                 juce::jlimit (minimum.y, juce::jmax (minimum.y, design.y), h) };
    }

    // The largest scale, up to `wanted`, at which `minimum` still fits:
    // a text size chosen on a desktop monitor must not push the window off
    // a laptop screen the next time it opens there.
    inline float fitScale (float wanted, juce::Point<int> minimum)
    {
        if (fittingDisabled())
            return wanted;

        const auto area = usableArea();
        const auto most = juce::jmin ((float) area.getWidth() / (float) minimum.x,
                                      (float) area.getHeight() / (float) minimum.y);
        return juce::jmin (wanted, juce::jmax (0.7f, most));
    }
}
