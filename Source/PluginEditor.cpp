#include "PluginEditor.h"
#include "../shared/UpdatePrompt.h"
#include "Achievements.h"
#include "../shared/Version.h"
#include <array>
#include <memory>

namespace
{
    // Every caption the update window shows, localised here because shared
    // code keeps no LocalisationManager of its own - the same shape as
    // UpdatePrompt's strings and ModuleScreenComponent's.
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

namespace
{
    // Every string the update prompt needs, pulled from this editor's own
    // LocalisationManager. The dialogue used to be hardcoded English on a
    // Russian interface, which is exactly the kind of seam that says
    // "this part was bolted on".
    UpdatePrompt::Strings updateStrings (const LocalisationManager& loc)
    {
        UpdatePrompt::Strings s;
        s.title        = loc.getText ("update.title");
        s.body         = loc.getText ("update.body");
        s.offerInstall = loc.getText ("update.offerInstall");
        s.noAsset      = loc.getText ("update.noAsset");
        s.updateNow    = loc.getText ("update.now");
        s.later        = loc.getText ("update.later");
        s.openPage     = loc.getText ("update.openPage");
        s.downloading  = loc.getText ("update.downloading");
        s.opening      = loc.getText ("update.opening");
        s.failed       = loc.getText ("update.failed");
        s.installed    = loc.getText ("update.installed");
        s.devBuild     = loc.getText ("update.devBuild");
        return s;
    }
}

namespace
{
    // The key the light/dark choice is stored under, in the same shared
    // "abcTrain" PropertiesFile the language preference already uses.
    constexpr const char* themeModeKey = "themeMode";
    constexpr const char* uiScaleKey = "uiScale";
    // Not "have you seen the welcome screen" any more - that shows every
    // launch now, by request. This only remembers whether the walkthrough
    // has been offered, which is a question worth asking exactly once.
    constexpr const char* tourOfferedKey = "tourOffered";

    // Maps each game's English getName()/getInstructions() text to its
    // i18n key, so the editor can show a localised name/instructions
    // without the Game interface itself (or any of the 9 game classes)
    // needing to know about LocalisationManager. A game not in this
    // table (e.g. a new one added later, before its i18n keys exist)
    // just falls back to its raw English text - see
    // translateGameName()/translateGameInstructions() below.
    struct GameI18nKeys
    {
        const char* englishName;
        const char* nameKey;
        const char* instructionsKey;
        const char* benefitKey;   // "what this training gives you"
    };

    const std::array<GameI18nKeys, 9> gameI18nKeys {{
        { "Guess the Band",             "game.eq.name",             "game.eq.instructions", "game.eq.benefit" },
        { "Guess the Compression",      "game.compression.name",    "game.compression.instructions", "game.compression.benefit" },
        { "Guess the Reverb",           "game.reverb.name",         "game.reverb.instructions", "game.reverb.benefit" },
        { "Guess the Pan Position",     "game.pan.name",            "game.pan.instructions", "game.pan.benefit" },
        { "Guess the Delay Time",       "game.delay.name",          "game.delay.instructions", "game.delay.benefit" },
        { "Guess the Distortion",       "game.distortion.name",     "game.distortion.instructions", "game.distortion.benefit" },
        { "Guess the Stereo Width",     "game.stereowidth.name",    "game.stereowidth.instructions", "game.stereowidth.benefit" },
        { "Guess the Gain Change",      "game.db.name",             "game.db.instructions", "game.db.benefit" },
        { "Name the Range",             "game.frequencyrange.name", "game.frequencyrange.instructions", "game.frequencyrange.benefit" }
    }};

    // A colour per exercise, so each training reads as its own room -
    // the thing the reference trainers get from a whole different
    // background per game. Grouped by category, so the four families stay
    // legible as families rather than nine unrelated hues.
    juce::Colour tintForGame (const juce::String& englishName)
    {
        using Family = AbcTrainTheme::Family;

        if (englishName == "Guess the Band" || englishName == "Name the Range")
            return AbcTrainTheme::accentFor (Family::frequency);

        if (englishName == "Guess the Compression" || englishName == "Guess the Gain Change")
            return AbcTrainTheme::accentFor (Family::dynamics);

        if (englishName == "Guess the Reverb" || englishName == "Guess the Pan Position"
            || englishName == "Guess the Delay Time" || englishName == "Guess the Stereo Width")
            return AbcTrainTheme::accentFor (Family::space);

        return AbcTrainTheme::accentFor (Family::character);
    }

    // Which skill each exercise builds. Grouping by this rather than by
    // registration order is the whole point of the home screen: nine flat
    // entries are a list, four labelled groups are a map of the subject.
    const char* categoryForGame (const juce::String& englishName)
    {
        if (englishName == "Guess the Band" || englishName == "Name the Range")
            return "home.category.frequency";

        if (englishName == "Guess the Compression" || englishName == "Guess the Gain Change")
            return "home.category.dynamics";

        if (englishName == "Guess the Reverb" || englishName == "Guess the Pan Position"
            || englishName == "Guess the Delay Time" || englishName == "Guess the Stereo Width")
            return "home.category.space";

        return "home.category.character";
    }

    // The A/B button captions. Each Game returns its own English pair (see
    // Game::getBeforeLabel); this maps them to i18n keys, the same shape
    // as translateGameName below - a game whose label isn't listed falls
    // back to its raw English, so adding one is never a crash.
    juce::String translateAbLabel (const juce::String& englishLabel, const LocalisationManager& loc)
    {
        static const std::array<std::pair<const char*, const char*>, 14> table {{
            { "A",            "ab.a" },
            { "B",            "ab.b" },
            { "EQ Off",       "ab.eqOff" },
            { "EQ On",        "ab.eqOn" },
            { "Comp Off",     "ab.compOff" },
            { "Comp On",      "ab.compOn" },
            { "Dry",          "ab.dry" },
            { "With Echo",    "ab.withEcho" },
            { "Clean",        "ab.clean" },
            { "Driven",       "ab.driven" },
            { "Centred",      "ab.centred" },
            { "Panned",       "ab.panned" },
            { "Before Gain",  "ab.beforeGain" },
            { "After Gain",   "ab.afterGain" }
        }};

        for (const auto& entry : table)
            if (englishLabel == entry.first)
                return loc.getText (entry.second);

        if (englishLabel == "Flat")     return loc.getText ("ab.flat");
        if (englishLabel == "Filtered") return loc.getText ("ab.filtered");

        return englishLabel;
    }

    juce::String translateGameName (const juce::String& englishName, const LocalisationManager& loc)
    {
        for (const auto& entry : gameI18nKeys)
            if (englishName == entry.englishName)
                return loc.getText (entry.nameKey);
        return englishName;
    }

    juce::String translateGameInstructions (const juce::String& englishName, const juce::String& englishInstructions, const LocalisationManager& loc)
    {
        for (const auto& entry : gameI18nKeys)
            if (englishName == entry.englishName)
                return loc.getText (entry.instructionsKey);
        return englishInstructions;
    }

    // Empty for a game with no benefit string yet, rather than the raw key
    // - the picker card just leaves that line blank in that case, same
    // graceful-degradation shape as the other two lookups above.
    juce::String translateGameBenefit (const juce::String& englishName, const LocalisationManager& loc)
    {
        for (const auto& entry : gameI18nKeys)
            if (englishName == entry.englishName)
            {
                const auto text = loc.getText (entry.benefitKey);
                return text == entry.benefitKey ? juce::String() : text;
            }
        return {};
    }
}

EarTrainerEditor::EarTrainerEditor (EarTrainerProcessor& p)
    : AudioProcessorEditor (&p),
      localisationProperties (LocalisationManager::makeDefaultOptions()),
      localisation (localisationProperties),
      processor (p),
      trainingSounds (p)
{
    // Restore the persisted light/dark choice *before* the LookAndFeel
    // reads the palette, so the very first paint is already in the right
    // mode rather than flashing dark and correcting itself.
    AbcTrainTheme::setMode (localisationProperties.getValue (themeModeKey, "dark") == "light"
                                ? AbcTrainTheme::Mode::light
                                : AbcTrainTheme::Mode::dark);
    lookAndFeel.refreshFromTheme();

    // Restore the stored appearance before anything paints: a saved text
    // scale or wallpaper that only appeared after visiting Settings would
    // look like it had not been saved at all.
    AbcTrainLookAndFeel::setTextScale ((float) localisationProperties.getDoubleValue (
                                            SettingsScreenComponent::textScaleKey, 1.0));
    SettingsScreenComponent::applyStoredBackground (localisationProperties);
    SettingsScreenComponent::applyStoredTypeface (localisationProperties);

    setLookAndFeel (&lookAndFeel);

    // The title is drawn by paint() with letter-spacing rather than by a
    // Label, since JUCE offers no tracking control on Label - see
    // AbcTrainLookAndFeel::drawTrackedText.
    titleLabel.setFont (AbcTrainLookAndFeel::titleFont());
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setVisible (false);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

    sizeSelector.addItem ("Small", 1, "S");
    sizeSelector.addItem ("Medium", 2, "M");
    sizeSelector.addItem ("Large", 3, "L");
    sizeSelector.addItem ("Extra large", 4, "XL");
    sizeSelector.onChange = [this]
    {
        switch (sizeSelector.getSelectedId())
        {
            case 1:  setUiScale (0.85f); break;
            case 3:  setUiScale (1.15f); break;
            case 4:  setUiScale (1.30f); break;
            default: setUiScale (1.00f); break;
        }
    };
    addAndMakeVisible (sizeSelector);

    for (const auto& code : LocalisationManager::getSupportedLanguageCodes())
        languageSelector.addItem (LocalisationManager::getDisplayName (code),
                                   LocalisationManager::getSupportedLanguageCodes().indexOf (code) + 1,
                                   code.upToFirstOccurrenceOf ("-", false, false).toUpperCase());
    languageSelector.setSelectedId (LocalisationManager::getSupportedLanguageCodes().indexOf (localisation.getCurrentLanguage()) + 1,
                                     juce::dontSendNotification);
    languageSelector.onChange = [this] { languageSelected(); };
    addAndMakeVisible (languageSelector);

    addAndMakeVisible (gameIcon);

    auto& gameManager = processor.getGameManager();

    currentGameLabel.setJustificationType (juce::Justification::centredLeft);
    currentGameLabel.setFont (AbcTrainLookAndFeel::headingFont());
    addAndMakeVisible (currentGameLabel);

    backButton.onClick = [this] { showScreen (Screen::home); };
    addAndMakeVisible (backButton);

    homeScreen.onGameChosen = [this] (int index)
    {
        auto& gm = processor.getGameManager();

        if (index != gm.getActiveGameIndex())
        {
            gm.getActiveGame().removeChangeListener (this);
            gm.setActiveGameIndex (index);
            gm.getActiveGame().addChangeListener (this);
            rebuildChoiceSlider();
        }

        // Picking a training starts it - the home screen's job is to get
        // out of the way, not to make you confirm twice. Through the
        // countdown path, so arriving with Survival or Blitz still armed
        // gets its 3-2-1 rather than an ambush.
        showScreen (Screen::training);
        beginRunWithCountdown();
    };

    homeScreen.onFavouriteToggled = [this] (int index, bool shouldBeFavourite)
    {
        processor.getProgressManager().setFavouriteGame (index, shouldBeFavourite);
        rebuildHomeSections();
    };

    supportScreen.onDismissed = [this]
    {
        localisationProperties.setValue (tourOfferedKey, true);
        localisationProperties.saveIfNeeded();
        showScreen (Screen::home);
    };

    supportScreen.onTourRequested = [this]
    {
        localisationProperties.setValue (tourOfferedKey, true);
        localisationProperties.saveIfNeeded();
        startTour();
    };
    addChildComponent (supportScreen);

    // No Viewport any more: the grid is sized to fit whatever room there
    // is, so there is nothing left to scroll.
    addAndMakeVisible (homeScreen);

    // Left, like every other line on this screen. A centred paragraph in
    // a left-aligned layout reads as a different document.
    instructionLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (instructionLabel);


    restartButton.onClick = [this] { beginRunWithCountdown(); };
    addChildComponent (restartButton);

    // The run HUD lives in the control row; the countdown sits over the
    // answer section (added later than the slider, so it paints above the
    // thing it is covering, but before the results/toast overlays, which
    // must stay on top of everything).
    addChildComponent (runHud);
    addChildComponent (runCountdown);

    // The way out of a live run.
    //
    // Hiding the mode pills during a run was right - a run you can
    // silently re-mode halfway through is a setting, not a run - but it
    // left Home as the only exit, and Home leaves the exercise entirely.
    // Somebody two minutes into a Blitz who just wants to stop is not
    // asking to leave the exercise. Ending the run shows the results,
    // which is also where "again" lives.
    endRunButton.onClick = [this]
    {
        ++pendingAdvanceId;
        runCountdown.cancel();

        const auto score = session.getRunScore();
        session.endRun();
        runStarted = false;

        showRunResults (score);
        refreshRunStatus();
        refreshFromGameState();
        resized();
    };
    addChildComponent (endRunButton);

    beforeButton.onClick = [this] { setPlayProcessed (false); };
    afterButton.onClick = [this] { setPlayProcessed (true); };
    beforeButton.setClickingTogglesState (false);
    afterButton.setClickingTogglesState (false);
    addAndMakeVisible (beforeButton);
    addAndMakeVisible (afterButton);

    // ...which is only reachable if the editor can hold focus.
    setWantsKeyboardFocus (true);

    hintButton.onClick = [this] { requestHint(); };
    addAndMakeVisible (hintButton);

    // The price stays as text beside the icon: an icon alone cannot say
    // "this costs you a life", and a cost you discover after paying is
    // exactly the thing decisions/014 was written about.

    processor.setVectorscope (&vectorscope);
    processor.setSpectrumAnalyzer (&hintSpectrum);
    addChildComponent (vectorscope);
    addChildComponent (hintSpectrum);

    // Three pills, always all visible. setClickingTogglesState makes JUCE
    // draw the active one with buttonOnColourId, which the shared theme
    // already fills - so "which mode am I in" is answered by looking,
    // never by opening anything.
    {
        auto index = 1;

        for (auto* button : { &practiceButton, &survivalButton, &blitzButton })
        {
            button->setClickingTogglesState (true);
            button->setRadioGroupId (modeRadioGroup);
            button->setConnectedEdges (index == 1 ? juce::Button::ConnectedOnRight
                                                  : (index == 3 ? juce::Button::ConnectedOnLeft
                                                                : juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight));
            button->onClick = [this] { modeSelected(); };
            addAndMakeVisible (button);
            ++index;
        }

        practiceButton.setToggleState (true, juce::dontSendNotification);
    }

    runStatusLabel.setJustificationType (juce::Justification::centredRight);
    runStatusLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (runStatusLabel);

    session.onRunEnded = [this] (int finalScore)
    {
        auto& progress = processor.getProgressManager();
        const auto gameIndex = processor.getGameManager().getActiveGameIndex();

        // The record is read *before* it is updated, so the results screen
        // can say "personal best" rather than comparing a number against
        // itself.
        const auto previousBest = session.getMode() == SessionManager::Mode::survival
                                      ? progress.getStatsForGame (gameIndex).bestSurvivalScore
                                      : progress.getStatsForGame (gameIndex).bestBlitzScore;

        if (session.getMode() == SessionManager::Mode::survival)
            progress.recordSurvivalScore (gameIndex, finalScore);
        else if (session.getMode() == SessionManager::Mode::blitz)
            progress.recordBlitzScore (gameIndex, finalScore);

        pendingPreviousBest = previousBest;
        showRunResults (finalScore);
    };

    // 1 Hz is all the Blitz clock needs, and it's the only thing on this
    // timer - no reason to run the whole editor at animation rate.
    // Absolutely last, so it covers every other overlay. An update is the
    // one thing that should never be behind something else.
    addChildComponent (updateWindow);
    updateWindow.onClosed = [this] { resized(); repaint(); };

    startTimerHz (1);

    scoreLabel.setJustificationType (juce::Justification::centredLeft);
    scoreLabel.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
    scoreLabel.setMinimumHorizontalScale (1.0f);

    levelProgressLabel.setJustificationType (juce::Justification::centredRight);
    levelProgressLabel.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
    addAndMakeVisible (levelProgressLabel);
    addAndMakeVisible (scoreLabel);

    feedbackLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setFont (AbcTrainLookAndFeel::headingFont());
    addAndMakeVisible (feedbackLabel);




    streakLabel.setJustificationType (juce::Justification::centredRight);
    streakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    // Not added to the editor: it is now the source of the banner's
    // localised streak string, not a widget of its own.
    addChildComponent (dailyBanner);
    donateLink.setJustificationType (juce::Justification::centredRight);
    addChildComponent (donateLink);

    dailyChallengeLabel.setJustificationType (juce::Justification::centredLeft);



    // Earning one is the only thing that puts it on screen, and only for
    // as long as it takes to read - the training screen has no permanent
    // progress furniture any more.
    processor.getProgressManager().onAchievementEarned =
        [safeThis = juce::Component::SafePointer<EarTrainerEditor> (this)] (const juce::String& id)
        {
            if (safeThis != nullptr)
                safeThis->showAchievementToast (id);
        };

    // The moment of answer (ADR 029, stage 2): what it earned, whether the
    // promotion test moved, whether a level was taken. Same SafePointer
    // shape as the achievement callback above, for the same reason.
    processor.getProgressManager().onAnswerScored =
        [safeThis = juce::Component::SafePointer<EarTrainerEditor> (this)]
        (int scoredGameIndex, const ProgressManager::AnswerOutcome& outcome)
        {
            if (safeThis != nullptr)
                safeThis->handleAnswerScored (scoredGameIndex, outcome);
        };

    addChildComponent (pointsFlyup);
    addChildComponent (promotionPips);

    instructionsButton.onClick = [this]
    {
        instructionsPinnedOpen = ! instructionsPinnedOpen;

        // Actually show it. This flipped the flag and re-laid the screen
        // out around a label that was still invisible, so pressing "?"
        // moved things very slightly and produced no text at all - the
        // button looked broken because it was.
        instructionLabel.setVisible (shouldShowInstructions());
        resized();
        repaint();
    };
    addChildComponent (instructionsButton);

    gameManager.getActiveGame().addChangeListener (this);
    processor.getProgressManager().addChangeListener (this);
    localisation.addChangeListener (this);

    updateButton.onClick = [this]
    {
        // Real bug reported by an actual user: clicking "Updates" gave no
        // visible reaction at all whenever no newer release was found (or
        // this repo simply had no releases yet) - checkForUpdatesAsync's
        // callback deliberately never fires in that case (see
        // decisions/007-update-checker.md), which used to leave the
        // button looking like it did nothing. It now shows "Checking...",
        // then either the existing "update available" prompt, a brief
        // "up to date" acknowledgement, or - if nothing came back at all
        // within a few seconds (offline, rate-limited, no releases) - a
        // brief "couldn't check" message, so every click gets *some*
        // visible outcome.
        juce::Component::SafePointer<EarTrainerEditor> safeThis (this);
        auto handled = std::make_shared<bool> (false);

        updateButton.setEnabled (false);
        updateButton.setTooltip (localisation.getText ("ui.checkingForUpdates"));

        // Captured now (editor definitely alive), not read from
        // `localisation` inside the async callback below, which may run
        // after the editor has been destroyed; the safeThis null check
        // guards every actual use of the editor/its components.
        const auto updateAvailableText = localisation.getText ("ui.updateAvailable");
        const auto openReleasePageText = localisation.getText ("ui.openReleasePage");
        const auto laterText = localisation.getText ("ui.later");
        const auto updatesText = localisation.getText ("ui.updates");
        const auto upToDateText = localisation.getText ("ui.upToDate");

        UpdateChecker::checkForUpdatesAsync (CurrentVersion::string, [safeThis, handled, updateAvailableText, openReleasePageText, laterText, updatesText, upToDateText] (bool foundNewer, UpdateChecker::ReleaseInfo release)
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);

            if (! foundNewer)
            {
                safeThis->updateButton.setTooltip (upToDateText);
                juce::Timer::callAfterDelay (2500, [safeThis, updatesText]
                {
                    if (safeThis != nullptr)
                        safeThis->updateButton.setTooltip (updatesText);
                });
                return;
            }

            safeThis->updateButton.setTooltip (updatesText);

            // A window, not a tooltip. The progress was always real and
            // always went somewhere nobody was looking.
            safeThis->updateWindow.setStrings (updateWindowStrings (safeThis->localisation));
            safeThis->updateWindow.show (release,
                juce::JUCEApplicationBase::isStandaloneApp());
        });

        // checkForUpdatesAsync's callback never fires at all on failure
        // (no internet, rate limiting, no releases published yet) - so
        // without this, the button would be stuck showing "Checking..."
        // forever. 6s comfortably outlasts the network call's own 5s
        // connection timeout.
        const auto checkFailedText = localisation.getText ("ui.checkFailed");
        juce::Timer::callAfterDelay (6000, [safeThis, handled, updatesText, checkFailedText]
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);
            safeThis->updateButton.setTooltip (checkFailedText);

            juce::Timer::callAfterDelay (2500, [safeThis, updatesText]
            {
                if (safeThis != nullptr)
                    safeThis->updateButton.setTooltip (updatesText);
            });
        });
    };
    addAndMakeVisible (updateButton);

    trainingSoundsButton.onClick = [this]
    {
        trainingSounds.refresh();
        trainingSounds.setVisible (true);
    };
    addAndMakeVisible (trainingSoundsButton);

    soundkorbLink.setFont (AbcTrainLookAndFeel::monoFont().withHeight (13.0f), false,
                            juce::Justification::centredRight);
    addAndMakeVisible (soundkorbLink);

    choiceSlider.onChoiceSelected = [this] (int choiceIndex) { choiceButtonClicked (choiceIndex); };

    choiceSlider.onContinuousChoice = [this] (float normalised)
    {
        if (! session.isRunActive())
            return;

        auto& game = processor.getGameManager().getActiveGame();
        game.submitNormalisedAnswer (normalised);
        afterAnswer (game.wasLastAnswerCorrect());
    };
    addAndMakeVisible (choiceSlider);

    // Added last so it paints on top of every other child, same as
    // shared/LessonController's integration in the Learner editors - JUCE
    // paints child components in the order they were added, and this
    // overlay needs to cover the choice slider/instructions/etc. whenever
    // it's shown. A real bug, found by actually running the app: adding
    // this earlier (before choiceSlider) left the slider painting on top
    // of the "closed" overlay instead of the other way around.
    settingsButton.onClick = [this]
    {
        settingsScreen.setVisible (true);
        settingsScreen.toFront (false);
        settingsScreen.refresh();
    };
    addAndMakeVisible (settingsButton);

    settingsScreen.onClosed = [this] { resized(); repaint(); };
    settingsScreen.onSettingsChanged = [this]
    {
        // A text-scale change alters how much room every label needs, so
        // the layout has to be redone rather than merely repainted.
        resized();
        repaint();
    };
    addChildComponent (settingsScreen);

    addChildComponent (trainingSounds);

    // Last of all, so it paints over the training screen and both
    // overlays. It never takes mouse events (see AchievementToast).
    runResults.onPlayAgain = [this]
    {
        runResults.setVisible (false);
        beginRunWithCountdown();
    };

    runResults.onGoHome = [this]
    {
        runResults.setVisible (false);
        showScreen (Screen::home);
    };

    runResults.onModeChosen = [this] (int mode)
    {
        runResults.setVisible (false);

        // The pills follow, so the row still says which mode is live -
        // choosing here and leaving them stale would be two sources of
        // truth for one thing.
        practiceButton.setToggleState (mode == 0, juce::dontSendNotification);
        survivalButton.setToggleState (mode == 1, juce::dontSendNotification);
        blitzButton.setToggleState (mode == 2, juce::dontSendNotification);

        session.setMode (mode == 1 ? SessionManager::Mode::survival
                         : mode == 2 ? SessionManager::Mode::blitz
                                     : SessionManager::Mode::practice);
        beginRunWithCountdown();
    };

    addChildComponent (runResults);

    achievementsScreen.onClosed = [this] { resized(); repaint(); };
    addChildComponent (achievementsScreen);

    // Added after every other child: it dims the window and cuts a hole,
    // which only works if it is on top of what it is dimming.
    tour.onFinished = [this] { repaint(); };
    addChildComponent (tour);

    // Idle only counts while nothing is playing and no overlay is open -
    // otherwise it would appear over a round somebody is in the middle of.
    screensaver.isBusy = [this]
    {
        return processor.isSignalEnabled()
               || tour.isRunning()
               || trainingSounds.isVisible()
               || settingsScreen.isVisible();
    };

    screensaver.onDismissed = [this] { repaint(); };
    screensaver.setIdleSeconds (localisationProperties.getIntValue (
        IdleScreensaver::idleSecondsKey, IdleScreensaver::defaultIdleSeconds));

    addChildComponent (screensaver);

    homeScreen.onBadgeStripClicked = [this] { showAchievementsScreen(); };

    addChildComponent (achievementToast);
    trainingSounds.onClosed = [this] { resized(); };

    // Grown again for the grouping/whitespace pass: the three section
    // panels each carry their own padding and caption, which is what buys
    // the "everything breathes" feel, and that space has to come from
    // somewhere. Same "grew the window to fit new content" precedent as
    // the slider redesign (015) and the Learner guide labels (010).
    // Restore the persisted UI scale; the logical layout never changes,
    // only the transform applied to it.
    // Draggable, within limits that keep the layout honest: the floor is
    // the height the sections actually need, and the ceiling stops a 5K
    // display leaving nine tiles the size of postage stamps.
    setResizable (true, true);
    setResizeLimits (logicalWidth, logicalBaseHeight,
                      (int) (logicalWidth * 2.0), (int) (logicalBaseHeight * 2.0));

    setUiScale (localisationProperties.getDoubleValue (uiScaleKey, 1.0));

    applyTheme();
    session.startRun();
    refreshHintButton();
    rebuildChoiceSlider();
    refreshFromGameState();
    refreshFromProgressState();
    refreshLocalisedText();

    // First launch gets the support screen once; after that, straight to
    // Home. Never mid-exercise.
    // The welcome screen every time, not once: it is the front door, it
    // says what the four words mean, and it costs one click to pass.
    if (! localisationProperties.getBoolValue (tourOfferedKey, false))
        supportScreen.setTourOffer (localisation.getText ("tour.offer"),
                                     localisation.getText ("tour.accept"),
                                     localisation.getText ("tour.decline"));

    showScreen (Screen::support);
}

void EarTrainerEditor::applyTheme()
{
    const auto& theme = AbcTrainTheme::current();

    // Widgets that set their own colours can't be reached by the
    // LookAndFeel's colour scheme, so they're refreshed here instead - and
    // every one of them now reads the palette rather than a literal, which
    // is what makes the light theme possible at all.
    instructionLabel.setColour (juce::Label::textColourId, theme.textDim);
    levelProgressLabel.setColour (juce::Label::textColourId, theme.textDim);
    scoreLabel.setColour (juce::Label::textColourId, theme.text);
    streakLabel.setColour (juce::Label::textColourId, theme.accentWarm);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, theme.accent);
    donateLink.setColour (juce::HyperlinkButton::textColourId, theme.accentWarm);

    // The icon shows the mode you'd switch *to*, and morphs between the
    // two rather than cutting - see IconButton.
    themeButton.setIcon (theme.mode == AbcTrainTheme::Mode::light ? AppIcons::Icon::moon
                                                                  : AppIcons::Icon::sun);
    themeButton.setTooltip (localisation.getText (theme.mode == AbcTrainTheme::Mode::light
                                                       ? "ui.themeDark" : "ui.themeLight"));

    currentGameLabel.setColour (juce::Label::textColourId, theme.textBright);
    feedbackLabel.setColour (juce::Label::textColourId, theme.text);
    runStatusLabel.setColour (juce::Label::textColourId, theme.text);

    // These are re-coloured per answer/run/progress state, so let the
    // normal refresh paths reapply them from the new palette - including
    // refreshRunStatus, which was missing and left the lives/clock in the
    // previous theme's colour.
    refreshFromGameState();
    refreshFromProgressState();
    refreshRunStatus();
    refreshBeforeAfter();

    repaint();
}

void EarTrainerEditor::toggleTheme()
{
    const auto newMode = AbcTrainTheme::getMode() == AbcTrainTheme::Mode::light
                             ? AbcTrainTheme::Mode::dark
                             : AbcTrainTheme::Mode::light;

    AbcTrainTheme::setMode (newMode);
    localisationProperties.setValue (themeModeKey,
                                      newMode == AbcTrainTheme::Mode::light ? "light" : "dark");

    lookAndFeel.refreshFromTheme();
    applyTheme();

    // The LookAndFeel's colour scheme feeds widgets at paint time, so a
    // repaint of the whole tree is all that's needed - no rebuild.
    for (auto* child : getChildren())
        child->repaint();
}

EarTrainerEditor::~EarTrainerEditor()
{
    // Deregister the scope feeds *first*: the audio thread writes into
    // them every block, and they are members of this editor. Leaving them
    // registered through teardown is a use-after-free waiting for the
    // next processBlock.
    processor.setSignalEnabled (true);
    processor.setVectorscope (nullptr);
    processor.setSpectrumAnalyzer (nullptr);

    processor.getGameManager().getActiveGame().removeChangeListener (this);
    processor.getProgressManager().removeChangeListener (this);
    localisation.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void EarTrainerEditor::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    // Each exercise gets its own room: the backdrop carries a hint of that
    // training's category colour while you're in it, and goes neutral on
    // the home screen where nine tints at once would be noise.
    AbcTrainLookAndFeel::paintPanelBackground (
        g, getLocalBounds().toFloat(),
        currentScreen == Screen::training
            ? tintForGame (processor.getGameManager().getActiveGame().getName())
            : juce::Colours::transparentBlack);

    // Grouped sections behind the controls: related things now sit on a
    // shared surface with a caption, instead of floating loose on the
    // backdrop with only whitespace implying the grouping.
    if (currentScreen == Screen::training)
    {
        // Headings, not boxes: three bordered panels cut one window into
        // three pieces rather than organising it.
        AbcTrainLookAndFeel::paintSectionHeading (g, exerciseSection.toFloat(),
                                                   localisation.getText ("ui.sectionExercise"));
        AbcTrainLookAndFeel::paintSectionHeading (g, answerSection.toFloat(),
                                                   localisation.getText ("ui.sectionAnswer"));


        // Only drawn when the hint has actually been bought - the panel
        // does not exist otherwise, and neither does its heading.
        if (hintRevealed && ! hintSection.isEmpty())
            AbcTrainLookAndFeel::paintSectionHeading (g, hintSection.toFloat(),
                                                       localisation.getText ("ui.sectionHint"));
    }

    // The title, letter-spaced. Drawn here rather than through a Label
    // because JUCE exposes no tracking control on Label/drawText.
    //
    // Both screens carry one now. The home screen's top strip used to hold
    // six control buttons, which made the first thing anyone read a
    // toolbar; those are on a bar along the bottom and this says where you
    // are instead.
    if (currentScreen != Screen::support)
        AbcTrainLookAndFeel::drawTrackedText (
            // "ABC" is the product name and does not translate; what
            // follows it does. The host shows "ABC Ear Trainer" whatever
            // the interface language is, and a window titled only
            // "Тренажёр слуха" makes the two look like different programs.
            g, "ABC " + (currentScreen == Screen::training
                             ? titleLabel.getText()
                             : localisation.getText ("app.eartrainer.name")),
            juce::Rectangle<float> ((float) AbcTrainTheme::Spacing::large,
                                     (float) AbcTrainTheme::Spacing::medium,
                                     (float) getWidth() * 0.6f, 32.0f),
            AbcTrainLookAndFeel::titleFont(), theme.textBright, 1.8f,
            juce::Justification::centredLeft);
}

void EarTrainerEditor::showUpdateOutcome (const juce::String& text)
{
    updateButton.setTooltip (text);
}

void EarTrainerEditor::mouseMove (const juce::MouseEvent&)
{
    // The editor sees moves over its own background; child components eat
    // their own, which is why the screensaver also watches for a click and
    // a keypress. Between the three, anything a person does counts.
    screensaver.noteActivity();
}

bool EarTrainerEditor::keyPressed (const juce::KeyPress& key)
{
    screensaver.noteActivity();

    if (key == juce::KeyPress::spaceKey && beforeButton.isVisible())
    {
        auto& game = processor.getGameManager().getActiveGame();
        setPlayProcessed (! game.isPlayingProcessed());
        return true;
    }

    return false;
}

void EarTrainerEditor::resized()
{
    updateWindow.setBounds (getLocalBounds());

    using namespace AbcTrainTheme;

    // Anything that changes the height while the hint is closed is a drag,
    // a scale change, or the initial size - all of them the height to come
    // back to when the hint closes again.
    if (! hintRevealed)
        heightWithoutHint = juce::jmax (logicalBaseHeight, getHeight());

    // The home screen owns everything under the title row; the training
    // screen's own layout below only runs when it's the visible one.
    // Below the title row, above the footer link.
    // One line of context above the grid - the streak and today's
    // challenge. The level lives on each tile now, because that is where
    // it is actually true.
    {
        auto statusRow = getLocalBounds().reduced (Spacing::large, 0)
                             .withTop (Spacing::large + 32 + Spacing::small)
                             .withHeight (homeStatusHeight);

        dailyBanner.setBounds (statusRow);
    }

    homeScreen.setBounds (getLocalBounds()
                              .reduced (Spacing::large, 0)
                              .withTop (Spacing::large + 32 + Spacing::small
                                         + homeStatusHeight + Spacing::small)
                              .withTrimmedBottom (Spacing::large + 30 + Spacing::small));

    auto area = getLocalBounds().reduced (Spacing::large);

    // --- title row: just the name of where you are ---------------------
    // The app's controls used to live up here, six of them in a row, and
    // that made the first thing anyone read a toolbar. They are on a bar
    // along the bottom now (see below); the top is a title.
    area.removeFromTop (32);

    // --- the tool bar, bottom left ---------------------------------------
    {
        constexpr int iconSize = 30;

        auto bar = getLocalBounds().reduced (Spacing::large)
                       .removeFromBottom (iconSize);

        settingsButton.setBounds (bar.removeFromLeft (iconSize));
        bar.removeFromLeft (Spacing::tight);
        trainingSoundsButton.setBounds (bar.removeFromLeft (iconSize));
        bar.removeFromLeft (Spacing::tight);
        themeButton.setBounds (bar.removeFromLeft (iconSize));
        bar.removeFromLeft (Spacing::tight);
        updateButton.setBounds (bar.removeFromLeft (iconSize));
        bar.removeFromLeft (Spacing::medium);

        // Indicators, not form fields: each asks for exactly the width its
        // own widest value needs (see CompactSelector).
        sizeSelector.setBounds (bar.removeFromLeft (sizeSelector.getPreferredWidth())
                                    .withSizeKeepingCentre (sizeSelector.getPreferredWidth(), 22));
        bar.removeFromLeft (Spacing::tight);
        languageSelector.setBounds (bar.removeFromLeft (languageSelector.getPreferredWidth())
                                        .withSizeKeepingCentre (languageSelector.getPreferredWidth(), 22));

        soundkorbLink.setBounds (bar.removeFromRight (130).withSizeKeepingCentre (130, 18));
        bar.removeFromRight (Spacing::medium);
        donateLink.setBounds (bar.removeFromRight (150).withSizeKeepingCentre (150, 18));
    }

    area.removeFromTop (Spacing::section);

    // --- exercise section: which game, and what you're listening for ---
    // Collapses to just the name row once the exercise is familiar (see
    // shouldShowInstructions); the freed height flows to the answer
    // section below, which is the part that improves by being taller.
    const auto instructionsShown = shouldShowInstructions();
    exerciseSection = area.removeFromTop (instructionsShown ? 124
                                                            : Spacing::large + 30 + Spacing::small);
    {
        auto inner = exerciseSection;
        inner.removeFromTop (Spacing::large);   // clear the section heading

        // "Home" as a word, not a 28px house. A labelled button is the
        // one control on this screen a lost player is looking for, and an
        // icon they have to decode is exactly the wrong shape for that.
        auto gameRow = inner.removeFromTop (30);
        backButton.setBounds (gameRow.removeFromLeft (104).withSizeKeepingCentre (104, 30));
        gameRow.removeFromLeft (Spacing::large);
        gameIcon.setBounds (gameRow.removeFromLeft (26).withSizeKeepingCentre (26, 26));
        gameRow.removeFromLeft (Spacing::small);
        instructionsButton.setBounds (gameRow.removeFromRight (24).withSizeKeepingCentre (22, 22));
        gameRow.removeFromRight (Spacing::tight);
        levelProgressLabel.setBounds (gameRow.removeFromRight (210));
        currentGameLabel.setBounds (gameRow);

        inner.removeFromTop (Spacing::small);
        instructionLabel.setBounds (inner);
    }

    area.removeFromTop (Spacing::large);

    // --- the hint, if it has been bought: only then does it exist ---
    // Between hearing and answering, which is the order you use it in.
    if (hintRevealed)
    {
        hintSection = area.removeFromTop (hintPanelHeight);

        auto inner = hintSection;
        inner.removeFromTop (Spacing::large);   // clear the section heading

        auto hintRow = inner.removeFromTop (hintRowHeight).reduced (Spacing::small, 0);
        vectorscope.setBounds (hintRow.removeFromLeft (hintRow.getHeight()));
        hintRow.removeFromLeft (Spacing::small);
        hintSpectrum.setBounds (hintRow);

        area.removeFromTop (Spacing::large);
    }
    else
    {
        hintSection = {};
    }

    vectorscope.setVisible (hintRevealed && currentScreen == Screen::training);
    hintSpectrum.setVisible (hintRevealed && currentScreen == Screen::training);

    // --- answer section: feedback, the slider itself, score/new round ---
    // 20 heading + 24 feedback + 4 + 186 scale + 12 + 30 A/B + 8 + 34 row.
    // Everything left over goes here, floored at the height its own rows
    // need. This is the section that *should* grow: a taller answer scale is
    // a more precise answer scale, while a taller exercise heading is just a
    // heading with air above it.
    //
    // Floored rather than divided, because removeFromTop clamps instead of
    // overflowing - a floor below the content height does not warn, it
    // silently shrinks whatever is last inside (three times in this
    // codebase now).
    // Everything left over *except* the tool bar's strip along the bottom.
    // That bar is positioned from getLocalBounds() rather than from `area`,
    // so taking the full remaining height here put the mode pills straight
    // on top of it - visible the moment the window was rendered at any size
    // other than the one it was designed at, and invisible before that.
    constexpr int toolBarReserve = 30 + Spacing::small;

    answerSection = area.removeFromTop (
        juce::jmax (318, area.getHeight() - toolBarReserve));
    {
        auto inner = answerSection;
        inner.removeFromTop (Spacing::large);

        // The pips live beside the verdict line - the promotion test is
        // part of the answer moment, not part of the header. Reserved on
        // both sides so the verdict text stays truly centred.
        auto feedbackRow = inner.removeFromTop (24);
        promotionPips.setBounds (feedbackRow.removeFromRight (90));
        feedbackRow.removeFromLeft (90);
        feedbackLabel.setBounds (feedbackRow);
        inner.removeFromTop (Spacing::tight);

        // A corridor for the points flyup: rises from the verdict line up
        // through the exercise section's lower edge.
        pointsFlyup.setBounds (juce::Rectangle<int> (answerSection.getCentreX() - 70,
                                                     juce::jmax (0, answerSection.getY() - 34),
                                                     140, 88));

        // The countdown covers the whole answer section: the numbers land
        // where the question is about to be.
        runCountdown.setBounds (answerSection);

        // *One* row under the scale, not two.
        //
        // A/B sat on its own row and the mode pills on another, which made
        // five levels of importance on a screen that should have four - and
        // the two are the same kind of thing anyway: how this round is being
        // played. Narrower pills and a narrower hint button pay for it.
        //
        // Taken off the bottom first so the scale can then have everything
        // between. Laying these out top-down with fixed heights is what left
        // a tall window with a strip of controls and a field of nothing
        // under it.
        auto controlRow = inner.removeFromBottom (34);
        inner.removeFromBottom (Spacing::medium);

        // Everything that is left, floored at the height the zones need to
        // stay legible: 40px of the scale is its value readout and 18 the
        // caption, so under ~120 the zones are too shallow for staggered
        // labels. This is the control the screen exists for, so it is the
        // one that should get the room - a taller scale is a more precise
        // scale, and nothing else here improves by being taller.
        choiceSlider.setBounds (inner.withHeight (juce::jmax (186, inner.getHeight()))
                                     .reduced (Spacing::small, 0));

        {
            // Modes left, narrow and quiet: chosen once a session. The hint
            // right, where a choice you spend something on belongs. A/B in
            // the middle, directly under the scale it compares - it is the
            // control touched most often, so it gets the centre.
            const auto pillWidth = 62;
            const auto modesSlotWidth = pillWidth * 3 + Spacing::medium + 92;

            if (isRunHudActive())
            {
                // The HUD takes exactly the slot the pills + session score
                // vacate, so A/B never shifts when a run starts or ends -
                // minus the width of the way out, which shares that slot.
                constexpr int endWidth = 74;

                endRunButton.setBounds (controlRow.removeFromLeft (endWidth)
                                            .withSizeKeepingCentre (endWidth, 28));
                controlRow.removeFromLeft (Spacing::small);

                runHud.setBounds (controlRow.removeFromLeft (modesSlotWidth - endWidth - Spacing::small)
                                      .withSizeKeepingCentre (modesSlotWidth - endWidth - Spacing::small, 30));
            }
            else
            {
                for (auto* pill : { &practiceButton, &survivalButton, &blitzButton })
                    pill->setBounds (controlRow.removeFromLeft (pillWidth)
                                         .withSizeKeepingCentre (pillWidth, 28));

                // Score and the lives/clock readout ride along with the modes:
                // they say how *this* run is going, which is the same subject
                // the pills set.
                controlRow.removeFromLeft (Spacing::medium);
                scoreLabel.setBounds (controlRow.removeFromLeft (92));
            }

            // Only reserve width for the lives/clock readout when there is
            // one. In Practice there is nothing to report, and holding 86px
            // for an empty label is what pushed A/B into the hint button -
            // five groups do not fit in this row at the design width unless
            // the empty one gives its space back.
            runStatusLabel.setBounds (runStatusLabel.getText().isNotEmpty()
                                          ? controlRow.removeFromLeft (78)
                                          : juce::Rectangle<int>());

            // The hint used to be a 30px glyph with a caption beside it, and
            // the glyph read as a "no entry" sign - so the one control you
            // reach for when stuck looked disabled and unlabelled. It is one
            // button now, with its price written on it.
            hintButton.setBounds (controlRow.removeFromRight (146)
                                      .withSizeKeepingCentre (146, 30));

            // No floor: a minimum wider than the space left is a minimum
            // that overlaps its neighbours instead of shrinking.
            auto centred = controlRow.withSizeKeepingCentre (
                juce::jmin (200, controlRow.getWidth() - Spacing::medium * 2), 30);

            restartButton.setBounds (centred.withSizeKeepingCentre (160, 30));

            const auto half = (centred.getWidth() - Spacing::tight) / 2;
            beforeButton.setBounds (centred.removeFromLeft (half));
            centred.removeFromLeft (Spacing::tight);
            afterButton.setBounds (centred);
        }
    }

    area.removeFromTop (Spacing::large);

    // The progress section used to live here - level, a bar, streak and
    // the daily challenge, on screen through every round. It has moved to
    // the home screen. A bar answers "how far to the next level", which is
    // a question about the app rather than about hearing, and a training
    // screen should hold the thing you are answering with and nothing
    // else. What replaces it *during* a run is a toast: nothing until you
    // earn something, then a moment of it. See decisions/024.
    progressSection = {};

    // Unconditional, same as shared/LessonController's integration in the
    // Learner editors - whether or not they're currently visible.
    trainingSounds.setBounds (getLocalBounds());
    settingsScreen.setBounds (getLocalBounds());

    // Centred under the title row, wide enough for an achievement name.
    runResults.setBounds (getLocalBounds());
    achievementsScreen.setBounds (getLocalBounds());
    tour.setBounds (getLocalBounds());
    screensaver.setBounds (getLocalBounds());

    achievementToast.setBounds (getLocalBounds().withTrimmedTop (Spacing::large + 34)
                                                 .withHeight (52)
                                                 .withSizeKeepingCentre (
                                                     juce::jmin (getWidth() - Spacing::large * 2, 360), 52)
                                                 .withY (Spacing::large + 34));
}

void EarTrainerEditor::startTour()
{
    // It points at real widgets, so the screen holding them has to be up
    // first. Exercise 0 in Practice: no lives, no clock, nothing that could
    // end while somebody is reading.
    openTrainingForSnapshot (0);

    tour.clearSteps();
    tour.setStrings (localisation.getText ("tour.next"),
                      localisation.getText ("tour.skip"),
                      localisation.getText ("tour.done"));

    tour.addStep (&instructionLabel, localisation.getText ("tour.step.instruction"));
    tour.addStep (&afterButton,      localisation.getText ("tour.step.ab"));
    tour.addStep (&choiceSlider,     localisation.getText ("tour.step.answer"));
    tour.addStep (&levelProgressLabel, localisation.getText ("tour.step.level"));
    tour.addStep (&backButton,       localisation.getText ("tour.step.home"));

    tour.start();
}

void EarTrainerEditor::showAchievementsScreen()
{
    auto& progress = processor.getProgressManager();
    auto& gameManager = processor.getGameManager();

    const auto snapshot = progress.makeAchievementSnapshot();
    std::vector<AchievementsScreenComponent::Entry> entries;

    for (const auto& definition : Achievements::all())
    {
        AchievementsScreenComponent::Entry entry;
        entry.name = localisation.getText (definition.nameKey);
        entry.description = localisation.getText (definition.descriptionKey);
        entry.earned = progress.hasAchievement (definition.id);
        entry.progress = Achievements::progressTowards (definition, snapshot);
        entry.tint = Achievements::colourForTier (definition.tier);
        entry.tierName = localisation.getText (Achievements::nameKeyForTier (definition.tier));
        entry.icon = definition.gameIndex >= 0 && definition.gameIndex < gameManager.getNumGames()
                         ? AppIcons::iconForGameName (gameManager.getGame (definition.gameIndex).getName())
                         : AppIcons::Icon::award;

        entries.push_back (std::move (entry));
    }

    achievementsScreen.setStrings (
        localisation.getText ("ui.achievements"),
        localisation.getText ("ui.achievementsSubtitle",
                               { { "earned", juce::String (progress.getNumAchievementsEarned()) },
                                 { "total", juce::String ((int) Achievements::all().size()) } }),
        localisation.getText ("ui.close"));

    achievementsScreen.setEntries (std::move (entries));
    achievementsScreen.setVisible (true);
    achievementsScreen.toFront (false);
}

void EarTrainerEditor::showRunResults (int finalScore)
{
    auto& progress = processor.getProgressManager();
    auto& gameManager = processor.getGameManager();

    const auto gameIndex = gameManager.getActiveGameIndex();
    const auto englishName = gameManager.getGame (gameIndex).getName();
    const auto stats = progress.getStatsForGame (gameIndex);

    RunResultsComponent::Summary summary;
    summary.exerciseName = translateGameName (englishName, localisation);
    summary.modeName = localisation.getText (session.getMode() == SessionManager::Mode::survival
                                                  ? "ui.modeSurvival" : "ui.modeBlitz");

    summary.score = finalScore;
    summary.rounds = stats.roundsPlayed;
    summary.bestStreakThisRun = session.getBestStreakThisRun();
    summary.previousBest = pendingPreviousBest;
    summary.isNewBest = finalScore > pendingPreviousBest;

    const auto roundsThisRun = juce::jmax (1, session.getRoundsThisRun());
    summary.runAccuracy = (float) finalScore / (float) roundsThisRun;
    summary.lifetimeAccuracy = stats.getAccuracy();

    // One standing per family, taken from the exercise the player has the
    // highest level in - "where am I strong" rather than "here are nine
    // more numbers".
    for (const char* categoryKey : { "home.category.frequency", "home.category.dynamics",
                                      "home.category.space", "home.category.character" })
    {
        RunResultsComponent::SkillStanding standing;
        standing.name = localisation.getText (categoryKey);

        for (int i = 0; i < gameManager.getNumGames(); ++i)
        {
            const auto name = gameManager.getGame (i).getName();

            if (juce::String (categoryForGame (name)) != categoryKey)
                continue;

            const auto level = progress.getLevelForGame (i);

            if (level >= standing.level)
            {
                standing.level = level;
                standing.levelProgress = progress.getLevelProgressForGame (i);
                standing.icon = AppIcons::iconForGameName (name);
            }

            if (i == gameIndex)
                standing.isCurrent = true;
        }

        summary.skills.push_back (std::move (standing));
    }

    runResults.setStrings (localisation.getText ("ui.runResults"),
                            localisation.getText ("ui.playAgain"),
                            localisation.getText ("ui.back"),
                            localisation.getText ("ui.score.caption"),
                            localisation.getText ("ui.accuracy"),
                            localisation.getText ("ui.bestStreak"),
                            localisation.getText ("ui.personalBest"),
                            localisation.getText ("ui.newBest"),
                            localisation.getText ("ui.whereYouStand"));

    // Offer the other two, but only for an exercise whose timed modes are
    // open at all - otherwise the results of a Practice run would offer
    // something the training screen still hides.
    if (progress.areModesUnlockedForGame (gameIndex))
    {
        runResults.setModeOffer (localisation.getText ("ui.tryAnotherMode"),
                                  { practiceButton.getButtonText(),
                                    survivalButton.getButtonText(),
                                    blitzButton.getButtonText() },
                                  session.getMode() == SessionManager::Mode::survival ? 1
                                  : session.getMode() == SessionManager::Mode::blitz  ? 2 : 0);
    }
    else
    {
        runResults.setModeOffer ({}, {}, 0);
    }

    runResults.show (std::move (summary));
}

void EarTrainerEditor::showAchievementToast (const juce::String& achievementId)
{
    if (const auto* definition = Achievements::find (achievementId))
        achievementToast.show (localisation.getText ("ui.achievementEarned"),
                                localisation.getText (definition->nameKey));

    // An id this build doesn't define (a save from a newer version) shows
    // nothing rather than an empty card - same graceful-miss rule as the
    // icon and i18n lookups.
}

bool EarTrainerEditor::shouldShowInstructions() const
{
    if (instructionsPinnedOpen)
        return true;

    // Familiar means a handful of lifetime correct answers on *this*
    // exercise. Below that, the paragraph earns its space; above it, the
    // space belongs to the answer scale and the "?" brings the text back.
    const auto index = processor.getGameManager().getActiveGameIndex();
    return processor.getProgressManager().getStatsForGame (index).correctAnswers < 3;
}

void EarTrainerEditor::handleAnswerScored (int scoredGameIndex, const ProgressManager::AnswerOutcome& outcome)
{
    // Answers land from the active game's own change broadcast, so a
    // different index here would mean a stale callback - drop it.
    if (scoredGameIndex != processor.getGameManager().getActiveGameIndex()
        || currentScreen != Screen::training)
        return;

    const auto& theme = AbcTrainTheme::current();

    if (outcome.pointsAwarded > 0)
    {
        // "+60" with the daily bonus folded in is one fact, not two - the
        // flyup is a number, and the daily card on Home explains itself.
        pointsFlyup.show ("+" + juce::String (outcome.pointsAwarded),
                          outcome.dailyChallengeJustCompleted ? theme.accentWarm : theme.positive);
    }

    // The promotion test: visible while live, gone while not. set() pops
    // the newest pip on its own.
    promotionPips.setVisible (outcome.promotionPending);
    if (outcome.promotionPending)
        promotionPips.set (outcome.promotionStreak, ProgressManager::promotionTestLength);

    // The one answer that opens the timed modes. Announced through the
    // same toast achievements use - one vocabulary for "something was
    // earned" - and only on the answer that crosses the line, because
    // areModesUnlockedForGame stays true from then on.
    {
        auto& progress = processor.getProgressManager();
        const auto streak = progress.getConsecutiveCorrectForGame (scoredGameIndex);

        if (outcome.wasCorrect && streak == ProgressManager::streakToUnlockModes)
        {
            achievementToast.show (localisation.getText ("ui.modesUnlockedCaption"),
                                    localisation.getText ("ui.modesUnlockedTitle"));
            refreshRunStatus();
            resized();
        }
    }

    if (outcome.leveledUp)
    {
        // The one moment the whole points system builds toward. The toast
        // is the same furniture achievements use - one vocabulary for
        // "something was earned".
        achievementToast.show (localisation.getText ("ui.levelTaken"),
                                translateGameName (processor.getGameManager().getActiveGame().getName(), localisation)
                                    + " - " + localisation.getText ("ui.levelNumber",
                                                                     { { "level", juce::String (outcome.level) } }));
    }
    else if (outcome.promotionJustOpened)
    {
        // The test opening is worth a beat of its own: from here, five in
        // a row takes the level, and a player two answers in should *feel*
        // that.
        achievementToast.show (localisation.getText ("ui.promotionOpenedCaption"),
                                localisation.getText ("ui.promotionOpenedTitle",
                                                       { { "count", juce::String (ProgressManager::promotionTestLength) } }));
    }
}

void EarTrainerEditor::languageSelected()
{
    const auto& codes = LocalisationManager::getSupportedLanguageCodes();
    const auto index = languageSelector.getSelectedId() - 1;
    if (index < 0 || index >= codes.size())
        return;

    localisation.setLanguage (codes[index]);
    refreshLocalisedText();
}


void EarTrainerEditor::rebuildGameSelectorItems()
{
    // The nine-name ComboBox is gone (see GamePickerComponent); what's
    // left in the title area is the *current* exercise's name plus a
    // button that opens the card grid.
    auto& gameManager = processor.getGameManager();
    currentGameLabel.setText (translateGameName (gameManager.getActiveGame().getName(), localisation),
                               juce::dontSendNotification);

}

void EarTrainerEditor::timerCallback()
{
    if (session.getMode() != SessionManager::Mode::blitz || ! session.isRunActive())
        return;

    if (session.tickOneSecond())
    {
        refreshFromGameState();   // the clock just ended the run
        refreshBeforeAfter();
    }

    refreshRunStatus();
}

void EarTrainerEditor::modeSelected()
{
    // setMode() starts a fresh run itself, so this is one call, not two.
    // The price changes with the mode, so the button's label must too.
    session.setMode (survivalButton.getToggleState() ? SessionManager::Mode::survival
                     : blitzButton.getToggleState()   ? SessionManager::Mode::blitz
                                                      : SessionManager::Mode::practice);

    beginRunWithCountdown();
}

void EarTrainerEditor::beginRunWithCountdown()
{
    // Practice starts instantly: it is the mode without pressure, and a
    // countdown before a pressureless run is theatre.
    if (session.getMode() == SessionManager::Mode::practice)
    {
        runStarted = false;
        startNewRun();
        return;
    }

    // Anything still in flight belongs to whatever this replaces.
    ++pendingAdvanceId;
    runStarted = false;

    // Quiet while the numbers fall - the run's first sound should be the
    // run's first question.
    processor.setSignalEnabled (false);
    refreshRunStatus();
    resized();

    const auto caption = session.getMode() == SessionManager::Mode::survival
                             ? survivalButton.getButtonText()
                             : blitzButton.getButtonText();

    // Capturing `this` raw is safe here: runCountdown is a member, its
    // timer stops in its own destructor, and the editor outlives it.
    runCountdown.setBounds (answerSection);
    runCountdown.start (caption, [this]
    {
        runStarted = true;
        processor.setSignalEnabled (currentScreen == Screen::training);
        startNewRun();
        resized();
    });
}

void EarTrainerEditor::startNewRun()
{
    // Any auto-advance still in flight belongs to the run being replaced.
    ++pendingAdvanceId;

    if (! session.isRunActive())
        session.startRun();

    clearHint();

    // Each round starts *unprocessed*: you hear the clean reference
    // first, then switch to hear what changed. That's the order an
    // engineer A/Bs in, and it stops the treated version being the only
    // thing ever heard.
    processor.getGameManager().getActiveGame().setPlayProcessed (false);
    refreshBeforeAfter();

    processor.getGameManager().getActiveGame().newRound();

    // A different clip from the chosen category each round, so a folder of
    // twenty loops is twenty loops rather than the one that came up first.
    processor.getGameManager().getReferenceAudioLibrary()
        .advanceToRandomClip (processor.getSampleRate());
    refreshRunStatus();
}

void EarTrainerEditor::setUiScale (float newScale)
{
    uiScale = juce::jlimit (0.8f, 1.4f, newScale);

    // One layout, drawn through a transform - rather than four sets of
    // hand-tuned sizes that would drift apart. Everything stays exactly
    // the same design at every size.
    setTransform (juce::AffineTransform::scale (uiScale));

    // Keep whatever size the window has been dragged to - scaling is a
    // separate axis from sizing, and resetting the width here would undo a
    // deliberate drag every time somebody changed the text size.
    setSize (juce::jmax (logicalWidth, getWidth()),
              heightWithoutHint + (hintRevealed ? hintPanelHeight : 0));

    // Keep the picker in step with the actual scale, including on the
    // restore path - without this it came up blank on launch.
    const auto id = uiScale < 0.93f ? 1 : uiScale < 1.08f ? 2 : uiScale < 1.23f ? 3 : 4;
    sizeSelector.setSelectedId (id, juce::dontSendNotification);

    localisationProperties.setValue (uiScaleKey, (double) uiScale);
}

void EarTrainerEditor::applyWindowSize()
{
    // Grow by exactly the panel, shrink by exactly the panel, and leave
    // whatever height was chosen underneath alone.
    setSize (juce::jmax (logicalWidth, getWidth()),
              heightWithoutHint + (hintRevealed ? hintPanelHeight : 0));
    resized();
    repaint();
}

void EarTrainerEditor::clearHint()
{
    if (! hintRevealed)
        return;

    hintRevealed = false;
    vectorscope.setVisible (false);
    hintSpectrum.setVisible (false);
    vectorscope.reset();
    refreshHintButton();

    // The window shrinks back to the training screen's own height.
    applyWindowSize();
}

void EarTrainerEditor::refreshRunStatus()
{
    // The HUD replaces the pills, the session score and the status label
    // while a run is being played - a run you can silently re-mode
    // mid-flight is a setting, not an event. Visibility is decided here
    // (the one place run state changes funnel through); bounds in
    // resized().
    {
        const auto onTraining = currentScreen == Screen::training;
        const auto hudNow = isRunHudActive();
        const auto hudWasVisible = runHud.isVisible();

        runHud.setVisible (onTraining && hudNow);
        endRunButton.setVisible (onTraining && hudNow);

        // Practice is where every exercise starts, and the timed modes
        // appear once you have shown you can hear the thing at all - see
        // ProgressManager::areModesUnlockedForGame. Practice itself stays
        // on screen throughout, so the row never empties.
        const auto unlocked = processor.getProgressManager()
                                  .areModesUnlockedForGame (processor.getGameManager().getActiveGameIndex());

        practiceButton.setVisible (onTraining && ! hudNow);
        survivalButton.setVisible (onTraining && ! hudNow && unlocked);
        blitzButton.setVisible (onTraining && ! hudNow && unlocked);
        scoreLabel.setVisible (onTraining && ! hudNow);

        if (hudNow)
            runHud.set (session.getMode(), session.getLivesRemaining(),
                        session.getSecondsRemaining(), session.getRunScore());

        if (hudWasVisible != (onTraining && hudNow))
            resized();
    }

    // Whether this label has text decides whether the control row reserves
    // width for it, so a mode change has to re-lay the row out. Without
    // this, switching to Survival wrote "3 lives" into a zero-width box.
    const auto hadText = runStatusLabel.getText().isNotEmpty();

    const auto mode = session.getMode();

    const auto& theme = AbcTrainTheme::current();

    if (mode == SessionManager::Mode::practice)
    {
        runStatusLabel.setText ({}, juce::dontSendNotification);
        runStatusLabel.setColour (juce::Label::textColourId, theme.text);

        if (hadText)
            resized();

        return;
    }

    if (! session.isRunActive())
    {
        runStatusLabel.setText (localisation.getText ("ui.runOver") + "  "
                                     + juce::String (session.getRunScore()),
                                 juce::dontSendNotification);
        runStatusLabel.setColour (juce::Label::textColourId, theme.negative);
        return;
    }

    // While the run is actually being played, the HUD (above) carries the
    // lives/clock/score - the old text readout would say the same thing
    // twice, in a smaller voice. It also stays empty during the countdown,
    // when the run is armed but not yet anything to report on.
    runStatusLabel.setText ({}, juce::dontSendNotification);
    if (hadText)
        resized();
}

void EarTrainerEditor::paintOverChildren (juce::Graphics& g)
{
    if (screenFade >= 1.0f)
        return;

    // A veil that lifts, not a blackout that clears. The first version went
    // to full opacity, so a screen change read as the window blinking off
    // and back on rather than as one screen replacing another. Forty per
    // cent is enough to register as a transition and little enough that the
    // arriving screen is legible the whole way through.
    constexpr float veil = 0.4f;

    g.setColour (AbcTrainTheme::current().windowBackground
                     .withAlpha (veil * (1.0f - screenFade)));
    g.fillAll();
}

void EarTrainerEditor::beginScreenFade()
{
    screenFadeUpdater.removeAnimator (screenFadeAnimator);

    screenFade = 0.0f;

    screenFadeAnimator = juce::ValueAnimatorBuilder{}
                             .withEasing (juce::Easings::createEaseOut())
                             .withDurationMs (AbcTrainTheme::Duration::transition)
                             .withValueChangedCallback ([this] (float t)
                             {
                                 screenFade = t;
                                 repaint();
                             })
                             .build();

    screenFadeUpdater.addAnimator (screenFadeAnimator);
    screenFadeAnimator.start();
}

void EarTrainerEditor::showScreen (Screen screen)
{
    if (screen != currentScreen && hasShownAScreen)
        beginScreenFade();

    hasShownAScreen = true;
    currentScreen = screen;

    // Any auto-advance in flight belongs to a round the player is
    // leaving; cancel it so it can't fire into the home screen.
    ++pendingAdvanceId;

    const auto onHome = (screen == Screen::home);
    const auto onSupport = (screen == Screen::support);
    const auto onTraining = (screen == Screen::training);

    if (onHome)
        rebuildHomeSections();

    supportScreen.setVisible (onSupport);
    supportScreen.setBounds (getLocalBounds());
    homeScreen.setVisible (onHome);

    // Nothing to listen for outside a training.
    processor.setSignalEnabled (onTraining);

    // Everything that belongs to the training screen.
    // The title-row controls belong to Home and Training, not the
    // one-time support screen.
    for (auto* c : { (juce::Component*) &themeButton, (juce::Component*) &updateButton,
                     (juce::Component*) &trainingSoundsButton, (juce::Component*) &settingsButton,
                     (juce::Component*) &sizeSelector,
                     (juce::Component*) &languageSelector, (juce::Component*) &soundkorbLink })
    {
        c->setVisible (! onSupport);
    }

    for (auto* c : { (juce::Component*) &backButton, (juce::Component*) &gameIcon,
                     (juce::Component*) &currentGameLabel, (juce::Component*) &instructionLabel,
                     (juce::Component*) &feedbackLabel, (juce::Component*) &choiceSlider,
                     (juce::Component*) &practiceButton, (juce::Component*) &survivalButton,
                     (juce::Component*) &blitzButton,
                     (juce::Component*) &hintButton,
                     (juce::Component*) &beforeButton, (juce::Component*) &afterButton,
                     (juce::Component*) &runStatusLabel, (juce::Component*) &scoreLabel,
                     (juce::Component*) &levelProgressLabel,
                     (juce::Component*) &instructionsButton })
    {
        c->setVisible (onTraining);
    }

    // The instruction label additionally collapses once the exercise is
    // familiar; the flyup and pips manage their own visibility (the flyup
    // is only ever shown by an answer, the pips only by a live test), but
    // both must vanish when the screen does. A countdown mid-flight is
    // abandoned outright - starting a run into a screen the player just
    // left would be worse than not starting it.
    if (onTraining)
        instructionLabel.setVisible (shouldShowInstructions());
    if (! onTraining)
    {
        pointsFlyup.setVisible (false);
        promotionPips.setVisible (false);
        runCountdown.cancel();
    }

    // Pills vs. HUD is run state, and refreshRunStatus() is where run
    // state funnels; without this, returning to Training mid-run showed
    // the pills for a frame (or forever, if nothing else refreshed).
    refreshRunStatus();

    // Level, streak and the daily challenge belong to the screen you plan
    // from, not the one you answer on.
    dailyBanner.setVisible (onHome);
    donateLink.setVisible (onHome);

    resized();
    repaint();
}

void EarTrainerEditor::rebuildHomeSections()
{
    auto& gameManager = processor.getGameManager();
    auto& progress = processor.getProgressManager();

    const auto makeCard = [&] (int i)
    {
        const auto englishName = gameManager.getGame (i).getName();

        HomeScreenComponent::CardInfo card;
        card.gameIndex = i;
        card.name = translateGameName (englishName, localisation);
        card.benefit = translateGameBenefit (englishName, localisation);
        card.icon = AppIcons::iconForGameName (englishName);
        card.isCurrent = (i == gameManager.getActiveGameIndex());
        card.isFavourite = progress.isFavouriteGame (i);

        card.level = progress.getLevelForGame (i);
        card.levelProgress = progress.getLevelProgressForGame (i);
        card.promotionPending = progress.isPromotionPendingForGame (i);
        card.promotionStreak = progress.getPromotionStreakForGame (i);
        card.promotionTestLength = ProgressManager::promotionTestLength;
        card.accent = tintForGame (englishName);

        const auto stats = progress.getStatsForGame (i);
        // Spelled out. "10%  ·  29" made the reader work out which number
        // was which and what either measured.
        card.statsLine = stats.roundsPlayed == 0
                             ? juce::String()
                             : localisation.getText ("ui.tileStats",
                                                      { { "accuracy", juce::String (juce::roundToInt (stats.getAccuracy() * 100.0f)) },
                                                        { "rounds", juce::String (stats.roundsPlayed) } });

        return card;
    };

    // One flat grid, no category headings, no starring-into-its-own-group:
    // the star sorts a tile to the front instead of duplicating it into a
    // second section, which is what used to make the catalogue longer the
    // more you cared about.
    std::vector<HomeScreenComponent::CardInfo> cards;

    for (int i = 0; i < gameManager.getNumGames(); ++i)
        cards.push_back (makeCard (i));

    homeScreen.setLevelCaption (localisation.getText ("ui.levelWord"));
    homeScreen.setCards (std::move (cards));

    // Achievements as badges. progressTowards() is what lets a locked one
    // show how close it is rather than being an identical grey disc.
    const auto snapshot = progress.makeAchievementSnapshot();
    std::vector<HomeScreenComponent::BadgeInfo> badges;

    for (const auto& definition : Achievements::all())
    {
        HomeScreenComponent::BadgeInfo badge;
        badge.name = localisation.getText (definition.nameKey);
        badge.description = localisation.getText (definition.descriptionKey);
        badge.earned = progress.hasAchievement (definition.id);
        badge.progress = Achievements::progressTowards (definition, snapshot);
        badge.icon = definition.gameIndex >= 0 && definition.gameIndex < gameManager.getNumGames()
                         ? AppIcons::iconForGameName (gameManager.getGame (definition.gameIndex).getName())
                         : AppIcons::Icon::award;
        badge.tint = Achievements::colourForTier (definition.tier);

        badges.push_back (std::move (badge));
    }

    homeScreen.setBadges (std::move (badges));
    homeScreen.setBadgeStripCaption (
        localisation.getText ("ui.achievements") + "  "
            + juce::String (progress.getNumAchievementsEarned())
            + " / " + juce::String ((int) Achievements::all().size()));
}

void EarTrainerEditor::refreshLocalisedText()
{
    supportScreen.refresh();
    settingsScreen.refresh();
    settingsButton.setTooltip (localisation.getText ("ui.settings"));
    instructionsButton.setTooltip (localisation.getText ("ui.showInstructions"));
    donateLink.setButtonText (localisation.getText ("ui.support"));
    endRunButton.setButtonText (localisation.getText ("ui.endRun"));

    trainingSounds.setStrings (localisation.getText ("ui.trainingSounds"),
                                localisation.getText ("ui.soundsSource"),
                                localisation.getText ("ui.soundsTrainOn"),
                                localisation.getText ("ui.chooseFolder"),
                                localisation.getText ("ui.pinkNoise"),
                                localisation.getText ("ui.close"),
                                localisation.getText ("ui.soundsEmpty"),
                                localisation.getText ("ui.importAndSort"),
                                localisation.getText ("ui.importing"),
                                localisation.getText ("ui.importedClips"),
                                localisation.getText ("ui.importedNothing"),
                                localisation.getText ("ui.importHint"));

    titleLabel.setText (localisation.getText ("app.eartrainer.name"), juce::dontSendNotification);
    updateButton.setTooltip (localisation.getText ("ui.updates"));
    trainingSoundsButton.setTooltip (localisation.getText ("ui.trainingSounds"));
    backButton.setButtonText (localisation.getText ("ui.back"));

    {
        practiceButton.setButtonText (localisation.getText ("ui.modePractice"));
        survivalButton.setButtonText (localisation.getText ("ui.modeSurvival"));
        blitzButton.setButtonText (localisation.getText ("ui.modeBlitz"));
    }

    rebuildGameSelectorItems();
    rebuildHomeSections();
    refreshFromGameState();
    refreshFromProgressState();

    // The screen title is *drawn*, not a Label, so nothing above repaints
    // it. Without this the language changed everywhere except the one
    // word at the top left, which is the first thing anybody looks at.
    repaint();
}

void EarTrainerEditor::rebuildChoiceSlider()
{
    auto& game = processor.getGameManager().getActiveGame();

    juce::StringArray labels;
    for (int i = 0; i < game.getNumChoices(); ++i)
        labels.add (game.getChoiceLabel (i));

    // setChoices() itself resets to an unanswered/no-preview state - unlike
    // the old per-choice TextButtons (see decisions/014's fadeIn-collapse
    // bug), this is one persistent Component whose bounds resized() has
    // already assigned, so there's no destroy/recreate-before-layout
    // ordering hazard here to begin with.
    choiceSlider.setChoices (labels);

    if (game.usesContinuousScale())
    {
        // The ruler and its tolerance both come from the game, so the
        // widget needs no idea what a frequency or a decibel is.
        choiceSlider.setContinuousScale (game.getGridMarks(),
                                          game.getToleranceNormalised(),
                                          [&game] (float t) { return game.formatNormalisedValue (t); });
    }
    else
    {
        choiceSlider.setDiscreteScale();
    }

    // "< first - last >" tells the player which way the scale runs, which
    // matters most on the games whose labels aren't self-evidently ordered
    // (pan, width). Derived from the labels themselves, so it needs no
    // per-game table and can't go stale when a game's choices change.
    // A continuous scale labels its own axis densely, so the "< first -
    // last >" caption underneath would just repeat the two end marks.
    if (game.usesContinuousScale())
        choiceSlider.setAxisCaption ({});
    else if (labels.size() >= 2)
        choiceSlider.setAxisCaption (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9 "))
                                          + labels[0] + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "))
                                          + labels[labels.size() - 1]
                                          + juce::String (juce::CharPointer_UTF8 (" \xe2\x80\xba")));
    else
        choiceSlider.setAxisCaption ({});

    choiceSlider.setPlaceholderText (localisation.getText ("ui.dragToChoose"));
}

void EarTrainerEditor::choiceButtonClicked (int choiceIndex)
{
    if (! session.isRunActive())
        return;   // survival/blitz run is over - the answer would score nothing

    auto& game = processor.getGameManager().getActiveGame();
    game.submitAnswer (choiceIndex);
    afterAnswer (game.wasLastAnswerCorrect());
}

void EarTrainerEditor::setPlayProcessed (bool shouldPlayProcessed)
{
    processor.getGameManager().getActiveGame().setPlayProcessed (shouldPlayProcessed);
    refreshBeforeAfter();
}

void EarTrainerEditor::refreshBeforeAfter()
{
    auto& game = processor.getGameManager().getActiveGame();
    const auto onTraining = (currentScreen == Screen::training);
    const auto runOver = ! session.isRunActive();

    // When a run has ended, the restart button takes the A/B row's place:
    // comparing before and after is pointless once there's nothing left
    // to answer, and the one thing you *do* want is another go.
    restartButton.setVisible (onTraining && runOver);
    restartButton.setButtonText (localisation.getText ("ui.startAgain"));

    const auto supported = game.supportsBeforeAfter() && ! runOver;

    beforeButton.setVisible (supported && onTraining);
    afterButton.setVisible (supported && onTraining);

    if (! supported)
        return;

    beforeButton.setButtonText (translateAbLabel (game.getBeforeLabel(), localisation));
    afterButton.setButtonText (translateAbLabel (game.getAfterLabel(), localisation));

    // The active side is coloured; the other is left plain. Two buttons
    // where one is lit says "you are hearing this one" without asking the
    // player to read.
    const auto& theme = AbcTrainTheme::current();
    const auto processed = game.isPlayingProcessed();

    beforeButton.setColour (juce::TextButton::buttonColourId,
                             processed ? theme.widgetBackground : theme.accent.withAlpha (0.55f));
    afterButton.setColour (juce::TextButton::buttonColourId,
                            processed ? theme.accent.withAlpha (0.55f) : theme.widgetBackground);
}

void EarTrainerEditor::requestHint()
{
    if (hintRevealed)
        return;

    if (! session.spendHint())
    {
        // Refused - say why rather than doing nothing, the same rule the
        // "Updates" button had to learn (see decisions/014).
        hintButton.setButtonText (localisation.getText ("ui.hintTooExpensive"));

        juce::Component::SafePointer<EarTrainerEditor> safeThis (this);
        juce::Timer::callAfterDelay (2000, [safeThis]
        {
            if (safeThis == nullptr)
                return;

            safeThis->refreshHintButton();
        });
        return;
    }

    hintRevealed = true;
    vectorscope.reset();
    hintSpectrum.setSampleRate (processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0);

    refreshRunStatus();
    refreshHintButton();

    // The panel does not exist in the layout until now, so the window has
    // to grow to make room for it - resized() sets the scopes' bounds and
    // visibility from hintRevealed.
    applyWindowSize();
}

void EarTrainerEditor::refreshHintButton()
{
    // The price goes on the button, not next to it. A caption beside a
    // glyph made you read two things to answer one question, and made the
    // glyph itself look like a decoration rather than the control.
    if (hintRevealed)
    {
        hintButton.setEnabled (false);
        hintButton.setButtonText (localisation.getText ("ui.hintShown"));
        return;
    }

    hintButton.setEnabled (session.isRunActive());

    switch (session.getMode())
    {
        case SessionManager::Mode::survival:
            hintButton.setButtonText (localisation.getText ("ui.hintCostLife"));
            break;
        case SessionManager::Mode::blitz:
            hintButton.setButtonText (localisation.getText ("ui.hintCostSeconds",
                                                            { { "seconds", juce::String (SessionManager::blitzHintSeconds) } }));
            break;
        default:
            hintButton.setButtonText (localisation.getText ("ui.hintFree"));
            break;
    }
}

void EarTrainerEditor::afterAnswer (bool wasCorrect)
{
    session.registerAnswer (wasCorrect);
    refreshRunStatus();
    refreshBeforeAfter();

    // Auto-advance: the player shouldn't have to press a button between
    // every question. The delay is longer after a wrong answer (more to
    // read) and zero once a run has ended, so the final result stays on
    // screen instead of being replaced by another round.
    const auto delayMs = session.getAutoAdvanceDelayMs (wasCorrect);
    if (delayMs <= 0)
        return;

    const auto advanceId = ++pendingAdvanceId;
    juce::Component::SafePointer<EarTrainerEditor> safeThis (this);

    juce::Timer::callAfterDelay (delayMs, [safeThis, advanceId]
    {
        // Ignore if the editor closed, or if anything started a newer
        // round in the meantime (game switch, mode switch, manual New
        // Round) - otherwise a queued advance would land on a run the
        // player has already left.
        if (safeThis == nullptr || safeThis->pendingAdvanceId != advanceId)
            return;

        if (! safeThis->session.isRunActive())
            return;

        // A bought hint lasts one round. Without this it survived every
        // auto-advance, so one purchase quietly bought the rest of the
        // run - which is not what was paid for.
        safeThis->clearHint();
        safeThis->processor.getGameManager().getActiveGame().setPlayProcessed (false);
        safeThis->refreshBeforeAfter();
        safeThis->processor.getGameManager().getActiveGame().newRound();
    });
}

void EarTrainerEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    // A language change also needs the game-selector's item text and
    // title/button text rebuilt, which refreshFromGameState()/
    // refreshFromProgressState() alone don't cover - but rebuilding the
    // combo box's items on *every* game-answer/progress tick (not just a
    // language switch) would be wasteful and could visibly flicker it,
    // so only take that heavier path when LocalisationManager itself is
    // the broadcaster.
    if (source == &localisation)
    {
        refreshLocalisedText();
        return;
    }

    refreshFromGameState();
    refreshFromProgressState();
}

void EarTrainerEditor::refreshFromGameState()
{
    auto& game = processor.getGameManager().getActiveGame();

    // A difficulty change (via ProgressManager, on level-up) can change
    // the active game's choice count at runtime - currently only
    // ReverbGame does this. Only rebuild at the start of a fresh round,
    // not mid-reveal, so the button layout doesn't shift while showing
    // an answer.
    if (! game.hasAnswered() && choiceSlider.getNumChoices() != game.getNumChoices())
        rebuildChoiceSlider();

    gameIcon.setIcon (AppIcons::iconForGameName (game.getName()));

    // Must be refreshed here, not only in rebuildGameSelectorItems(): a
    // real bug caught by picking a card and watching the icon and
    // instructions change while the name above them kept showing the
    // previous exercise. rebuildGameSelectorItems() only runs on a
    // language change, which a game switch is not.
    currentGameLabel.setText (translateGameName (game.getName(), localisation),
                               juce::dontSendNotification);

    instructionLabel.setText (translateGameInstructions (game.getName(), game.getInstructions(), localisation),
                               juce::dontSendNotification);

    // The score line answers "am I getting better" - and now also "what
    // has to happen for the level to change", which nothing on screen
    // said before. Three states, because there are exactly three:
    // climbing toward the threshold, sitting the promotion test, or done.
    {
        auto& progress = processor.getProgressManager();
        const auto index = processor.getGameManager().getActiveGameIndex();

        const auto tally = localisation.getText ("ui.score",
                                                  { { "correct", juce::String (game.getScore()) },
                                                    { "total", juce::String (game.getRoundsPlayed()) } });

        juce::String levelLine;

        if (progress.isPromotionPendingForGame (index))
        {
            levelLine = localisation.getText ("ui.promotionTest",
                                               { { "done", juce::String (progress.getPromotionStreakForGame (index)) },
                                                 { "needed", juce::String (ProgressManager::promotionTestLength) } });
        }
        else if (progress.getLevelForGame (index) < ProgressManager::maxLevel)
        {
            const auto level = progress.getLevelForGame (index);
            const auto have = progress.getPointsForGame (index)
                                  - ProgressManager::pointsRequiredForLevel (level);
            const auto need = ProgressManager::pointsRequiredForLevel (level + 1)
                                  - ProgressManager::pointsRequiredForLevel (level);

            levelLine = localisation.getText ("ui.toNextLevel",
                                               { { "level", juce::String (level + 1) },
                                                 { "have", juce::String (juce::jmax (0, have)) },
                                                 { "need", juce::String (need) } });
        }
        else
        {
            levelLine = localisation.getText ("ui.levelMaxed");
        }

        // Two lines, not one: at 120px the pair was ellipsised down to
        // "До уровня 2: ..." - a progress readout that will not tell you
        // the progress.
        scoreLabel.setText (tally, juce::dontSendNotification);
        levelProgressLabel.setText (levelLine, juce::dontSendNotification);

        // Pips mirror whichever exercise is now active; the collapse rule
        // re-evaluates as answers accumulate, and a change of verdict
        // needs a re-layout since the exercise section's height depends
        // on it.
        if (currentScreen == Screen::training)
        {
            promotionPips.setVisible (progress.isPromotionPendingForGame (index));
            promotionPips.set (progress.getPromotionStreakForGame (index),
                               ProgressManager::promotionTestLength);

            const auto wantInstructions = shouldShowInstructions();
            if (instructionLabel.isVisible() != wantInstructions)
            {
                instructionLabel.setVisible (wantInstructions);
                resized();
            }
        }
    }

    if (game.hasAnswered())
    {
        if (game.usesContinuousScale())
            choiceSlider.showContinuousAnswer (game.getChosenNormalised(),
                                                game.getCorrectNormalised(),
                                                game.wasLastAnswerCorrect());
        else
            choiceSlider.showAnswer (game.getCorrectChoiceIndex(), game.getChosenChoiceIndex(), game.wasLastAnswerCorrect());

        feedbackLabel.setText (game.getFeedbackText(), juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, game.wasLastAnswerCorrect() ? AbcTrainTheme::current().positive : AbcTrainTheme::current().negative);
    }
    else
    {
        choiceSlider.resetForNewRound();
        feedbackLabel.setText ({}, juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, AbcTrainTheme::current().text);
    }
}

void EarTrainerEditor::refreshFromProgressState()
{
    auto& progress = processor.getProgressManager();

    // Built here rather than in ProgressManager, so both the sentence and
    // the exercise name inside it are in the player's own language.
    const auto challengeIndex = progress.getDailyChallengeGameIndex();
    const auto challengeEnglishName = processor.getGameManager().getGame (challengeIndex).getName();

    DailyBanner::State banner;
    banner.streakDays = progress.getStreakDays();
    banner.streakCaption = localisation.getText ("ui.streak",
                                                  { { "days", juce::String (progress.getStreakDays()) } });
    banner.challengeLine = localisation.getText (
        progress.isDailyChallengeComplete() ? "ui.dailyDone" : "ui.daily",
        { { "count", juce::String (progress.getDailyChallengeTargetStreak()) },
          { "game", translateGameName (challengeEnglishName, localisation) },
          { "bonus", juce::String (progress.getDailyChallengeBonusPoints()) } });

    banner.challengeTarget = progress.getDailyChallengeTargetStreak();
    // Capped at the target: the run that completes the challenge keeps
    // going, and a sixth filled pip on a five-pip row would be a bug on
    // screen rather than a bonus.
    banner.challengeDone = juce::jmin (banner.challengeTarget,
                                        progress.getConsecutiveCorrectForGame (challengeIndex));
    banner.bonusPoints = progress.getDailyChallengeBonusPoints();
    banner.challengeComplete = progress.isDailyChallengeComplete();
    banner.challengeAccent = tintForGame (challengeEnglishName);

    dailyBanner.setState (std::move (banner));
}
