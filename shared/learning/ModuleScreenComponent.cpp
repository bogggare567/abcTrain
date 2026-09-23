#include "shared/learning/ModuleScreenComponent.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"

namespace
{
    constexpr int buttonHeight = 30;
    constexpr int headerHeight = 58;     // title + subtitle / name + phase marks

    // How much of the analysis section each explaining phase needs. The
    // check and the result take all of it (see the class comment).
    int heightFor (int phaseIndex)
    {
        switch (phaseIndex)
        {
            case 1:  return 238;   // demo
            case 2:  return 214;   // try it
            default: return 0;
        }
    }

    juce::String fill (juce::String templ, std::initializer_list<std::pair<const char*, juce::String>> values)
    {
        for (const auto& v : values)
            templ = templ.replace ("{{" + juce::String (v.first) + "}}", v.second);

        return templ;
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

        // A walkthrough has no check: its last step is its end.
        if (isWalkthrough (moduleIndex))
            closeModule();
        else
            goToPhase (Phase::tryIt);
    };

    // The end of a module (ADR 041). It used to lead into a graded check -
    // the plugin hid a value and you matched it by ear - and that task
    // belongs to the trainer, not to a processor you are learning to use.
    readyButton.onClick = [this]
    {
        if (auto* definition = currentModule())
            progress.markDone (definition->id);

        closeModule();
    };

    // Matching a hidden value without being able to switch back to it is
    // not a listening task, it is a guess.
    referenceButton.onClick = [this]
    {
        auditioningReference = true;

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
    closeButton.onClick = [this] { closePanel(); };

    startTimerHz (30);
}

ModuleScreenComponent::~ModuleScreenComponent()
{
    practiceSource.clearOverrideBuffer();

    if (clearOverride != nullptr)
        clearOverride();

    // Closing the plugin window mid-module used to leave the session on
    // whatever the demonstration had set. A teaching screen must leave the
    // mix the way it found it, window or no window.
    restoreParameters();
}

void ModuleScreenComponent::setModules (std::vector<TrainingModule::Definition> newModules)
{
    // Modules first, walkthroughs after, whatever order they arrived in.
    std::stable_partition (newModules.begin(), newModules.end(),
                           [] (const TrainingModule::Definition& d) { return d.check.parameterID.isNotEmpty(); });
    modules = std::move (newModules);
    repaint();
}

void ModuleScreenComponent::setStrings (Strings newStrings)
{
    text = std::move (newStrings);

    backButton.setButtonText (text.back);
    nextButton.setButtonText (text.next);
    readyButton.setButtonText (text.done);
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
    refreshAuditionButtons();
    repaint();
}

void ModuleScreenComponent::prepare (double sampleRate)
{
    bedSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

bool ModuleScreenComponent::isWalkthrough (int index) const
{
    return index >= 0 && index < (int) modules.size() && modules[(size_t) index].check.parameterID.isEmpty();
}

int ModuleScreenComponent::numModules() const
{
    int n = 0;

    for (const auto& m : modules)
        n += m.check.parameterID.isNotEmpty() ? 1 : 0;

    return n;
}

const TrainingModule::Definition* ModuleScreenComponent::currentModule() const
{
    if (moduleIndex < 0 || moduleIndex >= (int) modules.size())
        return nullptr;

    return &modules[(size_t) moduleIndex];
}

// ---- text ---------------------------------------------------------------

juce::String ModuleScreenComponent::textFor (const TrainingModule::Definition& d, const juce::String& field,
                                             const juce::String& fallback) const
{
    if (translate != nullptr)
    {
        const auto t = translate ("mod." + d.id + "." + field);

        if (t.isNotEmpty())
            return t;
    }

    return fallback;
}

juce::String ModuleScreenComponent::stepText (const TrainingModule::Definition& d, int step) const
{
    const auto& fallback = d.demoSteps[(size_t) juce::jlimit (0, (int) d.demoSteps.size() - 1, step)].explanationText;
    return textFor (d, "step" + juce::String (step + 1), fallback);
}

juce::String ModuleScreenComponent::unitSuffix (const TrainingModule::Check& check) const
{
    const auto bare = check.unitSuffix.trim();

    if (bare.isEmpty() || bare == "%" || bare.startsWith (":"))
        return check.unitSuffix;

    if (translate != nullptr)
    {
        const auto t = translate ("unit." + bare);

        if (t.isNotEmpty())
            return " " + t;
    }

    return check.unitSuffix;
}

juce::String ModuleScreenComponent::number (float value, int decimals) const
{
    auto s = decimals > 0 ? juce::String (value, decimals) : juce::String (juce::roundToInt (value));
    return text.decimal == "." ? s : s.replace (".", text.decimal);
}

juce::String ModuleScreenComponent::formatValue (float value) const
{
    auto* definition = currentModule();

    if (definition == nullptr)
        return {};

    const auto& check = definition->check;

    if (check.unit == TrainingModule::Unit::choice)
    {
        const auto index = juce::jlimit (0, (int) check.choiceLabels.size() - 1, juce::roundToInt (value));
        return check.choiceLabels.empty() ? number (value, 0) : check.choiceLabels[(size_t) index];
    }

    const auto shown = value * check.displayScale;

    // "10000 Hz" is a number you have to count the zeros of.
    if (check.unitSuffix.trim() == "Hz" && shown >= 999.5f)
    {
        const auto khz = translate != nullptr ? translate ("unit.kHz") : juce::String();
        return number (shown / 1000.0f, shown >= 9995.0f ? 1 : 2) + " " + (khz.isNotEmpty() ? khz : juce::String ("kHz"));
    }

    return number (shown, std::abs (shown) < 10.0f ? 1 : 0) + unitSuffix (check);
}

juce::String ModuleScreenComponent::formatTolerance (const TrainingModule::Check& check, int level) const
{
    const auto r = TrainingModule::readout (check, 0.0f, 0.0f, level);
    const auto pm = juce::String (juce::CharPointer_UTF8 ("\xc2\xb1"));

    if (check.unit == TrainingModule::Unit::choice)
        return {};

    if (r.percent)
        return pm + number (r.tolerance, 0) + "%";

    if (r.octaves)
        return pm + number (r.tolerance, 2) + " " + text.octaves;

    return pm + number (r.tolerance, r.tolerance < 10.0f ? 1 : 0) + unitSuffix (check);
}

juce::String ModuleScreenComponent::formatError (const TrainingModule::Check& check, float target,
                                                 float answer, int level) const
{
    const auto r = TrainingModule::readout (check, target, answer, level);
    const auto sign = r.signedError >= 0.0f ? juce::String ("+") : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"));
    const auto mag = std::abs (r.signedError);

    if (r.percent)
        return sign + number (mag, 0) + "%";

    if (r.octaves)
        return sign + number (mag, 2) + " " + text.octaves;

    return sign + number (mag, mag < 10.0f ? 1 : 0) + unitSuffix (check);
}

// ---- flow ---------------------------------------------------------------

void ModuleScreenComponent::openShelf()
{
    phase = Phase::shelf;
    moduleIndex = -1;
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

    if (onModuleOpened != nullptr)
        onModuleOpened (definition);

    playBed (definition.check.bed, 1);
    goToPhase (Phase::demo);
}

void ModuleScreenComponent::goToPhase (Phase newPhase)
{
    phase = newPhase;
    layoutButtons();
    repaint();
}

void ModuleScreenComponent::beginCheck()
{
    auto* definition = currentModule();

    if (definition == nullptr || isWalkthrough (moduleIndex))
        return;

    const auto& check = definition->check;
    checkLevel = progress.get (definition->id).level;

    hiddenTarget = TrainingModule::drawTarget (check, random);

    // Put the knob somewhere neutral once, then never touch it again: from
    // here on its position *is* the answer.
    const auto neutral = check.drawLogarithmically && check.minTarget > 0.0f
                             ? std::sqrt (check.minTarget * check.maxTarget)
                             : (check.minTarget + check.maxTarget) * 0.5f;

    setParameter (check.parameterID, check.unit == TrainingModule::Unit::choice ? check.minTarget : neutral);

    // A choice check must never start on the answer.
    if (check.unit == TrainingModule::Unit::choice && juce::roundToInt (hiddenTarget) == juce::roundToInt (check.minTarget))
        hiddenTarget = check.minTarget + 1.0f + (float) random.nextInt (juce::jmax (1, juce::roundToInt (check.maxTarget - check.minTarget)));

    auditioningReference = true;

    if (setOverride != nullptr)
        setOverride (check.parameterID, hiddenTarget);

    refreshAuditionButtons();
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
    AbcTrainLookAndFeel::makePrimary (referenceButton, auditioningReference);
    AbcTrainLookAndFeel::makePrimary (mineButton, ! auditioningReference);
    referenceButton.repaint();
    mineButton.repaint();
}

void ModuleScreenComponent::submitAnswer()
{
    auto* definition = currentModule();

    if (definition == nullptr)
        return;

    const auto& check = definition->check;
    playerValue = currentKnobValue();

    if (clearOverride != nullptr)
        clearOverride();

    levelBefore = checkLevel;
    const auto passed = TrainingModule::passesAtLevel (check, hiddenTarget, playerValue, checkLevel);
    lastOutcome = progress.recordAttempt (definition->id, passed);

    // Reveal: the knob travels to the answer, which shows how far off it
    // was rather than merely stating it.
    setParameter (check.parameterID, hiddenTarget);

    goToPhase (Phase::result);
}

void ModuleScreenComponent::closeModule()
{
    stopBed();
    restoreParameters();
    openShelf();
}

void ModuleScreenComponent::closePanel()
{
    stopBed();
    restoreParameters();
    setVisible (false);

    if (onClosed != nullptr)
        onClosed();
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
    practiceSource.publishOverrideBuffer (LessonAudioBed::render (which, bedSampleRate, seed));
}

void ModuleScreenComponent::stopBed()
{
    practiceSource.clearOverrideBuffer();

    if (clearOverride != nullptr)
        clearOverride();
}

// ---- layout -------------------------------------------------------------

juce::Rectangle<int> ModuleScreenComponent::panelBounds() const
{
    auto area = getLocalBounds();

    if (phase == Phase::demo || phase == Phase::tryIt)
        return area.withHeight (juce::jmin (area.getHeight(), heightFor (phase == Phase::demo ? 1 : 2)));

    return area;
}

bool ModuleScreenComponent::hitTest (int x, int y)
{
    // Below a short panel is the spectrum, which belongs to the plugin.
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

    const auto placeLeft = [&row] (juce::TextButton& button, int width)
    {
        button.setVisible (true);
        button.setBounds (row.removeFromLeft (width));
        row.removeFromLeft (AbcTrainTheme::Spacing::small);
    };

    switch (phase)
    {
        case Phase::shelf:
        {
            // Top right, beside the title - the shelf needs every row of
            // height for cards, and a page's close lives up there anyway.
            auto top = panelBounds().reduced (AbcTrainTheme::Spacing::large).removeFromTop (buttonHeight);
            closeButton.setVisible (true);
            closeButton.setBounds (top.removeFromRight (110));
            break;
        }

        case Phase::demo:
        {
            auto* d = currentModule();
            const auto last = d != nullptr && demoStep + 1 >= (int) d->demoSteps.size();
            nextButton.setButtonText (last && isWalkthrough (moduleIndex) ? text.finish : text.next);
            AbcTrainLookAndFeel::makePrimary (nextButton, true);
            place (nextButton, 120);

            if (demoStep > 0)
                place (backButton, 100);

            placeLeft (closeButton, 110);
            break;
        }

        case Phase::tryIt:
            AbcTrainLookAndFeel::makePrimary (readyButton, true);
            place (readyButton, 150);
            placeLeft (closeButton, 110);
            break;

        case Phase::check:
        {
            AbcTrainLookAndFeel::makePrimary (submitButton, true);
            place (submitButton, 130);
            placeLeft (closeButton, 110);

            // Reference / Mine is one pair, centred, the same shape and place
            // as the trainer's A/B - it is the same act.
            const auto pairWidth = juce::jmin (280, row.getWidth() - AbcTrainTheme::Spacing::medium * 2);

            if (pairWidth > 80)
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
            AbcTrainLookAndFeel::makePrimary (againButton, true);
            place (againButton, 150);
            place (doneButton, 110);
            break;
    }
}

// The shelf is a grid of cards, not a list: seven modules and two or
// three walkthroughs fit on one screen that way, where a list of rows ran
// off the bottom and hid the walkthroughs under a scroll nobody found.
namespace
{
    constexpr int cardGap = 10;
    constexpr int moduleCardHeight = 108;
    constexpr int walkCardHeight = 66;
    constexpr int walkCaptionHeight = 34;
}

juce::Rectangle<int> ModuleScreenComponent::shelfListBounds() const
{
    auto area = panelBounds().reduced (AbcTrainTheme::Spacing::large);
    area.removeFromTop (headerHeight);
    return area;
}

int ModuleScreenComponent::shelfColumns() const
{
    const auto width = shelfListBounds().getWidth();
    return width >= 760 ? 4 : width >= 540 ? 3 : 2;
}

int ModuleScreenComponent::shelfContentHeight() const
{
    const auto cols = shelfColumns();
    const auto moduleRows = (numModules() + cols - 1) / cols;
    const auto walks = (int) modules.size() - numModules();
    const auto walkCols = juce::jmax (1, juce::jmin (walks, cols));
    const auto walkRows = walks > 0 ? (walks + walkCols - 1) / walkCols : 0;

    return moduleRows * (moduleCardHeight + cardGap)
         + (walks > 0 ? walkCaptionHeight + walkRows * (walkCardHeight + cardGap) : 0);
}

juce::Rectangle<int> ModuleScreenComponent::shelfRowBounds (int index) const
{
    const auto list = shelfListBounds();
    const auto cols = shelfColumns();
    const auto scroll = juce::roundToInt (shelfScroll);

    if (index < numModules())
    {
        const auto cardWidth = (list.getWidth() - (cols - 1) * cardGap) / cols;
        const auto row = index / cols, col = index % cols;
        return { list.getX() + col * (cardWidth + cardGap),
                 list.getY() + row * (moduleCardHeight + cardGap) - scroll,
                 cardWidth, moduleCardHeight };
    }

    const auto walks = (int) modules.size() - numModules();
    const auto walkCols = juce::jmax (1, juce::jmin (walks, cols));
    const auto moduleRows = (numModules() + cols - 1) / cols;
    const auto top = list.getY() + moduleRows * (moduleCardHeight + cardGap) + walkCaptionHeight - scroll;
    const auto j = index - numModules();
    const auto cardWidth = (list.getWidth() - (walkCols - 1) * cardGap) / walkCols;

    return { list.getX() + (j % walkCols) * (cardWidth + cardGap),
             top + (j / walkCols) * (walkCardHeight + cardGap),
             cardWidth, walkCardHeight };
}

// ---- time ---------------------------------------------------------------

void ModuleScreenComponent::timerCallback()
{
    auto moved = false;

    if (appearAmount < 1.0f)
    {
        appearAmount = juce::jmin (1.0f, appearAmount + 0.2f);
        moved = true;
    }

    // The mark and the readout follow the knob, which this panel does not
    // own and gets no callback from.
    if (isVisible() && (phase == Phase::check || phase == Phase::demo))
        moved = true;

    if (moved)
        repaint();
}

void ModuleScreenComponent::completeAnimation()
{
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

void ModuleScreenComponent::openResultForSnapshot (int index, bool passed)
{
    openCheckForSnapshot (index);

    if (auto* d = currentModule())
    {
        const auto band = TrainingModule::acceptRange (d->check, hiddenTarget, checkLevel);
        setParameter (d->check.parameterID, passed ? hiddenTarget + (band.getEnd() - hiddenTarget) * 0.4f
                                                   : band.getEnd() + (band.getEnd() - hiddenTarget) * 0.6f);
    }

    submitAnswer();
    completeAnimation();
}

// ---- mouse --------------------------------------------------------------

void ModuleScreenComponent::mouseMove (const juce::MouseEvent& event)
{
    if (phase != Phase::shelf)
        return;

    auto found = -1;

    for (int i = 0; i < (int) modules.size(); ++i)
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

    if (! shelfListBounds().contains (event.getPosition()))
        return;

    for (int i = 0; i < (int) modules.size(); ++i)
        if (shelfRowBounds (i).contains (event.getPosition()))
        {
            openModule (i);
            return;
        }
}

void ModuleScreenComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (phase != Phase::shelf)
        return;

    const auto maxScroll = juce::jmax (0.0f, (float) (shelfContentHeight() - shelfListBounds().getHeight()));

    shelfScroll = juce::jlimit (0.0f, maxScroll, shelfScroll - wheel.deltaY * 160.0f);
    repaint();
}

// ---- painting -----------------------------------------------------------

void ModuleScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto panel = panelBounds();

    // Opaque: an overlay you can read the spectrum through is an overlay
    // you can read the answer through. A frame, not a floating card - the
    // same grammar as every panel in the trainer.
    g.setColour (theme.panelBackground);
    g.fillRect (panel);
    g.setColour (theme.outline);
    g.drawRect (panel, 1);
    g.setColour (accent);
    g.fillRect (panel.getX(), panel.getY(), panel.getWidth(), 2);

    auto area = panel.reduced (AbcTrainTheme::Spacing::large);

    if (phase == Phase::shelf)
        paintShelf (g, area);
    else
        paintRunner (g, area);
}

void ModuleScreenComponent::paintShelf (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::titleFont());
    g.drawText (text.shelfTitle, area.removeFromTop (28), juce::Justification::centredLeft, true);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    g.drawText (text.shelfSubtitle, area.removeFromTop (22), juce::Justification::centredLeft, true);

    const auto list = shelfListBounds();

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (list);

    for (int i = 0; i < (int) modules.size(); ++i)
    {
        const auto card = shelfRowBounds (i);

        if (! card.intersects (list))
            continue;

        if (isWalkthrough (i))
        {
            if (i == numModules())
            {
                // WALKTHROUGHS, then what one is, over a hairline.
                auto caption = juce::Rectangle<int> (list.getX(), card.getY() - walkCaptionHeight, list.getWidth(), walkCaptionHeight - 8);
                const auto label = AbcTrainLookAndFeel::toCaps (text.walkthroughs);
                const auto labelFont = AbcTrainLookAndFeel::labelFont();
                const auto w = (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (label, labelFont, 1.8f)) + 4;
                AbcTrainLookAndFeel::drawTrackedText (g, label, caption.removeFromLeft (w).toFloat(), labelFont, theme.text, 1.8f);
                caption.removeFromLeft (12);
                g.setColour (theme.textDim);
                g.setFont (AbcTrainLookAndFeel::captionFont());
                g.drawText (text.walkthroughWhy, caption, juce::Justification::centredLeft, true);
                g.setColour (theme.divider);
                g.fillRect (list.getX(), card.getY() - 6, list.getWidth(), 1);
            }

            paintWalkthroughCard (g, i, card);
        }
        else
        {
            paintModuleCard (g, i, card);
        }
    }

    // The fade *is* the scrollbar (same rule as the achievements shelf):
    // at either end that still has cards past it. On a short window the
    // walkthroughs sit below the fold and nothing used to say so.
    const auto panelColour = theme.panelBackground;
    const auto overflowBelow = (float) shelfContentHeight() - shelfScroll > (float) list.getHeight() + 1.0f;
    const auto overflowAbove = shelfScroll > 1.0f;
    const auto fade = 36.0f;

    if (overflowBelow)
    {
        const auto r = list.toFloat().removeFromBottom (fade);
        g.setGradientFill (juce::ColourGradient (panelColour.withAlpha (0.0f), r.getX(), r.getY(),
                                                 panelColour, r.getX(), r.getBottom(), false));
        g.fillRect (r);
    }

    if (overflowAbove)
    {
        const auto r = list.toFloat().removeFromTop (fade);
        g.setGradientFill (juce::ColourGradient (panelColour, r.getX(), r.getY(),
                                                 panelColour.withAlpha (0.0f), r.getX(), r.getBottom(), false));
        g.fillRect (r);
    }
}

void ModuleScreenComponent::paintModuleCard (juce::Graphics& g, int index, juce::Rectangle<int> card)
{
    const auto& theme = AbcTrainTheme::current();
    const auto& m = modules[(size_t) index];
    const auto tried = progress.isDone (m.id);

    // A frame; filled faintly on hover, and warm once it has a record -
    // the cards you have worked on are the ones that look worked on.
    if (tried)
    {
        g.setColour (accent.withAlpha (0.06f));
        g.fillRect (card);
    }

    if (index == hoveredRow)
    {
        g.setColour (theme.widgetBackground.withAlpha (0.8f));
        g.fillRect (card);
    }

    g.setColour (tried ? accent.withAlpha (0.55f) : theme.outline);
    g.drawRect (card, 1);

    if (tried)
        AbcTrainLookAndFeel::drawRegistrationMarks (g, card.toFloat(), accent.withAlpha (0.7f));

    auto r = card.reduced (12, 10);

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::headingFont());
    g.drawFittedText (textFor (m, "name", m.name), r.removeFromTop (22), juce::Justification::centredLeft, 1, 0.85f);

    // Bottom: whether you have worked through it. The ten-step ruler that
    // stood here measured the hearing check, which the plugins no longer
    // have (ADR 041).
    auto bottom = r.removeFromBottom (18);
    r.removeFromBottom (4);

    const auto status = AbcTrainLookAndFeel::toCaps (tried ? text.done : text.notTried);
    AbcTrainLookAndFeel::drawTrackedText (g, status, bottom.toFloat(), AbcTrainLookAndFeel::microFont(),
                                          tried ? accent : theme.textDim, 1.2f,
                                          juce::Justification::centredRight);

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    g.drawFittedText (textFor (m, "why", m.why), r.withTrimmedTop (2), juce::Justification::topLeft, 3, 0.9f);
}

void ModuleScreenComponent::paintWalkthroughCard (juce::Graphics& g, int index, juce::Rectangle<int> card)
{
    const auto& theme = AbcTrainTheme::current();
    const auto& m = modules[(size_t) index];

    if (index == hoveredRow)
    {
        g.setColour (theme.widgetBackground.withAlpha (0.8f));
        g.fillRect (card);
    }

    g.setColour (theme.outline);
    g.drawRect (card, 1);

    auto r = card.reduced (14, 10);

    // "01", "02" in the family colour: a walkthrough is a sequence, and
    // its number is the one ornament it needs.
    const auto number = juce::String (index - numModules() + 1).paddedLeft ('0', 2);
    g.setColour (accent);
    g.setFont (AbcTrainLookAndFeel::titleFont());
    g.drawText (number, r.removeFromLeft (36), juce::Justification::centredLeft, false);
    r.removeFromLeft (6);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    g.drawText (text.stepsCount.replace ("{{n}}", juce::String ((int) m.demoSteps.size())), r.removeFromBottom (16),
                juce::Justification::centredLeft, true);

    // Two lines if it needs them: "Pre-delay: staying in f..." said less
    // than the title the lesson actually has.
    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    g.drawFittedText (textFor (m, "name", m.name), r, juce::Justification::centredLeft, 2, 0.9f);
}

void ModuleScreenComponent::paintRunner (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();
    auto* definition = currentModule();

    if (definition == nullptr)
        return;

    const auto walkthrough = isWalkthrough (moduleIndex);

    // Name, and on the right where this module stands.
    {
        auto top = area.removeFromTop (28);

        if (! walkthrough && definition->check.unit != TrainingModule::Unit::choice)
        {
            const auto level = phase == Phase::result ? levelBefore : progress.get (definition->id).level;
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            g.drawText (fill (text.levelLine, { { "n", juce::String (level) },
                                                { "tol", formatTolerance (definition->check, level) } }),
                        top.removeFromRight (top.getWidth() / 2), juce::Justification::centredRight, true);
        }

        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::titleFont());
        g.drawText (textFor (*definition, "name", definition->name), top, juce::Justification::centredLeft, true);
    }

    // Where you are in the module, as marks rather than "step 2 of 4".
    if (! walkthrough)
    {
        auto marks = area.removeFromTop (22);
        const Phase order[] = { Phase::demo, Phase::tryIt };
        const juce::String names[] { text.phaseWatch, text.phaseTry };
        const auto font = AbcTrainLookAndFeel::microFont();
        auto x = (float) marks.getX();

        for (int i = 0; i < 2; ++i)
        {
            const auto here = order[i] == phase;
            const auto caps = AbcTrainLookAndFeel::toCaps (names[i]);
            const auto width = AbcTrainLookAndFeel::trackedTextWidth (caps, font, 1.4f);

            AbcTrainLookAndFeel::drawTrackedText (g, caps, { x, (float) marks.getY(), width + 2.0f, 16.0f },
                                                  font, here ? accent : theme.textDim.withAlpha (0.6f), 1.4f);

            if (here)
            {
                g.setColour (accent);
                g.fillRect (x, (float) marks.getY() + 17.0f, width, 2.0f);
            }

            x += width + 22.0f;
        }
    }
    else
    {
        area.removeFromTop (6);
    }

    area.removeFromTop (AbcTrainTheme::Spacing::small);
    area.removeFromBottom (buttonHeight + AbcTrainTheme::Spacing::small);

    switch (phase)
    {
        case Phase::demo:
        {
            g.setColour (theme.text);
            g.setFont (AbcTrainLookAndFeel::bodyFont());
            g.drawFittedText (stepText (*definition, demoStep), area.removeFromTop (area.getHeight() - 20),
                              juce::Justification::topLeft, 6, 1.0f);

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            g.drawText (fill (text.stepOf, { { "n", juce::String (demoStep + 1) },
                                             { "m", juce::String ((int) definition->demoSteps.size()) } }),
                        area, juce::Justification::centredLeft, true);
            break;
        }

        case Phase::tryIt:
            g.setColour (theme.text);
            g.setFont (AbcTrainLookAndFeel::bodyFont());
            g.drawFittedText (textFor (*definition, "try", definition->tryPrompt), area,
                              juce::Justification::topLeft, 6, 1.0f);
            break;

        case Phase::check:
            paintCheckScale (g, area);
            break;

        case Phase::result:
        {
            const auto& check = definition->check;
            const auto passed = lastOutcome.passed;

            // One block, centred in the space the panel has.
            area = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 210));

            auto verdict = area.removeFromTop (40);
            g.setColour (passed ? theme.positive : theme.textBright);
            g.setFont (AbcTrainLookAndFeel::titleFont().withHeight (AbcTrainLookAndFeel::titleFont().getHeight() * 1.3f));
            g.drawText (passed ? text.passed : text.notYet, verdict, juce::Justification::centred, false);

            area.removeFromTop (6);

            g.setColour (theme.textBright);
            g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (20.0f));
            g.drawText (text.itWas + " " + formatValue (hiddenTarget) + "   " + text.youSaid + " " + formatValue (playerValue),
                        area.removeFromTop (30), juce::Justification::centred, false);

            if (check.unit != TrainingModule::Unit::choice)
            {
                g.setColour (theme.text);
                g.setFont (AbcTrainLookAndFeel::bodyFont());
                g.drawText (fill (text.errorLine, { { "err", formatError (check, hiddenTarget, playerValue, levelBefore) },
                                                    { "tol", formatTolerance (check, levelBefore) } }),
                            area.removeFromTop (26), juce::Justification::centred, false);
            }

            area.removeFromTop (10);

            // What the staircase did - the part that makes the next check
            // mean something.
            const auto& after = lastOutcome.after;
            juce::String stair;

            if (lastOutcome.steppedUp)
                stair = fill (text.steppedUp, { { "from", juce::String (levelBefore) }, { "to", juce::String (after.level) } });
            else if (lastOutcome.steppedDown)
                stair = fill (text.steppedDown, { { "from", juce::String (levelBefore) }, { "to", juce::String (after.level) } });
            else if (after.level >= ModuleProgress::maxLevel && passed)
                stair = text.topStep;
            else
                stair = fill (text.toNextStep, { { "n", juce::String (ModuleProgress::stepUpAfter - after.stepRun) } });

            if (lastOutcome.newBest)
                stair << "  " << juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) << "  " << text.newRecord;

            auto stairRow = area.removeFromTop (26);

            // The three squares: passes in a row toward the next step.
            auto squares = stairRow.withSizeKeepingCentre (3 * 14 + 2 * 6, 14).translated (0, 30);

            for (int s = 0; s < ModuleProgress::stepUpAfter; ++s)
            {
                const auto sq = juce::Rectangle<int> (squares.getX() + s * 20, squares.getY(), 14, 14);

                if (s < after.stepRun || (lastOutcome.steppedUp && passed))
                {
                    g.setColour (accent);
                    g.fillRect (sq);
                }
                else
                {
                    g.setColour (theme.outline);
                    g.drawRect (sq, 1);
                }
            }

            g.setColour (lastOutcome.newBest ? accent : theme.textDim);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            g.drawText (stair, stairRow, juce::Justification::centred, false);
            break;
        }

        case Phase::shelf:
            break;
    }
}

void ModuleScreenComponent::paintCheckScale (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();
    auto* definition = currentModule();

    if (definition == nullptr)
        return;

    const auto& check = definition->check;

    // MATCH THIS, the three squares toward the next step beside it, and
    // one sentence saying exactly what to do - "match this" on its own
    // left people looking for a second slider.
    {
        auto row = area.removeFromTop (26);
        const auto caps = AbcTrainLookAndFeel::toCaps (text.match);
        const auto font = AbcTrainLookAndFeel::labelFont();
        const auto w = (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (caps, font, 1.8f)) + 4;
        AbcTrainLookAndFeel::drawTrackedText (g, caps, row.removeFromLeft (w).toFloat(), font, theme.textBright, 1.8f);
        row.removeFromLeft (16);

        const auto run = progress.get (definition->id).stepRun;

        for (int s = 0; s < ModuleProgress::stepUpAfter; ++s)
        {
            const auto sq = juce::Rectangle<int> (row.getX() + s * 20, row.getCentreY() - 6, 12, 12);

            if (s < run) { g.setColour (accent); g.fillRect (sq); }
            else         { g.setColour (theme.outline); g.drawRect (sq, 1); }
        }
    }

    // The sentence gives way before the scale does: on a short window the
    // scale used to be squeezed to nothing under two lines of instruction,
    // and the readout sat on top of the buttons.
    if (text.checkHint.isNotEmpty())
    {
        const auto lines = area.getHeight() >= 190 ? 2 : area.getHeight() >= 130 ? 1 : 0;

        if (lines > 0)
        {
            const auto knob = textFor (*definition, "name", definition->name);
            g.setColour (theme.text);
            g.setFont (AbcTrainLookAndFeel::bodyFont());
            g.drawFittedText (fill (text.checkHint, { { "knob", knob }, { "submit", text.submit } }),
                              area.removeFromTop (lines * 23), juce::Justification::topLeft, lines, 0.85f);
        }
    }

    area.removeFromTop (AbcTrainTheme::Spacing::medium);

    auto band = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), 170));

    // The readout shrinks with the room: its own row when there is height
    // for one, otherwise it rides in the top right of the scale.
    const auto roomy = band.getHeight() >= 96;
    auto readoutRow = roomy ? band.removeFromTop (34) : band.withTrimmedLeft (band.getWidth() / 2).removeFromTop (20);

    if (roomy)
        band.removeFromTop (4);
    auto well = band.toFloat();

    AbcTrainLookAndFeel::paintRecessedWell (g, well, 0.0f);

    const auto toNormalised = [&check] (float value)
    {
        if (check.drawLogarithmically && check.minTarget > 0.0f && value > 0.0f)
            return std::log (value / check.minTarget) / std::log (check.maxTarget / check.minTarget);

        return (value - check.minTarget) / juce::jmax (0.0001f, check.maxTarget - check.minTarget);
    };

    const auto fromNormalised = [&check] (float t)
    {
        if (check.drawLogarithmically && check.minTarget > 0.0f)
            return check.minTarget * std::pow (check.maxTarget / check.minTarget, t);

        return check.minTarget + (check.maxTarget - check.minTarget) * t;
    };

    const auto xFor = [&well] (float t) { return well.getX() + well.getWidth() * juce::jlimit (0.0f, 1.0f, t); };
    const auto labelRow = 18.0f;

    if (check.unit == TrainingModule::Unit::choice)
    {
        // A choice is named, not dialled: one cell per option, yours lit.
        const auto n = (int) check.choiceLabels.size();
        const auto here = juce::roundToInt (currentKnobValue());

        for (int i = 0; i < n; ++i)
        {
            auto cell = juce::Rectangle<float> (well.getX() + well.getWidth() * i / n, well.getY(),
                                                well.getWidth() / n, well.getHeight()).reduced (4.0f);

            if (i + juce::roundToInt (check.minTarget) == here)
            {
                g.setColour (accent.withAlpha (0.22f));
                g.fillRect (cell);
                g.setColour (accent);
                g.drawRect (cell, 1.5f);
            }

            g.setColour (theme.textBright);
            g.setFont (AbcTrainLookAndFeel::headingFont());
            g.drawText (check.choiceLabels[(size_t) i], cell, juce::Justification::centred, false);
        }

        return;
    }

    constexpr int numMarks = 5;

    for (int i = 0; i < numMarks; ++i)
    {
        const auto t = (float) i / (float) (numMarks - 1);
        const auto x = xFor (t);

        g.setColour (theme.textDim.withAlpha (0.24f));
        g.drawLine (x, well.getY() + 5.0f, x, well.getBottom() - labelRow - 3.0f, 1.0f);

        const auto boxWidth = 76.0f;
        const auto boxX = juce::jlimit (well.getX() + 2.0f, well.getRight() - boxWidth - 2.0f, x - boxWidth * 0.5f);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::microFont());
        g.drawText (formatValue (fromNormalised (t)),
                    juce::Rectangle<float> (boxX, well.getBottom() - labelRow - 1.0f, boxWidth, labelRow),
                    juce::Justification::centred, false);
    }

    const auto value = currentKnobValue();
    const auto here = toNormalised (value);

    // The accept band around *your* mark, in the knob's own units: exactly
    // the values that would pass at this step, mapped onto this scale.
    {
        juce::Graphics::ScopedSaveState clipped (g);
        g.reduceClipRegion (well.toNearestInt());

        const auto range = TrainingModule::acceptRange (check, value, checkLevel);
        const auto left = xFor (toNormalised (juce::jmax (range.getStart(), check.drawLogarithmically ? check.minTarget * 0.5f : range.getStart())));
        const auto right = xFor (toNormalised (range.getEnd()));
        const auto lit = juce::Rectangle<float> (left, well.getY(), juce::jmax (2.0f, right - left), well.getHeight() - labelRow);

        g.setColour (accent.withAlpha (0.16f));
        g.fillRect (lit);
        g.setColour (accent.withAlpha (0.4f));
        g.fillRect (lit.withWidth (1.0f));
        g.fillRect (lit.withX (lit.getRight() - 1.0f).withWidth (1.0f));

        const auto x = xFor (here);
        g.setColour (accent);
        g.fillRect (juce::Rectangle<float> (x - 1.0f, well.getY(), 2.0f, well.getHeight() - labelRow));
    }

    {
        const auto readoutText = formatValue (value);
        const auto font = roomy ? AbcTrainLookAndFeel::titleFont() : AbcTrainLookAndFeel::headingFont();
        const auto width = AbcTrainLookAndFeel::trackedTextWidth (readoutText, font, 1.0f) + 18.0f;
        const auto centreX = juce::jlimit (well.getX() + width * 0.5f, well.getRight() - width * 0.5f, xFor (here));

        AbcTrainLookAndFeel::drawTrackedText (g, readoutText,
                                              { centreX - width * 0.5f, (float) readoutRow.getY(), width, (float) readoutRow.getHeight() },
                                              font, theme.textBright, 1.0f, juce::Justification::centred);
    }
}
