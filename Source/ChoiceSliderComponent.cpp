#include "ChoiceSliderComponent.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
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

void ChoiceSliderComponent::setOptionNotes (juce::StringArray notes, juce::String caption)
{
    optionNotes = std::move (notes);
    optionCaption = std::move (caption);
    repaint();
}

void ChoiceSliderComponent::resetForNewRound()
{
    if (! feedbackAnimator.isComplete())
        feedbackAnimator.complete();

    previewIndex = -1;
    setPending (false);
    lastStep = -1;
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

    // The readout strip is the continuous mode's: it carries the value
    // riding above the cursor. Named alternatives have no value to read
    // out - the name is set at 52px inside its own panel - so reserving
    // the strip there was 40px of nothing above two cards.
    if (continuousMode)
        bounds.removeFromTop (bigLabelHeight);

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
        // On the dial (panorama) it is the direction from the listener.
        if (isDial())
        {
            const auto d = dialGeometry();
            const auto v = e.position - d.centre;
            const auto angle = std::atan2 (-v.y, v.x);   // pi at the left speaker, 0 at the right
            cursorNormalised = juce::jlimit (0.0f, 1.0f, 1.0f - juce::jlimit (0.0f, juce::MathConstants<float>::pi, angle < -juce::MathConstants<float>::halfPi ? juce::MathConstants<float>::pi : angle)
                                                                    / juce::MathConstants<float>::pi);
        }
        else
        {
            cursorNormalised = juce::jlimit (0.0f, 1.0f,
                                              (e.position.x - scaleArea.getX())
                                                  / juce::jmax (1.0f, scaleArea.getWidth()));
        }
        cursorEngaged = true;

        // Thirty steps across the ruler: a third of an octave on the
        // frequency scale, small enough to count, large enough to feel.
        const auto step = juce::roundToInt (cursorNormalised * 30.0f);
        if (e.mouseWasDraggedSinceMouseDown() && step != lastStep && lastStep >= 0 && onStepCrossed != nullptr)
            onStepCrossed();
        lastStep = step;

        repaint();
        return;
    }

    const auto index = indexForX (e.position.x, scaleArea);
    if (index != previewIndex && previewIndex >= 0 && onStepCrossed != nullptr)
        onStepCrossed();
    previewIndex = index;
    repaint();
}

void ChoiceSliderComponent::setPending (bool shouldBePending)
{
    if (pending == shouldBePending)
        return;

    pending = shouldBePending;

    if (onPendingChanged != nullptr)
        onPendingChanged (pending);
}

void ChoiceSliderComponent::commitPendingAnswer()
{
    if (! hasPendingAnswer())
        return;

    setPending (false);

    if (continuousMode)
    {
        if (onContinuousChoice != nullptr)
            onContinuousChoice (cursorNormalised);
    }
    else if (previewIndex >= 0 && onChoiceSelected != nullptr)
    {
        onChoiceSelected (previewIndex);
    }
}

void ChoiceSliderComponent::nudge (int direction, bool fine)
{
    if (answered || choiceLabels.isEmpty() || direction == 0)
        return;

    if (continuousMode)
    {
        const auto step = fine ? 1.0f / 150.0f : 1.0f / 30.0f;
        cursorNormalised = juce::jlimit (0.0f, 1.0f, (cursorEngaged ? cursorNormalised : 0.5f) + (float) direction * step);
        cursorEngaged = true;
    }
    else
    {
        const auto count = choiceLabels.size();
        previewIndex = previewIndex < 0 ? (direction > 0 ? 0 : count - 1)
                                        : juce::jlimit (0, count - 1, previewIndex + direction);
    }

    if (onStepCrossed != nullptr)
        onStepCrossed();

    setPending (true);
    repaint();
}

void ChoiceSliderComponent::commitFromKeyboard()
{
    if (answered || ! (continuousMode ? cursorEngaged : previewIndex >= 0))
        return;

    setPending (true);
    commitPendingAnswer();
}

void ChoiceSliderComponent::mouseDown (const juce::MouseEvent& e) { updatePreviewFromMouse (e); }
void ChoiceSliderComponent::mouseDrag (const juce::MouseEvent& e) { updatePreviewFromMouse (e); }

void ChoiceSliderComponent::mouseMove (const juce::MouseEvent& e)
{
    // Continuous mode tracks the bare pointer, not just drags - the value
    // readout is meant to follow the mouse the moment it's over the ruler,
    // which is what makes the scale feel like an instrument you sweep
    // rather than a control you have to grab first.
    // A placed answer stays put under a hovering pointer (an iPad with a
    // trackpad); only a touch or a drag moves it.
    if (continuousMode && ! (confirmMode && pending))
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

    if (confirmMode)
    {
        // Placed, not sent: the Answer button sends it.
        setPending (continuousMode ? cursorEngaged : previewIndex >= 0);
        repaint();
        return;
    }

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

void ChoiceSliderComponent::setHintRegion (float centreNormalised, float halfWidthNormalised)
{
    hintCentre = juce::jlimit (0.0f, 1.0f, centreNormalised);
    hintHalfWidth = juce::jmax (0.0f, halfWidthNormalised);
    repaint();
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

    // ---- the zones ----
    //
    // Two panels, each about one answer: a drawn frame, the option's
    // number in small capitals, its name at the size a name deserves, and
    // a line saying what it sounds like. Only the one you are on is
    // filled, in its family's colour at 7%, with the registration marks -
    // the same grammar as an exercise card on the home screen, because it
    // is the same kind of object.
    //
    // They used to be recessed wells with a word centred in each, capped
    // at 190px tall, and the render is what showed that was wrong: two
    // short slabs at the top of a tall space read as a screen that failed
    // to finish loading, not as restraint.
    constexpr float zoneGap = 10.0f;
    const auto reveal = AbcTrainTheme::Ease::out (revealAmount);
    const auto scale = AbcTrainLookAndFeel::getTextScale();

    for (int i = 0; i < n; ++i)
    {
        const auto zone = zoneForIndex (i, scaleArea);
        const auto isHighlighted = (i == highlighted);

        const auto face = zone.withTrimmedLeft (i == 0 ? 0.0f : zoneGap * 0.5f)
                              .withTrimmedRight (i == n - 1 ? 0.0f : zoneGap * 0.5f)
                              .reduced (0.5f);

        // Which colour this card is speaking in. Before an answer that is
        // the accent for the one under the pointer; after it, green for
        // the right answer and red for a wrong one you picked.
        auto voice = theme.outline;
        auto lit = false;

        if (answered && i == correctIndex)          { voice = theme.positive; lit = true; }
        else if (answered && i == chosenIndex)       { voice = theme.negative; lit = true; }
        else if (isHighlighted && ! answered)        { voice = theme.accent;   lit = true; }

        if (lit)
        {
            const auto strength = answered ? reveal : (0.85f + 0.15f * touch);
            g.setColour (voice.withAlpha (0.07f * strength));
            g.fillRect (face);
        }

        g.setColour (lit ? voice.withAlpha (answered ? reveal : 1.0f) : theme.outline);
        g.drawRect (face, 1.0f);

        if (lit)
            AbcTrainLookAndFeel::drawRegistrationMarks (
                g, face, voice.withAlpha (answered ? reveal : 1.0f), 5.0f, 3.0f);

        // ---- what is written inside ----
        //
        // Anchored to the middle third rather than centred in the whole
        // card: with a caption above the name and a sentence below it, a
        // block centred on the card's own centre sits visibly low, because
        // the sentence is the tallest of the three.
        auto inner = face.reduced (juce::jmin (28.0f, face.getWidth() * 0.06f), 20.0f);
        const auto hasNote = i < optionNotes.size() && optionNotes[i].isNotEmpty();

        auto block = inner.withHeight (juce::jmin (inner.getHeight(), 190.0f * scale))
                          .withCentre ({ inner.getCentreX(),
                                          inner.getY() + inner.getHeight() * 0.46f });

        if (optionCaption.isNotEmpty())
            AbcTrainLookAndFeel::drawTrackedText (
                g, AbcTrainLookAndFeel::toCaps (
                       optionCaption.replace ("{{n}}", juce::String (i + 1))),
                block.removeFromTop (18.0f * scale), AbcTrainLookAndFeel::microFont(),
                lit ? voice : theme.textDim, 1.6f, juce::Justification::centredLeft);

        block.removeFromTop (10.0f * scale);

        {
            // Sized against the card, floored so a narrow one stays
            // legible and capped so two cards across a stretched window
            // do not turn into a billboard. drawFittedText does the rest:
            // a long translated name shrinks and wraps rather than running
            // out of its own region.
            const auto height = juce::jlimit (22.0f, 52.0f, face.getWidth() * 0.095f) * scale;
            auto nameArea = block.removeFromTop (height * 1.25f);

            // One size for both cards: the smaller of what each name needs.
            auto nameScale = 1.0f;
            for (int k = 0; k < n; ++k)
                nameScale = juce::jmin (nameScale, AbcTrainLookAndFeel::fitLinesScale (
                    choiceLabels[k], AbcTrainLookAndFeel::displayFont().withHeight (height), nameArea.toNearestInt(), 2));

            g.setColour (answered || isHighlighted ? theme.textBright : theme.text);
            g.setFont (AbcTrainLookAndFeel::displayFont().withHeight (height * nameScale));
            AbcTrainLookAndFeel::fitLines (g, choiceLabels[i], nameArea.toNearestInt(),
                               juce::Justification::centredLeft, 2, 0.85f);
        }

        if (hasNote)
        {
            block.removeFromTop (14.0f * scale);

            const auto noteArea = block.withWidth (juce::jmin (block.getWidth(), 420.0f * scale)).toNearestInt();
            const auto noteLines = juce::jlimit (1, 3, (int) (noteArea.getHeight() / AbcTrainLookAndFeel::bodyFont().getHeight()));
            auto noteScale = 1.0f;
            for (int k = 0; k < juce::jmin (n, optionNotes.size()); ++k)
                noteScale = juce::jmin (noteScale, AbcTrainLookAndFeel::fitLinesScale (
                    optionNotes[k], AbcTrainLookAndFeel::bodyFont(), noteArea, noteLines));

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::bodyFont().withHeight (AbcTrainLookAndFeel::bodyFont().getHeight() * noteScale));
            AbcTrainLookAndFeel::fitLines (g, optionNotes[i], noteArea, juce::Justification::topLeft, noteLines, 0.95f);
        }
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

void ChoiceSliderComponent::paintOverPanel (juce::Graphics&)
{
    const auto n = choiceLabels.size();
    const auto highlighted = answered ? chosenIndex : previewIndex;

    auto bounds = getLocalBounds();

    // ---- the line above the cards ----
    //
    // Only the prompt. The big readout that used to live here echoed the
    // name of whichever card was highlighted - which was worth having when
    // a card was a small zone with a word in it, and became a duplicate
    // the moment the card grew into a panel with the name set at 52px.
    // Two copies of one word, one above the other, is not emphasis.
    //
    // Nothing is drawn before a card is highlighted. The prompt used to
    // sit here as well as in the band heading directly above it and in
    // the exercise line above that - the same sentence three times on one
    // screen, which is how a screen stops being read at all.
    juce::ignoreUnused (bounds, highlighted, n);
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

    // On the page, not in a black well (the author, 2026-10-24: "не нужен
    // чёрный фон, выбивается из общей картины"): a faint tint of the panel
    // and a hairline keep the scale's shape, the page shows through.
    g.setColour (theme.panelBackground.withAlpha (0.22f));
    g.fillRoundedRectangle (scaleArea, AbcTrainTheme::Radius::well);
    g.setColour (theme.outline.withAlpha (0.35f));
    g.drawRoundedRectangle (scaleArea, AbcTrainTheme::Radius::well, 1.0f);

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
        // At rest the band sits where the cursor rests. Hiding it until
        // the first drag left the whole scale an empty black well - the
        // largest thing on the screen saying nothing - and hid the one
        // answer to "what does my level actually change?".
        if (bandCentre >= 0.0f && ! isDial())
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
            const auto atRest = ! answered && ! cursorEngaged;
            auto bandColour = theme.accent.withAlpha (atRest ? 0.10f : 0.14f);
            auto edgeColour = theme.accent.withAlpha (atRest ? 0.26f : 0.3f);
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
    // 10 rather than 3. Measured on a rendered 918x795 window: the lower
    // series was sliced at about 60% of its glyph height even though the
    // arithmetic put its box three pixels clear of the well. I did not
    // find what eats those last few pixels - the numbers on both sides
    // agree and still disagree with the picture - so this is clearance,
    // not a diagnosis. Verified by rendering, which is the only thing
    // that ever catches this class of bug here.
    const auto lowerRowY = scaleArea.getBottom() - labelRowHeight - 10.0f;
    const auto upperRowY = lowerRowY - labelRowHeight + 1.0f;

    // The hint, if one was bought: everything outside the region is
    // dimmed, so what remains is a shorter stretch of the same ruler
    // rather than a marker pointing at the answer.
    if (hintHalfWidth > 0.0f && ! isDial())
    {
        const auto left = xFor (hintCentre - hintHalfWidth);
        const auto right = xFor (hintCentre + hintHalfWidth);

        // Outside the region is pushed well back - the first two versions
        // dimmed it so gently that players pressed the hint, saw nothing
        // change, and reported it as broken. The page colour at high
        // alpha, so it reads the same on both themes.
        g.setColour (theme.windowBackground.withAlpha (0.72f));

        if (left > scaleArea.getX())
            g.fillRect (scaleArea.withRight (left));

        if (right < scaleArea.getRight())
            g.fillRect (scaleArea.withLeft (right));

        // The live stretch is tinted, framed, and says what it is.
        const auto region = scaleArea.withLeft (left).withRight (right);
        juce::ColourGradient lift (theme.accent.withAlpha (0.20f), region.getX(), region.getY(),
                                   theme.accent.withAlpha (0.04f), region.getX(), region.getBottom(), false);
        g.setGradientFill (lift);
        g.fillRect (region);

        g.setColour (theme.accent.withAlpha (0.9f));
        g.fillRect (left - 1.0f, scaleArea.getY(), 2.0f, scaleArea.getHeight());
        g.fillRect (right - 1.0f, scaleArea.getY(), 2.0f, scaleArea.getHeight());

        if (hintCaption.isNotEmpty())
        {
            const auto caps = AbcTrainLookAndFeel::toCaps (hintCaption);
            const auto font = AbcTrainLookAndFeel::microFont();
            const auto width = AbcTrainLookAndFeel::trackedTextWidth (caps, font, 1.4f) + 16.0f;
            auto tag = juce::Rectangle<float> (width, 20.0f)
                           .withCentre ({ region.getCentreX(), scaleArea.getY() + 16.0f });
            tag.setX (juce::jlimit (scaleArea.getX() + 2.0f, scaleArea.getRight() - width - 2.0f, tag.getX()));

            g.setColour (theme.accent);
            g.fillRect (tag);
            AbcTrainLookAndFeel::drawTrackedText (g, caps, tag, font,
                                                  AbcTrainLookAndFeel::labelColourOn (theme.accent), 1.4f,
                                                  juce::Justification::centred);
        }
    }

    // ---- the picture of the answer (ADR 063) ----
    // Drawn above the label row, under the lines: the player's own value
    // in the accent while aiming, and once the answer is in, the round's
    // value in the warm colour beside it.
    if (isDial())
    {
        paintDial (g);
        return;   // the dial is the whole picture: no ruler lines, no vertical cursor
    }

    if (illustration != Game::ScaleIllustration::none)
    {
        const auto plot = scaleArea.withTop (scaleArea.getY() + 30.0f).withBottom (upperRowY - 6.0f).reduced (2.0f, 0.0f);

        if (answered && targetNormalised >= 0.0f)
            paintIllustration (g, plot, targetNormalised, theme.accentWarm.withAlpha (0.35f + 0.5f * AbcTrainTheme::Ease::out (revealAmount)), false);

        paintIllustration (g, plot, cursorNormalised,
                           answered ? (lastCorrect ? theme.positive : theme.negative) : theme.accent,
                           cursorEngaged || answered);
    }

    // Each row remembers where its last label ended: on a phone-width
    // ruler the marks come closer than their labels are wide, and a label
    // that would run into its neighbour is left out rather than drawn
    // over it ("-90-80-70-60"). The line itself always stays.
    float labelRowEnd[2] = { -1.0e9f, -1.0e9f };

    for (const auto& mark : gridMarks)
    {
        const auto x = xFor (mark.normalised);

        // A ruler's lines are scaffolding, not content. At 0.42 they were
        // as loud as the answer drawn on top of them; every reference
        // worth copying puts its grid at a few per cent and lets the
        // signal be the only bright thing in the frame.
        g.setColour (theme.textDim.withAlpha (mark.emphasised ? 0.16f : 0.08f));
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

        // One row of numbers. The half-octave labels used to sit in a
        // second row above, and on a phone the two rows read as two
        // different scales ("-90 -70" over "-100 -50"); their lines stay.
        if (! mark.emphasised)
            continue;

        const auto rowY = mark.emphasised ? lowerRowY : upperRowY;
        // Derived from the ladder's micro step so the text-size slider and
        // the typeface choice reach these labels too - they were raw
        // literals, which is why they stayed put while every other string
        // in the window scaled.
        const auto rowFont = markFont.withHeight (markFont.getHeight() * (mark.emphasised ? 1.05f : 0.95f));
        const auto inkWidth = juce::jmin (labelWidth, juce::GlyphArrangement::getStringWidth (rowFont, mark.label) + 6.0f);
        const auto inkLeft = labelX + (labelWidth - inkWidth) * 0.5f;
        auto& rowEnd = labelRowEnd[mark.emphasised ? 1 : 0];

        if (inkLeft < rowEnd)
            continue;

        rowEnd = inkLeft + inkWidth;
        g.setFont (rowFont);
        g.setColour (theme.textDim.withAlpha (mark.emphasised ? 0.62f : 0.34f));
        AbcTrainLookAndFeel::fitText (g, mark.label, juce::Rectangle<float> (labelX, rowY, labelWidth, labelRowHeight),
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
        // The one bright thing in the frame, and it glows. This is the
        // whole difference between a chart and an instrument.
        g.setColour (lineColour.withAlpha (0.30f + 0.16f * touch));
        g.drawLine (x, scaleArea.getY() + 2.0f, x, scaleArea.getBottom() - 2.0f, 5.0f);
        g.setColour (lineColour);
        g.drawLine (x, scaleArea.getY() + 2.0f, x, scaleArea.getBottom() - 2.0f, 1.8f);

        if (showsGrip && ! answered)
        {
            const auto size = 18.0f + 6.0f * touch;
            const auto grip = juce::Rectangle<float> (size, size).withCentre ({ x, upperRowY - size * 0.5f - 4.0f });
            g.setColour (lineColour.withAlpha (0.25f));
            g.fillRect (grip.expanded (4.0f));
            g.setColour (juce::Colours::white);
            g.fillRect (grip);
        }
    }
}

void ChoiceSliderComponent::paintIllustration (juce::Graphics& g, juce::Rectangle<float> plot, float normalised,
                                               juce::Colour colour, bool strong) const
{
    if (plot.getHeight() < 24.0f || plot.getWidth() < 40.0f || normalised < 0.0f)
        return;

    const auto xFor = [&] (float n) { return plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, n); };
    const auto alpha = strong ? 1.0f : 0.55f;

    switch (illustration)
    {
        case Game::ScaleIllustration::bell:
        {
            // An EQ bell a third of an octave wide either side, the way a
            // boost of this exercise's Q looks on an analyser.
            const auto octaves = illustrationOctaves > 0.0f ? illustrationOctaves : 10.0f;
            const auto up = ! answered || illustrationDirection >= 0;
            const auto base = up ? plot.getBottom() : plot.getY() + plot.getHeight() * 0.25f;
            const auto height = plot.getHeight() * (up ? 0.72f : 0.6f);
            juce::Path curve, fill;
            const int steps = juce::jmax (40, (int) plot.getWidth() / 3);

            for (int i = 0; i <= steps; ++i)
            {
                const auto n = (float) i / (float) steps;
                const auto d = (n - normalised) * octaves;
                const auto y = base + (up ? -1.0f : 1.0f) * height * std::exp (-(d * d) / (2.0f * 0.42f * 0.42f));
                const auto x = xFor (n);
                if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, base); }
                curve.lineTo (x, y);
                fill.lineTo (x, y);
            }

            fill.lineTo (plot.getRight(), base);
            fill.closeSubPath();
            g.setColour (colour.withAlpha (0.10f * alpha));
            g.fillPath (fill);
            g.setColour (colour.withAlpha (0.8f * alpha));
            g.strokePath (curve, juce::PathStrokeType (1.6f));
            break;
        }

        case Game::ScaleIllustration::stage:
        {
            // The stage seen from above: the listener at the front, the
            // arc from the left speaker to the right, the source on it.
            const auto radius = juce::jmin (plot.getWidth() * 0.46f, plot.getHeight() - 14.0f);

            if (radius < 20.0f)
                return;

            const auto centre = juce::Point<float> (plot.getCentreX(), plot.getBottom() - 6.0f);
            const auto angle = juce::MathConstants<float>::pi * (1.0f - juce::jlimit (0.0f, 1.0f, normalised));
            const auto source = centre + juce::Point<float> (std::cos (angle), -std::sin (angle)) * radius;

            {
                juce::Path arc;
                arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                   -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
                g.setColour (AbcTrainTheme::current().textDim.withAlpha (0.28f));
                g.strokePath (arc, juce::PathStrokeType (1.0f));

                for (const auto side : { -1.0f, 1.0f })
                    g.drawRect (juce::Rectangle<float> (10.0f, 14.0f).withCentre (centre + juce::Point<float> (side * radius, -2.0f)), 1.0f);

                g.drawRect (juce::Rectangle<float> (12.0f, 12.0f).withCentre (centre), 1.2f);
            }

            juce::Path ray;
            ray.startNewSubPath (centre);
            ray.lineTo (source);
            const float dashes[] = { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (ray, ray, dashes, 2);
            g.setColour (colour.withAlpha (0.5f * alpha));
            g.fillPath (ray);
            g.setColour (colour.withAlpha (alpha));
            g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (source));
            break;
        }

        case Game::ScaleIllustration::echoes:
        {
            // An echo at this time and the repeats it would make, each
            // quieter by the same step, on the same log axis as the ruler.
            const auto octaves = illustrationOctaves > 0.0f ? illustrationOctaves : 5.0f;
            auto level = 1.0f;

            for (int k = 1; k <= 8; ++k)
            {
                const auto n = normalised + std::log2 ((float) k) / octaves;

                if (n > 1.0f)
                    break;

                const auto x = xFor (n);
                const auto h = plot.getHeight() * 0.8f * level;
                g.setColour (colour.withAlpha (alpha * (k == 1 ? 1.0f : 0.75f)));
                g.fillRect (juce::Rectangle<float> (k == 1 ? 3.0f : 2.0f, h).withPosition (x - 1.0f, plot.getBottom() - h));
                level *= 0.62f;
            }
            break;
        }

        case Game::ScaleIllustration::none:
        default:
            break;
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

    if (valueFormatter == nullptr)
        return;

    // The readout rides above the cursor rather than sitting centred, so
    // the number and the line it refers to are never far apart - the whole
    // point of a value that tracks the pointer. Clamped to the panel so it
    // can't run off either end.
    const auto text = valueFormatter (cursorNormalised);
    const auto font = AbcTrainLookAndFeel::titleFont()
                          .withHeight (24.0f * AbcTrainLookAndFeel::getTextScale());
    const auto textWidth = AbcTrainLookAndFeel::trackedTextWidth (text, font, 1.2f) + 16.0f;

    // On the dial the readout stays over the listener: the needle moves
    // in a circle, and a number chasing it sideways reads as jitter.
    const auto centreX = isDial() ? scaleArea.getCentreX() + feedbackWobblePx
                                  : juce::jlimit (scaleArea.getX() + textWidth * 0.5f,
                                                  scaleArea.getRight() - textWidth * 0.5f,
                                                  xFor (cursorNormalised) + feedbackWobblePx);

    auto valueColour = theme.textBright;
    if (answered)
        valueColour = lastCorrect ? theme.positive : theme.negative;
    else if (! cursorEngaged)
        valueColour = theme.textDim;   // at rest: present, but not an answer yet

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
        const auto targetX = isDial() ? scaleArea.getCentreX() + textWidth * 0.5f + targetWidth * 0.5f + 8.0f
                                      : juce::jlimit (scaleArea.getX() + targetWidth * 0.5f,
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

//==============================================================================
// The panorama as a dial (2026-10-24, the author: "убирай полоску, оставь
// только спидометр, просто сделай больше"). The stage seen from above,
// as large as the well allows: the arc from the left speaker to the right,
// the listener at its centre, the answer as a needle. The ruler's lines,
// band and vertical cursor are not drawn - the arc is the scale. Labels sit
// on the arc where they mean what they say.

bool ChoiceSliderComponent::isDial() const noexcept
{
    return continuousMode && illustration == Game::ScaleIllustration::stage;
}

ChoiceSliderComponent::DialGeometry ChoiceSliderComponent::dialGeometry() const
{
    const auto area = getScaleArea().toFloat().reduced (12.0f, 10.0f);
    DialGeometry d;
    d.radius = juce::jmax (20.0f, juce::jmin (area.getWidth() * 0.46f, area.getHeight() - 34.0f));
    d.centre = { area.getCentreX(), area.getBottom() - 26.0f };
    return d;
}

void ChoiceSliderComponent::paintDial (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto d = dialGeometry();
    const auto pi = juce::MathConstants<float>::pi;
    const auto pointAt = [&] (float normalised, float r)
    {
        const auto a = pi * (1.0f - juce::jlimit (0.0f, 1.0f, normalised));
        return d.centre + juce::Point<float> (std::cos (a), -std::sin (a)) * r;
    };
    const auto arcPath = [&] (float from, float to, float r)
    {
        juce::Path p;
        // JUCE arc angles: 0 at 12 o'clock, clockwise. Normalised 0 is the
        // left speaker (-90 deg), 1 the right one (+90 deg).
        p.addCentredArc (d.centre.x, d.centre.y, r, r, 0.0f,
                         -juce::MathConstants<float>::halfPi + pi * juce::jlimit (0.0f, 1.0f, from),
                         -juce::MathConstants<float>::halfPi + pi * juce::jlimit (0.0f, 1.0f, to), true);
        return p;
    };

    // The scale: the arc, ticks every 10, labels at L100 L50 C R50 R100.
    g.setColour (theme.textDim.withAlpha (0.35f));
    g.strokePath (arcPath (0.0f, 1.0f, d.radius), juce::PathStrokeType (1.5f));

    for (int i = 0; i <= 20; ++i)
    {
        const auto n = (float) i / 20.0f;
        const auto big = i % 5 == 0;
        g.setColour (theme.textDim.withAlpha (big ? 0.55f : 0.25f));
        g.drawLine (juce::Line<float> (pointAt (n, d.radius - (big ? 10.0f : 5.0f)), pointAt (n, d.radius + (big ? 10.0f : 5.0f))), big ? 1.5f : 1.0f);
    }

    g.setFont (AbcTrainLookAndFeel::microFont());
    g.setColour (theme.textDim.withAlpha (0.75f));
    // The same signed numbers the readout and the answer use (the author:
    // a signed number reads as a position, "L50" as letters to decode).
    const auto minus = juce::String::fromUTF8 ("\xe2\x88\x92");
    const juce::String labels[] = { minus + "100", minus + "50", "0", "+50", "+100" };
    for (int i = 0; i < 5; ++i)
    {
        const auto at = pointAt ((float) i / 4.0f, d.radius + 26.0f);
        AbcTrainLookAndFeel::fitText (g, labels[i], juce::Rectangle<float> (48.0f, 16.0f).withCentre (at),
                                      juce::Justification::centred, false);
    }

    // A bought hint: the live stretch of the arc, the rest dimmed.
    if (hintHalfWidth > 0.0f)
    {
        g.setColour (theme.accent.withAlpha (0.28f));
        g.strokePath (arcPath (hintCentre - hintHalfWidth, hintCentre + hintHalfWidth, d.radius), juce::PathStrokeType (14.0f));
    }

    // The slack around the needle, as a stretch of the arc - it travels to
    // the answer once the round is over, like the band on a ruler.
    const auto reveal = AbcTrainTheme::Ease::out (revealAmount);
    if (toleranceNormalised > 0.0f && cursorNormalised >= 0.0f)
    {
        const auto centreN = answered ? cursorNormalised + (targetNormalised - cursorNormalised) * reveal : cursorNormalised;
        auto colour = theme.accent.withAlpha (cursorEngaged ? 0.30f : 0.16f);
        if (answered)
            colour = theme.accent.interpolatedWith (lastCorrect ? theme.positive : theme.negative, reveal).withAlpha (0.35f);
        g.setColour (colour);
        g.strokePath (arcPath (centreN - toleranceNormalised, centreN + toleranceNormalised, d.radius), juce::PathStrokeType (10.0f));
    }

    // Speakers and the listener.
    g.setColour (theme.textDim.withAlpha (0.6f));
    for (const auto side : { 0.0f, 1.0f })
    {
        const auto p = pointAt (side, d.radius);
        g.drawRect (juce::Rectangle<float> (16.0f, 22.0f).withCentre (p.translated (0.0f, 14.0f)), 1.2f);
    }
    g.drawEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (d.centre), 1.4f);

    // The round's answer, once it is in.
    if (answered && targetNormalised >= 0.0f)
    {
        const auto t = pointAt (targetNormalised, d.radius);
        g.setColour (theme.accentWarm.withAlpha (0.35f + 0.65f * reveal));
        g.drawLine (juce::Line<float> (d.centre, t), 2.0f);
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (t));
    }

    // The needle: yours.
    if (cursorNormalised >= 0.0f)
    {
        auto colour = cursorEngaged ? theme.accent : theme.textDim;
        if (answered)
            colour = lastCorrect ? theme.positive : theme.negative;
        const auto tip = pointAt (cursorNormalised, d.radius);
        g.setColour (colour.withAlpha (0.35f));
        g.drawLine (juce::Line<float> (d.centre, tip), 6.0f);
        g.setColour (colour);
        g.drawLine (juce::Line<float> (d.centre, tip), 2.0f);
        const auto size = 20.0f + 6.0f * AbcTrainTheme::Ease::out (touchAmount);
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (tip));
    }
}
