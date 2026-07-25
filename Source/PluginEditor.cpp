#include "PluginEditor.h"
#include "../shared/Version.h"
#include <array>
#include <memory>

namespace
{
    // Muted, theme-consistent tones (see shared/AbcTrainLookAndFeel.cpp's
    // scheme: #5b9bd5 blue, #d98c5f orange) instead of JUCE's stock
    // Colours::limegreen/orangered, which read as garish/cartoonish
    // against this project's otherwise sophisticated dark palette - a
    // real design complaint, not just a style nitpick.
    const juce::Colour correctColour { 0xff5fbf7d };
    const juce::Colour wrongColour { 0xffd9615f };
    const juce::Colour neutralButtonColour { 0xff2a2a3a };
    const juce::Colour bodyTextColour { 0xffe0e0e0 };
    const juce::Colour mutedTextColour { 0xffa0a0b0 };

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
    };

    const std::array<GameI18nKeys, 9> gameI18nKeys {{
        { "Guess the Band",             "game.eq.name",             "game.eq.instructions" },
        { "Guess the Compression",      "game.compression.name",    "game.compression.instructions" },
        { "Guess the Reverb",           "game.reverb.name",         "game.reverb.instructions" },
        { "Guess the Pan Position",     "game.pan.name",            "game.pan.instructions" },
        { "Guess the Delay Time",       "game.delay.name",          "game.delay.instructions" },
        { "Guess the Distortion",       "game.distortion.name",     "game.distortion.instructions" },
        { "Guess the Stereo Width",     "game.stereowidth.name",    "game.stereowidth.instructions" },
        { "Guess the Gain Change",      "game.db.name",             "game.db.instructions" },
        { "Name the Range",             "game.frequencyrange.name", "game.frequencyrange.instructions" }
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
}

EarTrainerEditor::EarTrainerEditor (EarTrainerProcessor& p)
    : AudioProcessorEditor (&p),
      localisationProperties (LocalisationManager::makeDefaultOptions()),
      localisation (localisationProperties),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setFont (AbcTrainLookAndFeel::titleFont());
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    for (const auto& code : LocalisationManager::getSupportedLanguageCodes())
        languageSelector.addItem (LocalisationManager::getDisplayName (code),
                                   LocalisationManager::getSupportedLanguageCodes().indexOf (code) + 1);
    languageSelector.setSelectedId (LocalisationManager::getSupportedLanguageCodes().indexOf (localisation.getCurrentLanguage()) + 1,
                                     juce::dontSendNotification);
    languageSelector.onChange = [this] { languageSelected(); };
    addAndMakeVisible (languageSelector);

    auto& gameManager = processor.getGameManager();
    rebuildGameSelectorItems();
    gameSelector.setSelectedId (gameManager.getActiveGameIndex() + 1, juce::dontSendNotification);
    gameSelector.onChange = [this] { gameSelected(); };
    addAndMakeVisible (gameSelector);

    instructionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (instructionLabel);

    newRoundButton.onClick = [this] { processor.getGameManager().getActiveGame().newRound(); };
    addAndMakeVisible (newRoundButton);

    scoreLabel.setJustificationType (juce::Justification::centredLeft);
    scoreLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (scoreLabel);

    feedbackLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    feedbackLabel.setColour (juce::Label::textColourId, bodyTextColour);
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
    streakLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd98c5f));
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

    // setSize() before rebuildChoiceButtons(): the choice buttons' fade-in
    // (inside rebuildChoiceButtons()) needs real bounds already assigned
    // by resized() - see the comment there for why.
    setSize (640, 468);

    rebuildChoiceButtons();
    refreshFromGameState();
    refreshFromProgressState();
    refreshLocalisedText();
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
    g.fillAll (juce::Colour (0xff1e1e2e));
}

void EarTrainerEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    auto titleRow = area.removeFromTop (32);
    languageSelector.setBounds (titleRow.removeFromRight (90));
    titleRow.removeFromRight (8);
    updateButton.setBounds (titleRow.removeFromRight (80));
    titleRow.removeFromRight (8);
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);
    gameSelector.setBounds (area.removeFromTop (28).withSizeKeepingCentre (280, 24));
    area.removeFromTop (8);
    instructionLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    feedbackLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);

    auto buttonRow = area.removeFromTop (56);
    if (! choiceButtons.isEmpty())
    {
        const auto buttonWidth = buttonRow.getWidth() / choiceButtons.size();
        for (auto* button : choiceButtons)
            button->setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (4));
    }

    area.removeFromTop (16);

    auto bottomRow = area.removeFromTop (36);
    newRoundButton.setBounds (bottomRow.removeFromLeft (140));
    scoreLabel.setBounds (bottomRow.reduced (8, 0));

    area.removeFromTop (16);

    auto progressRow = area.removeFromTop (24);
    levelLabel.setBounds (progressRow.removeFromLeft (100));
    levelSelector.setBounds (progressRow.removeFromLeft (50).reduced (0, 2));
    streakLabel.setBounds (progressRow.removeFromRight (100));
    levelProgressBar.setBounds (progressRow.reduced (8, 4));

    area.removeFromTop (8);
    dailyChallengeLabel.setBounds (area.removeFromTop (20));
}

void EarTrainerEditor::gameSelected()
{
    auto& gameManager = processor.getGameManager();
    gameManager.getActiveGame().removeChangeListener (this);

    gameManager.setActiveGameIndex (gameSelector.getSelectedId() - 1);

    gameManager.getActiveGame().addChangeListener (this);
    rebuildChoiceButtons();
    refreshFromGameState();
    resized();
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
    auto& gameManager = processor.getGameManager();
    const auto selectedId = gameSelector.getSelectedId();

    gameSelector.clear (juce::dontSendNotification);
    const auto names = gameManager.getGameNames();
    for (int i = 0; i < names.size(); ++i)
        gameSelector.addItem (translateGameName (names[i], localisation), i + 1); // ComboBox item IDs are 1-based

    if (selectedId > 0)
        gameSelector.setSelectedId (selectedId, juce::dontSendNotification);
}

void EarTrainerEditor::refreshLocalisedText()
{
    titleLabel.setText (localisation.getText ("app.eartrainer.name"), juce::dontSendNotification);
    updateButton.setButtonText (localisation.getText ("ui.updates"));
    rebuildGameSelectorItems();
    refreshFromGameState();
    refreshFromProgressState();
}

void EarTrainerEditor::rebuildChoiceButtons()
{
    choiceButtons.clear();

    auto& game = processor.getGameManager().getActiveGame();

    for (int i = 0; i < game.getNumChoices(); ++i)
    {
        auto* button = choiceButtons.add (new juce::TextButton (game.getChoiceLabel (i)));
        button->onClick = [this, i] { choiceButtonClicked (i); };
        addAndMakeVisible (button);
    }

    // Real bug, found by actually running the app and clicking a choice:
    // juce::ComponentAnimator::fadeIn() snapshots component->getBounds()
    // as the animation's destination rectangle, and forces the component
    // back to that exact rectangle once the fade completes (see
    // ComponentAnimator::AnimationTask::moveToFinalDestination() in
    // JUCE's own source). Starting the fade before these fresh buttons
    // had ever been through resized() meant that snapshot was
    // (0, 0, 0, 0) - so ~200ms after appearing to work, every choice
    // button silently collapsed to zero size: invisible and unclickable,
    // in every game, every time the buttons regenerated. resized() here
    // gives them their real bounds *before* fadeIn() ever looks at them.
    resized();

    auto& animator = juce::Desktop::getInstance().getAnimator();
    for (auto* button : choiceButtons)
    {
        // Basic fade-in whenever the choice buttons regenerate (switching
        // games, or a mid-session choice-count change like ReverbGame's
        // difficulty tiers) - one deliberately simple animation for this
        // pass, per the redesign's own iterative scope; see
        // decisions/009-look-and-feel.md for what else was deferred.
        button->setAlpha (0.0f);
        animator.fadeIn (button, 200);
    }
}

void EarTrainerEditor::choiceButtonClicked (int choiceIndex)
{
    processor.getGameManager().getActiveGame().submitAnswer (choiceIndex);
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
    if (! game.hasAnswered() && choiceButtons.size() != game.getNumChoices())
    {
        rebuildChoiceButtons();
        resized();
    }

    instructionLabel.setText (translateGameInstructions (game.getName(), game.getInstructions(), localisation),
                               juce::dontSendNotification);

    scoreLabel.setText (localisation.getText ("ui.score", { { "correct", juce::String (game.getScore()) },
                                                             { "total", juce::String (game.getRoundsPlayed()) } }),
                         juce::dontSendNotification);

    for (auto* button : choiceButtons)
    {
        button->setColour (juce::TextButton::buttonColourId, neutralButtonColour);
        button->setEnabled (! game.hasAnswered());
    }

    if (game.hasAnswered())
    {
        choiceButtons[game.getCorrectChoiceIndex()]->setColour (juce::TextButton::buttonColourId, correctColour);

        if (! game.wasLastAnswerCorrect())
            choiceButtons[game.getChosenChoiceIndex()]->setColour (juce::TextButton::buttonColourId, wrongColour);

        feedbackLabel.setText (game.getFeedbackText(), juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, game.wasLastAnswerCorrect() ? correctColour : wrongColour);
    }
    else
    {
        feedbackLabel.setText ({}, juce::dontSendNotification);
        feedbackLabel.setColour (juce::Label::textColourId, bodyTextColour);
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
                                    progress.isDailyChallengeComplete() ? correctColour : mutedTextColour);
}
