#include "ChoiceSliderComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include <cmath>

namespace
{
    constexpr int bigLabelHeight = 40;   // the large current-choice readout
    constexpr int captionHeight = 18;    // "< first - last >" under the scale
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

void ChoiceSliderComponent::setAxisCaption (const juce::String& caption)
{
    axisCaption = caption;
    repaint();
}

void ChoiceSliderComponent::setPlaceholderText (const juce::String& text)
{
    placeholderText = text;
    repaint();
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

juce::Rectangle<int> ChoiceSliderComponent::getScaleArea() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (bigLabelHeight);       // the large value readout
    bounds.removeFromBottom (captionHeight);     // the axis caption
    return bounds;
}

juce::Rectangle<float> ChoiceSliderComponent::zoneForIndex (int index, juce::Rectangle<float> scaleArea) const
{
    const auto n = choiceLabels.size();
    if (n <= 0)
        return scaleArea;

    // Zones are laid out by proportion rather than by an integer width, so
    // rounding never leaves a one-pixel gap between neighbours or lets the
    // last zone fall short of the panel's right edge.
    const auto left = scaleArea.getX() + scaleArea.getWidth() * (float) index / (float) n;
    const auto right = scaleArea.getX() + scaleArea.getWidth() * (float) (index + 1) / (float) n;

    return { left, scaleArea.getY(), right - left, scaleArea.getHeight() };
}

int ChoiceSliderComponent::indexForX (float x, juce::Rectangle<float> scaleArea) const
{
    const auto n = choiceLabels.size();
    if (n <= 1)
        return 0;

    const auto proportion = juce::jlimit (0.0f, 0.9999f,
                                           (x - scaleArea.getX()) / juce::jmax (1.0f, scaleArea.getWidth()));
    return juce::jlimit (0, n - 1, (int) (proportion * (float) n));
}

void ChoiceSliderComponent::updatePreviewFromMouse (const juce::MouseEvent& e)
{
    if (answered || choiceLabels.isEmpty())
        return;

    previewIndex = indexForX (e.position.x, getScaleArea().toFloat());
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

    paintScale (g);
    paintOverPanel (g);
}

void ChoiceSliderComponent::paintScale (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto touch = AbcTrainTheme::Ease::out (touchAmount);

    // Entrance: everything rises a few px into place while fading up.
    const auto entered = AbcTrainTheme::Ease::out (enterAmount);
    const auto enterOffsetY = (1.0f - entered) * 10.0f;

    juce::Graphics::ScopedSaveState saved (g);
    g.setOpacity (entered);
    g.addTransform (juce::AffineTransform::translation (0.0f, enterOffsetY));

    const auto n = choiceLabels.size();
    const auto highlighted = answered ? chosenIndex : previewIndex;

    const auto scaleArea = getScaleArea().toFloat();

    // ---- the recessed panel the zones live in ----
    AbcTrainLookAndFeel::paintDisplayWell (g, scaleArea);

    juce::Graphics::ScopedSaveState clipped (g);
    juce::Path panelClip;
    panelClip.addRoundedRectangle (scaleArea, AbcTrainTheme::Radius::well);
    g.reduceClipRegion (panelClip);

    for (int i = 0; i < n; ++i)
    {
        const auto zone = zoneForIndex (i, scaleArea);
        const auto isHighlighted = (i == highlighted);

        // Alternating zone shading. Without it a row of ticks reads as one
        // undifferentiated strip; with it, each choice is visibly its own
        // region, which is what makes a wide scale scannable. The step
        // between the two shades has to be bigger than it looks like it
        // should be on paper - at 0.045/0.015 (the first attempt, checked
        // in the running app) the bands were indistinguishable.
        auto zoneFill = (i % 2 == 0) ? theme.displayBackground.brighter (0.11f)
                                     : theme.displayBackground.brighter (0.02f);

        if (answered && i == correctIndex)
            zoneFill = theme.positive.withAlpha (0.28f);
        else if (answered && i == chosenIndex && ! lastCorrect)
            zoneFill = theme.negative.withAlpha (0.28f);
        else if (isHighlighted)
            zoneFill = theme.accent.withAlpha (0.26f + 0.08f * touch);

        g.setColour (zoneFill);
        g.fillRect (zone);

        // Hairline between zones (not after the last one).
        if (i > 0)
        {
            g.setColour (theme.outline.withAlpha (0.5f));
            g.drawLine (zone.getX(), zone.getY(), zone.getX(), zone.getBottom(), 1.0f);
        }
    }

    // ---- tick marks and staggered labels ----
    // Labels alternate between two rows: at eight or nine choices, one row
    // of labels across this width collides. Staggering is what the
    // reference scales do for exactly the same reason.
    g.setFont (juce::Font (juce::FontOptions (11.5f)));

    const auto labelRowHeight = 15.0f;
    const auto lowerRowY = scaleArea.getBottom() - labelRowHeight - 2.0f;
    const auto upperRowY = lowerRowY - labelRowHeight + 2.0f;

    for (int i = 0; i < n; ++i)
    {
        const auto zone = zoneForIndex (i, scaleArea);
        const auto centreX = zone.getCentreX();
        const auto isHighlighted = (i == highlighted);

        auto tickColour = theme.textDim.withAlpha (0.5f);
        if (answered && i == correctIndex)
            tickColour = theme.positive;
        else if (answered && i == chosenIndex && ! lastCorrect)
            tickColour = theme.negative;
        else if (isHighlighted)
            tickColour = theme.accent;

        // Tall thin tick spanning most of the panel, stopping above the
        // label rows.
        g.setColour (tickColour);
        g.drawLine (centreX, scaleArea.getY() + 8.0f, centreX, upperRowY - 3.0f,
                    isHighlighted ? 2.0f : 1.0f);

        const auto rowY = (i % 2 == 0) ? lowerRowY : upperRowY;
        const auto labelWidth = juce::jmax (zone.getWidth(), 54.0f);

        g.setColour (isHighlighted ? theme.textBright : theme.textDim);
        g.drawText (choiceLabels[i],
                     juce::Rectangle<float> (centreX - labelWidth * 0.5f, rowY, labelWidth, labelRowHeight),
                     juce::Justification::centred, false);
    }

    // ---- correct-answer glow, drawn over the chosen zone ----
    if (feedbackGlow > 0.001f && highlighted >= 0 && highlighted < n)
    {
        const auto zone = zoneForIndex (highlighted, scaleArea);
        for (int layer = 3; layer >= 1; --layer)
        {
            g.setColour (theme.positive.withAlpha (0.10f * feedbackGlow / (float) layer));
            g.fillRect (zone.expanded (3.0f * (float) layer, 0.0f));
        }
    }
}

void ChoiceSliderComponent::paintOverPanel (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto entered = AbcTrainTheme::Ease::out (enterAmount);
    const auto enterOffsetY = (1.0f - entered) * 10.0f;

    juce::Graphics::ScopedSaveState saved (g);
    g.setOpacity (entered);
    g.addTransform (juce::AffineTransform::translation (0.0f, enterOffsetY));

    const auto n = choiceLabels.size();
    const auto highlighted = answered ? chosenIndex : previewIndex;

    auto bounds = getLocalBounds();
    const auto bigLabelArea = bounds.removeFromTop (bigLabelHeight).toFloat();
    const auto captionArea = getLocalBounds().removeFromBottom (captionHeight).toFloat();

    // ---- big current-choice readout ----
    if (highlighted >= 0 && highlighted < n)
    {
        auto valueColour = theme.textBright;
        if (answered)
            valueColour = lastCorrect ? theme.positive : theme.negative;

        // The wobble nudges the readout, so a wrong answer reads as the
        // whole answer shaking its head rather than one element twitching.
        AbcTrainLookAndFeel::drawTrackedText (
            g, choiceLabels[highlighted],
            bigLabelArea.translated (feedbackWobblePx, 0.0f),
            juce::Font (juce::FontOptions (26.0f, juce::Font::bold)),
            valueColour, 1.6f, juce::Justification::centred);
    }
    else
    {
        g.setColour (theme.textDim.withAlpha (0.55f));
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (placeholderText, bigLabelArea, juce::Justification::centred, false);
    }

    // ---- axis caption ----
    if (axisCaption.isNotEmpty())
    {
        AbcTrainLookAndFeel::drawTrackedText (g, axisCaption, captionArea,
                                               AbcTrainLookAndFeel::captionFont(),
                                               theme.textDim.withAlpha (0.8f), 1.4f,
                                               juce::Justification::centred);
    }
}
