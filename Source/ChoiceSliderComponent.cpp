#include "ChoiceSliderComponent.h"
#include <cmath>

namespace
{
    // Same muted palette as PluginEditor.cpp/AbcTrainLookAndFeel (see
    // decisions/014) - kept as local constants rather than shared, since
    // this component has no other dependency on the rest of the editor.
    const juce::Colour correctColour { 0xff5fbf7d };
    const juce::Colour wrongColour { 0xffd9615f };
    const juce::Colour trackColour { 0xff2a2a3a };
    const juce::Colour accentColour { 0xff5b9bd5 };
    const juce::Colour bodyTextColour { 0xffe0e0e0 };
    const juce::Colour mutedTextColour { 0xffa0a0b0 };

    constexpr int bigLabelHeight = 28;
    constexpr int tickLabelHeight = 20;
    constexpr float thumbRadius = 8.0f;
}

void ChoiceSliderComponent::setChoices (const juce::StringArray& labels)
{
    choiceLabels = labels;
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
    if (answered || previewIndex < 0)
        return;

    if (onChoiceSelected)
        onChoiceSelected (previewIndex);
}

void ChoiceSliderComponent::paint (juce::Graphics& g)
{
    if (choiceLabels.isEmpty())
        return;

    auto bounds = getLocalBounds();
    auto bigLabelArea = bounds.removeFromTop (bigLabelHeight);
    auto tickLabelArea = bounds.removeFromBottom (tickLabelHeight);
    // getTrackArea() (not a local recomputation) so the inset that keeps
    // edge tick labels from clipping - see its comment - actually applies
    // here too, not just in the mouse handlers.
    auto trackArea = getTrackArea();

    const auto trackY = (float) trackArea.getCentreY();
    g.setColour (trackColour);
    g.fillRoundedRectangle ((float) trackArea.getX(), trackY - 3.0f, (float) trackArea.getWidth(), 6.0f, 3.0f);

    const auto n = choiceLabels.size();
    const auto highlighted = answered ? chosenIndex : previewIndex;

    g.setFont (juce::Font (juce::FontOptions (12.0f)));

    for (int i = 0; i < n; ++i)
    {
        const auto x = xForIndex (i, trackArea);

        auto tickColour = mutedTextColour;
        if (answered && i == correctIndex)
            tickColour = correctColour;
        else if (answered && i == chosenIndex && ! lastCorrect)
            tickColour = wrongColour;

        g.setColour (tickColour);
        g.drawLine (x, (float) trackArea.getY(), x, (float) trackArea.getBottom(), 2.0f);

        g.setColour (i == highlighted ? bodyTextColour : mutedTextColour);
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

        auto thumbColour = accentColour;
        if (answered)
            thumbColour = lastCorrect ? correctColour : wrongColour;

        // Correct-answer glow: a soft halo that fades out over ~900ms,
        // drawn as a couple of progressively larger/fainter circles
        // behind the crisp thumb - the same cheap fake-blur trick the
        // rotary sliders' drag glow uses.
        if (feedbackGlow > 0.001f)
        {
            const auto glowR1 = thumbRadius + 10.0f * feedbackGlow;
            const auto glowR2 = thumbRadius + 5.0f * feedbackGlow;
            g.setColour (correctColour.withAlpha (0.15f * feedbackGlow));
            g.fillEllipse (x - glowR1, trackY - glowR1, glowR1 * 2.0f, glowR1 * 2.0f);
            g.setColour (correctColour.withAlpha (0.25f * feedbackGlow));
            g.fillEllipse (x - glowR2, trackY - glowR2, glowR2 * 2.0f, glowR2 * 2.0f);
        }

        g.setColour (thumbColour);
        g.fillEllipse (x - thumbRadius, trackY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);

        g.setColour (bodyTextColour);
        g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        g.drawText (choiceLabels[highlighted], bigLabelArea.toFloat().translated (feedbackWobblePx, 0.0f).toNearestInt(),
                     juce::Justification::centred);
    }
}
