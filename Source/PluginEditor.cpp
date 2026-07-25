#include "PluginEditor.h"
#include "../shared/Version.h"
#include <array>
#include <memory>

namespace
{
    // The key the light/dark choice is stored under, in the same shared
    // "abcTrain" PropertiesFile the language preference already uses.
    constexpr const char* themeModeKey = "themeMode";

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

    setLookAndFeel (&lookAndFeel);

    // The title is drawn by paint() with letter-spacing rather than by a
    // Label, since JUCE offers no tracking control on Label - see
    // AbcTrainLookAndFeel::drawTrackedText.
    titleLabel.setFont (AbcTrainLookAndFeel::titleFont());
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setVisible (false);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

    for (const auto& code : LocalisationManager::getSupportedLanguageCodes())
        languageSelector.addItem (LocalisationManager::getDisplayName (code),
                                   LocalisationManager::getSupportedLanguageCodes().indexOf (code) + 1);
    languageSelector.setSelectedId (LocalisationManager::getSupportedLanguageCodes().indexOf (localisation.getCurrentLanguage()) + 1,
                                     juce::dontSendNotification);
    languageSelector.onChange = [this] { languageSelected(); };
    addAndMakeVisible (languageSelector);

    addAndMakeVisible (gameIcon);

    auto& gameManager = processor.getGameManager();

    currentGameLabel.setJustificationType (juce::Justification::centredLeft);
    currentGameLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    addAndMakeVisible (currentGameLabel);

    gameSelectorButton.onClick = [this]
    {
        rebuildGamePickerCards();
        gamePicker.setVisible (true);
    };
    addAndMakeVisible (gameSelectorButton);

    gamePicker.onGameChosen = [this] (int index)
    {
        gamePicker.setVisible (false);
        auto& gm = processor.getGameManager();
        if (index == gm.getActiveGameIndex())
            return;

        gm.getActiveGame().removeChangeListener (this);
        gm.setActiveGameIndex (index);
        gm.getActiveGame().addChangeListener (this);

        rebuildChoiceSlider();
        refreshFromGameState();
        resized();
    };
    gamePicker.onClosed = [this] { resized(); };

    instructionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (instructionLabel);

    newRoundButton.onClick = [this] { startNewRun(); };
    addAndMakeVisible (newRoundButton);

    modeSelector.addItem (localisation.getText ("ui.modePractice"), 1);
    modeSelector.addItem (localisation.getText ("ui.modeSurvival"), 2);
    modeSelector.addItem (localisation.getText ("ui.modeBlitz"), 3);
    modeSelector.setSelectedId (1, juce::dontSendNotification);
    modeSelector.onChange = [this] { modeSelected(); };
    addAndMakeVisible (modeSelector);

    runStatusLabel.setJustificationType (juce::Justification::centredRight);
    runStatusLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (runStatusLabel);

    session.onRunEnded = [this] (int finalScore)
    {
        auto& progress = processor.getProgressManager();
        const auto gameIndex = processor.getGameManager().getActiveGameIndex();

        if (session.getMode() == SessionManager::Mode::survival)
            progress.recordSurvivalScore (gameIndex, finalScore);
        else if (session.getMode() == SessionManager::Mode::blitz)
            progress.recordBlitzScore (gameIndex, finalScore);
    };

    // 1 Hz is all the Blitz clock needs, and it's the only thing on this
    // timer - no reason to run the whole editor at animation rate.
    startTimerHz (1);

    scoreLabel.setJustificationType (juce::Justification::centredLeft);
    scoreLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (scoreLabel);

    feedbackLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    addAndMakeVisible (feedbackLabel);

    levelLabel.setJustificationType (juce::Justification::centredLeft);
    levelLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (levelLabel);

    for (int lvl = 1; lvl <= ProgressManager::maxLevel; ++lvl)
        levelSelector.addItem (juce::String (lvl), lvl); // ComboBox item IDs match the level number directly
    levelSelector.onChange = [this] { levelSelected(); };
    addAndMakeVisible (levelSelector);

    addAndMakeVisible (levelProgressBar);

    streakLabel.setJustificationType (juce::Justification::centredRight);
    streakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (streakLabel);

    dailyChallengeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (dailyChallengeLabel);

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
        updateButton.setButtonText (localisation.getText ("ui.checkingForUpdates"));

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
                safeThis->updateButton.setButtonText (upToDateText);
                juce::Timer::callAfterDelay (2500, [safeThis, updatesText]
                {
                    if (safeThis != nullptr)
                        safeThis->updateButton.setButtonText (updatesText);
                });
                return;
            }

            safeThis->updateButton.setButtonText (updatesText);

            const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
                juce::MessageBoxIconType::InfoIcon,
                updateAvailableText,
                "Version " + release.tagName + " is available - you're on " + juce::String (CurrentVersion::string) + ".",
                openReleasePageText, laterText,
                safeThis.getComponent());

            juce::AlertWindow::showAsync (options, [release] (int result)
            {
                // makeOptionsOkCancel adds two buttons; per AlertWindow's
                // documented N-button result mapping, button[0] ("Open
                // Release Page") returns 1, button[1] ("Later") returns 0.
                if (result == 1)
                    juce::URL (release.htmlUrl).launchInDefaultBrowser();
            });
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
            safeThis->updateButton.setButtonText (checkFailedText);

            juce::Timer::callAfterDelay (2500, [safeThis, updatesText]
            {
                if (safeThis != nullptr)
                    safeThis->updateButton.setButtonText (updatesText);
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
    addAndMakeVisible (choiceSlider);

    // Added last so it paints on top of every other child, same as
    // shared/LessonController's integration in the Learner editors - JUCE
    // paints child components in the order they were added, and this
    // overlay needs to cover the choice slider/instructions/etc. whenever
    // it's shown. A real bug, found by actually running the app: adding
    // this earlier (before choiceSlider) left the slider painting on top
    // of the "closed" overlay instead of the other way around.
    addChildComponent (trainingSounds);
    trainingSounds.onClosed = [this] { resized(); };

    addChildComponent (gamePicker);

    // Grown again for the grouping/whitespace pass: the three section
    // panels each carry their own padding and caption, which is what buys
    // the "everything breathes" feel, and that space has to come from
    // somewhere. Same "grew the window to fit new content" precedent as
    // the slider redesign (015) and the Learner guide labels (010).
    setSize (680, 664);

    applyTheme();
    session.startRun();
    rebuildChoiceSlider();
    refreshFromGameState();
    refreshFromProgressState();
    refreshLocalisedText();
}

void EarTrainerEditor::applyTheme()
{
    const auto& theme = AbcTrainTheme::current();

    // Widgets that set their own colours can't be reached by the
    // LookAndFeel's colour scheme, so they're refreshed here instead - and
    // every one of them now reads the palette rather than a literal, which
    // is what makes the light theme possible at all.
    instructionLabel.setColour (juce::Label::textColourId, theme.textDim);
    scoreLabel.setColour (juce::Label::textColourId, theme.text);
    levelLabel.setColour (juce::Label::textColourId, theme.text);
    streakLabel.setColour (juce::Label::textColourId, theme.accentWarm);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, theme.accent);

    themeButton.setButtonText (theme.mode == AbcTrainTheme::Mode::light ? "Dark" : "Light");

    // These two are re-coloured per answer/progress state, so just let
    // the normal refresh paths reapply them from the new palette.
    refreshFromGameState();
    refreshFromProgressState();

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
    processor.getGameManager().getActiveGame().removeChangeListener (this);
    processor.getProgressManager().removeChangeListener (this);
    localisation.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void EarTrainerEditor::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    // Grouped sections behind the controls: related things now sit on a
    // shared surface with a caption, instead of floating loose on the
    // backdrop with only whitespace implying the grouping.
    AbcTrainLookAndFeel::paintSectionPanel (g, exerciseSection.toFloat(), "Exercise");
    AbcTrainLookAndFeel::paintSectionPanel (g, answerSection.toFloat(), "Your answer");
    AbcTrainLookAndFeel::paintSectionPanel (g, progressSection.toFloat(), "Progress");

    // The title, letter-spaced. Drawn here rather than through a Label
    // because JUCE exposes no tracking control on Label/drawText.
    AbcTrainLookAndFeel::drawTrackedText (
        g, titleLabel.getText(),
        juce::Rectangle<float> ((float) AbcTrainTheme::Spacing::large,
                                 (float) AbcTrainTheme::Spacing::medium,
                                 (float) getWidth() * 0.5f, 32.0f),
        AbcTrainLookAndFeel::titleFont(), theme.textBright, 1.8f,
        juce::Justification::centredLeft);
}

void EarTrainerEditor::resized()
{
    using namespace AbcTrainTheme;

    auto area = getLocalBounds().reduced (Spacing::large);

    // --- title row: identity on the left, global controls on the right ---
    auto titleRow = area.removeFromTop (32);
    languageSelector.setBounds (titleRow.removeFromRight (86));
    titleRow.removeFromRight (Spacing::small);
    themeButton.setBounds (titleRow.removeFromRight (62));
    titleRow.removeFromRight (Spacing::small);
    updateButton.setBounds (titleRow.removeFromRight (76));
    titleRow.removeFromRight (Spacing::small);
    trainingSoundsButton.setBounds (titleRow.removeFromRight (118));

    area.removeFromTop (Spacing::section);

    // --- exercise section: which game, and what you're listening for ---
    exerciseSection = area.removeFromTop (124);
    {
        auto inner = exerciseSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);   // clear the section caption

        auto gameRow = inner.removeFromTop (28);
        gameIcon.setBounds (gameRow.removeFromLeft (26));
        gameRow.removeFromLeft (Spacing::small);
        gameSelectorButton.setBounds (gameRow.removeFromRight (130));
        gameRow.removeFromRight (Spacing::small);
        currentGameLabel.setBounds (gameRow);

        inner.removeFromTop (Spacing::small);
        instructionLabel.setBounds (inner);
    }

    area.removeFromTop (Spacing::medium);

    // --- answer section: feedback, the slider itself, score/new round ---
    answerSection = area.removeFromTop (274);
    {
        auto inner = answerSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        feedbackLabel.setBounds (inner.removeFromTop (26));
        inner.removeFromTop (Spacing::small);

        // The scale needs real height to read as a panel of zones rather
        // than a thin strip: 40px of it is the value readout and 18px the
        // caption, so anything under ~120 leaves the zones too shallow to
        // fit staggered labels. Found by building it at 92 and looking.
        choiceSlider.setBounds (inner.removeFromTop (150).reduced (Spacing::small, 0));

        inner.removeFromTop (Spacing::medium);

        auto bottomRow = inner.removeFromTop (34);
        newRoundButton.setBounds (bottomRow.removeFromLeft (128));
        bottomRow.removeFromLeft (Spacing::small);
        modeSelector.setBounds (bottomRow.removeFromLeft (124));
        runStatusLabel.setBounds (bottomRow.removeFromRight (128));
        scoreLabel.setBounds (bottomRow.reduced (Spacing::medium, 0));
    }

    area.removeFromTop (Spacing::medium);

    // --- progress section: level, bar, streak, daily challenge ---
    progressSection = area.removeFromTop (100);
    {
        auto inner = progressSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        auto progressRow = inner.removeFromTop (26);
        levelLabel.setBounds (progressRow.removeFromLeft (92));
        levelSelector.setBounds (progressRow.removeFromLeft (54).reduced (0, 1));
        streakLabel.setBounds (progressRow.removeFromRight (96));
        levelProgressBar.setBounds (progressRow.reduced (Spacing::medium, 8));

        inner.removeFromTop (Spacing::small);
        dailyChallengeLabel.setBounds (inner.removeFromTop (20));
    }

    // --- footer ---
    soundkorbLink.setBounds (area.removeFromBottom (18).removeFromRight (130));

    // Unconditional, same as shared/LessonController's integration in the
    // Learner editors - whether or not they're currently visible.
    trainingSounds.setBounds (getLocalBounds());
    gamePicker.setBounds (getLocalBounds());
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

void EarTrainerEditor::levelSelected()
{
    // ComboBox item IDs were set to the level number directly (1..10),
    // so the selected ID *is* the target level.
    processor.getProgressManager().setLevelManually (levelSelector.getSelectedId());
}

void EarTrainerEditor::rebuildGameSelectorItems()
{
    // The nine-name ComboBox is gone (see GamePickerComponent); what's
    // left in the title area is the *current* exercise's name plus a
    // button that opens the card grid.
    auto& gameManager = processor.getGameManager();
    currentGameLabel.setText (translateGameName (gameManager.getActiveGame().getName(), localisation),
                               juce::dontSendNotification);
    gameSelectorButton.setButtonText (localisation.getText ("ui.chooseTraining"));
}

void EarTrainerEditor::timerCallback()
{
    if (session.getMode() != SessionManager::Mode::blitz || ! session.isRunActive())
        return;

    if (session.tickOneSecond())
        refreshFromGameState();   // the clock just ended the run

    refreshRunStatus();
}

void EarTrainerEditor::modeSelected()
{
    // setMode() starts a fresh run itself, so this is one call, not two.
    switch (modeSelector.getSelectedId())
    {
        case 2:  session.setMode (SessionManager::Mode::survival); break;
        case 3:  session.setMode (SessionManager::Mode::blitz); break;
        default: session.setMode (SessionManager::Mode::practice); break;
    }

    startNewRun();
}

void EarTrainerEditor::startNewRun()
{
    // Any auto-advance still in flight belongs to the run being replaced.
    ++pendingAdvanceId;

    if (! session.isRunActive())
        session.startRun();

    processor.getGameManager().getActiveGame().newRound();
    refreshRunStatus();
}

void EarTrainerEditor::refreshRunStatus()
{
    const auto mode = session.getMode();

    if (mode == SessionManager::Mode::practice)
    {
        runStatusLabel.setText ({}, juce::dontSendNotification);
        return;
    }

    const auto& theme = AbcTrainTheme::current();

    if (! session.isRunActive())
    {
        runStatusLabel.setText (localisation.getText ("ui.runOver") + "  "
                                     + juce::String (session.getRunScore()),
                                 juce::dontSendNotification);
        runStatusLabel.setColour (juce::Label::textColourId, theme.negative);
        return;
    }

    if (mode == SessionManager::Mode::survival)
    {
        // Lives as filled/empty pips rather than a number - readable at a
        // glance, which is the point of a life counter.
        // Wrapped in juce::String explicitly: juce::String has no
        // unambiguous operator+= for a raw CharPointer_UTF8, and the plain
        // const char* overload would not treat these as UTF-8 (the same
        // gotcha that mojibake'd the language display names, see ADR 011).
        const juce::String filledPip (juce::CharPointer_UTF8 ("\xe2\x97\x8f"));   // U+25CF
        const juce::String emptyPip  (juce::CharPointer_UTF8 ("\xe2\x97\x8b"));   // U+25CB

        juce::String pips;
        for (int i = 0; i < SessionManager::survivalLives; ++i)
            pips += (i < session.getLivesRemaining()) ? filledPip : emptyPip;

        runStatusLabel.setText (pips + "   " + juce::String (session.getRunScore()),
                                 juce::dontSendNotification);
        runStatusLabel.setColour (juce::Label::textColourId,
                                   session.getLivesRemaining() <= 1 ? theme.negative : theme.text);
        return;
    }

    const auto seconds = session.getSecondsRemaining();
    runStatusLabel.setText (juce::String (seconds) + "s   " + juce::String (session.getRunScore()),
                             juce::dontSendNotification);
    runStatusLabel.setColour (juce::Label::textColourId,
                               seconds <= 10 ? theme.negative : theme.text);
}

void EarTrainerEditor::rebuildGamePickerCards()
{
    auto& gameManager = processor.getGameManager();
    auto& progress = processor.getProgressManager();

    std::vector<GamePickerComponent::CardInfo> cards;
    cards.reserve ((size_t) gameManager.getNumGames());

    for (int i = 0; i < gameManager.getNumGames(); ++i)
    {
        const auto englishName = gameManager.getGame (i).getName();
        const auto stats = progress.getStatsForGame (i);

        GamePickerComponent::CardInfo card;
        card.name = translateGameName (englishName, localisation);
        card.benefit = translateGameBenefit (englishName, localisation);
        card.icon = AppIcons::iconForGameName (englishName);
        card.isCurrent = (i == gameManager.getActiveGameIndex());

        card.statsLine = stats.roundsPlayed == 0
                             ? localisation.getText ("ui.notPlayedYet")
                             : localisation.getText ("ui.accuracy") + ": "
                                   + juce::String (juce::roundToInt (stats.getAccuracy() * 100.0f)) + "%   "
                                   + localisation.getText ("ui.bestStreak") + ": "
                                   + juce::String (stats.bestStreak);

        cards.push_back (std::move (card));
    }

    gamePicker.setHeading (localisation.getText ("ui.chooseTraining"));
    gamePicker.setCards (std::move (cards));
}

void EarTrainerEditor::refreshLocalisedText()
{
    titleLabel.setText (localisation.getText ("app.eartrainer.name"), juce::dontSendNotification);
    updateButton.setButtonText (localisation.getText ("ui.updates"));

    {
        const auto selected = modeSelector.getSelectedId();
        modeSelector.clear (juce::dontSendNotification);
        modeSelector.addItem (localisation.getText ("ui.modePractice"), 1);
        modeSelector.addItem (localisation.getText ("ui.modeSurvival"), 2);
        modeSelector.addItem (localisation.getText ("ui.modeBlitz"), 3);
        modeSelector.setSelectedId (selected > 0 ? selected : 1, juce::dontSendNotification);
    }

    rebuildGameSelectorItems();
    refreshFromGameState();
    refreshFromProgressState();
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

    // "< first - last >" tells the player which way the scale runs, which
    // matters most on the games whose labels aren't self-evidently ordered
    // (pan, width). Derived from the labels themselves, so it needs no
    // per-game table and can't go stale when a game's choices change.
    if (labels.size() >= 2)
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

    const auto wasCorrect = game.wasLastAnswerCorrect();
    session.registerAnswer (wasCorrect);
    refreshRunStatus();

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

        if (safeThis->session.isRunActive())
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

    scoreLabel.setText (localisation.getText ("ui.score", { { "correct", juce::String (game.getScore()) },
                                                             { "total", juce::String (game.getRoundsPlayed()) } }),
                         juce::dontSendNotification);

    if (game.hasAnswered())
    {
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

    levelLabel.setText (localisation.getText ("ui.level", { { "level", juce::String (progress.getLevel()) } }),
                         juce::dontSendNotification);
    levelSelector.setSelectedId (progress.getLevel(), juce::dontSendNotification);
    levelProgressBar.setProgress (progress.getLevelProgressProportion());

    streakLabel.setText (localisation.getText ("ui.streak", { { "days", juce::String (progress.getStreakDays()) } }),
                          juce::dontSendNotification);

    dailyChallengeLabel.setText (progress.getDailyChallengeDescription(), juce::dontSendNotification);
    dailyChallengeLabel.setColour (juce::Label::textColourId,
                                    progress.isDailyChallengeComplete() ? AbcTrainTheme::current().positive : AbcTrainTheme::current().textDim);
}
