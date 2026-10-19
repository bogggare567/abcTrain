#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>

// Drawing something see-through without paying for the whole window.
//
// Component::setAlpha and Graphics::beginTransparencyLayer both end in
// CGContextBeginTransparencyLayer on macOS and iOS, and JUCE opens that
// layer with no rectangle - so CoreGraphics allocates and composites one
// the size of the *whole window*, at the display's pixel density, however
// small the dimmed thing is. Measured with `ClickMap --frames` at an
// iPhone's 3x: one dimmed Answer button (369 x 50) took the training
// screen from 4.6 ms a frame to 10.6. The welcome screen opened three such
// layers sixty times a second for its cards, long after they had finished
// arriving - the phone build's "terrible lag" was mostly this.
//
// So: render the thing into an image exactly its own size, at the
// display's density, and draw that image at the wanted opacity. Fully
// opaque costs nothing extra; fully transparent draws nothing.
namespace Dimming
{
    inline void paint (juce::Graphics& g, juce::Rectangle<int> area, float alpha,
                       const std::function<void (juce::Graphics&)>& paintContent)
    {
        if (alpha >= 0.999f)
        {
            paintContent (g);
            return;
        }

        if (alpha <= 0.001f || area.isEmpty())
            return;

        const auto scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        juce::Image image (juce::Image::ARGB,
                           juce::jmax (1, (int) std::ceil ((float) area.getWidth() * scale)),
                           juce::jmax (1, (int) std::ceil ((float) area.getHeight() * scale)),
                           true);
        {
            juce::Graphics inner (image);
            inner.addTransform (juce::AffineTransform::translation ((float) -area.getX(), (float) -area.getY())
                                    .scaled (scale));
            paintContent (inner);
        }

        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (alpha);
        g.drawImage (image, area.toFloat());
    }
}

// A TextButton that can be shown dimmed - a locked mode, an Answer with
// nothing placed yet - without Component::setAlpha (see above).
class DimmableTextButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    void setDimmed (float newAlpha)
    {
        newAlpha = juce::jlimit (0.0f, 1.0f, newAlpha);
        if (! juce::approximatelyEqual (dimmed, newAlpha))
        {
            dimmed = newAlpha;
            repaint();
        }
    }

    float getDimmed() const noexcept { return dimmed; }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        Dimming::paint (g, getLocalBounds(), dimmed, [&] (juce::Graphics& inner)
        {
            juce::TextButton::paintButton (inner, highlighted, down);
        });
    }

private:
    float dimmed = 1.0f;
};
