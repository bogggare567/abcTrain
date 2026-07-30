#include "TourOverlay.h"
#include "AbcTrainTheme.h"
#include "AbcTrainLookAndFeel.h"

namespace
{
    constexpr int captionHeight = 88;
    constexpr int captionWidth = 320;
    constexpr int buttonHeight = 28;
}

TourOverlay::TourOverlay()
{
    addChildComponent (nextButton);
    addChildComponent (skipButton);

    nextButton.onClick = [this] { goTo (currentStep + 1); };
    skipButton.onClick = [this] { stop(); };

    startTimerHz (60);
}

TourOverlay::~TourOverlay() = default;

void TourOverlay::addStep (juce::Component* target, juce::String text)
{
    steps.push_back ({ juce::Component::SafePointer<juce::Component> (target), std::move (text) });
}

void TourOverlay::clearSteps()
{
    steps.clear();
    currentStep = -1;
}

void TourOverlay::setStrings (juce::String next, juce::String skip, juce::String done)
{
    nextButton.setButtonText (next);
    skipButton.setButtonText (skip);
    doneText = std::move (done);
}

void TourOverlay::start()
{
    if (steps.empty())
        return;

    running = true;
    appear = 0.0f;
    setVisible (true);
    toFront (false);
    goTo (0);

    // Start the hole where it is going, at zero size, so the first step
    // opens outward from its own control rather than sliding in from a
    // corner it has nothing to do with.
    drawnHole = targetHole.withSizeKeepingCentre (0.0f, 0.0f);
}

void TourOverlay::stop()
{
    running = false;
    setVisible (false);

    if (onFinished != nullptr)
        onFinished();
}

void TourOverlay::goTo (int index)
{
    // Skip past any step whose control is not on screen - the tour points
    // at real widgets, and which ones exist depends on the exercise.
    while (index < (int) steps.size()
           && (steps[(size_t) index].target == nullptr
               || ! steps[(size_t) index].target->isVisible()))
    {
        ++index;
    }

    if (index >= (int) steps.size())
    {
        stop();
        return;
    }

    currentStep = index;
    targetHole = targetBoundsFor (index);

    const auto onLastStep = index + 1 >= (int) steps.size();
    nextButton.setButtonText (onLastStep ? doneText : nextButton.getButtonText());

    resized();
    repaint();
}

juce::Rectangle<float> TourOverlay::targetBoundsFor (int index) const
{
    if (index < 0 || index >= (int) steps.size())
        return getLocalBounds().toFloat().withSizeKeepingCentre (0.0f, 0.0f);

    auto* target = steps[(size_t) index].target.getComponent();

    if (target == nullptr)
        return getLocalBounds().toFloat().withSizeKeepingCentre (0.0f, 0.0f);

    return getLocalArea (target, target->getLocalBounds()).toFloat().expanded (8.0f);
}

juce::Rectangle<int> TourOverlay::captionBoundsFor (juce::Rectangle<float> hole) const
{
    // Below the hole if there is room, above it otherwise, and never off
    // the side. A caption that covers the thing it is describing is worse
    // than no caption.
    const auto area = getLocalBounds();
    auto x = juce::jlimit (area.getX() + 12, juce::jmax (area.getX() + 12, area.getRight() - captionWidth - 12),
                            (int) hole.getCentreX() - captionWidth / 2);

    const auto below = (int) hole.getBottom() + 20;
    const auto y = below + captionHeight + 12 <= area.getBottom()
                       ? below
                       : juce::jmax (area.getY() + 12, (int) hole.getY() - captionHeight - 20);

    return { x, y, captionWidth, captionHeight };
}

void TourOverlay::resized()
{
    if (currentStep < 0)
        return;

    auto caption = captionBoundsFor (targetHole).reduced (14, 12);
    auto row = caption.removeFromBottom (buttonHeight);

    nextButton.setVisible (true);
    skipButton.setVisible (true);
    nextButton.setBounds (row.removeFromRight (96));
    row.removeFromRight (AbcTrainTheme::Spacing::small);
    skipButton.setBounds (row.removeFromRight (110));
}

void TourOverlay::timerCallback()
{
    if (! running)
        return;

    auto moved = false;

    if (appear < 1.0f)
    {
        appear = juce::jmin (1.0f, appear + 1.0f / 18.0f);
        moved = true;
    }

    const auto approach = [] (float current, float goal)
    {
        return current + (goal - current) * 0.22f;
    };

    const juce::Rectangle<float> stepped (approach (drawnHole.getX(), targetHole.getX()),
                                           approach (drawnHole.getY(), targetHole.getY()),
                                           approach (drawnHole.getWidth(), targetHole.getWidth()),
                                           approach (drawnHole.getHeight(), targetHole.getHeight()));

    if (std::abs (stepped.getX() - drawnHole.getX()) > 0.2f
        || std::abs (stepped.getY() - drawnHole.getY()) > 0.2f
        || std::abs (stepped.getWidth() - drawnHole.getWidth()) > 0.2f
        || std::abs (stepped.getHeight() - drawnHole.getHeight()) > 0.2f)
    {
        drawnHole = stepped;
        moved = true;
    }

    if (moved)
        repaint();
}

void TourOverlay::completeAnimation()
{
    appear = 1.0f;
    drawnHole = targetHole;
}

void TourOverlay::mouseUp (const juce::MouseEvent& event)
{
    // Clicking anywhere outside the caption advances. The buttons are there
    // for people who want a button; nobody should have to find one.
    if (! captionBoundsFor (targetHole).contains (event.getPosition()))
        goTo (currentStep + 1);
}

void TourOverlay::paint (juce::Graphics& g)
{
    if (currentStep < 0 || currentStep >= (int) steps.size())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (appear);

    // Dim everything, then punch the hole out of the dimming rather than
    // drawing a ring on top of it: a ring leaves the control as dark as its
    // surroundings, and the point is that this one thing is lit.
    juce::Path shade;
    shade.addRectangle (getLocalBounds());
    shade.addRoundedRectangle (drawnHole, AbcTrainTheme::Radius::panel);
    shade.setUsingNonZeroWinding (false);

    g.setColour (theme.shadow.withAlpha (0.58f * eased));
    g.fillPath (shade);

    // Two rings: a soft wide one that reads as a glow, and a crisp one on
    // the edge. The control underneath is undimmed, so this only has to say
    // "here", not "look harder".
    g.setColour (theme.accent.withAlpha (0.20f * eased));
    g.drawRoundedRectangle (drawnHole.expanded (4.0f), AbcTrainTheme::Radius::panel + 4.0f, 6.0f);
    g.setColour (theme.accent.withAlpha (0.9f * eased));
    g.drawRoundedRectangle (drawnHole, AbcTrainTheme::Radius::panel, 1.6f);

    const auto caption = captionBoundsFor (targetHole).toFloat();

    // A speech bubble with a tail, not a floating panel. The tail is the
    // whole point: a card near a highlighted control still leaves you
    // working out which of the two things it belongs to, and an arrow
    // pointing at it does not. Comics settled this a century ago.
    juce::Path bubble;
    bubble.addRoundedRectangle (caption, AbcTrainTheme::Radius::panel);

    {
        // The tail leaves from whichever edge faces the hole, and its tip
        // stops just short of the outline so the two shapes read as
        // pointing rather than touching.
        const auto below = caption.getY() > drawnHole.getBottom();
        const auto tipY = below ? drawnHole.getBottom() + 3.0f : drawnHole.getY() - 3.0f;
        const auto baseY = below ? caption.getY() + 1.0f : caption.getBottom() - 1.0f;

        const auto tipX = juce::jlimit (caption.getX() + 24.0f, caption.getRight() - 24.0f,
                                         drawnHole.getCentreX());

        bubble.startNewSubPath (tipX - 11.0f, baseY);
        bubble.lineTo (tipX, tipY);
        bubble.lineTo (tipX + 11.0f, baseY);
        bubble.closeSubPath();
    }

    juce::DropShadow (theme.shadow.withAlpha (0.6f), 18, { 0, 4 }).drawForPath (g, bubble);

    g.setColour (theme.panelBackground);
    g.fillPath (bubble);
    g.setColour (theme.accent.withAlpha (0.55f));
    g.strokePath (bubble, juce::PathStrokeType (1.4f));

    auto text = caption.toNearestInt().reduced (14, 12);
    text.removeFromBottom (buttonHeight);

    {
        auto counter = text.removeFromTop (14);
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::microFont());
        g.drawText (juce::String (currentStep + 1) + " / " + juce::String ((int) steps.size()),
                     counter, juce::Justification::topRight, false);
    }

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    g.drawFittedText (steps[(size_t) currentStep].text, text, juce::Justification::topLeft, 3);
}
