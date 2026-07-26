#include "PluginEditor.h"
#include "../shared/Version.h"
#include <array>
#include <memory>

namespace
{
    // The key the light/dark choice is stored under, in the same shared
    // "abcTrain" PropertiesFile the language preference already uses.
    constexpr const char* themeModeKey = "themeMode";
    constexpr const char* uiScaleKey = "uiScale";
    constexpr const char* seenSupportKey = "seenSupportScreen";

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
        if (englishName == "Guess the Band" || englishName == "Name the Range")
            return juce::Colour (0xff4fa3c7);   // frequency - cool blue

        if (englishName == "Guess the Compression" || englishName == "Guess the Gain Change")
            return juce::Colour (0xffc77f4f);   // dynamics - warm amber

        if (englishName == "Guess the Reverb" || englishName == "Guess the Pan Position"
            || englishName == "Guess the Delay Time" || englishName == "Guess the Stereo Width")
            return juce::Colour (0xff5fb98c);   // space - green

        return juce::Colour (0xffa878c9);       // character - violet
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

    sizeSelector.addItem ("S", 1);
    sizeSelector.addItem ("M", 2);
    sizeSelector.addItem ("L", 3);
    sizeSelector.addItem ("XL", 4);
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
        // out of the way, not to make you confirm twice.
        showScreen (Screen::training);
        startNewRun();
    };

    homeScreen.onFavouriteToggled = [this] (int index, bool shouldBeFavourite)
    {
        processor.getProgressManager().setFavouriteGame (index, shouldBeFavourite);
        rebuildHomeSections();
    };

    supportScreen.onDismissed = [this]
    {
        localisationProperties.setValue (seenSupportKey, true);
        showScreen (Screen::home);
    };
    addChildComponent (supportScreen);

    homeViewport.setViewedComponent (&homeScreen, false);
    homeViewport.setScrollBarsShown (true, false);
    homeViewport.setScrollBarThickness (8);
    addAndMakeVisible (homeViewport);

    instructionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (instructionLabel);


    restartButton.onClick = [this] { startNewRun(); };
    addChildComponent (restartButton);

    beforeButton.onClick = [this] { setPlayProcessed (false); };
    afterButton.onClick = [this] { setPlayProcessed (true); };
    beforeButton.setClickingTogglesState (false);
    afterButton.setClickingTogglesState (false);
    addAndMakeVisible (beforeButton);
    addAndMakeVisible (afterButton);

    hintButton.onClick = [this] { requestHint(); };
    addAndMakeVisible (hintButton);

    // The price stays as text beside the icon: an icon alone cannot say
    // "this costs you a life", and a cost you discover after paying is
    // exactly the thing decisions/014 was written about.
    hintCostLabel.setJustificationType (juce::Justification::centredLeft);
    hintCostLabel.setFont (AbcTrainLookAndFeel::captionFont());
    addAndMakeVisible (hintCostLabel);

    processor.setVectorscope (&vectorscope);
    processor.setSpectrumAnalyzer (&hintSpectrum);
    addChildComponent (vectorscope);
    addChildComponent (hintSpectrum);

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
    addChildComponent (trainingSounds);
    trainingSounds.onClosed = [this] { resized(); };

    // Grown again for the grouping/whitespace pass: the three section
    // panels each carry their own padding and caption, which is what buys
    // the "everything breathes" feel, and that space has to come from
    // somewhere. Same "grew the window to fit new content" precedent as
    // the slider redesign (015) and the Learner guide labels (010).
    // Restore the persisted UI scale; the logical layout never changes,
    // only the transform applied to it.
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
    showScreen (localisationProperties.getBoolValue (seenSupportKey, false)
                    ? Screen::home : Screen::support);
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

    // The icon shows the mode you'd switch *to*, and morphs between the
    // two rather than cutting - see IconButton.
    themeButton.setIcon (theme.mode == AbcTrainTheme::Mode::light ? AppIcons::Icon::moon
                                                                  : AppIcons::Icon::sun);
    themeButton.setTooltip (localisation.getText (theme.mode == AbcTrainTheme::Mode::light
                                                       ? "ui.themeDark" : "ui.themeLight"));
    hintCostLabel.setColour (juce::Label::textColourId, theme.textDim);

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

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

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
        AbcTrainLookAndFeel::paintSectionHeading (g, progressSection.toFloat(),
                                                   localisation.getText ("ui.sectionProgress"));

        // The scope strip keeps its room whether or not it's been bought,
        // so the window never resizes mid-round. Unbought, it draws as a
        // quiet placeholder rather than an empty hole.
        if (! hintRevealed && ! scopeStrip.isEmpty())
        {
            const auto& theme = AbcTrainTheme::current();
            const auto strip = scopeStrip.toFloat();

            g.setColour (theme.displayBackground.withAlpha (0.45f));
            g.fillRoundedRectangle (strip, AbcTrainTheme::Radius::well);
            g.setColour (theme.outline.withAlpha (0.35f));
            g.drawRoundedRectangle (strip, AbcTrainTheme::Radius::well, 1.0f);

            g.setColour (theme.textDim.withAlpha (0.5f));
            g.setFont (AbcTrainLookAndFeel::captionFont());
            g.drawText (localisation.getText ("ui.hintPlaceholder"), strip,
                         juce::Justification::centred, false);
        }
    }

    // The title, letter-spaced. Drawn here rather than through a Label
    // because JUCE exposes no tracking control on Label/drawText. The
    // home screen paints its own header, so this would double up there.
    if (currentScreen == Screen::training)
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

    // The home screen owns everything under the title row; the training
    // screen's own layout below only runs when it's the visible one.
    // Below the title row, above the footer link.
    homeViewport.setBounds (getLocalBounds()
                                .withTrimmedTop (Spacing::large + 32 + Spacing::small)
                                .withTrimmedBottom (Spacing::large + 18));

    homeScreen.setSize (juce::jmax (0, homeViewport.getMaximumVisibleWidth()),
                         juce::jmax (homeViewport.getHeight(), homeScreen.getContentHeight()));

    auto area = getLocalBounds().reduced (Spacing::large);

    // --- title row: identity on the left, global controls on the right ---
    auto titleRow = area.removeFromTop (32);
    // Square icon buttons: the old row of wide text buttons took most of
    // the title bar for controls pressed once a session.
    constexpr int iconSize = 30;

    languageSelector.setBounds (titleRow.removeFromRight (86));
    titleRow.removeFromRight (Spacing::small);
    sizeSelector.setBounds (titleRow.removeFromRight (56));
    titleRow.removeFromRight (Spacing::medium);
    themeButton.setBounds (titleRow.removeFromRight (iconSize).withSizeKeepingCentre (iconSize, iconSize));
    titleRow.removeFromRight (Spacing::tight);
    updateButton.setBounds (titleRow.removeFromRight (iconSize).withSizeKeepingCentre (iconSize, iconSize));
    titleRow.removeFromRight (Spacing::tight);
    trainingSoundsButton.setBounds (titleRow.removeFromRight (iconSize).withSizeKeepingCentre (iconSize, iconSize));

    area.removeFromTop (Spacing::section);

    // --- exercise section: which game, and what you're listening for ---
    exerciseSection = area.removeFromTop (124);
    {
        auto inner = exerciseSection;
        inner.removeFromTop (Spacing::large);   // clear the section heading

        auto gameRow = inner.removeFromTop (28);
        backButton.setBounds (gameRow.removeFromLeft (28).withSizeKeepingCentre (28, 28));
        gameRow.removeFromLeft (Spacing::medium);
        gameIcon.setBounds (gameRow.removeFromLeft (26));
        gameRow.removeFromLeft (Spacing::small);
        currentGameLabel.setBounds (gameRow);

        inner.removeFromTop (Spacing::small);
        instructionLabel.setBounds (inner);
    }

    area.removeFromTop (Spacing::large);

    // --- answer section: feedback, the slider itself, score/new round ---
    // The scopes need their own room. Stealing it from the scale left it
    // 10px tall and unusable - found immediately on revealing a hint - so
    // the section (and the window) grow instead.
    answerSection = area.removeFromTop (274 + scopeRowHeight + Spacing::small + 38);
    {
        auto inner = answerSection;
        inner.removeFromTop (Spacing::large);

        feedbackLabel.setBounds (inner.removeFromTop (26));
        inner.removeFromTop (Spacing::small);

        // The scale needs real height to read as a panel of zones rather
        // than a thin strip: 40px of it is the value readout and 18px the
        // caption, so anything under ~120 leaves the zones too shallow to
        // fit staggered labels. Found by building it at 92 and looking.
        // The scope strip is always laid out, bought or not - its room is
        // reserved so revealing a hint never resizes the window under the
        // player. paint() draws the dimmed "not bought yet" state.
        scopeStrip = inner.removeFromTop (scopeRowHeight).reduced (Spacing::small, 0);
        {
            auto scopeRow = scopeStrip;
            vectorscope.setBounds (scopeRow.removeFromLeft (scopeRow.getHeight()));
            scopeRow.removeFromLeft (Spacing::small);
            hintSpectrum.setBounds (scopeRow);
        }
        vectorscope.setVisible (hintRevealed && currentScreen == Screen::training);
        hintSpectrum.setVisible (hintRevealed && currentScreen == Screen::training);
        inner.removeFromTop (Spacing::small);

        choiceSlider.setBounds (inner.removeFromTop (150).reduced (Spacing::small, 0));

        inner.removeFromTop (Spacing::medium);

        // A/B on its own centred row under the scale.
        auto abRow = inner.removeFromTop (30);
        {
            auto centred = abRow.withSizeKeepingCentre (240, 28);
            restartButton.setBounds (centred.withSizeKeepingCentre (160, 28));

            beforeButton.setBounds (centred.removeFromLeft (118));
            centred.removeFromLeft (Spacing::tight);
            afterButton.setBounds (centred);
        }
        inner.removeFromTop (Spacing::small);

        auto bottomRow = inner.removeFromTop (34);
        modeSelector.setBounds (bottomRow.removeFromLeft (118));
        bottomRow.removeFromLeft (Spacing::small);
        hintButton.setBounds (bottomRow.removeFromLeft (30).withSizeKeepingCentre (30, 30));
        bottomRow.removeFromLeft (Spacing::tight);
        hintCostLabel.setBounds (bottomRow.removeFromLeft (120));
        runStatusLabel.setBounds (bottomRow.removeFromRight (120));
        scoreLabel.setBounds (bottomRow.reduced (Spacing::small, 0));
    }

    area.removeFromTop (Spacing::large);

    // --- progress section: level, bar, streak, daily challenge ---
    progressSection = area.removeFromTop (100);
    {
        auto inner = progressSection;
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

    clearHint();

    // Each round starts *unprocessed*: you hear the clean reference
    // first, then switch to hear what changed. That's the order an
    // engineer A/Bs in, and it stops the treated version being the only
    // thing ever heard.
    processor.getGameManager().getActiveGame().setPlayProcessed (false);
    refreshBeforeAfter();

    processor.getGameManager().getActiveGame().newRound();
    refreshRunStatus();
}

void EarTrainerEditor::setUiScale (float newScale)
{
    uiScale = juce::jlimit (0.8f, 1.4f, newScale);

    // One layout, drawn through a transform - rather than four sets of
    // hand-tuned sizes that would drift apart. Everything stays exactly
    // the same design at every size.
    setTransform (juce::AffineTransform::scale (uiScale));
    setSize (logicalWidth, logicalHeight);

    // Keep the picker in step with the actual scale, including on the
    // restore path - without this it came up blank on launch.
    const auto id = uiScale < 0.93f ? 1 : uiScale < 1.08f ? 2 : uiScale < 1.23f ? 3 : 4;
    sizeSelector.setSelectedId (id, juce::dontSendNotification);

    localisationProperties.setValue (uiScaleKey, (double) uiScale);
}

void EarTrainerEditor::clearHint()
{
    hintRevealed = false;
    vectorscope.setVisible (false);
    hintSpectrum.setVisible (false);
    vectorscope.reset();
    refreshHintButton();
    repaint();
}

void EarTrainerEditor::refreshRunStatus()
{
    const auto mode = session.getMode();

    const auto& theme = AbcTrainTheme::current();

    if (mode == SessionManager::Mode::practice)
    {
        runStatusLabel.setText ({}, juce::dontSendNotification);
        runStatusLabel.setColour (juce::Label::textColourId, theme.text);
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

void EarTrainerEditor::showScreen (Screen screen)
{
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
    homeViewport.setVisible (onHome);

    // Nothing to listen for outside a training.
    processor.setSignalEnabled (onTraining);

    // Everything that belongs to the training screen.
    // The title-row controls belong to Home and Training, not the
    // one-time support screen.
    for (auto* c : { (juce::Component*) &themeButton, (juce::Component*) &updateButton,
                     (juce::Component*) &trainingSoundsButton, (juce::Component*) &sizeSelector,
                     (juce::Component*) &languageSelector, (juce::Component*) &soundkorbLink })
    {
        c->setVisible (! onSupport);
    }

    for (auto* c : { (juce::Component*) &backButton, (juce::Component*) &gameIcon,
                     (juce::Component*) &currentGameLabel, (juce::Component*) &instructionLabel,
                     (juce::Component*) &feedbackLabel, (juce::Component*) &choiceSlider,
                     (juce::Component*) &modeSelector,
                     (juce::Component*) &hintButton, (juce::Component*) &hintCostLabel,
                     (juce::Component*) &beforeButton, (juce::Component*) &afterButton,
                     (juce::Component*) &runStatusLabel, (juce::Component*) &scoreLabel,
                     (juce::Component*) &levelLabel, (juce::Component*) &levelSelector,
                     (juce::Component*) &levelProgressBar, (juce::Component*) &streakLabel,
                     (juce::Component*) &dailyChallengeLabel })
    {
        c->setVisible (onTraining);
    }

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
        const auto stats = progress.getStatsForGame (i);

        HomeScreenComponent::CardInfo card;
        card.gameIndex = i;
        card.name = translateGameName (englishName, localisation);
        card.benefit = translateGameBenefit (englishName, localisation);
        card.icon = AppIcons::iconForGameName (englishName);
        card.isCurrent = (i == gameManager.getActiveGameIndex());
        card.isFavourite = progress.isFavouriteGame (i);

        card.statsLine = stats.roundsPlayed == 0
                             ? localisation.getText ("ui.notPlayedYet")
                             : localisation.getText ("ui.accuracy") + ": "
                                   + juce::String (juce::roundToInt (stats.getAccuracy() * 100.0f)) + "%   "
                                   + localisation.getText ("ui.bestStreak") + ": "
                                   + juce::String (stats.bestStreak);

        return card;
    };

    std::vector<HomeScreenComponent::Section> sections;

    // Starred trainings first, as their own group - the shortlist is only
    // worth having if it's the thing you see before anything else.
    if (progress.hasAnyFavourites())
    {
        HomeScreenComponent::Section focus { localisation.getText ("home.section.focus"), {} };

        for (int i = 0; i < gameManager.getNumGames(); ++i)
            if (progress.isFavouriteGame (i))
                focus.cards.push_back (makeCard (i));

        sections.push_back (std::move (focus));
    }

    // Then every training, grouped by the skill it builds. A starred
    // training still appears in its own category too, so the map of the
    // subject stays complete rather than developing holes.
    for (const char* categoryKey : { "home.category.frequency", "home.category.dynamics",
                                      "home.category.space", "home.category.character" })
    {
        HomeScreenComponent::Section section { localisation.getText (categoryKey), {} };

        for (int i = 0; i < gameManager.getNumGames(); ++i)
            if (juce::String (categoryForGame (gameManager.getGame (i).getName())) == categoryKey)
                section.cards.push_back (makeCard (i));

        if (! section.cards.empty())
            sections.push_back (std::move (section));
    }

    // Size must be reapplied after a rebuild - the content height changes
    // when a "Your focus" section appears or disappears.
    homeScreen.setHeader (localisation.getText ("app.eartrainer.name"),
                           localisation.getText ("ui.level", { { "level", juce::String (progress.getLevel()) } })
                               + "   ·   "
                               + localisation.getText ("ui.streak", { { "days", juce::String (progress.getStreakDays()) } }));
    homeScreen.setSections (std::move (sections));
    homeScreen.setSize (juce::jmax (0, homeViewport.getMaximumVisibleWidth()),
                         juce::jmax (homeViewport.getHeight(), homeScreen.getContentHeight()));
}

void EarTrainerEditor::refreshLocalisedText()
{
    titleLabel.setText (localisation.getText ("app.eartrainer.name"), juce::dontSendNotification);
    updateButton.setTooltip (localisation.getText ("ui.updates"));
    trainingSoundsButton.setTooltip (localisation.getText ("ui.trainingSounds"));
    backButton.setTooltip (localisation.getText ("ui.back"));

    {
        const auto selected = modeSelector.getSelectedId();
        modeSelector.clear (juce::dontSendNotification);
        modeSelector.addItem (localisation.getText ("ui.modePractice"), 1);
        modeSelector.addItem (localisation.getText ("ui.modeSurvival"), 2);
        modeSelector.addItem (localisation.getText ("ui.modeBlitz"), 3);
        modeSelector.setSelectedId (selected > 0 ? selected : 1, juce::dontSendNotification);
    }

    rebuildGameSelectorItems();
    rebuildHomeSections();
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

    beforeButton.setButtonText (game.getBeforeLabel());
    afterButton.setButtonText (game.getAfterLabel());

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
        hintCostLabel.setText (localisation.getText ("ui.hintTooExpensive"), juce::dontSendNotification);
        hintCostLabel.setColour (juce::Label::textColourId, AbcTrainTheme::current().negative);

        juce::Component::SafePointer<EarTrainerEditor> safeThis (this);
        juce::Timer::callAfterDelay (2000, [safeThis]
        {
            if (safeThis == nullptr)
                return;

            safeThis->hintCostLabel.setColour (juce::Label::textColourId,
                                                AbcTrainTheme::current().textDim);
            safeThis->refreshHintButton();
        });
        return;
    }

    hintRevealed = true;
    vectorscope.reset();
    hintSpectrum.setSampleRate (processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0);

    // Set visibility here rather than leaving it to resized(): this path
    // doesn't relayout (that's the whole point - the window must not move),
    // so relying on resized() left the strip blank after paying for it.
    vectorscope.setVisible (true);
    hintSpectrum.setVisible (true);

    refreshRunStatus();
    refreshHintButton();
    repaint();
}

void EarTrainerEditor::refreshHintButton()
{
    if (hintRevealed)
    {
        hintButton.setEnabled (false);
        hintCostLabel.setText (localisation.getText ("ui.hintShown"), juce::dontSendNotification);
        return;
    }

    hintButton.setEnabled (session.isRunActive());
    hintButton.setTooltip (localisation.getText ("ui.hintFree"));

    switch (session.getMode())
    {
        case SessionManager::Mode::survival:
            hintCostLabel.setText (localisation.getText ("ui.hintCostLife"), juce::dontSendNotification);
            break;
        case SessionManager::Mode::blitz:
            hintCostLabel.setText (localisation.getText ("ui.hintCostSeconds",
                                                          { { "seconds", juce::String (SessionManager::blitzHintSeconds) } }),
                                    juce::dontSendNotification);
            break;
        default:
            hintCostLabel.setText (localisation.getText ("ui.hintFree"), juce::dontSendNotification);
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

    scoreLabel.setText (localisation.getText ("ui.score", { { "correct", juce::String (game.getScore()) },
                                                             { "total", juce::String (game.getRoundsPlayed()) } }),
                         juce::dontSendNotification);

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
