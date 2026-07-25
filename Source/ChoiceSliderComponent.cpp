#include "ChoiceSliderComponent.h"

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
    previewIndex = -1;
    answered = false;
    correctIndex = -1;
    chosenIndex = -1;
    lastCorrect = false;
    repaint();
}

void ChoiceSliderComponent::showAnswer (int newCorrectIndex, int newChosenIndex, bool wasCorrect)
{
    answered = true;
    correctIndex = newCorrectIndex;
    chosenIndex = newChosenIndex;
    lastCorrect = wasCorrect;
    previewIndex = newChosenIndex;
    repaint();
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
        const auto x = xForIndex (highlighted, trackArea);

        auto thumbColour = accentColour;
        if (answered)
            thumbColour = lastCorrect ? correctColour : wrongColour;

        g.setColour (thumbColour);
        g.fillEllipse (x - thumbRadius, trackY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);

        g.setColour (bodyTextColour);
        g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        g.drawText (choiceLabels[highlighted], bigLabelArea, juce::Justification::centred);
    }
}
