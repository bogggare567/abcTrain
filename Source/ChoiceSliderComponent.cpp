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

    const auto previousReveal = revealAmount;
    if (answered && revealAmount < 1.0f)
        revealAmount = juce::jmin (1.0f, revealAmount + step / (float) AbcTrainTheme::Duration::transition);

    if (std::abs (touchAmount - previousTouch) > 0.002f
        || std::abs (enterAmount - previousEnter) > 0.002f
        || std::abs (revealAmount - previousReveal) > 0.002f)
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
    cursorEngaged = false;
    targetNormalised = -1.0f;
    feedbackGlow = 0.0f;
    feedbackWobblePx = 0.0f;
    revealAmount = 1.0f;   // idle; the next answer restarts it at 0
    repaint();
}

void ChoiceSliderComponent::showAnswer (int newCorrectIndex, int newChosenIndex, bool wasCorrect)
{
    answered = true;
    correctIndex = newCorrectIndex;
    chosenIndex = newChosenIndex;
    lastCorrect = wasCorrect;
    previewIndex = newChosenIndex;
    revealAmount = 0.0f;
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

        // A decaying oscillation, tuned as a *sway* rather than a shake.
        // The first version ran 2.5 cycles in 450 ms - about 5.5 Hz, which
        // is squarely in the range the eye reads as vibration, and it is
        // exactly the kind of jitter that makes an interface feel cheap.
        // 1.5 cycles over 720 ms is roughly 2 Hz: slow enough to read as a
        // deliberate head-shake. The envelope is squared rather than
        // linear so the motion has already almost stopped by the time the
        // animation ends, instead of being cut off with velocity left in
        // it - a linear envelope is what makes the *end* of a wobble look
        // clipped even when the wobble itself is gentle.
        feedbackAnimator = juce::ValueAnimatorBuilder{}
                               .withEasing (juce::Easings::createLinear())
                               .withDurationMs (AbcTrainTheme::Duration::sway)
                               .withValueChangedCallback ([this] (float t)
                               {
                                   constexpr float maxSwayPx = 6.0f;
                                   const auto decay = (1.0f - t) * (1.0f - t);
                                   feedbackWobblePx = std::sin (t * juce::MathConstants<float>::twoPi * 1.5f)
                                                     * decay * maxSwayPx;
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

    // Only reserved when there is actually a caption to put there. It was
    // taken unconditionally, which cost every scale 18px of height for a
    // strip that has been empty since the names moved into the zones.
    if (axisCaption.isNotEmpty())
        bounds.removeFromBottom (captionHeight);

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

    const auto scaleArea = getScaleArea().toFloat();

    if (continuousMode)
    {
        // No snapping: the answer is wherever the pointer is, to the pixel.
        cursorNormalised = juce::jlimit (0.0f, 1.0f,
                                          (e.position.x - scaleArea.getX())
                                              / juce::jmax (1.0f, scaleArea.getWidth()));
        cursorEngaged = true;
        repaint();
        return;
    }

    previewIndex = indexForX (e.position.x, scaleArea);
    repaint();
}

void ChoiceSliderComponent::mouseDown (const juce::MouseEvent& e) { updatePreviewFromMouse (e); }
void ChoiceSliderComponent::mouseDrag (const juce::MouseEvent& e) { updatePreviewFromMouse (e); }

void ChoiceSliderComponent::mouseMove (const juce::MouseEvent& e)
{
    // Continuous mode tracks the bare pointer, not just drags - the value
    // readout is meant to follow the mouse the moment it's over the ruler,
    // which is what makes the scale feel like an instrument you sweep
    // rather than a control you have to grab first.
    if (continuousMode)
        updatePreviewFromMouse (e);
}

void ChoiceSliderComponent::mouseUp (const juce::MouseEvent&)
{
    // A drag can end with the pointer outside the component, in which case
    // no mouseExit ever arrives - without this the thumb would stay swollen
    // and glowing with the cursor nowhere near it.
    touchTarget = isMouseOver (true) ? 1.0f : 0.0f;

    if (answered)
        return;

    if (continuousMode)
    {
        if (cursorEngaged && onContinuousChoice != nullptr)
            onContinuousChoice (cursorNormalised);
        return;
    }

    if (previewIndex < 0)
        return;

    if (onChoiceSelected)
        onChoiceSelected (previewIndex);
}

void ChoiceSliderComponent::paint (juce::Graphics& g)
{
    if (choiceLabels.isEmpty() && gridMarks.empty())
        return;

    const auto entered = AbcTrainTheme::Ease::out (enterAmount);

    juce::Graphics::ScopedSaveState saved (g);
    g.setOpacity (entered);
    g.addTransform (juce::AffineTransform::translation (0.0f, (1.0f - entered) * 10.0f));

    if (continuousMode)
    {
        paintContinuousScale (g);
        paintContinuousOverlay (g);
    }
    else
    {
        paintScale (g);
        paintOverPanel (g);
    }

    if (axisCaption.isNotEmpty())
    {
        AbcTrainLookAndFeel::drawTrackedText (g, axisCaption,
                                               getLocalBounds().removeFromBottom (captionHeight).toFloat(),
                                               AbcTrainLookAndFeel::captionFont(),
                                               AbcTrainTheme::current().textDim.withAlpha (0.8f), 1.4f,
                                               juce::Justification::centred);
    }
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

        // Alternating zone shading, for a row long enough that the eye
        // needs help counting it. With exactly two it does the opposite:
        // one lighter half beside one darker half reads as "the left one
        // is already selected", which is a lie the moment the panel opens.
        // Two identical halves and the hairline between them is all a
        // pair needs.
        const auto zoneFill = (n > 2 && i % 2 == 0) ? theme.displayBackground.brighter (0.11f)
                                                    : theme.displayBackground.brighter (0.02f);

        g.setColour (zoneFill);
        g.fillRect (zone);

        // Verdict and highlight are overlays on the stripe rather than
        // replacements for it, so the answer colours can fade in on the
        // reveal instead of snapping - the stripe stays put underneath.
        const auto reveal = AbcTrainTheme::Ease::out (revealAmount);
        auto overlay = juce::Colours::transparentBlack;

        if (answered && i == correctIndex)
            overlay = theme.positive.withAlpha (0.28f * reveal);
        else if (answered && i == chosenIndex && ! lastCorrect)
            overlay = theme.negative.withAlpha (0.28f * reveal);
        else if (isHighlighted)
            overlay = theme.accent.withAlpha (0.26f + 0.08f * touch);

        if (! overlay.isTransparent())
        {
            g.setColour (overlay);
            g.fillRect (zone);
        }

        // Hairline between zones (not after the last one).
        if (i > 0)
        {
            g.setColour (theme.outline.withAlpha (0.5f));
            g.drawLine (zone.getX(), zone.getY(), zone.getX(), zone.getBottom(), 1.0f);
        }
    }

    // ---- the choice names ----
    // No tick marks. A tick is a slider's way of saying "the value is
    // *here* on a continuum", and these zones are not a continuum - they
    // are two named things you pick between. The line down the middle of
    // each one was inherited from the ruler this widget started as, and
    // it only ever pointed at its own label. The zone already has its own
    // shading and its own hairline edges; what it needed was for the name
    // to be the thing you see, centred in the region you click.
    const auto scale = AbcTrainLookAndFeel::getTextScale();

    for (int i = 0; i < n; ++i)
    {
        const auto zone = zoneForIndex (i, scaleArea);
        const auto isHighlighted = (i == highlighted);

        auto textColour = theme.text;
        if (answered && i == correctIndex)
            textColour = theme.textBright;
        else if (answered && i == chosenIndex && ! lastCorrect)
            textColour = theme.textBright;
        else if (isHighlighted)
            textColour = theme.textBright;

        // Sized against the zone, floored so a narrow one stays legible
        // and capped so two zones across a stretched window don't turn
        // into a billboard. drawFittedText does the rest: a long
        // translated name shrinks and wraps rather than running out of
        // its own region.
        const auto height = juce::jlimit (16.0f, 34.0f, zone.getWidth() * 0.075f) * scale;

        g.setColour (textColour);
        g.setFont (AbcTrainLookAndFeel::displayFont().withHeight (height));
        g.drawFittedText (choiceLabels[i],
                           zone.reduced (14.0f, 12.0f).toNearestInt(),
                           juce::Justification::centred, 2, 1.0f);
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
    const auto n = choiceLabels.size();
    const auto highlighted = answered ? chosenIndex : previewIndex;

    auto bounds = getLocalBounds();
    const auto bigLabelArea = bounds.removeFromTop (bigLabelHeight).toFloat();

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
            AbcTrainLookAndFeel::titleFont().withHeight (26.0f * AbcTrainLookAndFeel::getTextScale()),
            valueColour, 1.6f, juce::Justification::centred);
    }
    else
    {
        g.setColour (theme.textDim.withAlpha (0.55f));
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (placeholderText, bigLabelArea, juce::Justification::centred, false);
    }

}

// ============================ continuous mode ============================

void ChoiceSliderComponent::setContinuousScale (std::vector<Game::GridMark> marks,
                                                 float newTolerance,
                                                 std::function<juce::String (float)> formatter)
{
    continuousMode = true;
    gridMarks = std::move (marks);
    toleranceNormalised = newTolerance;
    valueFormatter = std::move (formatter);
    repaint();
}

void ChoiceSliderComponent::setDiscreteScale()
{
    continuousMode = false;
    gridMarks.clear();
    valueFormatter = nullptr;
    repaint();
}

void ChoiceSliderComponent::showContinuousAnswer (float chosen, float target, bool wasCorrect)
{
    answered = true;
    lastCorrect = wasCorrect;
    cursorNormalised = juce::jlimit (0.0f, 1.0f, chosen);
    cursorEngaged = true;
    targetNormalised = juce::jlimit (0.0f, 1.0f, target);
    revealAmount = 0.0f;
    startFeedbackAnimation (wasCorrect);
    repaint();
}

void ChoiceSliderComponent::paintContinuousScale (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto touch = AbcTrainTheme::Ease::out (touchAmount);
    const auto scaleArea = getScaleArea().toFloat();

    AbcTrainLookAndFeel::paintDisplayWell (g, scaleArea);

    juce::Graphics::ScopedSaveState clipped (g);
    juce::Path panelClip;
    panelClip.addRoundedRectangle (scaleArea, AbcTrainTheme::Radius::well);
    g.reduceClipRegion (panelClip);

    const auto xFor = [&] (float normalised)
    {
        return scaleArea.getX() + scaleArea.getWidth() * juce::jlimit (0.0f, 1.0f, normalised);
    };

    // ---- tolerance band ----
    // Before answering it travels with the cursor, so the player can see
    // exactly how much slack this difficulty allows *while aiming*. After
    // answering it *slides* from the guess to the target - the size of the
    // miss is shown as motion, not left to be read off two numbers - and
    // settles there, so "was I inside?" is answerable at a glance.
    if (toleranceNormalised > 0.0f)
    {
        const auto reveal = AbcTrainTheme::Ease::out (revealAmount);
        const auto bandCentre = answered
                                    ? cursorNormalised + (targetNormalised - cursorNormalised) * reveal
                                    : cursorNormalised;
        const auto showBand = answered || cursorEngaged;

        if (showBand && bandCentre >= 0.0f)
        {
            const auto left = xFor (bandCentre - toleranceNormalised);
            const auto right = xFor (bandCentre + toleranceNormalised);
            const auto band = juce::Rectangle<float> (left, scaleArea.getY(),
                                                       right - left, scaleArea.getHeight());

            // Strong enough to actually see: at 0.055 alpha (the first
            // attempt) the band was invisible against the well, which
            // defeats its whole purpose of showing the player their slack.
            // The colour crosses from accent to verdict as the band
            // travels, so it arrives at the target already telling you
            // how the round went.
            const auto verdict = lastCorrect ? theme.positive : theme.negative;
            auto bandColour = theme.accent.withAlpha (0.14f);
            auto edgeColour = theme.accent.withAlpha (0.3f);
            if (answered)
            {
                bandColour = theme.accent.interpolatedWith (verdict, reveal)
                                          .withAlpha (0.14f + 0.06f * reveal);
                edgeColour = theme.accent.interpolatedWith (verdict, reveal)
                                          .withAlpha (0.3f + 0.15f * reveal);
            }

            g.setColour (bandColour);
            g.fillRect (band);

            g.setColour (edgeColour);
            g.drawLine (band.getX(), band.getY(), band.getX(), band.getBottom(), 1.0f);
            g.drawLine (band.getRight(), band.getY(), band.getRight(), band.getBottom(), 1.0f);
        }
    }

    // ---- ruler ----
    // Row height measured from the font rather than written down as 15.
    // The font already scales with the accessibility text-size setting -
    // the comment further down says as much - but the box it is drawn
    // into did not, so at any scale above 1 the glyphs grew past their
    // own row and the lower series was sliced in half by the panel edge.
    // The same literal-versus-scaled mismatch the note below describes,
    // one level up.
    const auto markFont = AbcTrainLookAndFeel::microFont();
    const auto labelRowHeight = std::ceil (markFont.getHeight() * 1.05f) + 5.0f;
    const auto lowerRowY = scaleArea.getBottom() - labelRowHeight - 3.0f;
    const auto upperRowY = lowerRowY - labelRowHeight + 1.0f;

    for (const auto& mark : gridMarks)
    {
        const auto x = xFor (mark.normalised);

        g.setColour (theme.textDim.withAlpha (mark.emphasised ? 0.42f : 0.22f));
        g.drawLine (x, scaleArea.getY() + 6.0f, x, upperRowY - 4.0f, 1.0f);

        // Emphasised marks (the octave centres) take the lower row, the
        // half-octave boundaries the upper one - so neither series ever
        // collides with the other however dense the ruler gets.
        // Clamp each label box inside the panel. Without this the first
        // and last marks - which sit right on the panel's edges - lose
        // half their text to the clip region, which showed up in the
        // running app as a bare "Hz" on the left and a truncated "18.1"
        // on the right.
        constexpr auto labelWidth = 56.0f;
        const auto labelX = juce::jlimit (scaleArea.getX() + 1.0f,
                                           scaleArea.getRight() - labelWidth - 1.0f,
                                           x - labelWidth * 0.5f);

        const auto rowY = mark.emphasised ? lowerRowY : upperRowY;
        // Derived from the ladder's micro step so the text-size slider and
        // the typeface choice reach these labels too - they were raw
        // literals, which is why they stayed put while every other string
        // in the window scaled.
        g.setFont (markFont.withHeight (markFont.getHeight() * (mark.emphasised ? 1.05f : 0.95f)));
        g.setColour (theme.textDim.withAlpha (mark.emphasised ? 0.85f : 0.5f));
        g.drawText (mark.label, juce::Rectangle<float> (labelX, rowY, labelWidth, labelRowHeight),
                     juce::Justification::centred, false);
    }

    // ---- the target marker, once the round is over ----
    // Sweeps down as the band arrives rather than appearing fully formed.
    if (answered && targetNormalised >= 0.0f)
    {
        const auto reveal = AbcTrainTheme::Ease::out (revealAmount);
        const auto x = xFor (targetNormalised);
        const auto top = scaleArea.getY() + 2.0f;
        const auto fullHeight = scaleArea.getHeight() - 4.0f;
        g.setColour (theme.accentWarm.withAlpha (0.35f + 0.65f * reveal));
        g.drawLine (x, top, x, top + fullHeight * reveal, 2.0f);
    }

    // ---- the player's own line ----
    if (cursorEngaged)
    {
        const auto x = xFor (cursorNormalised) + feedbackWobblePx;

        auto lineColour = theme.accent;
        if (answered)
            lineColour = lastCorrect ? theme.positive : theme.negative;

        // A soft bloom under the line so it reads as lit, and so it stays
        // findable where it crosses a gridline.
        g.setColour (lineColour.withAlpha (0.22f + 0.10f * touch));
        g.drawLine (x, scaleArea.getY() + 2.0f, x, scaleArea.getBottom() - 2.0f, 5.0f);
        g.setColour (lineColour);
        g.drawLine (x, scaleArea.getY() + 2.0f, x, scaleArea.getBottom() - 2.0f, 1.8f);
    }
}

void ChoiceSliderComponent::paintContinuousOverlay (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto scaleArea = getScaleArea().toFloat();
    const auto bigLabelArea = getLocalBounds().removeFromTop (bigLabelHeight).toFloat();

    const auto xFor = [&] (float normalised)
    {
        return scaleArea.getX() + scaleArea.getWidth() * juce::jlimit (0.0f, 1.0f, normalised);
    };

    if (! cursorEngaged || valueFormatter == nullptr)
    {
        g.setColour (theme.textDim.withAlpha (0.55f));
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (placeholderText, bigLabelArea, juce::Justification::centred, false);
        return;
    }

    // The readout rides above the cursor rather than sitting centred, so
    // the number and the line it refers to are never far apart - the whole
    // point of a value that tracks the pointer. Clamped to the panel so it
    // can't run off either end.
    const auto text = valueFormatter (cursorNormalised);
    const auto font = AbcTrainLookAndFeel::titleFont()
                          .withHeight (24.0f * AbcTrainLookAndFeel::getTextScale());
    const auto textWidth = AbcTrainLookAndFeel::trackedTextWidth (text, font, 1.2f) + 16.0f;

    const auto centreX = juce::jlimit (scaleArea.getX() + textWidth * 0.5f,
                                        scaleArea.getRight() - textWidth * 0.5f,
                                        xFor (cursorNormalised) + feedbackWobblePx);

    auto valueColour = theme.textBright;
    if (answered)
        valueColour = lastCorrect ? theme.positive : theme.negative;

    AbcTrainLookAndFeel::drawTrackedText (
        g, text,
        juce::Rectangle<float> (centreX - textWidth * 0.5f, bigLabelArea.getY(),
                                 textWidth, bigLabelArea.getHeight()),
        font, valueColour, 1.2f, juce::Justification::centred);

    // The true answer, in the warm accent, offset from the guess so the
    // two readouts never overlap.
    if (answered && targetNormalised >= 0.0f && ! lastCorrect)
    {
        const auto targetText = valueFormatter (targetNormalised);
        const auto targetFont = AbcTrainLookAndFeel::headingFont();
        const auto targetWidth = AbcTrainLookAndFeel::trackedTextWidth (targetText, targetFont, 1.0f) + 12.0f;
        const auto targetX = juce::jlimit (scaleArea.getX() + targetWidth * 0.5f,
                                            scaleArea.getRight() - targetWidth * 0.5f,
                                            xFor (targetNormalised));

        AbcTrainLookAndFeel::drawTrackedText (
            g, targetText,
            juce::Rectangle<float> (targetX - targetWidth * 0.5f,
                                     bigLabelArea.getY() + bigLabelArea.getHeight() * 0.55f,
                                     targetWidth, bigLabelArea.getHeight() * 0.45f),
            targetFont, theme.accentWarm, 1.0f, juce::Justification::centred);
    }
}
