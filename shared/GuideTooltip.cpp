#include "GuideTooltip.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"
#include <cmath>

namespace
{
    constexpr int tickHz = 60;
    constexpr float riseDistance = 6.0f;   // px the card travels while fading in
    constexpr float blurRadius = 6.0f;
}

GuideTooltip::GuideTooltip()
{
    // Never steal clicks: this sits over the control the user is dragging.
    setInterceptsMouseClicks (false, false);
    startTimerHz (tickHz);
}

GuideTooltip::~GuideTooltip()
{
    stopTimer();
}

void GuideTooltip::setText (const juce::String& newText, int autoDismissMs)
{
    if (newText.isNotEmpty() && newText != text)
        text = newText;

    // A fresh appearance gets a fresh look at what is behind the card;
    // while it stays up, the cached blur is reused frame after frame.
    if (newText.isNotEmpty() && visibility <= 0.001f)
        backdropCache = {};

    visibilityTarget = newText.isEmpty() ? 0.0f : 1.0f;
    dismissCountdownMs = newText.isEmpty() ? 0 : juce::jmax (0, autoDismissMs);
}

void GuideTooltip::timerCallback()
{
    if (dismissCountdownMs > 0)
    {
        dismissCountdownMs -= (int) (1000.0 / (double) tickHz);

        if (dismissCountdownMs <= 0)
        {
            dismissCountdownMs = 0;
            visibilityTarget = 0.0f;
        }
    }

    // Build the blurred backdrop exactly once, at the moment the card is
    // about to appear - while our own paint() is still a no-op (visibility
    // is 0), so the snapshot of the parent contains everything *except* us:
    // no echo of a previous frame, and no way for the snapshot to re-enter
    // this component's paint. This used to be done inside paint() itself,
    // every frame, which re-rendered the whole editor and ran a 2D
    // convolution tens of times a second for the length of every knob drag
    // - the single biggest source of "the plugin feels laggy". Worse, the
    // result was drawn offset by the card's own position in parent space
    // and clipped away entirely, so all that cost bought nothing visible.
    if (visibilityTarget > 0.5f && visibility <= 0.001f
        && (! backdropCache.isValid() || backdropCacheArea != getBounds()))
    {
        if (auto* parent = getParentComponent())
        {
            backdropCache = AbcTrainLookAndFeel::blurredSnapshot (*parent, getBounds(), blurRadius);
            backdropCacheArea = getBounds();
        }
    }

    if (std::abs (visibility - visibilityTarget) < 0.001f)
        return;

    // Appearing is quicker than disappearing: the text needs to be there
    // the moment the user starts dragging, but should linger a beat on
    // release rather than snatching itself away mid-read.
    const auto duration = visibilityTarget > visibility ? AbcTrainTheme::Duration::hover
                                                        : AbcTrainTheme::Duration::transition;
    const auto step = (float) (1000.0 / (double) tickHz / duration);

    visibility = std::abs (visibilityTarget - visibility) <= step
                     ? visibilityTarget
                     : visibility + (visibilityTarget > visibility ? step : -step);

    repaint();
}

void GuideTooltip::paint (juce::Graphics& g)
{
    if (visibility <= 0.001f || text.isEmpty())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (visibility);

    // Rises into place as it fades up.
    const auto offsetY = (1.0f - eased) * riseDistance;
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, offsetY);

    juce::Graphics::ScopedSaveState saved (g);
    g.setOpacity (eased);

    // The cached backdrop (built in timerCallback at the moment of
    // appearance, never here). Blurred at the card's resting position and
    // merely *drawn* at the animated one - the few pixels of rise select
    // marginally different source content, which a Gaussian at this radius
    // has already smeared into invisibility.
    if (backdropCache.isValid() && backdropCacheArea == getBounds())
    {
        juce::Path clip;
        clip.addRoundedRectangle (bounds, AbcTrainTheme::Radius::panel);

        juce::Graphics::ScopedSaveState clipState (g);
        g.reduceClipRegion (clip);
        g.drawImageAt (backdropCache, 0, (int) offsetY);
    }

    // Tinted glass over the blur, so text contrast doesn't depend on
    // whatever happens to be underneath.
    g.setColour (theme.panelBackground.withAlpha (theme.mode == AbcTrainTheme::Mode::light ? 0.82f : 0.76f));
    g.fillRoundedRectangle (bounds, AbcTrainTheme::Radius::panel);

    g.setColour (theme.outline.withAlpha (0.65f));
    g.drawRoundedRectangle (bounds, AbcTrainTheme::Radius::panel, 1.0f);

    // A short accent rule down the left edge marks this as guidance rather
    // than a status readout - the same role a blockquote bar plays in text.
    const auto rule = juce::Rectangle<float> (bounds.getX() + 8.0f,
                                               bounds.getY() + 8.0f,
                                               2.0f,
                                               bounds.getHeight() - 16.0f);
    g.setColour (theme.accent.withAlpha (0.75f));
    g.fillRoundedRectangle (rule, 1.0f);

    g.setColour (theme.text);
    // Through the ladder, not a raw FontOptions: this is one of the three
    // strings that ignored the user's text-size setting (and the chosen
    // typeface) while everything around it obeyed both.
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    g.drawFittedText (text,
                      bounds.reduced (18.0f, 8.0f).toNearestInt(),
                      juce::Justification::centredLeft, 3);
}
