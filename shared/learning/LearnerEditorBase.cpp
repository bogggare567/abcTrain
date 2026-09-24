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

    // What the plugin listens to, as chips: the host (or, in the app, the
    // audio input) and every category of the shared library. The dropdown
    // underneath does the work and is what shows when there are too many
    // categories for a row.
    practiceSelector.onListChanged = [this] { refreshMaterialChips(); };
    practiceSelector.setLabels (t ("lp.source", "source"), t ("lp.hostAudio", "Host audio"), t ("lp.host", "host"));
    addChildComponent (practiceSelector);

    materialChips.setCaption (t ("lp.material", "Material"));
    materialChips.onChosen = [this] (int index) { practiceSelector.choose (index); };
    addAndMakeVisible (materialChips);
    refreshMaterialChips();

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
    modulesButton.onClick = [this] { setCompanionOpen (! companionOpen); };
    addAndMakeVisible (modulesButton);

    // The same shelf, as a word, for the toolbar inside the app - where
    // there is no title row for the icon to live in.
    lessonsButton.setButtonText (localisation.getText ("module.shelfTitle"));
    lessonsButton.onClick = [this] { setCompanionOpen (! companionOpen); };
    addChildComponent (lessonsButton);

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

    companion.setStrings (companionStrings());
    openedAtMs = juce::Time::getMillisecondCounterHiRes();
}

LearnerEditorBase::~LearnerEditorBase()
{
    // The window first: it shows the module screen and the lesson, which
    // are this editor's members.
    if (companionWindow != nullptr)
        settingsFile.setValue ("companionBounds." + identity.title, companionWindow->getBounds().toString());

    companionWindow.reset();

    if (companionOpen)
        takeBackFromCompanion();

    setLookAndFeel (nullptr);
}

CompanionPanel::Strings LearnerEditorBase::companionStrings() const
{
    CompanionPanel::Strings s;
    s.modules = localisation.getText ("module.shelfTitle");
    s.lesson = t ("companion.lesson", s.lesson);
    s.underPointer = t ("companion.underPointer", s.underPointer);
    s.underPointerEmpty = t ("companion.underPointerEmpty", s.underPointerEmpty);
    s.hearing = t ("companion.hearing", s.hearing);
    s.session = t ("companion.session", s.session);
    s.week = t ("companion.week", s.week);
    s.untilBreak = t ("companion.untilBreak", s.untilBreak);
    s.minutes = t ("companion.minutes", s.minutes);
    s.inMinutes = t ("companion.inMinutes", s.inMinutes);
    s.notCalibrated = t ("companion.notCalibrated", s.notCalibrated);
    s.pluginTime = t ("companion.pluginTime", s.pluginTime);
    s.pluginNote = t ("companion.pluginNote", s.pluginNote);
    return s;
}

void LearnerEditorBase::lendToCompanion()
{
    removeChildComponent (&moduleScreen);
    moduleScreen.setFillsHost (true);

    if (! moduleScreen.isRunning())
        moduleScreen.openShelf();

    auto* lesson = companionLesson();
    lessonLent = lesson != nullptr;

    if (lesson != nullptr)
        removeChildComponent (lesson);

    companion.attach (moduleScreen, lesson);
}

void LearnerEditorBase::takeBackFromCompanion()
{
    auto* lesson = lessonLent ? companionLesson() : nullptr;
    companion.detach();

    moduleScreen.setFillsHost (false);
    moduleScreen.setVisible (false);
    addChildComponent (moduleScreen);
    updateWindow.toFront (false);

    if (lesson != nullptr)
        addAndMakeVisible (lesson);

    lessonLent = false;
}

void LearnerEditorBase::setCompanionOpen (bool shouldBeOpen)
{
    if (shouldBeOpen == companionOpen)
        return;

    companionOpen = shouldBeOpen;

    if (shouldBeOpen)
    {
        lendToCompanion();
        companion.setStrings (companionStrings());
        companion.setAccent (accent);
        guideTooltip.setVisible (false);

        if (! WindowFit::fittingDisabled())
        {
            companionWindow = std::make_unique<CompanionWindow> (identity.title, companion,
                                                                 AbcTrainTheme::current().panelBackground);
            juce::Component::SafePointer<LearnerEditorBase> safeThis (this);
            companionWindow->onCloseRequested = [safeThis]
            {
                // Not from inside the window's own callback: closing deletes it.
                juce::MessageManager::callAsync ([safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->setCompanionOpen (false);
                });
            };

            // Where it was last time; the first time, beside the plugin.
            auto bounds = juce::Rectangle<int>::fromString (settingsFile.getValue ("companionBounds." + identity.title));

            if (bounds.isEmpty())
            {
                const auto editorOnScreen = getScreenBounds();
                bounds = { editorOnScreen.getRight() + 12, editorOnScreen.getY(), 380,
                           juce::jmax (600, editorOnScreen.getHeight()) };
            }

            if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (bounds))
                bounds = bounds.constrainedWithin (display->userArea);

            companionWindow->setBounds (bounds);
            companionWindow->setVisible (true);
            companionWindow->toFront (false);
        }

        refreshCompanion();
    }
    else
    {
        if (companionWindow != nullptr)
            settingsFile.setValue ("companionBounds." + identity.title, companionWindow->getBounds().toString());

        companionWindow.reset();
        takeBackFromCompanion();
        guideTooltip.setVisible (true);
    }

    // Not from the snapshot tools: a picture must not reopen a window on
    // the next real launch.
    if (! WindowFit::fittingDisabled())
    {
        settingsFile.setValue ("companionOpen." + identity.title, shouldBeOpen);
        settingsFile.saveIfNeeded();
    }
    resized();
    repaint();
}

CompanionPanel& LearnerEditorBase::openCompanionForSnapshot()
{
    if (! companionOpen)
        setCompanionOpen (true);

    companion.setSize (380, 820);
    refreshCompanion();
    return companion;
}

void LearnerEditorBase::refreshCompanion()
{
    if (! companionOpen)
        return;

    CompanionHearing h;

    if (hearingProvider != nullptr)
    {
        h = hearingProvider();
        h.fromApp = true;
    }
    else
    {
        h.sessionMinutes = (int) ((juce::Time::getMillisecondCounterHiRes() - openedAtMs) / 60000.0);
    }

    companion.setHearing (h);

    if (companionWindow != nullptr)
    {
        // Above the host while the host is the app in front, an ordinary
        // window otherwise; hidden while the plugin's own window is.
        const auto front = juce::Process::isForegroundProcess();

        if (companionWindow->isAlwaysOnTop() != front)
            companionWindow->setAlwaysOnTop (front);

        const auto showing = isShowing();

        if (companionWindow->isVisible() != showing)
            companionWindow->setVisible (showing);
    }
}

void LearnerEditorBase::refreshMaterialChips()
{
    materialChips.setItems (practiceSelector.getShortLabels());
    materialChips.setChosen (practiceSelector.getChosenIndex());

    // Not from the base constructor: resized() asks the subclass for its
    // heights, and the subclass does not exist yet.
    if (setupFinished)
        resized();
}

void LearnerEditorBase::placeMaterial (juce::Rectangle<int> area)
{
    // Chips when they fit, the dropdown when the library has grown past
    // a row.
    const auto chipsFit = materialChips.getPreferredWidth() <= area.getWidth();
    materialChips.setVisible (chipsFit);
    practiceSelector.setVisible (! chipsFit);

    if (chipsFit)
    {
        materialChips.setBounds (area.withWidth (materialChips.getPreferredWidth()));
        return;
    }

    const auto practiceWidth = juce::jmin (area.getWidth(), practiceSelector.getPreferredWidth());
    practiceSelector.setBounds (area.removeFromRight (practiceWidth).withSizeKeepingCentre (practiceWidth, 26));
}

void LearnerEditorBase::placeWithMaterial (ChipRow& own, juce::Rectangle<int> area)
{
    const auto gap = AbcTrainTheme::Spacing::large;

    if (own.getPreferredWidth() + gap + materialChips.getPreferredWidth() <= area.getWidth())
    {
        own.setBounds (area.removeFromLeft (own.getPreferredWidth()));
        area.removeFromLeft (gap);
        placeMaterial (area);
        return;
    }

    materialChips.setVisible (false);
    practiceSelector.setVisible (true);

    const auto practiceWidth = juce::jmin (area.getWidth() / 3, practiceSelector.getPreferredWidth());
    practiceSelector.setBounds (area.removeFromRight (practiceWidth).withSizeKeepingCentre (practiceWidth, 26));
    area.removeFromRight (AbcTrainTheme::Spacing::small);
    own.setBounds (area.withWidth (juce::jmin (area.getWidth(), own.getPreferredWidth())));
}

void LearnerEditorBase::setEmbedded (bool shouldBeEmbedded)
{
    embedded = shouldBeEmbedded;

    pluginIcon.setVisible (! embedded);
    updateButton.setVisible (! embedded);
    themeButton.setVisible (! embedded);
    modulesButton.setVisible (! embedded);
    soundkorbLink.setVisible (! embedded);
    lessonsButton.setVisible (embedded);

    // In the app the host is the audio interface's input.
    practiceSelector.setLabels (t ("lp.source", "source"), t ("lp.hostAudio", "Host audio"),
                                embedded ? t ("lp.input", "input") : t ("lp.host", "host"));
    resized();
    repaint();
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
    setupFinished = true;
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

    // Open again if it was open when the plugin last closed.
    if (! WindowFit::fittingDisabled() && settingsFile.getBoolValue ("companionOpen." + identity.title, false))
    {
        juce::Component::SafePointer<LearnerEditorBase> safeThis (this);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr && safeThis->isShowing())
                safeThis->setCompanionOpen (true);
        });
    }
}

void LearnerEditorBase::applyTheme()
{
    const auto& theme = AbcTrainTheme::current();

    themeButton.setIcon (theme.mode == AbcTrainTheme::Mode::light ? AppIcons::Icon::moon : AppIcons::Icon::sun);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, theme.accent);
    pluginIcon.setIconColour (accent);
    moduleScreen.setAccentColour (accent);
    materialChips.setAccent (accent);
    companion.setAccent (accent);

    if (companionWindow != nullptr)
        companionWindow->setBackgroundColour (theme.panelBackground);

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
        UpdateChecker::channelFor (CurrentVersion::string, settingsFile.getBoolValue (UpdateChecker::betaOptInKey, false)),
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
    if (! embedded)
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
        AbcTrainLookAndFeel::fitText (g, t (familyKeys[f], familyFallbacks[f]), row, juce::Justification::centredLeft, true);
    }

    // No panel behind the analysis: the displays are their own wells, and
    // a box round boxes was what made the old layout read as forms. The
    // controls keep one hairline box, which is what groups the knobs.
    {
        const auto box = controlSection.withTrimmedBottom (controlsFooterHeight() > 0
                                                               ? controlsFooterHeight() + AbcTrainTheme::Spacing::medium : 0)
                                       .toFloat().reduced (0.5f);
        g.setColour (theme.panelBackground.withAlpha (0.5f));
        g.fillRect (box);
        g.setColour (theme.outline);
        g.drawRect (box, 1.0f);
    }
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

    auto area = getLocalBounds().reduced (embedded ? Spacing::large - 4 : Spacing::large);

    // The plugin's own title row: icon, name, family, and the things a
    // plugin window has to carry for itself - modules, theme, updates.
    if (! embedded)
    {
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
        familyLimit = titleRow.getRight() - Spacing::small;

        area.removeFromTop (Spacing::medium);
    }

    // The toolbar: what is playing on the left, A/B and bypass on the right.
    {
        auto toolbar = area.removeFromTop (32);

        bypassButton.setBounds (toolbar.removeFromRight (104));
        toolbar.removeFromRight (Spacing::small);

        slotB.setBounds (toolbar.removeFromRight (38));
        slotA.setBounds (toolbar.removeFromRight (38));
        toolbar.removeFromRight (Spacing::small);

        if (embedded)
        {
            const auto width = juce::jmax (96, (int) AbcTrainLookAndFeel::trackedTextWidth (
                                                   AbcTrainLookAndFeel::toCaps (lessonsButton.getButtonText()),
                                                   AbcTrainLookAndFeel::labelFont(), 1.2f) + 32);
            lessonsButton.setBounds (toolbar.removeFromRight (width));
            toolbar.removeFromRight (Spacing::medium);
        }

        layoutToolbar (toolbar);
    }

    area.removeFromTop (Spacing::medium);

    if (! embedded)
    {
        auto footer = area.removeFromBottom (footerHeight);
        soundkorbLink.setBounds (footer.removeFromRight (140));
        area.removeFromBottom (Spacing::small);
    }

    // The guide text has a strip of its own at the foot of the window.
    // It used to float over the bottom of the analysis - exactly the part
    // of the picture the knob being explained was changing (the author:
    // "перекрывают обзор").
    // With the companion window open the guide is there, and the strip's
    // height goes back to the plugin.
    juce::Rectangle<int> guideStrip;

    if (! companionOpen)
    {
        guideStrip = area.removeFromBottom (44);
        area.removeFromBottom (Spacing::small);
    }

    const auto controlsHeight = controlsContentHeight() + 2 * Spacing::medium;

    // In a short window the analysis section gives way first: a display
    // 160 px tall still reads, a knob too small to grab does not.
    const auto analysisFloor = isCompact() ? 160 : analysisContentHeight();
    analysisSection = area.removeFromTop (juce::jmax (analysisFloor, area.getHeight() - controlsHeight - Spacing::medium));
    area.removeFromTop (Spacing::medium);
    controlSection = area.removeFromTop (controlsHeight);

    layoutAnalysis (analysisSection);
    layoutControls (controlSection.reduced (Spacing::medium));

    if (! companionOpen)
        moduleScreen.setBounds (analysisSection);

    guideTooltip.setBounds (guideStrip);
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

    if (++tickCount % 15 == 0)
        refreshCompanion();

    tick();
}
