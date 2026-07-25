#include "ChoiceSliderComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include <cmath>

namespace
{
    constexpr int bigLabelHeight = 32;
    constexpr int tickLabelHeight = 20;
    constexpr float baseThumbRadius = 8.0f;
    constexpr int tickHz = 60;
}

ChoiceSliderComponent::ChoiceSliderComponent()
{
    startTimerHz (tickHz);
}

ChoiceSliderComponent::~ChoiceSliderComponent()
{
    stopTimer();
}

void ChoiceSliderComponent::timerCallback()
{
    const auto step = (float) (1000.0 / (double) tickHz);

    const auto previousTouch = touchAmount;
    const auto touchStep = step / (float) AbcTrainTheme::Duration::hover;
    if (std::abs (touchTarget - touchAmount) <= touchStep)
        touchAmount = touchTarget;
    else
        touchAmount += touchTarget > touchAmount ? touchStep : -touchStep;

    const auto previousEnter = enterAmount;
    if (enterAmount < 1.0f)
        enterAmount = juce::jmin (1.0f, enterAmount + step / (float) AbcTrainTheme::Duration::transition);

    if (std::abs (touchAmount - previousTouch) > 0.002f
        || std::abs (enterAmount - previousEnter) > 0.002f)
    {
        repaint();
    }
}

void ChoiceSliderComponent::mouseEnter (const juce::MouseEvent&)
{
    touchTarget = 1.0f;
}

void ChoiceSliderComponent::mouseExit (const juce::MouseEvent&)
{
    if (! isMouseButtonDown())
        touchTarget = 0.0f;
}

void ChoiceSliderComponent::setChoices (const juce::StringArray& labels)
{
    choiceLabels = labels;
    // Switching games re-labels this widget rather than rebuilding it, so
    // the transition is an entrance for the new choices: they rise into
    // place and fade up instead of blinking into existence.
    enterAmount = 0.0f;
    resetForNewRound();
}

void ChoiceSliderComponent::resetForNewRound()
{
    if (! feedbackAnimator.isComplete())
        feedbackAnimator.complete();

    previewIndex = -1;
    answered = false;
    correctIndex = -1;
    chosenIndex = -1;
    lastCorrect = false;
    feedbackGlow = 0.0f;
    feedbackWobblePx = 0.0f;
    repaint();
}

void ChoiceSliderComponent::showAnswer (int newCorrectIndex, int newChosenIndex, bool wasCorrect)
{
    answered = true;
    correctIndex = newCorrectIndex;
    chosenIndex = newChosenIndex;
    lastCorrect = wasCorrect;
    previewIndex = newChosenIndex;
    startFeedbackAnimation (wasCorrect);
    repaint();
}

void ChoiceSliderComponent::startFeedbackAnimation (bool wasCorrect)
{
    if (! feedbackAnimator.isComplete())
        feedbackAnimator.complete();

    if (wasCorrect)
    {
        feedbackGlow = 1.0f;
        feedbackWobblePx = 0.0f;

        feedbackAnimator = juce::ValueAnimatorBuilder{}
                               .withEasing (juce::Easings::createEaseOut())
                               .withDurationMs (900.0)
                               .withValueChangedCallback ([this] (float t)
                               {
                                   feedbackGlow = 1.0f - t;
                                   repaint();
                               })
                               .build();
    }
    else
    {
        feedbackGlow = 0.0f;

        // A short, decaying oscillation (sine envelope shrinking as t
        // grows) reads as a gentle "disappointed" wobble rather than an
        // aggressive shake - three quick back-and-forths, each smaller
        // than the last.
        feedbackAnimator = juce::ValueAnimatorBuilder{}
                               .withEasing (juce::Easings::createLinear())
                               .withDurationMs (450.0)
                               .withValueChangedCallback ([this] (float t)
                               {
                                   constexpr float maxWobblePx = 5.0f;
                                   feedbackWobblePx = std::sin (t * juce::MathConstants<float>::twoPi * 2.5f)
                                                     * (1.0f - t) * maxWobblePx;
                                   repaint();
                               })
                               .build();
    }

    feedbackUpdater.addAnimator (feedbackAnimator, [this]
    {
        feedbackUpdater.removeAnimator (feedbackAnimator);
        feedbackGlow = 0.0f;
        feedbackWobblePx = 0.0f;
        repaint();
    });
    feedbackAnimator.start();
}

juce::Rectangle<int> ChoiceSliderComponent::getTrackArea() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (bigLabelHeight);
    bounds.removeFromBottom (tickLabelHeight);

    // Tick labels are drawn centred on their tick, up to 80px wide (see
    // paint()) - without this inset, the first/last tick sits flush
    // against the component's edge and half its label draws outside the
    // component (and, in practice, outside the whole plugin window),
    // clipping to something like "Hz" instead of "100 Hz". Found by
    // actually running the app and looking at the edge labels, not by
    // reading the layout math.
    return bounds.reduced (40, 0);
}

float ChoiceSliderComponent::xForIndex (int index, const juce::Rectangle<int>& trackArea) const
{
    const auto n = choiceLabels.size();
    if (n <= 1)
        return (float) trackArea.getCentreX();

    const auto proportion = (float) index / (float) (n - 1);
    return (float) trackArea.getX() + proportion * (float) trackArea.getWidth();
}

int ChoiceSliderComponent::indexForX (float x, const juce::Rectangle<int>& trackArea) const
{
    const auto n = choiceLabels.size();
    if (n <= 1)
        return 0;

    const auto proportion = juce::jlimit (0.0f, 1.0f, (x - (float) trackArea.getX()) / (float) trackArea.getWidth());
    return juce::jlimit (0, n - 1, (int) std::round (proportion * (float) (n - 1)));
}

void ChoiceSliderComponent::updatePreviewFromMouse (const juce::MouseEvent& e)
{
    if (answered || choiceLabels.isEmpty())
        return;

    previewIndex = indexForX (e.position.x, getTrackArea());
    repaint();
}

void ChoiceSliderComponent::mouseDown (const juce::MouseEvent& e) { updatePreviewFromMouse (e); }
void ChoiceSliderComponent::mouseDrag (const juce::MouseEvent& e) { updatePreviewFromMouse (e); }

void ChoiceSliderComponent::mouseUp (const juce::MouseEvent&)
{
    // A drag can end with the pointer outside the component, in which case
    // no mouseExit ever arrives - without this the thumb would stay swollen
    // and glowing with the cursor nowhere near it.
    touchTarget = isMouseOver (true) ? 1.0f : 0.0f;

    if (answered || previewIndex < 0)
        return;

    if (onChoiceSelected)
        onChoiceSelected (previewIndex);
}

void ChoiceSliderComponent::paint (juce::Graphics& g)
{
    if (choiceLabels.isEmpty())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto touch = AbcTrainTheme::Ease::out (touchAmount);

    // Entrance: everything rises a few px into place while fading up.
    const auto entered = AbcTrainTheme::Ease::out (enterAmount);
    const auto enterOffsetY = (1.0f - entered) * 10.0f;

    juce::Graphics::ScopedSaveState saved (g);
    g.setOpacity (entered);
    g.addTransform (juce::AffineTransform::translation (0.0f, enterOffsetY));

    auto bounds = getLocalBounds();
    auto bigLabelArea = bounds.removeFromTop (bigLabelHeight);
    auto tickLabelArea = bounds.removeFromBottom (tickLabelHeight);
    // getTrackArea() (not a local recomputation) so the inset that keeps
    // edge tick labels from clipping - see its comment - actually applies
    // here too, not just in the mouse handlers.
    auto trackArea = getTrackArea();

    const auto trackY = (float) trackArea.getCentreY();
    const auto trackThickness = 7.0f + 1.5f * touch;
    const auto trackRect = juce::Rectangle<float> ((float) trackArea.getX(), trackY - trackThickness * 0.5f,
                                                    (float) trackArea.getWidth(), trackThickness);
    const auto trackRadius = trackThickness * 0.5f;

    // Recessed track: a dark well, then a bright hairline along its *lower*
    // inner edge only. Light falling into a real groove catches the far
    // wall, which is the bottom - a uniform outline instead just reads as a
    // drawn pill. Verified by running the app: the first version of this
    // was too low-contrast against the section panel to read as a groove at
    // all, which is why the fill is a flat dark tone rather than a gradient
    // that meets the panel colour at its bottom edge.
    g.setColour (theme.displayBackground);
    g.fillRoundedRectangle (trackRect, trackRadius);

    g.setColour (theme.outline.withAlpha (0.85f));
    g.drawRoundedRectangle (trackRect.reduced (0.5f), trackRadius, 1.0f);

    g.setColour (theme.textBright.withAlpha (0.07f));
    g.drawLine (trackRect.getX() + trackRadius, trackRect.getBottom() - 0.5f,
                trackRect.getRight() - trackRadius, trackRect.getBottom() - 0.5f, 1.0f);

    const auto n = choiceLabels.size();
    const auto highlighted = answered ? chosenIndex : previewIndex;

    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (int i = 0; i < n; ++i)
    {
        const auto x = xForIndex (i, trackArea);

        auto tickColour = theme.textDim.withAlpha (0.55f);
        if (answered && i == correctIndex)
            tickColour = theme.positive;
        else if (answered && i == chosenIndex && ! lastCorrect)
            tickColour = theme.negative;

        // Ticks reach further out from the track as the widget is touched:
        // the scale "opens up" under the pointer.
        const auto tickExtent = 1.0f + 0.25f * touch;
        const auto tickHalf = (float) trackArea.getHeight() * 0.5f * tickExtent;

        g.setColour (tickColour);
        g.drawLine (x, trackY - tickHalf, x, trackY + tickHalf, 2.0f);

        g.setColour (i == highlighted ? theme.text : theme.textDim);
        g.drawText (choiceLabels[i],
                     (int) x - 40, tickLabelArea.getY(), 80, tickLabelArea.getHeight(),
                     juce::Justification::centred);
    }

    if (highlighted >= 0 && highlighted < n)
    {
        // The wobble nudges both the thumb and the big label together, as
        // if the whole answer briefly shook its head "no" - not just one
        // isolated element twitching.
        const auto x = xForIndex (highlighted, trackArea) + feedbackWobblePx;

        auto thumbColour = theme.accent;
        if (answered)
            thumbColour = lastCorrect ? theme.positive : theme.negative;

        // Correct-answer glow: a soft halo that fades out over ~900ms,
        // drawn as a couple of progressively larger/fainter circles
        // behind the crisp thumb - the same cheap fake-blur trick the
        // rotary sliders' drag glow uses.
        if (feedbackGlow > 0.001f)
        {
            const auto glowR1 = baseThumbRadius + 12.0f * feedbackGlow;
            const auto glowR2 = baseThumbRadius + 6.0f * feedbackGlow;
            g.setColour (theme.positive.withAlpha (0.15f * feedbackGlow));
            g.fillEllipse (x - glowR1, trackY - glowR1, glowR1 * 2.0f, glowR1 * 2.0f);
            g.setColour (theme.positive.withAlpha (0.25f * feedbackGlow));
            g.fillEllipse (x - glowR2, trackY - glowR2, glowR2 * 2.0f, glowR2 * 2.0f);
        }

        // The thumb comes alive under the pointer: it swells ~30% and
        // gains a halo, so the thing you're about to drag announces itself.
        const auto thumbRadius = baseThumbRadius * (1.0f + 0.3f * touch);

        if (touch > 0.01f)
        {
            const auto haloRadius = thumbRadius + 7.0f * touch;
            g.setColour (thumbColour.withAlpha (0.20f * touch));
            g.fillEllipse (x - haloRadius, trackY - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);
        }

        const auto thumbBounds = juce::Rectangle<float> (x - thumbRadius, trackY - thumbRadius,
                                                          thumbRadius * 2.0f, thumbRadius * 2.0f);

        juce::Path thumbPath;
        thumbPath.addEllipse (thumbBounds);
        juce::DropShadow thumbShadow (theme.shadow.withAlpha (0.4f * theme.shadowStrength), 6, { 0, 2 });
        thumbShadow.drawForPath (g, thumbPath);

        juce::ColourGradient thumbGradient (thumbColour.brighter (0.22f), thumbBounds.getX(), thumbBounds.getY(),
                                             thumbColour.darker (0.18f), thumbBounds.getX(), thumbBounds.getBottom(),
                                             false);
        g.setGradientFill (thumbGradient);
        g.fillEllipse (thumbBounds);

        // Wide-tracked bold type for the big current-choice readout - this
        // is the one heading-scale element in the whole EarTrainer window.
        AbcTrainLookAndFeel::drawTrackedText (
            g, choiceLabels[highlighted],
            bigLabelArea.toFloat().translated (feedbackWobblePx, 0.0f),
            juce::Font (juce::FontOptions (21.0f, juce::Font::bold)),
            theme.textBright, 1.4f, juce::Justification::centred);
    }
}
