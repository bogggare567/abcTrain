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
            default: return 0;    // shelf and check size themselves
        }
    }
}

ModuleScreenComponent::ModuleScreenComponent (juce::AudioProcessorValueTreeState& state,
                                              ModuleProgress& progressToUse,
                                              PracticeAudioSource& source)
    : apvts (state), progress (progressToUse), practiceSource (source)
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
        setParameter (currentModule()->check.parameterID, hiddenTarget);
        refreshAuditionButtons();
        repaint();
    };

    mineButton.onClick = [this]
    {
        auditioningReference = false;
        setParameter (currentModule()->check.parameterID, playerValue);
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

    answerSlider.setRange (0.0, 1.0, 0.0);
    addChildComponent (answerSlider);

    answerSlider.onValueChange = [this]
    {
        auto* definition = currentModule();

        if (definition == nullptr)
            return;

        const auto& check = definition->check;
        const auto t = (float) answerSlider.getValue();

        playerValue = check.drawLogarithmically && check.minTarget > 0.0f
                          ? check.minTarget * std::pow (check.maxTarget / check.minTarget, t)
                          : check.minTarget + t * (check.maxTarget - check.minTarget);

        if (! auditioningReference)
            setParameter (check.parameterID, playerValue);

        repaint();
    };

    startTimerHz (60);
}

ModuleScreenComponent::~ModuleScreenComponent()
{
    // The processor holds a raw pointer to `bed`. Clear it before this
    // object - and the buffer with it - goes away.
    practiceSource.setOverrideBuffer (nullptr);
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
    expansionTarget = newPhase == Phase::check || newPhase == Phase::result ? 1.0f : 0.0f;
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

    // Start the player somewhere neutral rather than on the answer.
    playerValue = check.drawLogarithmically && check.minTarget > 0.0f
                      ? std::sqrt (check.minTarget * check.maxTarget)
                      : (check.minTarget + check.maxTarget) * 0.5f;

    if (check.unit == TrainingModule::Unit::choice)
        playerValue = check.minTarget;

    const auto normalised = check.drawLogarithmically && check.minTarget > 0.0f
                                ? std::log (playerValue / check.minTarget)
                                      / std::log (check.maxTarget / check.minTarget)
                                : (playerValue - check.minTarget) / (check.maxTarget - check.minTarget);

    answerSlider.setValue (juce::jlimit (0.0, 1.0, (double) normalised),
                            juce::dontSendNotification);

    auditioningReference = true;
    setParameter (check.parameterID, hiddenTarget);
    refreshAuditionButtons();

    // A fresh instance of the bed, so the check is not the same recording
    // the demonstration used.
    playBed (check.bed, random.nextInt (10000));

    goToPhase (Phase::check);
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
    bed = LessonAudioBed::render (which, bedSampleRate, seed);
    practiceSource.setOverrideBuffer (&bed);
}

void ModuleScreenComponent::stopBed()
{
    practiceSource.setOverrideBuffer (nullptr);
}

juce::Rectangle<int> ModuleScreenComponent::panelBounds() const
{
    auto area = getLocalBounds();

    if (phase == Phase::shelf)
    {
        const auto rows = numShelfRows() * rowHeight + (walkthroughs.isEmpty() ? 0 : 26);
        const auto needed = AbcTrainTheme::Spacing::large * 2 + 48 + rows + buttonHeight + 12;

        return area.withHeight (juce::jmin (area.getHeight(), needed));
    }

    const auto resting = juce::jmin (area.getHeight(),
                                      contentHeightFor (phase == Phase::demo ? 1 : 2));
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

    answerSlider.setVisible (false);

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
            place (submitButton, 96);
            place (mineButton, 80);
            place (referenceButton, 104);
            place (closeButton, 84);

            answerSlider.setVisible (true);
            answerSlider.setBounds (checkBandBounds().removeFromBottom (36).reduced (0, 6));
            break;

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
    const auto decimals = std::abs (shown) < 10.0f ? 1 : 0;

    return juce::String (shown, decimals) + check.unitSuffix;
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

    AbcTrainLookAndFeel::drawTrackedText (g, "Modules", area.removeFromTop (26).toFloat(),
                                           juce::Font (juce::FontOptions (17.0f).withStyle ("Bold")),
                                           theme.textBright, 1.2f);

    g.setColour (theme.textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("One knob each. Watch it, try it, then prove you can hear it.",
                 area.removeFromTop (20), juce::Justification::centredLeft, true);

    for (int i = 0; i < numShelfRows(); ++i)
    {
        const auto row = shelfRowBounds (i);

        if (! row.intersects (getLocalBounds()))
            continue;

        const auto isWalkthrough = i >= (int) modules.size();

        if (isWalkthrough && i == (int) modules.size())
        {
            g.setColour (theme.textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText ("WALKTHROUGHS", row.translated (0, -22).withHeight (16),
                         juce::Justification::centredLeft, true);
        }

        const auto name = isWalkthrough ? walkthroughs[i - (int) modules.size()]
                                        : modules[(size_t) i].name;
        const auto why = isWalkthrough ? juce::String ("A whole workflow, not one control.")
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
        g.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
        g.drawText (name, text.removeFromTop (18), juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
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
                                           juce::Font (juce::FontOptions (17.0f).withStyle ("Bold")),
                                           theme.textBright, 1.2f);

    // Where you are, as four marks rather than "step 2 of 4" - the shape of
    // the journey is the useful part, not its arithmetic.
    {
        auto marks = area.removeFromTop (16);
        const Phase order[] = { Phase::demo, Phase::tryIt, Phase::check, Phase::result };

        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> bar ((float) (marks.getX() + i * 34), (float) marks.getY() + 6.0f,
                                               26.0f, 3.0f);
            g.setColour (order[i] == phase ? accent
                                            : theme.outline.withAlpha (0.8f));
            g.fillRoundedRectangle (bar, 1.5f);
        }
    }

    area.removeFromTop (AbcTrainTheme::Spacing::small);
    area.removeFromBottom (buttonHeight + AbcTrainTheme::Spacing::small);

    const auto bodyFont = juce::Font (juce::FontOptions (13.0f));
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
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
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

            g.setColour (theme.text);
            g.drawFittedText ("Something is set to a value you cannot see. Switch between "
                              "Reference and Mine, and move the slider until they match.",
                               band.removeFromTop (44), juce::Justification::centredTop, 3);

            g.setColour (theme.textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText ("Tier " + juce::String (checkTier) + " of "
                            + juce::String (TrainingModule::numTiers) + " - "
                            + describeTolerance (definition->check, checkTier),
                         band.removeFromTop (18), juce::Justification::centred, true);

            // The value the player is on, big and in the middle, directly
            // above the slider that sets it. Small text at the top and a
            // control at the bottom of an empty panel is two halves of one
            // control with the window in between them.
            band.removeFromBottom (36);
            auto readout = band.removeFromBottom (56);
            g.setColour (auditioningReference ? theme.textDim : accent);
            g.setFont (juce::Font (AbcTrainLookAndFeel::monoFont()).withHeight (26.0f));
            g.drawText (auditioningReference ? "reference" : formatValue (playerValue),
                         readout, juce::Justification::centred, true);

            if (! auditioningReference)
            {
                g.setColour (theme.textDim);
                g.setFont (juce::Font (juce::FontOptions (11.0f)));
                g.drawText ("yours", readout.translated (0, 26),
                             juce::Justification::centred, true);
            }

            break;
        }

        case Phase::result:
        {
            g.setColour (lastAttemptPassed ? accent : theme.textBright);
            g.setFont (juce::Font (juce::FontOptions (15.0f).withStyle ("Bold")));
            g.drawText (lastAttemptPassed ? "Passed" : "Not this time",
                         area.removeFromTop (24), juce::Justification::centredLeft, true);

            g.setColour (theme.text);
            g.setFont (bodyFont);
            g.drawText ("It was " + formatValue (hiddenTarget) + ". You said "
                            + formatValue (playerValue) + ".",
                         area.removeFromTop (22), juce::Justification::centredLeft, true);

            g.setColour (theme.textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (lastAttemptPassed
                            ? juce::String (juce::roundToInt (lastAttemptQuality * 100.0f))
                                  + "% of the way to dead on. The knob has moved to the answer."
                            : "The knob has moved to the answer - listen to it against what "
                              "you had.",
                         area.removeFromTop (18), juce::Justification::centredLeft, true);
            break;
        }

        case Phase::shelf:
            break;
    }
}
