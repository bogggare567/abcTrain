#include "shared/learning/LearnerEditorBase.h"
#include "shared/updates/UpdateChecker.h"
#include "shared/updates/Version.h"

namespace
{
    constexpr const char* themeModeKey = "themeMode";
    constexpr int titleRowHeight = 36;
    constexpr int footerHeight = 20;

    UpdateWindow::Strings updateWindowStrings (const LocalisationManager& loc)
    {
        UpdateWindow::Strings s;
        s.title          = loc.getText ("upd.title");
        s.body           = loc.getText ("upd.body");
        s.installedHere  = loc.getText ("upd.installedHere");
        s.nothingFound   = loc.getText ("upd.nothingFound");
        s.noAsset        = loc.getText ("upd.noAsset");
        s.install        = loc.getText ("upd.install");
        s.later          = loc.getText ("upd.later");
        s.cancel         = loc.getText ("upd.cancel");
        s.openPage       = loc.getText ("upd.openPage");
        s.downloading    = loc.getText ("upd.downloading");
        s.opening        = loc.getText ("upd.opening");
        s.failed         = loc.getText ("upd.failed");
        s.finishedPlugin = loc.getText ("upd.finishedPlugin");
        s.finishedApp    = loc.getText ("upd.finishedApp");
        s.versionUnknown = loc.getText ("upd.versionUnknown");
        return s;
    }
}

LearnerEditorBase::LearnerEditorBase (juce::AudioProcessor& p, Services s, Identity id)
    : AudioProcessorEditor (&p),
      settingsFile (LocalisationManager::makeDefaultOptions()),
      services (std::move (s)),
      identity (std::move (id)),
      moduleProgress (services.libraryProperties),
      moduleScreen (services.apvts, moduleProgress, services.practiceSource,
                    services.setCheckOverride, services.clearCheckOverride),
      abCompare (services.apvts, { services.bypassParamId }),
      practiceSelector (services.practiceLibrary, services.practiceSource, services.libraryProperties,
                        [this] { return processor.getSampleRate(); })
{
    AbcTrainTheme::setMode (settingsFile.getValue (themeModeKey, "dark") == "light" ? AbcTrainTheme::Mode::light
                                                                                     : AbcTrainTheme::Mode::dark);
    AbcTrainLookAndFeel::setTextScale ((float) settingsFile.getDoubleValue ("textScale", 1.0));
    accent = AbcTrainTheme::accentFor (identity.family);
    lookAndFeel.refreshFromTheme (accent);
    setLookAndFeel (&lookAndFeel);

    pluginIcon.setIcon (identity.icon);
    addAndMakeVisible (pluginIcon);

    practiceSelector.setLabels (t ("lp.source", "source"), t ("lp.hostAudio", "Host audio"), t ("lp.host", "host"));
    addAndMakeVisible (practiceSelector);

    // Bypass as a square toggle in the trainer's grammar: an outline when
    // the plugin is working, filled when it is not - the state you most
    // need to see from across the room.
    bypassButton.setButtonText (localisation.getText ("ui.bypass"));
    bypassButton.setClickingTogglesState (true);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        services.apvts, services.bypassParamId, bypassButton);
    bypassButton.onStateChange = [this] { AbcTrainLookAndFeel::makePrimary (bypassButton, bypassButton.getToggleState()); };
    AbcTrainLookAndFeel::makePrimary (bypassButton, bypassButton.getToggleState());
    addAndMakeVisible (bypassButton);

    // A and B: two settings one click apart (shared/learning/ABCompare.h).
    for (auto* slot : { &slotA, &slotB })
    {
        slot->setTooltip (t ("lp.abTip", "A and B each remember their own knobs. Set one, switch, change something, switch back."));
        addAndMakeVisible (*slot);
    }

    slotA.onClick = [this] { abCompare.select (0); refreshSlots(); };
    slotB.onClick = [this] { abCompare.select (1); refreshSlots(); };
    refreshSlots();

    updateButton.setTooltip (localisation.getText ("ui.updates"));
    updateButton.onClick = [this] { checkForUpdates(); };
    addAndMakeVisible (updateButton);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

    modulesButton.setTooltip (localisation.getText ("module.shelfTitle"));
    modulesButton.onClick = [this] { moduleScreen.openShelf(); };
    addAndMakeVisible (modulesButton);

    soundkorbLink.setFont (AbcTrainLookAndFeel::monoFont().withHeight (13.0f), false, juce::Justification::centredRight);
    addAndMakeVisible (soundkorbLink);

    moduleScreen.translate = [this] (const juce::String& key)
    {
        const auto text = localisation.getText (key);
        return text == key ? juce::String() : text;
    };
    moduleScreen.setStrings (moduleStrings());
    moduleScreen.onClosed = [this] { repaint(); };
    moduleScreen.prepare (processor.getSampleRate());
}

LearnerEditorBase::~LearnerEditorBase()
{
    setLookAndFeel (nullptr);
}

void LearnerEditorBase::refreshSlots()
{
    const auto active = abCompare.getActive();
    AbcTrainLookAndFeel::makePrimary (slotA, active == 0);
    AbcTrainLookAndFeel::makePrimary (slotB, active == 1);
}

juce::String LearnerEditorBase::decimalPoint() const
{
    const auto lang = localisation.getCurrentLanguage();
    return (lang == "en" || lang == "ja" || lang == "ko" || lang == "zh-Hans") ? "." : ",";
}

juce::String LearnerEditorBase::t (const juce::String& key, const juce::String& fallback) const
{
    const auto text = localisation.getText (key);
    return text == key || text.isEmpty() ? fallback : text;
}

ModuleScreenComponent::Strings LearnerEditorBase::moduleStrings() const
{
    const auto g = [this] (const char* key) { return localisation.getText (key); };

    ModuleScreenComponent::Strings s;
    s.match = g ("module.match");
    s.reference = g ("module.reference");
    s.mine = g ("module.mine");
    s.submit = g ("module.submit");
    s.passed = g ("module.passed");
    s.notYet = g ("module.notYet");
    s.itWas = g ("module.itWas");
    s.youSaid = g ("module.youSaid");
    s.again = g ("module.again");
    s.done = g ("module.done");
    s.phaseWatch = g ("module.phaseWatch");
    s.phaseTry = g ("module.phaseTry");
    s.phaseCheck = g ("module.phaseCheck");
    s.phaseResult = g ("module.phaseResult");
    s.shelfTitle = g ("module.shelfTitle");
    s.shelfSubtitle = g ("module.shelfSubtitle");
    s.walkthroughs = g ("module.walkthroughs");
    s.walkthroughWhy = g ("module.walkthroughWhy");
    s.close = g ("module.close");
    s.back = g ("module.back");
    s.next = g ("module.next");
    s.ready = g ("module.ready");
    s.finish = g ("module.finish");
    s.stepOf = g ("module.stepOf");
    s.levelLine = g ("module.levelLine");
    s.errorLine = g ("module.errorLine");
    s.steppedUp = g ("module.steppedUp");
    s.steppedDown = g ("module.steppedDown");
    s.toNextStep = g ("module.toNextStep");
    s.topStep = g ("module.topStep");
    s.newRecord = g ("module.newRecord");
    s.octaves = g ("unit.oct");
    s.notTried = g ("module.notTried");
    s.checkHint = g ("module.checkHint");
    s.stepsCount = g ("module.stepsCount");

    s.decimal = decimalPoint();
    return s;
}

void LearnerEditorBase::finishSetup (std::vector<TrainingModule::Definition> modules, int width, int height)
{
    moduleScreen.setModules (std::move (modules));

    // After the subclass's own controls, so it floats above what it covers.
    addAndMakeVisible (guideTooltip);

    // The module panel over everything in the analysis section, and the
    // update window over absolutely everything.
    addChildComponent (moduleScreen);
    addChildComponent (updateWindow);
    updateWindow.onClosed = [this] { resized(); repaint(); };

    // The design size is what the window prefers; it opens at whatever
    // part of that the screen can show, down to a floor the compact layout
    // is built for (shared/ui/WindowFit.h, ADR 038).
    designSize = { width, height };
    const juce::Point<int> minimum { juce::jmin (width, 820), 600 };

    setResizable (true, true);
    setResizeLimits (minimum.x, minimum.y, (int) (width * 1.6f), (int) (height * 1.5f));
    getConstrainer()->setFixedAspectRatio (0.0);

    const auto fitted = WindowFit::fit (designSize, minimum);
    setSize (fitted.x, fitted.y);

    applyTheme();
    tick();
    startTimerHz (30);
}

void LearnerEditorBase::applyTheme()
{
    const auto& theme = AbcTrainTheme::current();

    themeButton.setIcon (theme.mode == AbcTrainTheme::Mode::light ? AppIcons::Icon::moon : AppIcons::Icon::sun);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, theme.accent);
    pluginIcon.setIconColour (accent);
    moduleScreen.setAccentColour (accent);

    themeChanged();
    repaint();
}

void LearnerEditorBase::toggleTheme()
{
    const auto newMode = AbcTrainTheme::getMode() == AbcTrainTheme::Mode::light ? AbcTrainTheme::Mode::dark
                                                                               : AbcTrainTheme::Mode::light;
    AbcTrainTheme::setMode (newMode);
    settingsFile.setValue (themeModeKey, newMode == AbcTrainTheme::Mode::light ? "light" : "dark");
    settingsFile.saveIfNeeded();

    accent = AbcTrainTheme::accentFor (identity.family);
    lookAndFeel.refreshFromTheme (accent);
    applyTheme();

    for (auto* child : getChildren())
        child->repaint();
}

void LearnerEditorBase::checkForUpdates()
{
    // Every click gets a visible outcome (ADR 014): checking, then a
    // window, "up to date", or - if nothing came back - "couldn't check".
    juce::Component::SafePointer<LearnerEditorBase> safeThis (this);
    auto handled = std::make_shared<bool> (false);

    updateButton.setEnabled (false);
    showGuide (localisation.getText ("ui.checkingForUpdates"));

    UpdateChecker::checkForUpdatesAsync (CurrentVersion::string,
        [safeThis, handled] (bool foundNewer, UpdateChecker::ReleaseInfo release)
    {
        if (safeThis == nullptr || *handled)
            return;

        *handled = true;
        safeThis->updateButton.setEnabled (true);

        if (! foundNewer)
        {
            safeThis->showGuide (safeThis->localisation.getText ("ui.upToDate")
                                 + " (" + juce::String (CurrentVersion::string) + ")", 4000);
            return;
        }

        safeThis->showGuide ({});
        safeThis->updateWindow.setStrings (updateWindowStrings (safeThis->localisation));
        safeThis->updateWindow.show (release, juce::JUCEApplicationBase::isStandaloneApp());
    });

    juce::Timer::callAfterDelay (6000, [safeThis, handled]
    {
        if (safeThis == nullptr || *handled)
            return;

        *handled = true;
        safeThis->updateButton.setEnabled (true);
        safeThis->showGuide (safeThis->t ("lp.updateFailed", "Couldn't reach the update server."), 5000);
    });
}

void LearnerEditorBase::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat(), accent);

    // The name, tracked, beside the icon; a hairline; then the family in
    // its own colour - which part of the subject this plugin teaches.
    {
        auto row = getLocalBounds().reduced (AbcTrainTheme::Spacing::large).removeFromTop (titleRowHeight);
        row.removeFromLeft (38);

        const auto font = AbcTrainLookAndFeel::titleFont();
        const auto room = familyLimit - row.getX();

        // "ABC Learner Comp" where there is room, "Learner Comp" where
        // there is not - the prefix is for sorting in a plugin list, and
        // this window is already open.
        auto title = identity.title;
        auto nameWidth = (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (title, font, 1.6f)) + 4;

        if (nameWidth > room - 120 && title.startsWith ("ABC "))
        {
            title = title.substring (4);
            nameWidth = (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (title, font, 1.6f)) + 4;
        }

        nameWidth = juce::jmin (nameWidth, juce::jmax (0, room));
        AbcTrainLookAndFeel::drawTrackedText (g, title, row.removeFromLeft (nameWidth).toFloat(),
                                              font, theme.textBright, 1.6f);

        row.removeFromLeft (12);
        g.setColour (theme.outline);
        g.fillRect (row.removeFromLeft (1).withSizeKeepingCentre (1, 22));
        row.removeFromLeft (12);

        static const char* familyKeys[] = { "lp.family.frequency", "lp.family.dynamics", "lp.family.space", "lp.family.character" };
        static const char* familyFallbacks[] = { "Frequency", "Dynamics", "Space", "Character" };
        const auto f = juce::jlimit (0, 3, (int) identity.family);

        if (row.getRight() > familyLimit)
            row.setRight (familyLimit);

        g.setColour (accent);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (t (familyKeys[f], familyFallbacks[f]), row, juce::Justification::centredLeft, true);
    }

    AbcTrainLookAndFeel::paintSectionPanel (g, analysisSection.toFloat(), localisation.getText ("lp.analysis"));
    AbcTrainLookAndFeel::paintSectionPanel (g, controlSection.toFloat(), localisation.getText (identity.controlsCaptionKey));
}

void LearnerEditorBase::paintOverChildren (juce::Graphics& g)
{
    if (bypassVeil <= 0.004f || analysisSection.isEmpty() || moduleScreen.isRunning())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (bypassVeil);
    const auto area = analysisSection.toFloat().reduced (AbcTrainTheme::Spacing::medium);

    // Desaturated rather than hidden: you still want to see the signal
    // going past, you just need to tell at a glance nothing is done to it.
    g.setColour (theme.windowBackground.withAlpha (0.62f * eased));
    g.fillRect (area);

    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (localisation.getText ("lp.bypassed")),
                                          area.withSizeKeepingCentre (area.getWidth(), 22.0f),
                                          AbcTrainLookAndFeel::headingFont(), theme.textDim.withAlpha (eased), 3.0f,
                                          juce::Justification::centred);
}

void LearnerEditorBase::resized()
{
    using namespace AbcTrainTheme;

    updateWindow.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (Spacing::large);

    auto titleRow = area.removeFromTop (titleRowHeight);
    pluginIcon.setBounds (titleRow.removeFromLeft (28).withSizeKeepingCentre (28, 28));

    const auto square = [&titleRow] (juce::Component& c)
    {
        c.setBounds (titleRow.removeFromRight (32).withSizeKeepingCentre (32, 32));
        titleRow.removeFromRight (Spacing::small);
    };

    square (modulesButton);
    square (themeButton);
    square (updateButton);

    bypassButton.setBounds (titleRow.removeFromRight (104).withSizeKeepingCentre (104, 30));
    titleRow.removeFromRight (Spacing::small);

    slotB.setBounds (titleRow.removeFromRight (34).withSizeKeepingCentre (34, 30));
    titleRow.removeFromRight (2);
    slotA.setBounds (titleRow.removeFromRight (34).withSizeKeepingCentre (34, 30));
    titleRow.removeFromRight (Spacing::medium);

    const auto practiceWidth = practiceSelector.getPreferredWidth();
    practiceSelector.setBounds (titleRow.removeFromRight (practiceWidth).withSizeKeepingCentre (practiceWidth, 26));
    familyLimit = practiceSelector.getX() - Spacing::small;

    area.removeFromTop (Spacing::medium);

    auto footer = area.removeFromBottom (footerHeight);
    soundkorbLink.setBounds (footer.removeFromRight (140));
    area.removeFromBottom (Spacing::small);

    const auto captionSpace = Spacing::large + 6;
    const auto controlsHeight = controlsContentHeight() + captionSpace + Spacing::medium;

    // In a short window the analysis section gives way first: a spectrum
    // 180 px tall still reads, a knob too small to grab does not.
    const auto analysisFloor = isCompact() ? 180 : analysisContentHeight();
    analysisSection = area.removeFromTop (juce::jmax (analysisFloor + captionSpace + Spacing::medium,
                                                      area.getHeight() - controlsHeight - Spacing::medium));
    area.removeFromTop (Spacing::medium);
    controlSection = area.removeFromTop (controlsHeight);

    {
        auto inner = analysisSection.reduced (Spacing::medium);
        inner.removeFromTop (captionSpace - Spacing::medium);
        layoutAnalysis (inner);
    }

    {
        auto inner = controlSection.reduced (Spacing::medium);
        inner.removeFromTop (captionSpace - Spacing::medium);
        layoutControls (inner);
    }

    moduleScreen.setBounds (analysisSection);

    guideTooltip.setBounds (analysisSection.reduced (Spacing::large, 0)
                                           .withHeight (72)
                                           .withY (analysisSection.getBottom() - 84));
}

void LearnerEditorBase::timerCallback()
{
    const auto bypassed = services.apvts.getRawParameterValue (services.bypassParamId)->load() > 0.5f;
    const auto target = bypassed ? 1.0f : 0.0f;

    if (! juce::approximatelyEqual (bypassVeil, target))
    {
        const auto step = (float) (1000.0 / 30.0 / AbcTrainTheme::Duration::release);
        bypassVeil = std::abs (target - bypassVeil) <= step ? target : bypassVeil + (target > bypassVeil ? step : -step);
        repaint();
    }

    // A module saves the knobs on entry and puts them back on exit;
    // switching slots in between would hand it the wrong ones to restore.
    const auto slotsLive = ! moduleScreen.isRunning();

    if (slotA.isEnabled() != slotsLive)
    {
        slotA.setEnabled (slotsLive);
        slotB.setEnabled (slotsLive);
    }

    tick();
}
