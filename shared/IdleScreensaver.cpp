#include "IdleScreensaver.h"
#include "AbcTrainTheme.h"
#include "AbcTrainLookAndFeel.h"

namespace
{
    constexpr int frameHz = 60;

    // The four skill-family colours, so even this is the product's palette
    // rather than a rainbow.
    juce::Colour bounceColour (int index)
    {
        using Family = AbcTrainTheme::Family;
        const Family families[] = { Family::frequency, Family::dynamics,
                                    Family::space, Family::character };

        return AbcTrainTheme::accentFor (families[(size_t) (index & 3)]);
    }
}

IdleScreensaver::IdleScreensaver()
{
    setWantsKeyboardFocus (true);
    startTimerHz (frameHz);
}

IdleScreensaver::~IdleScreensaver() = default;

void IdleScreensaver::setIdleSeconds (int seconds)
{
    idleSeconds = (double) seconds;

    if (idleSeconds <= 0.0 && isVisible())
        dismiss();
}

void IdleScreensaver::noteActivity()
{
    untouchedFor = 0.0;

    if (isVisible())
        dismiss();
}

void IdleScreensaver::dismiss()
{
    setVisible (false);
    untouchedFor = 0.0;

    if (onDismissed != nullptr)
        onDismissed();
}

juce::Rectangle<float> IdleScreensaver::markBounds() const
{
    const auto font = AbcTrainLookAndFeel::titleFont();
    const auto width = AbcTrainLookAndFeel::trackedTextWidth (text, font, 2.6f) + 4.0f;

    return { position.x, position.y, width, 24.0f };
}

void IdleScreensaver::resized()
{
    // Keep it inside a window that just got smaller, rather than letting it
    // drift off and take a few seconds to wander back.
    const auto mark = markBounds();

    position.x = juce::jlimit (0.0f, juce::jmax (0.0f, (float) getWidth() - mark.getWidth()), position.x);
    position.y = juce::jlimit (0.0f, juce::jmax (0.0f, (float) getHeight() - mark.getHeight()), position.y);
}

void IdleScreensaver::timerCallback()
{
    const auto step = 1.0 / (double) frameHz;

    if (! isVisible())
    {
        if (idleSeconds <= 0.0)
            return;

        // Anything playing counts as use. A screensaver over a round in
        // progress is a bug wearing a costume.
        if (isBusy != nullptr && isBusy())
        {
            untouchedFor = 0.0;
            return;
        }

        untouchedFor += step;

        if (untouchedFor >= idleSeconds && getWidth() > 0)
        {
            untouchedFor = 0.0;
            setVisible (true);
            toFront (true);
        }

        return;
    }

    const auto mark = markBounds();
    const auto maxX = juce::jmax (0.0f, (float) getWidth() - mark.getWidth());
    const auto maxY = juce::jmax (0.0f, (float) getHeight() - mark.getHeight());

    position += velocity * (float) step;

    // Reflect, and reposition to the wall rather than past it: letting it
    // overshoot and bounce next frame is what makes cheap versions of this
    // jitter along the edge.
    if (position.x <= 0.0f || position.x >= maxX)
    {
        position.x = juce::jlimit (0.0f, maxX, position.x);
        velocity.x = -velocity.x;
        ++colourIndex;
    }

    if (position.y <= 0.0f || position.y >= maxY)
    {
        position.y = juce::jlimit (0.0f, maxY, position.y);
        velocity.y = -velocity.y;
        ++colourIndex;
    }

    repaint();
}

void IdleScreensaver::mouseMove (const juce::MouseEvent&)   { dismiss(); }
void IdleScreensaver::mouseDown (const juce::MouseEvent&)   { dismiss(); }

bool IdleScreensaver::keyPressed (const juce::KeyPress&)
{
    dismiss();
    return true;
}

void IdleScreensaver::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    // Nearly black rather than transparent: the point is a dark screen with
    // one moving thing on it, and a translucent wash over nine exercise
    // tiles is just a dimmed app.
    g.fillAll (theme.windowBackground.darker (0.85f));

    // No plate, no glow, no border. Text on black, and nothing else - the
    // plate made it a badge sliding around, and a badge is a thing with a
    // purpose. This has none, which is the point.
    AbcTrainLookAndFeel::drawTrackedText (g, text, markBounds(),
                                           AbcTrainLookAndFeel::titleFont(),
                                           bounceColour (colourIndex).withAlpha (0.72f), 2.6f);
}
