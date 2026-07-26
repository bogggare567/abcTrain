#include "AbcTrainTheme.h"
#include <cmath>

namespace AbcTrainTheme
{
    namespace
    {
        Mode activeMode = Mode::dark;

        Palette makeDark()
        {
            Palette p;
            p.mode = Mode::dark;

            // Base #1e1e2e, kept from the original theme - it's a good
            // near-neutral with a faint blue-violet cast that stops the UI
            // reading as dead grey. The surface ramp above it is spaced
            // just far enough apart to be legible as separate planes
            // without any one step looking like a mistake.
            p.windowBackground  = juce::Colour (0xff1e1e2e);
            p.panelBackground   = juce::Colour (0xff242434);
            p.widgetBackground  = juce::Colour (0xff2a2a3a);
            p.displayBackground = juce::Colour (0xff14141a);

            p.outline = juce::Colour (0xff3a3a4a);
            p.divider = juce::Colour (0xff32323f);

            p.text       = juce::Colour (0xffe0e0e0);
            p.textDim    = juce::Colour (0xffa0a0b0);
            p.textBright = juce::Colour (0xfff2f2f7);

            p.accent     = juce::Colour (0xff5b9bd5);
            p.accentWarm = juce::Colour (0xffd98c5f);
            p.positive   = juce::Colour (0xff6fbf8b);
            p.negative   = juce::Colour (0xffd9615f);

            p.shadow          = juce::Colours::black;
            p.shadowStrength  = 1.0f;
            p.textureStrength = 1.0f;
            return p;
        }

        Palette makeLight()
        {
            Palette p;
            p.mode = Mode::light;

            // Warm off-white rather than #ffffff: a pure-white plugin panel
            // is fatiguing next to a DAW's own dark chrome, and leaves no
            // headroom to make an *elevated* surface look lighter than the
            // page behind it. Here the page is the darkest light surface and
            // panels/widgets step *up* toward white, mirroring how the dark
            // palette steps up from its own base.
            p.windowBackground  = juce::Colour (0xffe8e6e1);
            p.panelBackground   = juce::Colour (0xfff1efeb);
            p.widgetBackground  = juce::Colour (0xfff8f7f4);

            // Darker than the panel it sits in, and the darkest surface in
            // this palette - the one place the light theme deliberately
            // steps *down* instead of up. It used to be #f4f2ee, lighter
            // than the panel, which made the spectrum and waveform read as
            // raised white plates rather than as wells cut into the
            // surface, and left a 1px curve to carry itself against the
            // brightest thing on screen. Caught by rendering the editors in
            // both themes (tools/EditorSnapshots).
            p.displayBackground = juce::Colour (0xffdedad2);

            p.outline = juce::Colour (0xffc9c5bd);
            p.divider = juce::Colour (0xffd8d4cc);

            // Not pure black: near-black warm grey keeps the same softness
            // the dark mode's off-white text has, instead of maximum
            // contrast that reads as harsh on a warm ground.
            p.text       = juce::Colour (0xff33322f);
            p.textDim    = juce::Colour (0xff6d6a64);
            p.textBright = juce::Colour (0xff1b1a18);

            // Deeper and slightly desaturated versions of the dark accents.
            // The dark palette's #5b9bd5 on this background has far too
            // little contrast to carry a 1px spectrum curve.
            p.accent     = juce::Colour (0xff2f6fa8);
            p.accentWarm = juce::Colour (0xffb26134);
            p.positive   = juce::Colour (0xff3d8a5f);
            p.negative   = juce::Colour (0xffb03c3a);

            // Cool grey, not black, and weaker: on paper a shadow is
            // ambient occlusion. Black at dark-mode strength would look
            // like a hard drop-shadow sticker.
            p.shadow          = juce::Colour (0xff5a5f6e);
            p.shadowStrength  = 0.55f;
            p.textureStrength = 0.45f;
            return p;
        }
    }

    const Palette& dark() noexcept
    {
        static const Palette p = makeDark();
        return p;
    }

    const Palette& light() noexcept
    {
        static const Palette p = makeLight();
        return p;
    }

    const Palette& current() noexcept
    {
        return activeMode == Mode::light ? light() : dark();
    }

    void setMode (Mode newMode) noexcept
    {
        activeMode = newMode;
    }

    Mode getMode() noexcept
    {
        return activeMode;
    }

    juce::Colour accentFor (Family family) noexcept
    {
        // Dark-mode values first - these are the hues the whole product was
        // designed against. The light variants are darkened and slightly
        // desaturated by the same reasoning the light palette itself uses:
        // a colour that reads as "clearly blue" on #16161c washes out to a
        // pastel on warm paper.
        const auto base = [family]
        {
            switch (family)
            {
                case Family::frequency: return juce::Colour (0xff4fa3c7);   // cool blue
                case Family::dynamics:  return juce::Colour (0xffc77f4f);   // warm amber
                case Family::space:     return juce::Colour (0xff5fb98c);   // green
                case Family::character: return juce::Colour (0xffa878c9);   // violet
            }

            return juce::Colour (0xff4fa3c7);
        }();

        if (activeMode == Mode::light)
            return base.darker (0.35f).withMultipliedSaturation (0.9f);

        return base;
    }

    namespace Ease
    {
        float out (float t) noexcept
        {
            t = juce::jlimit (0.0f, 1.0f, t);
            const auto inv = 1.0f - t;
            return 1.0f - inv * inv * inv;      // cubic ease-out
        }

        float inOut (float t) noexcept
        {
            t = juce::jlimit (0.0f, 1.0f, t);
            return t < 0.5f ? 4.0f * t * t * t
                            : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) * 0.5f;
        }

        float backOut (float t) noexcept
        {
            t = juce::jlimit (0.0f, 1.0f, t);
            // Standard "back" overshoot constants: peaks ~10% past the
            // target around t=0.7, then settles - enough to read as sprung
            // without looking like a bug.
            constexpr float c1 = 1.70158f;
            constexpr float c3 = c1 + 1.0f;
            const auto inv = t - 1.0f;
            return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
        }
    }
}
