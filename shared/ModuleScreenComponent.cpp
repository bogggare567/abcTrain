#include "ModuleScreenComponent.h"
#include "AbcTrainTheme.h"
#include "AbcTrainLookAndFeel.h"

namespace
{
    constexpr int rowHeight = 46;
    constexpr int buttonHeight = 30;

    // Reading text explains a knob; a panel sized to the whole window
    // just to hold four lines of it says the opposite. Each phase asks for
    // the height it needs, and the check asks for all of it - not to be
    // grand, but because the knobs and the spectrum under it are the
    // answer.
    int contentHeightFor (int phaseIndex)
    {
        switch (phaseIndex)
        {
            case 1:  return 210;  // demo: explanation, step counter
            case 2:  return 190;  // try it
            case 3:  return 250;  // check: question, tier, live readout
            case 4:  return 230;  // result
            default: return 0;    // the shelf sizes itself
        }
    }
}

ModuleScreenComponent::ModuleScreenComponent (juce::AudioProcessorValueTreeState& state,
                                              ModuleProgress& progressToUse,
                                              PracticeAudioSource& source,
                                              std::function<void (const juce::String&, float)> set,
                                              std::function<void()> clear)
    : apvts (state), progress (progressToUse), practiceSource (source),
      setOverride (std::move (set)), clearOverride (std::move (clear))
{
    for (auto* button : { &backButton, &nextButton, &readyButton, &referenceButton,
                          &mineButton, &submitButton, &againButton, &doneButton, &closeButton })
        addChildComponent (*button);

    backButton.onClick = [this]
    {
        if (demoStep > 0)
        {
            --demoStep;
            applyStep (currentModule()->demoSteps[(size_t) demoStep]);
            layoutButtons();
            repaint();
        }
    };

    nextButton.onClick = [this]
    {
        auto* definition = currentModule();

        if (definition == nullptr)
            return;

        if (demoStep + 1 < (int) definition->demoSteps.size())
        {
            ++demoStep;
            applyStep (definition->demoSteps[(size_t) demoStep]);
            layoutButtons();
            repaint();
            return;
        }

        progress.markDemoSeen (definition->id);
        goToPhase (Phase::tryIt);
    };

    readyButton.onClick = [this] { beginCheck(); };

    // The two audition buttons are the whole reason a check by ear is
    // possible at all. Matching a hidden value without being able to
    // switch back to it is not a listening task, it is a guess.
    referenceButton.onClick = [this]
    {
        auditioningReference = true;

        // The knob stays where the player left it; the reference goes
        // straight into the DSP past it.
        if (setOverride != nullptr)
            if (auto* definition = currentModule())
                setOverride (definition->check.parameterID, hiddenTarget);

        refreshAuditionButtons();
        repaint();
    };

    mineButton.onClick = [this]
    {
        auditioningReference = false;

        if (clearOverride != nullptr)
            clearOverride();

        refreshAuditionButtons();
        repaint();
    };

    submitButton.onClick = [this] { submitAnswer(); };

    againButton.onClick = [this] { beginCheck(); };
    doneButton.onClick = [this] { closeModule(); };

    closeButton.onClick = [this]
    {
        stopBed();
        restoreParameters();
        setVisible (false);

        if (onClosed != nullptr)
            onClosed();
    };


    startTimerHz (60);
}

ModuleScreenComponent::~ModuleScreenComponent()
{
    // Stop the bed. The buffer itself is owned by PracticeAudioSource in
    // the processor and deliberately outlives this panel.
    practiceSource.clearOverrideBuffer();

    if (clearOverride != nullptr)
        clearOverride();
}

void ModuleScreenComponent::setModules (std::vector<TrainingModule::Definition> newModules)
{
    modules = std::move (newModules);
    repaint();
}

void ModuleScreenComponent::setWalkthroughs (juce::StringArray names)
{
    walkthroughs = std::move (names);
    repaint();
}

void ModuleScreenComponent::setStrings (Strings newStrings)
{
    text = std::move (newStrings);

    backButton.setButtonText (text.back);
    nextButton.setButtonText (text.next);
    readyButton.setButtonText (text.ready);
    referenceButton.setButtonText (text.reference);
    mineButton.setButtonText (text.mine);
    submitButton.setButtonText (text.submit);
    againButton.setButtonText (text.again);
    doneButton.setButtonText (text.done);
    closeButton.setButtonText (text.close);

    layoutButtons();
    repaint();
}

void ModuleScreenComponent::setAccentColour (juce::Colour colour)
{
    accent = colour;
    repaint();
}

void ModuleScreenComponent::prepare (double sampleRate)
{
    bedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

const TrainingModule::Definition* ModuleScreenComponent::currentModule() const
{
    if (moduleIndex < 0 || moduleIndex >= (int) modules.size())
        return nullptr;

    return &modules[(size_t) moduleIndex];
}

void ModuleScreenComponent::openShelf()
{
    phase = Phase::shelf;
    moduleIndex = -1;
    expansionTarget = 0.0f;
    appearAmount = 0.0f;
    stopBed();
    layoutButtons();
    setVisible (true);
    toFront (false);
    repaint();
}

void ModuleScreenComponent::openModule (int index)
{
    if (index < 0 || index >= (int) modules.size())
        return;

    moduleIndex = index;
    demoStep = 0;
    saveParameters();

    const auto& definition = modules[(size_t) index];

    if (! definition.demoSteps.empty())
        applyStep (definition.demoSteps[0]);

    playBed (definition.check.bed, 1);
    goToPhase (Phase::demo);
}

void ModuleScreenComponent::goToPhase (Phase newPhase)
{
    phase = newPhase;
    // No phase covers the knobs any more: during the check they are how
    // you answer.
    expansionTarget = 0.0f;
    layoutButtons();
    repaint();
}

void ModuleScreenComponent::beginCheck()
{
    auto* definition = currentModule();

    if (definition == nullptr)
        return;

    const auto& check = definition->check;

    // One tier above whatever has already been passed, so a module that is
    // done at "roughly" opens at "confidently" rather than replaying a
    // question already answered.
    checkTier = juce::jlimit (1, TrainingModule::numTiers,
                               progress.getTierPassed (definition->id) + 1);

    hiddenTarget = TrainingModule::drawTarget (check, random);

    // Put the knob somewhere neutral once, then never touch it again: from
    // here on its position *is* the answer, and moving it would be the
    // panel answering for the player.
    const auto neutral = check.drawLogarithmically && check.minTarget > 0.0f
                             ? std::sqrt (check.minTarget * check.maxTarget)
                             : (check.minTarget + check.maxTarget) * 0.5f;

    setParameter (check.parameterID, check.unit == TrainingModule::Unit::choice
                                         ? check.minTarget : neutral);

    auditioningReference = true;

    if (setOverride != nullptr)
        setOverride (check.parameterID, hiddenTarget);

    refreshAuditionButtons();

    // A fresh instance of the bed, so the check is not the same recording
    // the demonstration used.
    playBed (check.bed, random.nextInt (10000));

    goToPhase (Phase::check);
}

float ModuleScreenComponent::currentKnobValue() const
{
    auto* definition = currentModule();

    return definition != nullptr ? getParameter (definition->check.parameterID) : 0.0f;
}

void ModuleScreenComponent::refreshAuditionButtons()
{
    // Which one you are hearing has to be visible at a glance: two
    // identical buttons and a line of text is a state you have to read.
    referenceButton.setColour (juce::TextButton::buttonColourId,
                                auditioningReference ? accent.withAlpha (0.85f)
                                                     : AbcTrainTheme::current().widgetBackground);
    mineButton.setColour (juce::TextButton::buttonColourId,
                           auditioningReference ? AbcTrainTheme::current().widgetBackground
                                                : accent.withAlpha (0.85f));
    referenceButton.repaint();
    mineButton.repaint();
}

void ModuleScreenComponent::submitAnswer()
{
    auto* definition = currentModule();

    if (definition == nullptr)
        return;

    const auto& check = definition->check;

    // Whatever the knob says now is the answer.
    playerValue = currentKnobValue();

    if (clearOverride != nullptr)
        clearOverride();

    lastAttemptPassed = TrainingModule::passes (check, hiddenTarget, playerValue, checkTier);
    lastAttemptQuality = TrainingModule::quality (check, hiddenTarget, playerValue, checkTier);

    if (lastAttemptPassed)
        progress.recordPass (definition->id, checkTier);

    // Reveal: put the knob on the answer and leave it there, so the eased
    // travel shows how far off it was rather than merely stating it.
    setParameter (check.parameterID, hiddenTarget);

    goToPhase (Phase::result);
}

void ModuleScreenComponent::closeModule()
{
    stopBed();
    restoreParameters();
    openShelf();
}

void ModuleScreenComponent::saveParameters()
{
    savedParameters.clear();

    for (auto* parameter : apvts.processor.getParameters())
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            savedParameters.push_back ({ withID->paramID, getParameter (withID->paramID) });
}

void ModuleScreenComponent::restoreParameters()
{
    for (const auto& saved : savedParameters)
        setParameter (saved.first, saved.second);

    savedParameters.clear();
}

void ModuleScreenComponent::applyStep (const LessonStep& step)
{
    for (const auto& target : step.targetParameters)
        setParameter (target.first, target.second);
}

void ModuleScreenComponent::setParameter (const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float ModuleScreenComponent::getParameter (const juce::String& id) const
{
    if (auto* raw = apvts.getRawParameterValue (id))
        return raw->load();

    return 0.0f;
}

void ModuleScreenComponent::playBed (TrainingModule::Bed which, int seed)
{
    // Handed to PracticeAudioSource, which owns it from here. It lives in
    // the processor, so the bed outlives this panel - closing the plugin
    // window mid-check used to free the buffer the audio thread was
    // reading. See publishOverrideBuffer.
    practiceSource.publishOverrideBuffer (
        LessonAudioBed::render (which, bedSampleRate, seed));
}

void ModuleScreenComponent::stopBed()
{
    practiceSource.clearOverrideBuffer();

    // An override left armed would silently keep processing at a value no
    // knob shows, for as long as the plugin stayed loaded.
    if (clearOverride != nullptr)
        clearOverride();
}

juce::Rectangle<int> ModuleScreenComponent::panelBounds() const
{
    auto area = getLocalBounds();

    if (phase == Phase::shelf)
    {
        const auto rows = numShelfRows() * rowHeight
                              + ((walkthroughs.isEmpty() || modules.empty()) ? 0 : 26);
        const auto needed = AbcTrainTheme::Spacing::large * 2 + 48 + rows + buttonHeight + 12;

        return area.withHeight (juce::jmin (area.getHeight(), needed));
    }

    const auto forPhase = phase == Phase::demo   ? 1
                        : phase == Phase::tryIt  ? 2
                        : phase == Phase::check  ? 3 : 4;

    const auto resting = juce::jmin (area.getHeight(), contentHeightFor (forPhase));
    const auto height = resting + (int) ((float) (area.getHeight() - resting) * expansion);

    return area.withHeight (juce::jlimit (0, area.getHeight(), height));
}

bool ModuleScreenComponent::hitTest (int x, int y)
{
    // Everything outside the painted panel belongs to whatever is under it -
    // which during a lesson is the plugin's own knobs, and they have to
    // stay usable or the lesson is a slideshow.
    return panelBounds().contains (x, y);
}

void ModuleScreenComponent::resized()
{
    layoutButtons();
}

void ModuleScreenComponent::layoutButtons()
{
    for (auto* button : { &backButton, &nextButton, &readyButton, &referenceButton,
                          &mineButton, &submitButton, &againButton, &doneButton, &closeButton })
        button->setVisible (false);


    auto area = panelBounds().reduced (AbcTrainTheme::Spacing::large);

    if (area.getHeight() < buttonHeight + 8)
        return;

    auto row = area.removeFromBottom (buttonHeight);

    const auto place = [&row] (juce::TextButton& button, int width)
    {
        button.setVisible (true);
        button.setBounds (row.removeFromRight (width));
        row.removeFromRight (AbcTrainTheme::Spacing::small);
    };

    switch (phase)
    {
        case Phase::shelf:
            place (closeButton, 92);
            break;

        case Phase::demo:
            place (nextButton, 92);
            place (backButton, 84);
            place (closeButton, 84);
            break;

        case Phase::tryIt:
            place (readyButton, 110);
            place (closeButton, 84);
            break;

        case Phase::check:
        {
            // Laid out like the trainer's answer row, not as four equal
            // grey buttons in a huddle. Reference/Mine is one *pair*,
            // centred under the scale it compares - the same shape and the
            // same place as A/B - because it is the same act: hearing the
            // treated thing against your own. Submit sits right, where a
            // commit belongs; leaving sits left, out of the way.
            //
            // Four identically-weighted buttons made the two that matter
            // most look like housekeeping.
            place (submitButton, 96);

            auto leaveArea = row.removeFromLeft (84);
            closeButton.setVisible (true);
            closeButton.setBounds (leaveArea);

            const auto pairWidth = juce::jmin (200, row.getWidth() - AbcTrainTheme::Spacing::medium * 2);

            if (pairWidth > 60)
            {
                auto pair = row.withSizeKeepingCentre (pairWidth, buttonHeight);
                const auto half = (pair.getWidth() - AbcTrainTheme::Spacing::tight) / 2;

                referenceButton.setVisible (true);
                referenceButton.setBounds (pair.removeFromLeft (half));
                pair.removeFromLeft (AbcTrainTheme::Spacing::tight);

                mineButton.setVisible (true);
                mineButton.setBounds (pair);
            }

            break;
        }

        case Phase::result:
            place (doneButton, 84);
            place (againButton, 104);
            break;
    }
}

juce::String ModuleScreenComponent::describeTolerance (const TrainingModule::Check& check,
                                                       int tier)
{
    const auto tolerance = TrainingModule::toleranceForTier (check, tier);

    // "within 0.70 as a ratio" is a number from the implementation, not a
    // sentence anybody could act on.
    switch (check.unit)
    {
        case TrainingModule::Unit::proportion:
            return "get within " + juce::String (juce::roundToInt (tolerance * 100.0f)) + "%";

        case TrainingModule::Unit::decibels:
            return "get within " + juce::String (tolerance, 1) + " dB";

        case TrainingModule::Unit::octaves:
            return "get within " + juce::String (tolerance, 2) + " of an octave";

        case TrainingModule::Unit::rangeFraction:
            return "get within " + juce::String (juce::roundToInt (tolerance * 100.0f))
                       + "% of the range";

        case TrainingModule::Unit::choice:
            return "name it exactly";
    }

    return {};
}

juce::String ModuleScreenComponent::formatValue (float value) const
{
    auto* definition = currentModule();

    if (definition == nullptr)
        return {};

    const auto& check = definition->check;

    if (check.unit == TrainingModule::Unit::choice)
    {
        const auto index = juce::jlimit (0, (int) check.choiceLabels.size() - 1, (int) value);
        return check.choiceLabels.empty() ? juce::String (value) : check.choiceLabels[(size_t) index];
    }

    const auto shown = value * check.displayScale;

    // juce::String (double, 0) does *not* mean "round to an integer" - it
    // means "use the shortest representation that round-trips", so 26.7496
    // printed as "26.7496 ms" wherever a value happened to be over ten.
    // It had nowhere to show before the check grew a scale with numbers on
    // it; it was wrong in every module the whole time.
    if (std::abs (shown) < 10.0f)
        return juce::String (shown, 1) + check.unitSuffix;

    return juce::String (juce::roundToInt (shown)) + check.unitSuffix;
}

void ModuleScreenComponent::timerCallback()
{
    const auto step = 1.0f / (float) (AbcTrainTheme::Duration::transition * 0.06);
    auto moved = false;

    if (std::abs (expansion - expansionTarget) > 0.001f)
    {
        expansion += juce::jlimit (-step, step, expansionTarget - expansion);
        moved = true;
    }

    // The mark and the readout follow the knob, which this panel does not
    // own and gets no callback from. Tracked during the whole check, not
    // just while auditioning your own setting: the knob is yours to turn
    // at any point, and a scale that froze whenever the reference played
    // was most of why the old check screen looked dead.
    if (phase == Phase::check)
        moved = true;

    if (appearAmount < 1.0f)
    {
        appearAmount = juce::jmin (1.0f, appearAmount + step);
        moved = true;
    }

    if (moved)
    {
        layoutButtons();
        repaint();
    }
}

void ModuleScreenComponent::completeAnimation()
{
    expansion = expansionTarget;
    appearAmount = 1.0f;
    layoutButtons();
}

void ModuleScreenComponent::openCheckForSnapshot (int index)
{
    setVisible (true);
    openModule (index);
    beginCheck();
    completeAnimation();
}

void ModuleScreenComponent::mouseMove (const juce::MouseEvent& event)
{
    if (phase != Phase::shelf)
        return;

    auto found = -1;

    for (int i = 0; i < numShelfRows(); ++i)
        if (shelfRowBounds (i).contains (event.getPosition()))
            found = i;

    if (found != hoveredRow)
    {
        hoveredRow = found;
        repaint();
    }
}

void ModuleScreenComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoveredRow != -1)
    {
        hoveredRow = -1;
        repaint();
    }
}

void ModuleScreenComponent::mouseUp (const juce::MouseEvent& event)
{
    if (phase != Phase::shelf)
        return;

    for (int i = 0; i < numShelfRows(); ++i)
    {
        if (! shelfRowBounds (i).contains (event.getPosition()))
            continue;

        if (i < (int) modules.size())
        {
            openModule (i);
        }
        else if (onWalkthroughSelected != nullptr)
        {
            stopBed();
            setVisible (false);
            onWalkthroughSelected (i - (int) modules.size());
        }

        return;
    }
}

juce::Rectangle<int> ModuleScreenComponent::shelfRowBounds (int index) const
{
    auto area = panelBounds().reduced (AbcTrainTheme::Spacing::large);
    area.removeFromTop (48);                       // heading
    area.removeFromBottom (buttonHeight + 8);      // the close button's row

    // The walkthroughs sit under a gap, so "one knob" and "one workflow"
    // do not read as the same kind of thing in one flat list.
    const auto divider = index >= (int) modules.size() && ! walkthroughs.isEmpty() ? 26 : 0;

    return { area.getX(), area.getY() + index * rowHeight + divider, area.getWidth(), rowHeight - 4 };
}

juce::Rectangle<int> ModuleScreenComponent::checkBandBounds() const
{
    // The check panel has to be tall enough to cover the knobs, which
    // leaves far more room than the question needs. Rather than pinning
    // the readout to the top and the slider to the bottom - two halves of
    // one control with a window between them - the whole group sits in the
    // middle of the space it was given.
    auto area = panelBounds().reduced (AbcTrainTheme::Spacing::large);
    area.removeFromTop (26 + 16 + AbcTrainTheme::Spacing::small);
    area.removeFromBottom (buttonHeight + AbcTrainTheme::Spacing::small);

    constexpr int bandHeight = 176;

    return area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), bandHeight));
}

void ModuleScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto panel = panelBounds().toFloat();

    juce::Path shape;
    shape.addRoundedRectangle (panel.reduced (1.0f), AbcTrainTheme::Radius::panel);

    juce::DropShadow (theme.shadow.withAlpha (0.55f * theme.shadowStrength), 22, { 0, 5 })
        .drawForPath (g, shape);

    // Opaque, not translucent. An overlay you can read the spectrum through
    // is an overlay you can read the answer through.
    g.setColour (theme.panelBackground);
    g.fillPath (shape);
    g.setColour (theme.outline);
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    auto area = panel.toNearestInt().reduced (AbcTrainTheme::Spacing::large);

    if (phase == Phase::shelf)
        paintShelf (g, area);
    else
        paintRunner (g, area);
}

void ModuleScreenComponent::paintShelf (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::drawTrackedText (g, text.shelfTitle, area.removeFromTop (26).toFloat(),
                                           AbcTrainLookAndFeel::headingFont(),
                                           theme.textBright, 1.2f);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    g.drawText (text.shelfSubtitle,
                 area.removeFromTop (20), juce::Justification::centredLeft, true);

    for (int i = 0; i < numShelfRows(); ++i)
    {
        const auto row = shelfRowBounds (i);

        if (! row.intersects (getLocalBounds()))
            continue;

        const auto isWalkthrough = i >= (int) modules.size();

        // The sub-heading exists to separate walkthroughs from the
        // modules above them. With no modules - Learner EQ, which has none
        // by choice (ADR 027) - there is nothing to separate them from,
        // and a heading over the only group on the shelf is noise.
        if (isWalkthrough && i == (int) modules.size() && ! modules.empty())
        {
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            g.drawText (AbcTrainLookAndFeel::toCaps (text.walkthroughs), row.translated (0, -22).withHeight (16),
                         juce::Justification::centredLeft, true);
        }

        const auto name = isWalkthrough ? walkthroughs[i - (int) modules.size()]
                                        : modules[(size_t) i].name;
        const auto why = isWalkthrough ? text.walkthroughWhy
                                       : modules[(size_t) i].why;
        const auto tier = isWalkthrough ? 0 : progress.getTierPassed (modules[(size_t) i].id);

        if (i == hoveredRow)
        {
            g.setColour (theme.widgetBackground.withAlpha (0.55f));
            g.fillRoundedRectangle (row.toFloat(), AbcTrainTheme::Radius::button);
        }

        auto text = row.reduced (10, 4);

        // Three pips, one per tier. A number would need a legend; three
        // dots that fill in do not.
        auto pips = text.removeFromRight (52);

        for (int pip = 0; pip < (isWalkthrough ? 0 : TrainingModule::numTiers); ++pip)
        {
            const juce::Rectangle<float> dot ((float) (pips.getX() + pip * 15),
                                               (float) pips.getCentreY() - 4.0f, 8.0f, 8.0f);

            if (pip < tier)
            {
                g.setColour (accent);
                g.fillEllipse (dot);
            }
            else
            {
                g.setColour (theme.outline);
                g.drawEllipse (dot, 1.2f);
            }
        }

        g.setColour (tier > 0 ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::headingFont());
        g.drawText (name, text.removeFromTop (18), juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.drawText (why, text.removeFromTop (16), juce::Justification::centredLeft, true);
    }
}

void ModuleScreenComponent::paintRunner (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();
    auto* definition = currentModule();

    if (definition == nullptr)
        return;

    AbcTrainLookAndFeel::drawTrackedText (g, definition->name, area.removeFromTop (26).toFloat(),
                                           AbcTrainLookAndFeel::headingFont(),
                                           theme.textBright, 1.2f);

    // Where you are, as four marks rather than "step 2 of 4" - the shape of
    // the journey is the useful part, not its arithmetic.
    {
        auto marks = area.removeFromTop (16);
        const Phase order[] = { Phase::demo, Phase::tryIt, Phase::check, Phase::result };
        const juce::String names[] { text.phaseWatch, text.phaseTry,
                                      text.phaseCheck, text.phaseResult };

        auto x = (float) marks.getX();

        for (int i = 0; i < 4; ++i)
        {
            const auto here = order[i] == phase;
            const auto font = AbcTrainLookAndFeel::microFont();
            const auto width = AbcTrainLookAndFeel::trackedTextWidth (names[i], font, 0.8f) + 16.0f;

            g.setColour (here ? accent : theme.textDim.withAlpha (0.5f));
            g.setFont (font);
            g.drawText (names[i], juce::Rectangle<float> (x, (float) marks.getY(),
                                                          width, 16.0f).toNearestInt(),
                         juce::Justification::centredLeft, false);

            if (here)
                g.fillRoundedRectangle ({ x, (float) marks.getBottom() - 1.0f,
                                           width - 16.0f, 2.0f }, 1.0f);

            x += width;
        }
    }

    area.removeFromTop (AbcTrainTheme::Spacing::small);
    area.removeFromBottom (buttonHeight + AbcTrainTheme::Spacing::small);

    const auto bodyFont = AbcTrainLookAndFeel::bodyFont();
    g.setFont (bodyFont);

    switch (phase)
    {
        case Phase::demo:
        {
            const auto& step = definition->demoSteps[(size_t) juce::jlimit (0, (int) definition->demoSteps.size() - 1, demoStep)];

            g.setColour (theme.text);
            g.drawFittedText (step.explanationText, area.removeFromTop (96),
                               juce::Justification::topLeft, 6);

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            g.drawText ("Step " + juce::String (demoStep + 1) + " of "
                            + juce::String ((int) definition->demoSteps.size())
                            + " - the knobs below are moving as you read.",
                         area.removeFromTop (18), juce::Justification::centredLeft, true);
            break;
        }

        case Phase::tryIt:
            g.setColour (theme.text);
            g.drawFittedText (definition->tryPrompt, area.removeFromTop (96),
                               juce::Justification::topLeft, 6);
            break;

        case Phase::check:
        {
            auto band = checkBandBounds();

            // Rebuilt to speak the trainer's language (ADR 030).
            //
            // What was here: an 8px line that showed *nothing at all*
            // while the reference played - which is most of the time - so
            // the screen was a black strip with the word "Reference" in
            // it, and the instruction "turn the knob" written underneath.
            // A check explained in sentences, in a product whose whole
            // answer mechanic is a scale you can read.
            //
            // What is here now is the trainer's own vocabulary: a
            // recessed well with real-unit grid marks, your knob drawn on
            // it live as you turn it, the accept band around your mark,
            // and a value readout riding above it. The same picture the
            // trainer shows, driven by the plugin's own knob instead of a
            // drag.
            g.setColour (theme.textBright);
            g.setFont (AbcTrainLookAndFeel::headingFont());
            g.drawText (text.match, band.removeFromTop (22), juce::Justification::centred, false);

            band.removeFromTop (6);

            const auto& check = definition->check;

            const auto toNormalised = [&check] (float value)
            {
                if (check.drawLogarithmically && check.minTarget > 0.0f && value > 0.0f)
                    return juce::jlimit (0.0f, 1.0f,
                        std::log (value / check.minTarget)
                            / std::log (check.maxTarget / check.minTarget));

                return juce::jlimit (0.0f, 1.0f,
                    (value - check.minTarget)
                        / juce::jmax (0.0001f, check.maxTarget - check.minTarget));
            };

            const auto fromNormalised = [&check] (float t)
            {
                if (check.drawLogarithmically && check.minTarget > 0.0f)
                    return check.minTarget * std::pow (check.maxTarget / check.minTarget, t);

                return check.minTarget + (check.maxTarget - check.minTarget) * t;
            };

            // The value readout rides over the mark rather than sitting
            // centred, so the number and the line it belongs to are never
            // far apart - exactly as on the trainer's scale.
            auto readoutRow = band.removeFromTop (30);
            band.removeFromTop (4);

            auto well = band.removeFromBottom (juce::jmax (72, band.getHeight() - 6)).toFloat();
            AbcTrainLookAndFeel::paintRecessedWell (g, well, AbcTrainTheme::Radius::well);

            const auto xFor = [&well] (float t)
            {
                return well.getX() + well.getWidth() * juce::jlimit (0.0f, 1.0f, t);
            };

            // Grid marks in the parameter's real unit. A scale with no
            // numbers on it cannot tell you what "a bit more" means, which
            // is the one thing a knob check has to teach.
            const auto labelRow = 15.0f;
            constexpr int numMarks = 5;

            for (int i = 0; i < numMarks; ++i)
            {
                const auto t = (float) i / (float) (numMarks - 1);
                const auto x = xFor (t);

                g.setColour (theme.textDim.withAlpha (0.24f));
                g.drawLine (x, well.getY() + 5.0f, x, well.getBottom() - labelRow - 3.0f, 1.0f);

                if (check.unit == TrainingModule::Unit::choice)
                    continue;

                const auto boxWidth = 62.0f;
                const auto boxX = juce::jlimit (well.getX() + 1.0f,
                                                 well.getRight() - boxWidth - 1.0f,
                                                 x - boxWidth * 0.5f);

                g.setColour (theme.textDim.withAlpha (0.5f));
                g.setFont (AbcTrainLookAndFeel::microFont());
                g.drawText (formatValue (fromNormalised (t)),
                            juce::Rectangle<float> (boxX, well.getBottom() - labelRow - 1.0f,
                                                     boxWidth, labelRow),
                            juce::Justification::centred, false);
            }

            const auto here = toNormalised (currentKnobValue());
            const auto half = juce::jlimit (0.02f, 0.45f,
                TrainingModule::toleranceForTier (check, checkTier) * 0.5f);

            // Everything that belongs *inside* the well is drawn in this
            // scope. The readout below is deliberately outside it: it sits
            // above the well, and reduceClipRegion intersects rather than
            // replaces, so drawing it under the well's clip produced an
            // empty region and no text at all - the same mistake the
            // tooltip backdrop made (ADR 029).
            {
                juce::Graphics::ScopedSaveState clipped (g);
                juce::Path wellClip;
                wellClip.addRoundedRectangle (well, AbcTrainTheme::Radius::well);
                g.reduceClipRegion (wellClip);

            // The accept band, drawn around *your* mark: its width is the
            // answer to "how close is close enough" with no number in it.
            {
                const auto left = xFor (here - half);
                const auto right = xFor (here + half);
                const auto lit = juce::Rectangle<float> (left, well.getY(),
                                                          right - left, well.getHeight() - labelRow);

                g.setColour (accent.withAlpha (0.16f));
                g.fillRect (lit);

                g.setColour (accent.withAlpha (0.34f));
                g.drawLine (lit.getX(), lit.getY(), lit.getX(), lit.getBottom(), 1.0f);
                g.drawLine (lit.getRight(), lit.getY(), lit.getRight(), lit.getBottom(), 1.0f);
            }

            // Your knob, live. Shown while the reference is playing too -
            // it is still where your knob is, and hiding it left the
            // screen blank for most of the exercise.
            {
                const auto x = xFor (here);

                g.setColour (accent.withAlpha (0.22f));
                g.fillRect (juce::Rectangle<float> (x - 3.0f, well.getY(),
                                                     6.0f, well.getHeight() - labelRow));

                g.setColour (accent);
                g.fillRect (juce::Rectangle<float> (x - 1.0f, well.getY(),
                                                     2.0f, well.getHeight() - labelRow));
            }
            }

            // The value readout rides over the mark, clamped inside the
            // well so it cannot run off either end.
            {
                const auto readoutText = formatValue (currentKnobValue());
                const auto font = AbcTrainLookAndFeel::titleFont();
                const auto width = AbcTrainLookAndFeel::trackedTextWidth (readoutText, font, 1.0f) + 18.0f;
                const auto centreX = juce::jlimit (well.getX() + width * 0.5f,
                                                    well.getRight() - width * 0.5f,
                                                    xFor (here));

                AbcTrainLookAndFeel::drawTrackedText (
                    g, readoutText,
                    juce::Rectangle<float> (centreX - width * 0.5f, (float) readoutRow.getY(),
                                             width, (float) readoutRow.getHeight()),
                    font, theme.textBright, 1.0f, juce::Justification::centred);
            }

            break;
        }

        case Phase::result:
        {
            g.setColour (lastAttemptPassed ? accent : theme.textBright);
            g.setFont (AbcTrainLookAndFeel::headingFont());
            g.drawText (lastAttemptPassed ? text.passed : text.notYet,
                         area.removeFromTop (26), juce::Justification::centred, false);

            area.removeFromTop (10);

            g.setColour (theme.textBright);
            g.setFont (juce::Font (AbcTrainLookAndFeel::monoFont()).withHeight (20.0f));
            g.drawText (text.itWas + " " + formatValue (hiddenTarget),
                         area.removeFromTop (26), juce::Justification::centred, false);

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            g.drawText (text.youSaid + " " + formatValue (playerValue),
                         area.removeFromTop (20), juce::Justification::centred, false);
            break;
        }

        case Phase::shelf:
            break;
    }
}
