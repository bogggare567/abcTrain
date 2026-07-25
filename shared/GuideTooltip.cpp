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

void GuideTooltip::setText (const juce::String& newText)
{
    if (newText.isNotEmpty() && newText != text)
        text = newText;

    visibilityTarget = newText.isEmpty() ? 0.0f : 1.0f;
}

void GuideTooltip::timerCallback()
{
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

    if (auto* parent = getParentComponent())
    {
        // Snapshot the parent, not this component: createComponentSnapshot
        // renders the target *and its children*, so snapshotting ourselves
        // would recurse into this very paint() call. Taking the parent's
        // pixels for our own screen area gives exactly the content sitting
        // behind the card.
        //
        // The snapshot still contains this tooltip's previous frame, which
        // is harmless here - the card is translucent and heavily blurred,
        // so a faint echo of the last frame is indistinguishable from the
        // blur itself, and avoiding it entirely would mean hiding and
        // re-showing the component mid-paint.
        const auto areaInParent = getBounds().translated (0, (int) offsetY);
        AbcTrainLookAndFeel::paintBlurredBackdrop (g, *parent, areaInParent, blurRadius,
                                                    AbcTrainTheme::Radius::panel);
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
    g.setFont (juce::Font (juce::FontOptions (AbcTrainLookAndFeel::bodyFontHeight)));
    g.drawFittedText (text,
                      bounds.reduced (18.0f, 8.0f).toNearestInt(),
                      juce::Justification::centredLeft, 3);
}
