#include "SettingsScreenComponent.h"

#include <cstdlib>
#include <algorithm>
#include "shared/ui/CommunityLink.h"
#include "shared/ui/IdleScreensaver.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/updates/Version.h"
#include "shared/updates/UpdateChecker.h"
#include <BrandBinaryData.h>

namespace
{
    constexpr int railWidth = AbcTrainTheme::Layout::sideRail;
    constexpr int menuRowHeight = 36;
    constexpr int rowHeight = 62;
    constexpr int titleColumn = 330;
    constexpr int controlHeight = 30;

    // One width for every control that is not a row of segments or a
    // composite: menus, buttons and faders used to be 140, 220 or the whole
    // row, so each row's chevron and edge sat somewhere else and the page
    // read as patched together.
    constexpr int standardControlWidth = 280;

    // Compact rows: the title and two lines of hint on top, the control
    // under them at a thumb's height.
    constexpr int compactTextHeight = 58;
    constexpr int compactControlHeight = 40;

    juce::String n (int value) { return juce::String (value); }
}

SettingsScreenComponent::SettingsScreenComponent (LocalisationManager& localisationToUse,
                                                   juce::PropertiesFile& propertiesToUse)
    : localisation (localisationToUse), properties (propertiesToUse)
{
    setOpaque (true);

    // ---- mode --------------------------------------------------------
    modeSwitch.setComponentID ("settings.proMode");
    modeSwitch.onClick = [this]
    {
        settings.setMode (modeSwitch.getToggleState() ? TrainerSettings::Mode::pro : TrainerSettings::Mode::beginner);
        syncControlsFromSettings();

        if (onTrainerSettingsChanged != nullptr)
            onTrainerSettingsChanged();
    };
    addChildComponent (modeSwitch);   // kept for the key handler; the rail shows modeChoice

    modeChoice.setComponentID ("settings.mode");
    modeChoice.onChange = [this] (int v)
    {
        modeSwitch.setToggleState (v == 1, juce::dontSendNotification);
        settings.setMode (v == 1 ? TrainerSettings::Mode::pro : TrainerSettings::Mode::beginner);
        syncControlsFromSettings();

        if (onTrainerSettingsChanged != nullptr)
            onTrainerSettingsChanged();
    };
    addAndMakeVisible (modeChoice);

    answerModeChoice.setValue (properties.getIntValue (answerByButtonKey, 0));
    answerModeChoice.onChange = [this] (int v)
    {
        properties.setValue (answerByButtonKey, v);
        properties.saveIfNeeded();
        if (onSettingsChanged != nullptr)
            onSettingsChanged();
    };
    addAndMakeVisible (answerModeChoice);

    onTopChoice.setValue (properties.getBoolValue (alwaysOnTopKey, false) ? 1 : 0);
    onTopChoice.onChange = [this] (int v)
    {
        properties.setValue (alwaysOnTopKey, v == 1);
        properties.saveIfNeeded();
        if (onSettingsChanged != nullptr)
            onSettingsChanged();
    };
    addAndMakeVisible (onTopChoice);

    compactBackButton.setButtonText (juce::String::fromUTF8 ("\xe2\x80\xb9  ") + localisation.getText ("ui.settings"));
    compactBackButton.onClick = [this]
    {
        compactShowingPage = false;
        compactScroll = 0;
        scroller.setOffset (0.0f);
        resized();
        repaint();
    };
    addChildComponent (compactBackButton);

    scroller.onChange = [this] (float offset)
    {
        compactScroll = juce::roundToInt (offset);
        resized();
        repaint();
    };

    // ---- training and hearing choices ----------------------------------
    //
    // Every one of these writes straight to TrainerSettings and tells the
    // editor; what the value *means* is TrainerSettings' business.
    const auto bind = [this] (SegmentedChoice& control, Id id)
    {
        control.onChange = [this, id] (int value)
        {
            settings.set (id, value);
            syncControlsFromSettings();

            if (onTrainerSettingsChanged != nullptr)
                onTrainerSettingsChanged();
        };

        addAndMakeVisible (control);
    };

    bind (stepRule, Id::stepRule);
    bind (answerPause, Id::answerPause);
    bind (survivalLives, Id::survivalLives);
    bind (blitzSeconds, Id::blitzSeconds);
    bind (blitzPenalty, Id::blitzPenalty);
    bind (hints, Id::hints);
    bind (allModes, Id::allModesOpen);
    bind (hearingOn, Id::hearingOn);
    bind (breakMinutes, Id::breakReminderMinutes);
    bind (fatigueHint, Id::fatigueHint);
    bind (weeklyLimit, Id::weeklyLimit);

    resetButton.onClick = [this]
    {
        settings.resetAll();
        syncControlsFromSettings();

        if (onTrainerSettingsChanged != nullptr)
            onTrainerSettingsChanged();
    };
    addAndMakeVisible (resetButton);

    // ---- calibration ---------------------------------------------------
    calibrationSlider.setRange (60.0, 110.0, 1.0);
    calibrationSlider.setValue (80.0, juce::dontSendNotification);
    calibrationSlider.setTextValueSuffix (" dB(A)");
    calibrationRow.addAndMakeVisible (calibrationNoiseButton);
    calibrationRow.addAndMakeVisible (calibrationSlider);
    calibrationRow.addAndMakeVisible (calibrationSaveButton);
    calibrationRow.addAndMakeVisible (calibrationClearButton);
    calibrationRow.addAndMakeVisible (calibrationDown);
    calibrationRow.addAndMakeVisible (calibrationUp);
    addAndMakeVisible (calibrationRow);

    // The row covers its own title and hint, which the page paints: let
    // clicks through everywhere except on the controls themselves.
    calibrationRow.setInterceptsMouseClicks (false, true);

    // A meter reads in whole dB, so the fader has to be settable in whole
    // dB without a steady hand: one press, one dB.
    calibrationDown.onClick = [this] { calibrationSlider.setValue (calibrationSlider.getValue() - 1.0); };
    calibrationUp.onClick   = [this] { calibrationSlider.setValue (calibrationSlider.getValue() + 1.0); };

    calibrationNoiseButton.onClick = [this]
    {
        noisePlaying = ! noisePlaying;

        if (onCalibrationNoise != nullptr)
            onCalibrationNoise (noisePlaying);

        refresh();
    };

    calibrationSaveButton.onClick = [this]
    {
        settings.set (Id::calibrationDb, juce::roundToInt (calibrationSlider.getValue()));

        // Measuring is done; leave the room quiet again.
        if (noisePlaying && onCalibrationNoise != nullptr)
            onCalibrationNoise (false);

        noisePlaying = false;
        syncControlsFromSettings();

        if (onTrainerSettingsChanged != nullptr)
            onTrainerSettingsChanged();
    };

    calibrationClearButton.onClick = [this]
    {
        settings.set (Id::calibrationDb, 0);
        syncControlsFromSettings();

        if (onTrainerSettingsChanged != nullptr)
            onTrainerSettingsChanged();
    };

    // ---- hearing calibration (ADR 051) ----------------------------------
    profileSelector.setComponentID ("settings.hearingProfile");
    profileRow.addAndMakeVisible (profileSelector);
    profileRow.addAndMakeVisible (profileTestButton);
    profileRow.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (profileRow);

    profileSelector.onChange = [this]
    {
        if (hearingProfiles == nullptr)
            return;

        const auto index = profileSelector.getSelectedId() - 2;
        const auto& all = hearingProfiles->getProfiles();
        hearingProfiles->setActive (index >= 0 && index < (int) all.size() ? all[(size_t) index].id : juce::String());
        refreshHearingProfileRow();

        if (onHearingProfileChanged != nullptr)
            onHearingProfileChanged();
    };

    profileTestButton.onClick = [this]
    {
        if (onHearingTest != nullptr)
            onHearingTest();
    };

    applyChoice.onChange = [this] (int value)
    {
        if (hearingProfiles == nullptr)
            return;

        hearingProfiles->setApplying (value == 1);

        if (onHearingProfileChanged != nullptr)
            onHearingProfileChanged();
    };
    addAndMakeVisible (applyChoice);

    // ---- exposure from outside the trainer -----------------------------
    exposureHours.setValue (4);
    exposureLevel.setValue (95);

    for (auto* c : { &weeklyLimit, &exposureHours, &exposureLevel })
        c->setUppercase (false);
    exposureRow.addAndMakeVisible (exposureHours);
    exposureRow.addAndMakeVisible (exposureLevel);
    exposureRow.addAndMakeVisible (exposureAddButton);
    addAndMakeVisible (exposureRow);

    exposureAddButton.onClick = [this]
    {
        if (onAddExposure != nullptr)
            onAddExposure ((double) exposureHours.getValue(), (double) exposureLevel.getValue());

        timerCallback();
    };

    // ---- appearance: theme and language -------------------------------
    themeChoice.setValue (AbcTrainTheme::getMode() == AbcTrainTheme::Mode::dark ? 1 : 0);
    themeChoice.onChange = [this] (int value)
    {
        if (onThemeChosen != nullptr)
            onThemeChosen (value == 1);
    };
    addAndMakeVisible (themeChoice);

    {
        // Each language in its own name - somebody who cannot read the
        // current one still finds theirs.
        const auto& codes = LocalisationManager::getSupportedLanguageCodes();

        for (int i = 0; i < codes.size(); ++i)
            languageChoice.addItem (LocalisationManager::getDisplayName (codes[i]), i + 1,
                                    LocalisationManager::getDisplayName (codes[i]));

        languageChoice.setSelectedId (codes.indexOf (localisation.getCurrentLanguage()) + 1, juce::dontSendNotification);
        languageChoice.onChange = [this]
        {
            const auto& all = LocalisationManager::getSupportedLanguageCodes();
            const auto index = languageChoice.getSelectedId() - 1;

            if (index >= 0 && index < all.size() && onLanguageChosen != nullptr)
                onLanguageChosen (all[index]);
        };
        addAndMakeVisible (languageChoice);
    }

    // ---- loudness matching ---------------------------------------------
    loudnessMatchChoice.setValue (properties.getIntValue (loudnessMatchKey, 0));
    loudnessMatchChoice.onChange = [this] (int value)
    {
        properties.setValue (loudnessMatchKey, value);
        properties.saveIfNeeded();
        if (onLoudnessMatchChanged != nullptr)
            onLoudnessMatchChanged (value);
    };
    addAndMakeVisible (loudnessMatchChoice);

    // ---- the day's dose ------------------------------------------------
    dailyMinutesChoice.setValue (properties.getIntValue (dailyMinutesKey, 15));
    dailyMinutesChoice.onChange = [this] (int value)
    {
        properties.setValue (dailyMinutesKey, value);
        properties.saveIfNeeded();
        if (onDailyMinutesChanged != nullptr)
            onDailyMinutesChanged();
    };
    addAndMakeVisible (dailyMinutesChoice);

    // ---- the click on the scales (ADR 062) --------------------------------
    hapticsChoice.setValue (properties.getIntValue (hapticsKey, hapticsDefault));
    hapticsChoice.onChange = [this] (int value)
    {
        properties.setValue (hapticsKey, value);
        properties.saveIfNeeded();
    };
    addAndMakeVisible (hapticsChoice);

    // ---- the app's own sounds ------------------------------------------
    uiSoundsChoice.setValue (properties.getIntValue (uiSoundsKey, 2));
    uiSoundsChoice.onChange = [this] (int value)
    {
        properties.setValue (uiSoundsKey, value);
        properties.saveIfNeeded();

        if (onUiSoundsChanged != nullptr)
            onUiSoundsChanged (value);
    };
    addAndMakeVisible (uiSoundsChoice);

    answerSoundChoice.setValue (properties.getIntValue (answerSoundKey, answerSoundDefault));
    answerSoundChoice.onChange = [this] (int value)
    {
        properties.setValue (answerSoundKey, value);
        properties.saveIfNeeded();
    };
    addAndMakeVisible (answerSoundChoice);

    // ---- Live and the account ------------------------------------------
    liveTabChoice.setValue (properties.getBoolValue (liveTabKey, true) ? 1 : 0);
    liveTabChoice.onChange = [this] (int value)
    {
        properties.setValue (liveTabKey, value == 1);
        properties.saveIfNeeded();

        if (onLiveTabChanged != nullptr)
            onLiveTabChanged (value == 1);
    };
    addAndMakeVisible (liveTabChoice);

    accountButton.onClick = [this]
    {
        if (accountSignedIn)
        {
            if (onSignOut != nullptr)
                onSignOut();
        }
        else if (onSignIn != nullptr)
        {
            onSignIn();
        }
    };
    addAndMakeVisible (accountButton);

    syncChoice.setValue (1);
    syncChoice.onChange = [this] (int value)
    {
        if (onSyncChanged != nullptr)
            onSyncChanged (value == 1);
    };
    addAndMakeVisible (syncChoice);

    syncNowButton.onClick = [this]
    {
        if (onSyncNow != nullptr)
            onSyncNow();
    };
    addAndMakeVisible (syncNowButton);

    // ---- updates and the audio device ----------------------------------
    autoUpdateChoice.setValue (properties.getBoolValue (autoUpdateKey, true) ? 1 : 0);
    autoUpdateChoice.onChange = [this] (int value)
    {
        properties.setValue (autoUpdateKey, value == 1);
        properties.saveIfNeeded();
    };
    addAndMakeVisible (autoUpdateChoice);

    checkNowButton.onClick = [this]
    {
        if (onCheckForUpdates != nullptr)
            onCheckForUpdates();
    };
    addAndMakeVisible (checkNowButton);

    audioDeviceButton.onClick = [this]
    {
        if (onAudioDevice != nullptr)
            onAudioDevice();
    };
    addAndMakeVisible (audioDeviceButton);

    // ---- appearance ----------------------------------------------------
    //
    // 0.8 to 1.4: below 0.8 the layout can no longer hold the text it was
    // designed around, and above 1.4 two-line labels start colliding.
    textScaleSlider.setRange (0.8, 1.4, 0.05);
    textScaleSlider.setValue (properties.getDoubleValue (textScaleKey, 1.0), juce::dontSendNotification);
    textScaleSlider.setNumDecimalPlacesToDisplay (2);
    textScaleSlider.onValueChange = [this]
    {
        properties.setValue (textScaleKey, textScaleSlider.getValue());
        properties.saveIfNeeded();
        AbcTrainLookAndFeel::setTextScale ((float) textScaleSlider.getValue());

        if (onSettingsChanged != nullptr)
            onSettingsChanged();
    };
    addAndMakeVisible (textScaleSlider);

    {
        const auto names = AbcTrainLookAndFeel::availableTypefaceNames();
        const auto saved = properties.getValue (AbcTrainLookAndFeel::typefaceKey, "System");

        for (int i = 0; i < names.size(); ++i)
            typefaceSelector.addItem (names[i] == "System" ? localisation.getText ("set.typeface.system") : names[i], i + 1);

        typefaceSelector.setSelectedId (juce::jmax (1, names.indexOf (saved) + 1), juce::dontSendNotification);

        typefaceSelector.onChange = [this]
        {
            const auto chosen = AbcTrainLookAndFeel::availableTypefaceNames()[typefaceSelector.getSelectedId() - 1];

            properties.setValue (AbcTrainLookAndFeel::typefaceKey, chosen);
            properties.saveIfNeeded();
            AbcTrainLookAndFeel::setTypefaceName (chosen);

            if (onSettingsChanged != nullptr)
                onSettingsChanged();

            if (auto* top = getTopLevelComponent())
                top->repaint();
        };

        addAndMakeVisible (typefaceSelector);
    }

    {
        // Off first, because "make it stop" is the request somebody
        // arrives here with.
        const int seconds[] { 0, 60, 180, 300, 600 };
        const auto saved = properties.getIntValue (IdleScreensaver::idleSecondsKey,
                                                    IdleScreensaver::defaultIdleSeconds);
        auto selected = 3;

        for (int i = 0; i < 5; ++i)
        {
            screensaverSelector.addItem (seconds[i] == 0 ? localisation.getText ("set.off") : localisation.getText ("set.minutes", { { "n", n (seconds[i] / 60) } }), i + 1);

            if (seconds[i] == saved)
                selected = i + 1;
        }

        screensaverSelector.setSelectedId (selected, juce::dontSendNotification);

        screensaverSelector.onChange = [this]
        {
            const int options[] { 0, 60, 180, 300, 600 };
            properties.setValue (IdleScreensaver::idleSecondsKey,
                                 options[juce::jlimit (0, 4, screensaverSelector.getSelectedId() - 1)]);
            properties.saveIfNeeded();

            if (onSettingsChanged != nullptr)
                onSettingsChanged();
        };

        addAndMakeVisible (screensaverSelector);
    }

    // ---- background ----------------------------------------------------
    scrimSlider.setRange (0.0, 0.9, 0.05);
    scrimSlider.setValue (properties.getDoubleValue (backgroundScrimKey, 0.55), juce::dontSendNotification);
    scrimSlider.setNumDecimalPlacesToDisplay (2);
    scrimSlider.onValueChange = [this]
    {
        properties.setValue (backgroundScrimKey, scrimSlider.getValue());
        properties.saveIfNeeded();
        AbcTrainLookAndFeel::setCustomBackground (AbcTrainLookAndFeel::customBackground(),
                                                   (float) scrimSlider.getValue());

        if (onSettingsChanged != nullptr)
            onSettingsChanged();
    };
    addAndMakeVisible (scrimSlider);

    chooseBackgroundButton.onClick = [this] { chooseBackground(); };
    clearBackgroundButton.onClick = [this] { clearBackground(); };
    backgroundButtons.addAndMakeVisible (chooseBackgroundButton);
    backgroundButtons.addAndMakeVisible (clearBackgroundButton);
    addAndMakeVisible (backgroundButtons);

    // ---- about ---------------------------------------------------------
    licenceView.setMultiLine (true);
    licenceView.setReadOnly (true);
    licenceView.setScrollbarsShown (true);
    licenceView.setCaretVisible (false);
    licenceView.setFont (AbcTrainLookAndFeel::captionFont());
    addAndMakeVisible (licenceView);

    licenceToggle.onClick = [this]
    {
        licenceExpanded = ! licenceExpanded;
        refreshLicenceView();
        resized();
    };
    addAndMakeVisible (licenceToggle);

    feedbackButton.onClick = []
    {
        feedbackUrl (CurrentVersion::string, juce::SystemStats::getOperatingSystemName()).launchInDefaultBrowser();
    };
    addAndMakeVisible (feedbackButton);

    communityButton.setComponentID ("settings.community");
    communityButton.onClick = [] { CommunityLink::open(); };
    addAndMakeVisible (communityButton);

    betaToggle.onClick = [this]
    {
        properties.setValue (UpdateChecker::betaOptInKey,
                             ! properties.getBoolValue (UpdateChecker::betaOptInKey, false));
        properties.saveIfNeeded();
        refreshBetaToggle();
    };
    addAndMakeVisible (betaToggle);

    // Not shown: this is a page reached from a tab, and the way out is
    // another tab. Kept as a child for the key handler and the tests.
    closeButton.onClick = [this]
    {
        setVisible (false);

        if (onClosed != nullptr)
            onClosed();
    };
    addChildComponent (closeButton);

    // ---- search and keys (Set-PC boards) --------------------------------
    searchBox.setComponentID ("settings.search");
    searchBox.setFont (AbcTrainLookAndFeel::labelFont());
    searchBox.onTextChange = [this]
    {
        searchQuery = searchBox.getText().trim();

        // Jump to a page that has what is asked for, if this one has not.
        if (searchQuery.isNotEmpty() && pageCount (currentPage) == 0)
            for (const auto& e : railEntries())
                if (e.page >= 0 && railOffers ((Page) e.page) && pageCount ((Page) e.page) > 0)
                {
                    deskScroll = 0;
                    selectPage ((Page) e.page);
                    break;
                }

        resized();
        repaint();
    };
    searchBox.onEscapeKey = [this] { searchBox.clear(); searchBox.onTextChange(); };
    addAndMakeVisible (searchBox);

    addChildComponent (hotkeysEditor);

    buildRows();
    refresh();
    selectPage (Page::training);
}

SettingsScreenComponent::~SettingsScreenComponent()
{
    if (noisePlaying && onCalibrationNoise != nullptr)
        onCalibrationNoise (false);
}

void SettingsScreenComponent::buildRows()
{
    // Order here is order on the page.
    rows = {
        { "set.answerMode.title",   "set.answerMode.hint",   &answerModeChoice, 0, false, Page::training },
        { "set.haptics.title",      "set.haptics.hint",      &hapticsChoice,  0, false, Page::training },
        { "set.answerPause.title",  "set.answerPause.hint",  &answerPause,    0, true,  Page::training },
        { "set.stepRule.title",     "set.stepRule.hint",     &stepRule,       0, true,  Page::training },
        { "set.hints.title",        "set.hints.hint",        &hints,          0, true,  Page::training },
        { "set.allModes.title",     "set.allModes.hint",     &allModes,       0, true,  Page::training },
        { "set.lives.title",        "set.lives.hint",        &survivalLives,  0, true,  Page::training },
        { "set.blitz.title",        "set.blitz.hint",        &blitzSeconds,   0, true,  Page::training },
        { "set.penalty.title",      "set.penalty.hint",      &blitzPenalty,   0, true,  Page::training },

        { "set.audioDevice.title",  "set.audioDevice.hint",  &audioDeviceButton, standardControlWidth, false, Page::hearing },
        { "set.hearingProfile.title", "set.hearingProfile.hintNone", &profileRow, 0, false, Page::hearing },
        { "set.hearingApply.title", "set.hearingApply.hint",  &applyChoice,    0, false, Page::hearing },
        { "set.uiSounds.title",     "set.uiSounds.hint",     &uiSoundsChoice, 0, false, Page::hearing },
        { "set.answerSound.title",  "set.answerSound.hint",  &answerSoundChoice, 0, false, Page::hearing },
        { "set.hearingOn.title",    "set.hearingOn.hint",    &hearingOn,      0, false, Page::hearing },
        { "set.break.title",        "set.break.hint",        &breakMinutes,   0, true,  Page::hearing },
        { "set.fatigue.title",      "set.fatigue.hint",      &fatigueHint,    0, true,  Page::hearing },
        { "set.calib.title",        "set.calib.hint",        &calibrationRow, 0, false, Page::hearing },
        { "set.weekly.title",       "set.weekly.hint",       &weeklyLimit,    0, true,  Page::hearing },
        { "set.exposure.title",     "set.exposure.hint",     &exposureRow,    0, true,  Page::hearing },

        { "set.loudness.title",     "set.loudness.hint",     &loudnessMatchChoice, 0, false, Page::training },
        { "set.daily.title",        "set.daily.hint",        &dailyMinutesChoice, 0, false, Page::training },
        { "set.theme.title",        "set.theme.hint",        &themeChoice,         0,   false, Page::appearance },
        { "set.language.title",     "set.language.hint",     &languageChoice,      standardControlWidth, false, Page::appearance },
        { "ui.textSize",            "set.textSize.hint",     &textScaleSlider,     standardControlWidth,   false, Page::appearance },
        { "set.typeface.title",     "set.typeface.hint",     &typefaceSelector,    standardControlWidth, false, Page::appearance },
        { "set.screensaver.title",  "set.screensaver.hint",  &screensaverSelector, standardControlWidth, false, Page::appearance },

        { "ui.backgroundImage",     "set.background.hint",   &backgroundButtons, 0, false, Page::appearance },
        { "ui.backgroundDim",       "set.backgroundDim.hint", &scrimSlider,      standardControlWidth, false, Page::appearance },
        { "set.onTop.title",        "set.onTop.hint",        &onTopChoice,      0, false, Page::appearance },

        { "set.liveTab.title",      "set.liveTab.hint",      &liveTabChoice,    0,   false, Page::live },
        { "set.account.title",      "set.account.hint",      &accountButton,    standardControlWidth, false, Page::live },
        { "set.sync.title",         "set.sync.hint",         &syncChoice,       0,   false, Page::live },
        { "set.syncNow.title",      "set.syncNow.hint",      &syncNowButton,    standardControlWidth, false, Page::live },

        { "set.autoUpdate.title",   "set.autoUpdate.hint",   &autoUpdateChoice, 0,   false, Page::about },
        { "set.checkNow.title",     "set.checkNow.hint",     &checkNowButton,   standardControlWidth, false, Page::about },
    };

    // The framed groups (Set-PC board). A row's group is decided by what
    // it is about; rows of one group are consecutive on their page.
    const std::pair<juce::Component*, const char*> groups[] {
        { &answerModeChoice, "set.group.answer" }, { &hapticsChoice, "set.group.answer" }, { &answerPause, "set.group.answer" },
        { &stepRule, "set.group.difficulty" }, { &hints, "set.group.difficulty" },
        { &allModes, "set.group.runs" }, { &survivalLives, "set.group.runs" },
        { &blitzSeconds, "set.group.runs" }, { &blitzPenalty, "set.group.runs" },
        { &loudnessMatchChoice, "set.group.sound" }, { &dailyMinutesChoice, "set.group.daily" },
        { &audioDeviceButton, "set.group.output" }, { &profileRow, "set.group.output" }, { &applyChoice, "set.group.output" },
        { &uiSoundsChoice, "set.group.feedback" }, { &answerSoundChoice, "set.group.feedback" },
        { &hearingOn, "set.group.protection" }, { &breakMinutes, "set.group.protection" }, { &fatigueHint, "set.group.protection" },
        { &calibrationRow, "set.group.protection" }, { &weeklyLimit, "set.group.protection" }, { &exposureRow, "set.group.protection" },
        { &themeChoice, "set.group.look" }, { &languageChoice, "set.group.look" }, { &textScaleSlider, "set.group.look" },
        { &typefaceSelector, "set.group.look" }, { &screensaverSelector, "set.group.look" },
        { &backgroundButtons, "set.group.background" }, { &scrimSlider, "set.group.background" },
        { &onTopChoice, "set.group.window" },
        { &liveTabChoice, "set.group.live" }, { &accountButton, "set.group.account" }, { &syncChoice, "set.group.account" },
        { &syncNowButton, "set.group.account" },
        { &autoUpdateChoice, "set.group.updates" }, { &checkNowButton, "set.group.updates" },
    };

    for (auto& row : rows)
        for (const auto& [control, key] : groups)
            if (row.control == control)
                row.groupKey = key;
}

const std::vector<SettingsScreenComponent::RailEntry>& SettingsScreenComponent::railEntries()
{
    static const std::vector<RailEntry> entries {
        { "set.rail.app", -1 },
        { nullptr, (int) Page::training }, { nullptr, (int) Page::hearing }, { nullptr, (int) Page::appearance },
        { nullptr, (int) Page::keys },
        { "set.rail.online", -1 },
        { nullptr, (int) Page::live },
        { "set.rail.system", -1 },
        { nullptr, (int) Page::about },
    };
    return entries;
}

bool SettingsScreenComponent::railOffers (Page page) const
{
    // Keys need a keyboard: not on a phone.
    return page != Page::keys || ! compact();
}

juce::String SettingsScreenComponent::pageLabel (Page page) const
{
    switch (page)
    {
        case Page::training:   return localisation.getText ("set.page.exercises");
        case Page::hearing:    return localisation.getText ("set.page.sound");
        case Page::appearance: return localisation.getText ("ui.settingsAppearance");
        case Page::background: return localisation.getText ("ui.settingsBackground");
        case Page::live:       return localisation.getText ("set.page.live");
        case Page::keys:       return localisation.getText ("set.page.keys");
        case Page::about:
        default:               return localisation.getText ("ui.about");
    }
}

bool SettingsScreenComponent::rowMatches (const Row& row) const
{
    return searchQuery.isNotEmpty()
        && (localisation.getText (row.titleKey).containsIgnoreCase (searchQuery)
            || hintFor (row).containsIgnoreCase (searchQuery));
}

int SettingsScreenComponent::pageCount (Page page) const
{
    int count = 0;

    if (page == Page::keys)
    {
        for (const auto& a : Hotkeys::actions())
            count += searchQuery.isEmpty() || localisation.getText (a.titleKey).containsIgnoreCase (searchQuery) ? 1 : 0;
        return count;
    }

    for (const auto& row : rows)
        if (row.page == page && rowIsOffered (row))
            count += searchQuery.isEmpty() || rowMatches (row) ? 1 : 0;

    return count;
}

int SettingsScreenComponent::footerHeight() const
{
    return compact() ? 0 : 30;
}

void SettingsScreenComponent::refreshHotkeysStrings()
{
    HotkeysEditor::Strings k;
    const auto t = [this] (const char* key) { return localisation.getText (key); };
    k.all = t ("keys.all");
    k.action = t ("keys.col.action");
    k.key = t ("keys.col.key");
    k.second = t ("keys.col.second");
    k.where = t ("keys.col.where");
    k.defaults = t ("keys.defaults");
    k.clashes = t ("keys.clashes");
    k.noClashes = t ("keys.noClashes");
    k.press = t ("keys.press");
    k.note = t ("keys.note");
    k.text = [this] (const char* key) { return localisation.getText (key); };
    hotkeysEditor.setStrings (std::move (k));
}

void SettingsScreenComponent::syncControlsFromSettings()
{
    const auto pro = settings.getMode() == TrainerSettings::Mode::pro;
    modeSwitch.setToggleState (pro, juce::dontSendNotification);
    modeChoice.setValue (pro ? 1 : 0);

    // In Beginner the Pro rows show what is actually in force - the
    // defaults - not the Pro values waiting in the file.
    const auto show = [this, pro] (SegmentedChoice& c, Id id)
    {
        c.setValue (pro ? settings.stored (id) : settings.get (id));
        c.setEnabled (pro || TrainerSettings::spec (id).beginner);
    };

    show (stepRule, Id::stepRule);
    show (answerPause, Id::answerPause);
    show (survivalLives, Id::survivalLives);
    show (blitzSeconds, Id::blitzSeconds);
    show (blitzPenalty, Id::blitzPenalty);
    show (hints, Id::hints);
    show (allModes, Id::allModesOpen);
    show (hearingOn, Id::hearingOn);
    show (breakMinutes, Id::breakReminderMinutes);
    show (fatigueHint, Id::fatigueHint);
    show (weeklyLimit, Id::weeklyLimit);

    resetButton.setVisible (currentPage == Page::training && pro);

    // Hearing switched off greys everything under it: those rows describe
    // a feature that is not running.
    const auto hearing = settings.get (Id::hearingOn) == 1;

    for (auto* c : { (juce::Component*) &breakMinutes, (juce::Component*) &fatigueHint,
                     (juce::Component*) &weeklyLimit, (juce::Component*) &exposureRow })
        c->setEnabled (hearing && pro);

    calibrationRow.setEnabled (hearing);

    const auto calibrated = settings.stored (Id::calibrationDb);

    if (calibrated > 0)
        calibrationSlider.setValue (calibrated, juce::dontSendNotification);

    calibrationClearButton.setEnabled (calibrated > 0);

    refresh();
}

bool SettingsScreenComponent::rowIsOffered (const Row& row) const
{
    // No audio device to choose where there is no standalone holder (the
    // snapshot tools): the row is left out rather than shown dead.
    if (row.control == &syncChoice || row.control == &syncNowButton)
        return accountSignedIn;   // sync is a question only a signed-in computer has

    if (row.control == &hapticsChoice)
    {
       #if JUCE_IOS || JUCE_ANDROID
        return true;
       #else
        return std::getenv ("SNAP_SAFE") != nullptr;   // the snapshot tool's phone
       #endif
    }

    return row.control != &audioDeviceButton || onAudioDevice != nullptr;
}

juce::String SettingsScreenComponent::lockedHint() const
{
    return localisation.getText ("set.lockedHint");
}

void SettingsScreenComponent::applyDefaultMarks()
{
    // The standard value of every choice, as a dot in its cell.
    const std::pair<SegmentedChoice*, Id> trainer[] {
        { &stepRule, Id::stepRule }, { &answerPause, Id::answerPause }, { &survivalLives, Id::survivalLives },
        { &blitzSeconds, Id::blitzSeconds }, { &blitzPenalty, Id::blitzPenalty }, { &hints, Id::hints },
        { &allModes, Id::allModesOpen }, { &hearingOn, Id::hearingOn }, { &breakMinutes, Id::breakReminderMinutes },
        { &fatigueHint, Id::fatigueHint }, { &weeklyLimit, Id::weeklyLimit },
    };
    for (const auto& [c, id] : trainer)
        c->setDefaultValue (TrainerSettings::spec (id).defaultValue);

    loudnessMatchChoice.setDefaultValue (0);
    dailyMinutesChoice.setDefaultValue (15);
    uiSoundsChoice.setDefaultValue (2);
    answerSoundChoice.setDefaultValue (answerSoundDefault);
    hapticsChoice.setDefaultValue (hapticsDefault);
    answerModeChoice.setDefaultValue (0);
    onTopChoice.setDefaultValue (0);
    liveTabChoice.setDefaultValue (1);
    autoUpdateChoice.setDefaultValue (1);
}

juce::String SettingsScreenComponent::hintFor (const Row& row) const
{
    // A Pro rule while in Beginner: say so, instead of a greyed row that
    // just does not respond (the author: "чтобы людей не запутать").
    if (row.proOnly && settings.getMode() == TrainerSettings::Mode::beginner && ! row.control->isEnabled())
        return lockedHint();

    // A few hints say what the *current* value does, which is the whole
    // point of having them.
    if (row.control == &stepRule)
        return localisation.getText ("set.stepRule.hint" + n (stepRule.getValue()));

    if (row.control == &checkNowButton && updateStatusText.isNotEmpty())
        return updateStatusText;

    if (row.control == &accountButton && accountSignedIn)
        return localisation.getText ("set.account.signedInHint").replace ("{{nick}}", accountNick.isNotEmpty() ? accountNick : juce::String ("-"));

    if (row.control == &syncNowButton && syncStatusText.isNotEmpty())
        return syncStatusText;

    if (row.control == &profileRow && hearingProfiles != nullptr)
        if (const auto* active = hearingProfiles->getActive())
            return localisation.getText ("set.hearingProfile.hintSet",
                                         { { "pair", active->pairName() },
                                           { "date", active->created.formatted ("%d.%m.%Y") } });

    if (row.control == &calibrationRow)
    {
        const auto db = settings.stored (Id::calibrationDb);
        return db > 0 ? localisation.getText ("set.calib.hintSet", { { "db", n (db) } })
                      : localisation.getText ("set.calib.hintNone");
    }

    return localisation.getText (row.hintKey);
}

void SettingsScreenComponent::refresh()
{
    const auto& theme = AbcTrainTheme::current();
    const auto t = [this] (const char* key) { return localisation.getText (key); };
    const auto minutes = [this] (int m) { return localisation.getText ("set.minutes", { { "n", n (m) } }); };
    const auto secs = [this] (int s) { return localisation.getText ("set.seconds", { { "n", n (s) } }); };

    modeSwitch.setButtonText (t ("set.mode.switch"));
    modeChoice.setOptions ({ 0, 1 }, { t ("set.mode.beginner"), t ("set.mode.pro") });
    answerModeChoice.setOptions ({ 0, 1 }, { t ("set.answerMode.release"), t ("set.answerMode.button") });
    onTopChoice.setOptions ({ 0, 1 }, { t ("set.off"), t ("set.on") });
    applyDefaultMarks();

    stepRule.setOptions ({ 2, 3, 4 }, { "2", "3", "4" });
    answerPause.setOptions ({ 0, 1, 2 }, { t ("set.pause.short"), t ("set.pause.normal"), t ("set.pause.long") });
    survivalLives.setOptions ({ 1, 3, 5 }, { "1", "3", "5" });
    blitzSeconds.setOptions ({ 60, 90, 120, 180 }, { secs (60), secs (90), secs (120), secs (180) });
    blitzPenalty.setOptions ({ 0, 5, 10 }, { t ("set.none"), secs (5), secs (10) });
    hints.setOptions ({ 0, 1 }, { t ("set.off"), t ("set.on") });
    allModes.setOptions ({ 0, 1 }, { t ("set.allModes.streak"), t ("set.allModes.open") });

    hearingOn.setOptions ({ 0, 1 }, { t ("set.off"), t ("set.on") });
    breakMinutes.setOptions ({ 0, 30, 45, 60, 90 }, { t ("set.off"), minutes (30), minutes (45), minutes (60), minutes (90) });
    fatigueHint.setOptions ({ 0, 1 }, { t ("set.off"), t ("set.on") });
    const auto dba = [this] (int v) { return localisation.getText ("set.dba", { { "n", n (v) } }); };
    const auto hours = [this] (int v) { return localisation.getText ("set.hours", { { "n", n (v) } }); };

    weeklyLimit.setOptions ({ 0, 1 }, { dba (80), dba (75) });
    exposureHours.setOptions ({ 1, 2, 4, 8 }, { hours (1), hours (2), hours (4), hours (8) });
    exposureLevel.setOptions ({ 85, 90, 95, 100 }, { dba (85), dba (90), dba (95), dba (100) });

    resetButton.setButtonText (t ("set.reset"));
    calibrationNoiseButton.setButtonText (noisePlaying ? t ("set.calib.stop") : t ("set.calib.play"));
    AbcTrainLookAndFeel::makePrimary (calibrationNoiseButton, noisePlaying);
    calibrationSaveButton.setButtonText (t ("set.calib.save"));
    calibrationClearButton.setButtonText (t ("set.calib.clear"));
    exposureAddButton.setButtonText (t ("set.exposure.add"));

    chooseBackgroundButton.setButtonText (t ("ui.chooseImage"));
    clearBackgroundButton.setButtonText (t ("ui.clearImage"));
    feedbackButton.setButtonText (t ("ui.feedback"));
    communityButton.setButtonText (t ("community.button"));
    refreshBetaToggle();

    themeChoice.setOptions ({ 0, 1 }, { t ("set.theme.light"), t ("set.theme.dark") });
    themeChoice.setValue (AbcTrainTheme::getMode() == AbcTrainTheme::Mode::dark ? 1 : 0);
    languageChoice.setSelectedId (LocalisationManager::getSupportedLanguageCodes().indexOf (localisation.getCurrentLanguage()) + 1,
                                  juce::dontSendNotification);
    autoUpdateChoice.setOptions ({ 0, 1 }, { t ("set.autoUpdate.manual"), t ("set.autoUpdate.auto") });
    checkNowButton.setButtonText (t ("set.checkNow.button"));
    liveTabChoice.setOptions ({ 0, 1 }, { t ("set.off"), t ("set.on") });
    loudnessMatchChoice.setOptions ({ 0, 1 }, { "RMS", "BS.1770" });
    loudnessMatchChoice.setValue (properties.getIntValue (loudnessMatchKey, 0));
    dailyMinutesChoice.setOptions ({ 10, 15, 20, 30 }, { t ("set.daily.n").replace ("{{n}}", "10"), t ("set.daily.n").replace ("{{n}}", "15"),
                                                         t ("set.daily.n").replace ("{{n}}", "20"), t ("set.daily.n").replace ("{{n}}", "30") });
    dailyMinutesChoice.setValue (properties.getIntValue (dailyMinutesKey, 15));
    answerSoundChoice.setOptions ({ 0, 1, 2 }, { t ("set.off"), t ("set.answerSound.wrong"), t ("set.answerSound.both") });
    answerSoundChoice.setValue (properties.getIntValue (answerSoundKey, answerSoundDefault));
    uiSoundsChoice.setOptions ({ 0, 1, 2 }, { t ("set.off"), t ("set.uiSounds.quiet"), t ("set.uiSounds.normal") });
    uiSoundsChoice.setValue (properties.getIntValue (uiSoundsKey, 2));
    hapticsChoice.setOptions ({ 0, 1, 2 }, { t ("set.off"), t ("set.haptics.light"), t ("set.haptics.strong") });
    hapticsChoice.setValue (properties.getIntValue (hapticsKey, hapticsDefault));
    accountButton.setButtonText (t (accountSignedIn ? "set.account.signOut" : "set.account.signIn"));
    syncChoice.setOptions ({ 0, 1 }, { t ("set.off"), t ("set.on") });
    syncNowButton.setButtonText (t ("set.syncNow.button"));
    audioDeviceButton.setButtonText (t ("set.audioDevice.button"));
    profileTestButton.setButtonText (t ("set.hearingProfile.test"));
    applyChoice.setOptions ({ 0, 1 }, { t ("set.off"), t ("set.on") });
    refreshHearingProfileRow();
    closeButton.setButtonText (t ("ui.close"));
    searchBox.setTextToShowWhenEmpty (juce::String::fromUTF8 ("\xe2\x8c\x95  ") + t ("set.search"), theme.textDim);
    searchBox.setColour (juce::TextEditor::backgroundColourId, theme.displayBackground);
    searchBox.setColour (juce::TextEditor::outlineColourId, theme.outline);
    searchBox.setColour (juce::TextEditor::textColourId, theme.textBright);
    refreshHotkeysStrings();

    // A Slider's text box keeps the colours it was built with; the light
    // theme drew white numbers on a white box.
    for (auto* slider : { &calibrationSlider, &textScaleSlider, &scrimSlider })
    {
        slider->setColour (juce::Slider::textBoxTextColourId, theme.textBright);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, theme.displayBackground);
        slider->setColour (juce::Slider::textBoxOutlineColourId, theme.outline);
    }

    licenceView.setColour (juce::TextEditor::textColourId, theme.text);
    refreshLicenceView();

    const auto hasImage = AbcTrainLookAndFeel::customBackground().isValid();
    scrimSlider.setEnabled (hasImage);
    clearBackgroundButton.setEnabled (hasImage);

    if (hearingStatus != nullptr)
        hearingStatusText = hearingStatus();

    resized();
    repaint();
}

void SettingsScreenComponent::setHearingProfiles (HearingProfileStore* storeToUse)
{
    hearingProfiles = storeToUse;
    refresh();
}

void SettingsScreenComponent::refreshHearingProfileRow()
{
    profileSelector.clearItems();
    profileSelector.addItem (localisation.getText ("set.hearingProfile.none"), 1);
    auto selected = 1;

    if (hearingProfiles != nullptr)
    {
        const auto* active = hearingProfiles->getActive();
        const auto& all = hearingProfiles->getProfiles();

        for (size_t i = 0; i < all.size(); ++i)
        {
            profileSelector.addItem (all[i].pairName(), (int) i + 2);

            if (active != nullptr && active->id == all[i].id)
                selected = (int) i + 2;
        }

        applyChoice.setValue (hearingProfiles->isApplying() ? 1 : 0);
    }

    profileSelector.setSelectedId (selected, juce::dontSendNotification);
    // The switch means nothing without a pair to apply.
    applyChoice.setEnabled (selected > 1);
    repaint();
}

void SettingsScreenComponent::refreshLicenceView()
{
    auto text = licenceText (licenceExpanded);

    if (soundCredits != nullptr)
        if (const auto credits = soundCredits(); credits.isNotEmpty())
            text << "\n\n" << credits;

    licenceView.setText (text);
    // A TextEditor stamps its colour onto text as it is inserted, so text
    // set under the dark theme stayed near-white after switching to light.
    licenceView.applyColourToAllText (licenceView.findColour (juce::TextEditor::textColourId));
    licenceView.moveCaretToTop (false);
    licenceToggle.setButtonText (localisation.getText (licenceExpanded ? "ui.licenceLess" : "ui.licenceMore"));
}

void SettingsScreenComponent::setUpdateStatus (const juce::String& text)
{
    updateStatusText = text;
    checkNowButton.setEnabled (text != localisation.getText ("ui.checkingForUpdates"));
    repaint();
}

void SettingsScreenComponent::refreshBetaToggle()
{
    betaToggle.setButtonText (localisation.getText (properties.getBoolValue (UpdateChecker::betaOptInKey, false)
                                                        ? "ui.betaOn" : "ui.betaOff"));
}

juce::URL SettingsScreenComponent::feedbackUrl (const juce::String& version, const juce::String& system)
{
    const auto body = juce::String ("**Version:** ") + version + "\n"
                    + "**System:** " + system + "\n\n"
                    + "**What happened, or what would you suggest? / Что случилось или что предложить?**\n\n";

    return juce::URL ("https://github.com/bogggare567/abcTrain/issues/new")
               .withParameter ("labels", "feedback")
               .withParameter ("body", body);
}

juce::String SettingsScreenComponent::licenceText (bool full)
{
    const auto whole = juce::String::fromUTF8 (BrandBinaryData::LICENSE, BrandBinaryData::LICENSESize);

    if (full)
        return whole;

    // From the top to the second heading, quoted verbatim; if the headings
    // ever move, the whole thing rather than a confident wrong excerpt.
    const auto from = whole.indexOf ("WHAT YOU MAY DO");
    const auto to   = whole.indexOf ("WHAT YOU MAY NOT DO");

    if (from < 0 || to <= from)
        return whole;

    return whole.substring (0, to).trimEnd();
}

void SettingsScreenComponent::applyStoredTypeface (juce::PropertiesFile& file)
{
    AbcTrainLookAndFeel::setTypefaceName (file.getValue (AbcTrainLookAndFeel::typefaceKey, "System"));
}

void SettingsScreenComponent::applyStoredBackground (juce::PropertiesFile& file)
{
    const juce::File image (file.getValue (backgroundPathKey));

    if (! image.existsAsFile())
    {
        AbcTrainLookAndFeel::setCustomBackground ({}, 0.55f);
        return;
    }

    AbcTrainLookAndFeel::setCustomBackground (juce::ImageFileFormat::loadFrom (image),
                                               (float) file.getDoubleValue (backgroundScrimKey, 0.55));
}

void SettingsScreenComponent::chooseBackground()
{
    fileChooser = std::make_unique<juce::FileChooser> (localisation.getText ("ui.chooseImage"),
                                                        juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                                        "*.png;*.jpg;*.jpeg");

    juce::Component::SafePointer<SettingsScreenComponent> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto file = chooser.getResult();
        const auto image = file.existsAsFile() ? juce::ImageFileFormat::loadFrom (file) : juce::Image();

        if (! image.isValid())
            return;

        safeThis->properties.setValue (backgroundPathKey, file.getFullPathName());
        safeThis->properties.saveIfNeeded();
        AbcTrainLookAndFeel::setCustomBackground (image, (float) safeThis->scrimSlider.getValue());
        safeThis->refresh();

        if (safeThis->onSettingsChanged != nullptr)
            safeThis->onSettingsChanged();
    });
}

void SettingsScreenComponent::clearBackground()
{
    properties.setValue (backgroundPathKey, juce::String());
    properties.saveIfNeeded();
    AbcTrainLookAndFeel::setCustomBackground ({}, (float) scrimSlider.getValue());
    refresh();

    if (onSettingsChanged != nullptr)
        onSettingsChanged();
}

void SettingsScreenComponent::timerCallback()
{
    if (hearingStatus == nullptr)
        return;

    const auto text = hearingStatus();

    if (text != hearingStatusText)
    {
        hearingStatusText = text;
        repaint();
    }
}

void SettingsScreenComponent::visibilityChanged()
{
    if (isVisible() && currentPage == Page::hearing)
    {
        startTimer (1000);
    }
    else
    {
        stopTimer();

        // Never leave the noise running behind a page nobody is looking at.
        if (! isVisible() && noisePlaying)
        {
            noisePlaying = false;

            if (onCalibrationNoise != nullptr)
                onCalibrationNoise (false);

            refresh();
        }
    }
}

// ---- layout -----------------------------------------------------------

juce::Rectangle<int> SettingsScreenComponent::sideMenuBounds() const
{
    if (compact())
        return compactShowingPage ? juce::Rectangle<int>() : getLocalBounds();

    return getLocalBounds().removeFromLeft (railWidth);
}

namespace
{
    constexpr int modeCaption = 18, modeNote = 36;
}

juce::Rectangle<int> SettingsScreenComponent::modeSwitchBounds() const
{
    // On a computer: at the top of the rail, under the search - "Beginner |
    // Pro" decides what every page below means, so it comes first (it was
    // a switch at the foot that people did not find). On a phone: at the
    // foot of the menu, as before.
    auto rail = sideMenuBounds().reduced (AbcTrainTheme::Spacing::medium, AbcTrainTheme::Spacing::large);

    if (compact())
    {
        rail.removeFromBottom (modeNote + 4);
        return rail.removeFromBottom (controlHeight);
    }

    rail.removeFromTop (8 + controlHeight + AbcTrainTheme::Spacing::medium + modeCaption);
    return rail.removeFromTop (controlHeight);
}

int SettingsScreenComponent::menuTop() const
{
    // On a computer the search and the mode sit over the menu.
    return sideMenuBounds().getY() + AbcTrainTheme::Spacing::large + 8
         + (compact() ? 0 : controlHeight + AbcTrainTheme::Spacing::medium + modeCaption + controlHeight + 4 + modeNote
                            + AbcTrainTheme::Spacing::small);
}

juce::Rectangle<int> SettingsScreenComponent::pageBounds() const
{
    if (compact())
    {
        if (! compactShowingPage)
            return {};

        // Under the back button; the heading and rows scroll beneath it.
        return getLocalBounds().reduced (AbcTrainTheme::Spacing::large, AbcTrainTheme::Spacing::small)
                               .withTrimmedTop (compactControlHeight + AbcTrainTheme::Spacing::small)
                               .translated (0, -compactScroll);
    }

    return getLocalBounds().withTrimmedLeft (railWidth).withTrimmedBottom (footerHeight())
                           .reduced (AbcTrainTheme::Spacing::large + 8, AbcTrainTheme::Spacing::large)
                           .translated (0, currentPage == Page::keys ? 0 : -deskScroll);
}

int SettingsScreenComponent::menuRowAt (juce::Point<int> p) const
{
    auto area = sideMenuBounds().withTrimmedTop (menuTop() - sideMenuBounds().getY())
                                .reduced (AbcTrainTheme::Spacing::small, 0);

    for (const auto& e : railEntries())
    {
        if (e.page < 0)
        {
            area.removeFromTop (compact() ? 34 : 28);
            continue;
        }

        if (! railOffers ((Page) e.page))
            continue;

        if (area.removeFromTop (compact() ? 48 : menuRowHeight).contains (p))
            return e.page;

        area.removeFromTop (2);
    }

    return -1;
}

juce::Rectangle<int> SettingsScreenComponent::compactViewport() const
{
    return getLocalBounds().withTrimmedTop (AbcTrainTheme::Spacing::small + compactControlHeight
                                            + AbcTrainTheme::Spacing::small);
}

void SettingsScreenComponent::selectPage (Page page)
{
    if (page != currentPage)
        deskScroll = 0;

    currentPage = page;
    applyPageVisibility();
    syncControlsFromSettings();
    visibilityChanged();
}

void SettingsScreenComponent::applyPageVisibility()
{
    const auto page = currentPage;

    for (auto& row : rows)
        row.control->setVisible (row.page == page && rowIsOffered (row));

    resetButton.setVisible (page == Page::training && settings.getMode() == TrainerSettings::Mode::pro);
    licenceView.setVisible (page == Page::about);
    licenceToggle.setVisible (page == Page::about);
    feedbackButton.setVisible (page == Page::about);
    communityButton.setVisible (page == Page::about);
    betaToggle.setVisible (page == Page::about);
    hotkeysEditor.setVisible (page == Page::keys);
}

void SettingsScreenComponent::resized()
{
    modeChoice.setBounds (modeSwitchBounds());
    modeSwitch.setBounds ({});

    const auto isCompact = compact();
    compactBackButton.setVisible (isCompact && compactShowingPage);
    modeChoice.setVisible (! isCompact || ! compactShowingPage);

    if (isCompact)
    {
        applyPageVisibility();
        compactBackButton.setBounds (getLocalBounds().reduced (AbcTrainTheme::Spacing::small, 0)
                                         .withTrimmedTop (AbcTrainTheme::Spacing::small)
                                         .removeFromTop (compactControlHeight).removeFromLeft (200));
    }

    auto page = pageBounds();
    auto headingRow = page.removeFromTop (30);
    page.removeFromTop (AbcTrainTheme::Spacing::small);

    searchBox.setVisible (! isCompact);
    groupFrames.clear();

    if (! isCompact)
    {
        searchBox.setBounds (sideMenuBounds().reduced (AbcTrainTheme::Spacing::medium, 0)
                                             .withTrimmedTop (AbcTrainTheme::Spacing::large + 8)
                                             .removeFromTop (controlHeight));

        // "Back to the standard rules" on the heading's line, at the right
        // edge, as the board has it.
        if (currentPage == Page::training)
            resetButton.setBounds (headingRow.removeFromRight (240).withSizeKeepingCentre (240, controlHeight));
    }

    if (currentPage == Page::training || currentPage == Page::hearing)
        page.removeFromTop (26);                                // the mode note / status line
    else if (currentPage == Page::about)
        page.removeFromTop (22);                                // the version line

    // A page longer than the window runs on below it and scrolls: laid out
    // in the window's height, the last rows were squeezed to nothing and
    // drawn on top of each other (the hearing and exercise pages).
    if (! isCompact && currentPage != Page::about && currentPage != Page::keys)
        page.setHeight (100000);

    const auto contentWidth = juce::jmin (page.getWidth(), 980);
    constexpr int groupCaption = 26, groupInset = 16;
    juce::String openGroup;

    // A row taken off the page inside its group's frame.
    const auto takeRow = [&] (int height)
    {
        auto full = page.removeFromTop (height).withWidth (contentWidth);

        if (! groupFrames.empty())
            groupFrames.back().bounds = groupFrames.back().bounds.isEmpty() ? full : groupFrames.back().bounds.getUnion (full);

        return full.reduced (groupInset, 0);
    };

    for (auto& row : rows)
    {
        row.bounds = {};

        if (row.page != currentPage || ! rowIsOffered (row))
            continue;

        if (! isCompact && row.groupKey != openGroup)
        {
            if (! groupFrames.empty())
                page.removeFromTop (AbcTrainTheme::Spacing::large);

            page.removeFromTop (groupCaption);
            groupFrames.push_back ({ row.groupKey, {} });
            openGroup = row.groupKey;
        }

        if (isCompact)
        {
            if (row.control == &calibrationRow)
            {
                // Three lines: play, the fader, keep or forget.
                row.bounds = page.removeFromTop (compactTextHeight + 3 * (compactControlHeight + 8) + 8);
                calibrationRow.setBounds (row.bounds.withTrimmedTop (compactTextHeight).withTrimmedBottom (8));
                continue;
            }

            row.bounds = page.removeFromTop (compactTextHeight + compactControlHeight + 14);
            auto box = row.bounds.withTrimmedTop (compactTextHeight).removeFromTop (compactControlHeight);

            if (row.controlWidth > 0)
                box = box.withWidth (juce::jmin (box.getWidth(), juce::jmax (row.controlWidth, 120)));

            if (auto* slider = dynamic_cast<juce::Slider*> (row.control))
                slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, compactControlHeight - 10);

            row.control->setBounds (box);
            continue;
        }

        // Calibration gets a second line and the whole width for its fader:
        // squeezed into the control column it was a thumb's width long,
        // and 50 dB over a thumb's width is a fader that barely moves.
        if (row.control == &calibrationRow)
        {
            row.bounds = takeRow (rowHeight + controlHeight + 22);
            calibrationRow.setBounds (row.bounds.withTrimmedBottom (16));
            continue;
        }

        row.bounds = takeRow (rowHeight);

        auto controlArea = row.bounds.withTrimmedLeft (titleColumn);
        const auto width = row.controlWidth > 0 ? row.controlWidth : controlArea.getWidth();
        const auto box = controlArea.removeFromLeft (width).withSizeKeepingCentre (width, controlHeight)
                                    .withY (row.bounds.getY() + 6);

        if (auto* slider = dynamic_cast<juce::Slider*> (row.control))
            slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, controlHeight - 6);

        if (auto* seg = dynamic_cast<SegmentedChoice*> (row.control))
            seg->setBounds (box.withWidth (juce::jmin (box.getWidth(), juce::jmax (seg->getPreferredWidth(), standardControlWidth))));
        else
            row.control->setBounds (box);
    }

    // Composite rows lay out their own children.
    {
        // Play noise, then − fader +, then save or forget - the order you
        // do it in: play, read the meter, set, keep.
        // "Play noise" on the title line, where the other rows keep their
        // control; the fader and what to do with it on the line under it.
        auto all = calibrationRow.getLocalBounds();

        if (isCompact)
        {
            const auto line = [&all]
            {
                auto r = all.removeFromTop (compactControlHeight);
                all.removeFromTop (8);
                return r;
            };

            calibrationNoiseButton.setBounds (line().removeFromLeft (180));
            auto r = line();
            calibrationDown.setBounds (r.removeFromLeft (r.getHeight()));
            r.removeFromLeft (6);
            calibrationUp.setBounds (r.removeFromRight (r.getHeight()));
            r.removeFromRight (6);
            calibrationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 88, r.getHeight());
            calibrationSlider.setBounds (r);
            auto keep = line();
            calibrationSaveButton.setBounds (keep.removeFromLeft ((keep.getWidth() - 8) / 2));
            keep.removeFromLeft (8);
            calibrationClearButton.setBounds (keep);
        }
        else
        {
        calibrationNoiseButton.setBounds (all.removeFromTop (controlHeight + 6).withTrimmedTop (6)
                                             .removeFromRight (160));
        auto r = all.removeFromBottom (controlHeight);
        calibrationClearButton.setBounds (r.removeFromRight (100));
        r.removeFromRight (8);
        calibrationSaveButton.setBounds (r.removeFromRight (100));
        r.removeFromRight (12);
        calibrationDown.setBounds (r.removeFromLeft (r.getHeight()));
        r.removeFromLeft (6);
        calibrationUp.setBounds (r.removeFromRight (r.getHeight()));
        r.removeFromRight (6);
        calibrationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 88, r.getHeight());
        calibrationSlider.setBounds (r);
        }
    }
    {
        // The pair on the left, the test on the right - the same edge as
        // every other row's button.
        auto r = profileRow.getLocalBounds();
        profileTestButton.setBounds (r.removeFromRight (juce::jmin (180, r.getWidth() * 2 / 5)));
        r.removeFromRight (AbcTrainTheme::Spacing::small);
        profileSelector.setBounds (r.removeFromLeft (juce::jmin (r.getWidth(), standardControlWidth)));
    }
    {
        // Shares of whatever width the row has, not fixed pixels: 170 + 310
        // + 130 was wider than the row on a laptop-sized window, and the
        // level choice drew "85 dB(A)90 dB(A)95..." into itself.
        auto r = exposureRow.getLocalBounds();
        const auto w = (float) juce::jmax (0, r.getWidth() - 16);
        exposureHours.setBounds (r.removeFromLeft ((int) (w * 0.28f)));
        r.removeFromLeft (8);
        exposureLevel.setBounds (r.removeFromLeft ((int) (w * 0.50f)));
        r.removeFromLeft (8);
        exposureAddButton.setBounds (r);
    }
    {
        auto r = backgroundButtons.getLocalBounds();
        chooseBackgroundButton.setBounds (r.removeFromLeft (juce::jmin (200, (r.getWidth() - 8) * 3 / 5)));
        r.removeFromLeft (8);
        clearBackgroundButton.setBounds (r.removeFromLeft (juce::jmin (120, r.getWidth())));
    }

    if (! groupFrames.empty())
        page.removeFromTop (AbcTrainTheme::Spacing::large);

    if (currentPage == Page::keys)
        hotkeysEditor.setBounds (page);

    if (currentPage == Page::training && isCompact)
        resetButton.setBounds (page.removeFromTop (12 + controlHeight).removeFromBottom (controlHeight)
                                   .withX (page.getX() + (isCompact ? 0 : titleColumn))
                                   .withWidth (juce::jmin (240, page.getWidth())));

    if (currentPage == Page::about)
    {
        page.removeFromTop (AbcTrainTheme::Spacing::medium);
        auto body = page.removeFromTop (isCompact ? 260 : juce::jmin (page.getHeight() - 88, 420))
                        .removeFromLeft (juce::jmin (page.getWidth(), 760));
        licenceView.setBounds (body);
        auto row = juce::Rectangle<int> (body.getX(), body.getBottom() + AbcTrainTheme::Spacing::small,
                                         body.getWidth(), controlHeight);
        if (isCompact)
        {
            // One button per line, full width, under the licence.
            for (auto* b : { &licenceToggle, &feedbackButton, &betaToggle, &communityButton })
            {
                b->setBounds (row.withHeight (compactControlHeight));
                row = row.withY (row.getY() + compactControlHeight + AbcTrainTheme::Spacing::small);
            }

            page.setY (row.getY());
        }
        else
        {
        licenceToggle.setBounds (row.removeFromLeft (juce::jmin (240, row.getWidth() / 3)));
        row.removeFromLeft (AbcTrainTheme::Spacing::small);
        feedbackButton.setBounds (row.removeFromLeft (juce::jmin (280, row.getWidth() / 2)));
        row.removeFromLeft (AbcTrainTheme::Spacing::small);
        betaToggle.setBounds (row.removeFromLeft (juce::jmin (220, row.getWidth())));

        // The people around abcTrain: a second row, quieter than the rest.
        auto row2 = juce::Rectangle<int> (body.getX(), row.getBottom() + AbcTrainTheme::Spacing::small,
                                          body.getWidth(), controlHeight);
        communityButton.setBounds (row2.removeFromLeft (juce::jmin (280, row2.getWidth())));
        }
    }

    if (! isCompact)
    {
        // The page scrolls under the wheel when it is longer than the
        // window (the hearing page with its groups is).
        const auto contentBottom = page.getY() + (currentPage == Page::keys ? 0 : deskScroll);
        const auto visibleBottom = getHeight() - footerHeight() - AbcTrainTheme::Spacing::medium;
        deskScrollMax = currentPage == Page::keys ? 0 : juce::jmax (0, contentBottom - visibleBottom);

        if (deskScroll > deskScrollMax)
        {
            deskScroll = deskScrollMax;
            resized();
            return;
        }
    }

    if (isCompact)
    {
        // How far the column runs past the screen, and nothing that has
        // scrolled up under the back button stays clickable there.
        const auto view = compactViewport();
        compactScrollMax = juce::jmax (0, page.getY() + compactScroll - view.getBottom());

        // Clamps only when nothing is moving: mid-fling and mid-stretch
        // the column is allowed past its ends.
        if (scroller.getLimit() != (float) compactScrollMax)
        {
            scroller.setLimit ((float) compactScrollMax);

            if (compactScroll != juce::roundToInt (scroller.getOffset()))
                return;   // setLimit moved it and laid the page out again
        }

        for (auto* child : getChildren())
            if (child != &compactBackButton && child != &modeChoice && child->isVisible()
                && (! compactShowingPage || child->getY() < view.getY()))
                child->setVisible (false);
    }

    closeButton.setBounds (pageBounds().removeFromBottom (32).removeFromRight (100));
}

// ---- painting ---------------------------------------------------------

void SettingsScreenComponent::paintSideMenu (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();

    g.setColour (theme.windowBackground.withAlpha (0.5f));
    g.fillRect (area);
    g.setColour (theme.divider);
    g.fillRect (area.getRight() - 1, area.getY(), 1, area.getHeight());

    // The mode: its caption over it, and a line under it saying what it is
    // doing right now.
    const auto modeBox = modeSwitchBounds();
    const auto pro = settings.getMode() == TrainerSettings::Mode::pro;

    if (! compact())
    {
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::microFont());
        AbcTrainLookAndFeel::fitText (g, localisation.getText ("set.mode.caption").toUpperCase(),
                                      modeBox.translated (0, -modeCaption).withHeight (modeCaption).withTrimmedLeft (2),
                                      juce::Justification::centredLeft, true);
    }

    g.setColour (pro ? theme.accentWarm : theme.textDim);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    AbcTrainLookAndFeel::fitLines (g, localisation.getText (pro ? "set.mode.hintPro" : "set.mode.hintBeginner"),
                      modeBox.translated (0, controlHeight + 4).withHeight (modeNote), juce::Justification::topLeft, 2, 1.0f);

    if (! compact())
    {
        g.setColour (theme.divider);
        g.fillRect (area.getX() + AbcTrainTheme::Spacing::medium, menuTop() - AbcTrainTheme::Spacing::small,
                    area.getWidth() - AbcTrainTheme::Spacing::medium * 2, 1);
    }

    auto rowArea = area.withTrimmedTop (menuTop() - area.getY()).reduced (AbcTrainTheme::Spacing::small, 0);

    for (const auto& e : railEntries())
    {
        if (e.page < 0)
        {
            // A section caption: small, spaced, the accent's colour.
            auto cap = rowArea.removeFromTop (compact() ? 34 : 28);
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::microFont());
            AbcTrainLookAndFeel::fitText (g, localisation.getText (e.captionKey).toUpperCase(),
                                          cap.withTrimmedLeft (14).withTrimmedTop (8), juce::Justification::centredLeft, true);
            continue;
        }

        const auto page = (Page) e.page;

        if (! railOffers (page))
            continue;

        const auto bounds = rowArea.removeFromTop (compact() ? 48 : menuRowHeight);
        rowArea.removeFromTop (2);

        const auto selected = currentPage == page;
        const auto count = pageCount (page);
        const auto empty = searchQuery.isNotEmpty() && count == 0;

        if (selected || hoveredMenuRow == e.page)
        {
            g.setColour (selected ? theme.accent.withAlpha (0.18f) : theme.widgetBackground.withAlpha (0.6f));
            g.fillRect (bounds.toFloat().reduced (2.0f, 0.0f));
        }

        if (selected)
        {
            g.setColour (theme.accent);
            g.fillRect (bounds.toFloat().withWidth (3.0f).reduced (0.0f, 6.0f));
        }

        auto label = bounds.withTrimmedLeft (14).withTrimmedRight (10);

        // How many rows the page has - or, while searching, how many match.
        g.setColour (searchQuery.isNotEmpty() && count > 0 ? theme.accentWarm : theme.textDim);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
        AbcTrainLookAndFeel::fitText (g, juce::String (count), label.removeFromRight (30), juce::Justification::centredRight, false);

        g.setColour (empty ? theme.textDim : selected ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitText (g, pageLabel (page), label, juce::Justification::centredLeft, true);
    }
}

void SettingsScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());
    g.setColour (theme.panelBackground);
    g.fillRect (getLocalBounds());

    if (! sideMenuBounds().isEmpty())
        paintSideMenu (g, sideMenuBounds());

    auto page = pageBounds();

    if (page.isEmpty())
        return;

    juce::Graphics::ScopedSaveState clipToColumn (g);

    if (compact())
        g.reduceClipRegion (compactViewport());

    const auto heading = pageLabel (currentPage);

    {
        // The page's name, and beside it, quieter, what to expect of it.
        auto headingRow = page.removeFromTop (30);
        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::titleFont());
        const auto titleWidth = juce::jmin (headingRow.getWidth(),
                                            juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), heading)) + 4);
        AbcTrainLookAndFeel::fitText (g, heading, headingRow.removeFromLeft (titleWidth), juce::Justification::centredLeft, false);

        if (! compact())
        {
            headingRow.removeFromLeft (16);
            if (currentPage == Page::training && resetButton.isVisible())
                headingRow.removeFromRight (256);
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            const char* intro = currentPage == Page::keys       ? "set.sub.keys"
                              : currentPage == Page::training   ? "set.intro.exercises"
                              : currentPage == Page::hearing    ? "set.intro.sound"
                              : currentPage == Page::appearance ? "set.intro.look"
                              : currentPage == Page::live       ? "set.intro.live"
                                                                : "set.intro.about";
            AbcTrainLookAndFeel::fitText (g, localisation.getText (intro), headingRow, juce::Justification::centredLeft, true);
        }
    }
    page.removeFromTop (AbcTrainTheme::Spacing::small);

    const auto pro = settings.getMode() == TrainerSettings::Mode::pro;

    if (currentPage == Page::training)
    {
        g.setColour (pro ? theme.textDim : theme.accentWarm);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitText (g, localisation.getText (pro ? "set.training.notePro" : "set.training.noteBeginner"),
                    page.removeFromTop (26), juce::Justification::centredLeft, true);
    }
    else if (currentPage == Page::hearing)
    {
        g.setColour (theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitText (g, hearingStatusText, page.removeFromTop (26), juce::Justification::centredLeft, true);
    }
    else if (currentPage == Page::about)
    {
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        const juce::String city (CurrentVersion::codename);
        AbcTrainLookAndFeel::fitText (g, "abcTrain " + juce::String (CurrentVersion::string)
                                           + (city.isNotEmpty() ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + city : juce::String()),
                                      page.removeFromTop (18),
                    juce::Justification::centredLeft, false);
    }

    // The groups: a caption over a frame round its rows.
    for (const auto& frame : groupFrames)
    {
        g.setColour (theme.windowBackground.withAlpha (0.35f));
        g.fillRect (frame.bounds);
        g.setColour (theme.outline);
        g.drawRect (frame.bounds, 1);

        g.setColour (theme.accent);
        g.setFont (AbcTrainLookAndFeel::microFont());
        AbcTrainLookAndFeel::fitText (g, localisation.getText (frame.key).toUpperCase(),
                                      frame.bounds.withHeight (22).translated (0, -24), juce::Justification::centredLeft, true);
    }

    // Rows: title and what it does on the left, hairline between rows.
    for (const auto& row : rows)
    {
        if (row.page != currentPage || row.bounds.isEmpty())
            continue;

        const auto enabled = row.control->isEnabled();
        auto rowBounds = row.bounds;
        auto text = compact() ? rowBounds.removeFromTop (compactTextHeight - 4)
                              : rowBounds.removeFromLeft (titleColumn - 24);

        // A search hit: a warm bar at the row's edge and a warm title.
        const auto hit = rowMatches (row);

        if (hit)
        {
            g.setColour (theme.accentWarm);
            g.fillRect (row.bounds.getX() - (compact() ? 8 : 12), row.bounds.getY() + 8, 3, row.bounds.getHeight() - 16);
        }

        g.setColour (hit ? theme.accentWarm : enabled ? theme.textBright : theme.textDim);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitText (g, localisation.getText (row.titleKey), text.removeFromTop (24).withTrimmedTop (4),
                    juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        AbcTrainLookAndFeel::fitLines (g, hintFor (row), text.withTrimmedBottom (4), juce::Justification::topLeft, 2, 0.9f);

        const auto lastInFrame = std::any_of (groupFrames.begin(), groupFrames.end(),
                                              [&row] (const GroupFrame& f) { return f.bounds.getBottom() == row.bounds.getBottom(); });

        if (! lastInFrame)
        {
            g.setColour (theme.divider);
            g.fillRect (row.bounds.getX(), row.bounds.getBottom() - 1, row.bounds.getWidth(), 1);
        }
    }
}

void SettingsScreenComponent::paintOverChildren (juce::Graphics& g)
{
    if (footerHeight() == 0)
        return;

    // The foot of the page: where the sound goes and where the settings
    // live. Over the children, so a row scrolled under it is covered.
    const auto& theme = AbcTrainTheme::current();
    auto foot = getLocalBounds().withTrimmedLeft (railWidth).removeFromBottom (footerHeight());

    g.setColour (theme.panelBackground);
    g.fillRect (foot);
    g.setColour (theme.divider);
    g.fillRect (foot.removeFromTop (1));

    foot.reduce (AbcTrainTheme::Spacing::large + 8, 0);
    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));

    const auto path = properties.getFile().getFullPathName();
    g.setColour (theme.textDim);
    auto right = foot.removeFromRight (juce::jmin (foot.getWidth() / 2, 520));
    AbcTrainLookAndFeel::fitText (g, path, right, juce::Justification::centredRight, true);

    // What the dot in a choice means.
    {
        g.setFont (AbcTrainLookAndFeel::captionFont());
        const auto legend = localisation.getText ("set.defaultLegend");
        const auto w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), legend)) + 18;
        auto box = foot.removeFromRight (juce::jmin (w, foot.getWidth() / 2));
        foot.removeFromRight (12);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ (float) box.getX() + 4.0f, (float) box.getCentreY() }));
        AbcTrainLookAndFeel::fitText (g, legend, box.withTrimmedLeft (12), juce::Justification::centredLeft, true);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
    }

    if (outputSummary != nullptr)
    {
        g.setColour (theme.text);
        AbcTrainLookAndFeel::fitText (g, outputSummary(), foot, juce::Justification::centredLeft, true);
    }
}

void SettingsScreenComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (compact() || deskScrollMax <= 0)
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    const auto next = juce::jlimit (0, deskScrollMax, deskScroll - juce::roundToInt (wheel.deltaY * 240.0f));

    if (next != deskScroll)
    {
        deskScroll = next;
        resized();
        repaint();
    }
}

// ---- mouse ------------------------------------------------------------

void SettingsScreenComponent::mouseMove (const juce::MouseEvent& event)
{
    const auto found = menuRowAt (event.getPosition());

    if (found != hoveredMenuRow)
    {
        hoveredMenuRow = found;
        repaint();
    }
}

void SettingsScreenComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredMenuRow = -1;
    repaint();
}

void SettingsScreenComponent::mouseUp (const juce::MouseEvent& event)
{
    scroller.mouseUp (event);

    if (! event.mouseWasClicked())
        return;   // the end of a drag-scroll, not a tap

    const auto found = menuRowAt (event.getPosition());

    if (found >= 0)
    {
        if (compact())
        {
            compactShowingPage = true;
            compactScroll = 0;
            scroller.setOffset (0.0f);
        }

        selectPage ((Page) found);
        resized();
        repaint();
    }
}

void SettingsScreenComponent::mouseDown (const juce::MouseEvent& event)
{
    scroller.mouseDown (event);
}

void SettingsScreenComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (! compact() || ! compactShowingPage)
        return;

    scroller.mouseDrag (event);
}

void SettingsScreenComponent::setAccountState (bool signedIn, const juce::String& nick, bool syncOn,
                                               const juce::String& syncStatus)
{
    const auto shapeChanged = signedIn != accountSignedIn;

    accountSignedIn = signedIn;
    accountNick = nick;
    syncStatusText = syncStatus;
    syncChoice.setValue (syncOn ? 1 : 0);
    accountButton.setButtonText (localisation.getText (signedIn ? "set.account.signOut" : "set.account.signIn"));

    // Rows appear or go with signing in; the page has to be laid out again.
    if (shapeChanged)
        resized();

    repaint();
}
