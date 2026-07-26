#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_animation/juce_animation.h>
#include "PluginProcessor.h"
#include "ChoiceSliderComponent.h"
#include "TrainingSoundsComponent.h"
#include "HomeScreenComponent.h"
#include "SupportScreenComponent.h"
#include "SettingsScreenComponent.h"
#include "SessionManager.h"
#include "AchievementToast.h"
#include "../shared/UpdateChecker.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/CompactSelector.h"
#include "../shared/AppIcons.h"
#include "../shared/Vectorscope.h"
#include "../shared/SpectrumAnalyzer.h"
#include "../shared/i18n/LocalisationManager.h"
#include <cmath>

// Generic multiple-choice UI driven entirely by the active Game's
// interface (name/instructions/choice count/labels/feedback). Adding a
// new game to GameManager needs no changes here.
class EarTrainerEditor : public juce::AudioProcessorEditor,
                          private juce::ChangeListener,
                          private juce::Timer
{
public:
    explicit EarTrainerEditor (EarTrainerProcessor&);
    ~EarTrainerEditor() override;

    // For tools/EditorSnapshots only - see SupportScreenComponent::
    // completeReveal for why a rendered still needs this.
    void completeWelcomeReveal() { supportScreen.completeReveal(); }

    // Also for tools/EditorSnapshots: opens a training so the screenshot
    // tool can photograph it. There is no other way in from outside - the
    // only route is a mouse click on a home-screen tile - and a product
    // gallery that cannot show the screen people spend all their time on
    // is not much of a gallery.
    void openTrainingForSnapshot (int gameIndex)
    {
        processor.getGameManager().setActiveGameIndex (gameIndex);
        showScreen (Screen::training);
        refreshFromGameState();
    }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Plain filled-rectangle progress bar, its fill now eased via JUCE's
    // own juce_animation module (Animator/ValueAnimatorBuilder) instead
    // of snapping straight to the new value - the first step of the
    // phased animation pass described in decisions/013-ui-libraries.md.
    // Also "breathes" continuously - a slow, low-amplitude glow pulse at
    // the leading edge of the fill via a plain Timer, independent of the
    // eased fill-transition Animator above (see decisions/018).
    class LevelProgressBar : public juce::Component,
                              private juce::Timer
    {
    public:
        LevelProgressBar() { startTimerHz (30); }
        ~LevelProgressBar() override { stopTimer(); }

        void setProgress (float newProgress) noexcept
        {
            const auto target = juce::jlimit (0.0f, 1.0f, newProgress);
            if (juce::approximatelyEqual (target, targetProgress))
                return;

            targetProgress = target;

            // Where the bar actually *is* right now - read before the
            // complete() below, which jumps the outgoing animation to its
            // own end value. Reading it afterwards meant a retarget while
            // one was still running snapped the fill to the previous
            // target and only then eased to the new one: a visible jump
            // in the middle of what should be one continuous move. It
            // only shows up when two answers land inside 400 ms, which is
            // routine in Blitz and rare enough elsewhere to have hidden.
            const auto startValue = displayedProgress;

            // Fast-track any still-running animation to completion, so its
            // own onComplete callback removes *itself* from `updater`
            // before `currentAnimator` gets reassigned below - otherwise
            // that callback (which reads the member by name, not by
            // captured value) would end up removing the new animator
            // instead of the one it actually belongs to.
            if (! currentAnimator.isComplete())
                currentAnimator.complete();

            const auto endValue = target;

            currentAnimator = juce::ValueAnimatorBuilder{}
                                   .withEasing (juce::Easings::createEaseOut())
                                   .withDurationMs (400.0)
                                   .withValueChangedCallback ([this, startValue, endValue] (float t)
                                   {
                                       displayedProgress = startValue + (endValue - startValue) * t;
                                       repaint();
                                   })
                                   .build();

            updater.addAnimator (currentAnimator, [this] { updater.removeAnimator (currentAnimator); });
            currentAnimator.start();
        }

        void paint (juce::Graphics& g) override
        {
            const auto& theme = AbcTrainTheme::current();
            auto bounds = getLocalBounds().toFloat();
            const auto radius = bounds.getHeight() * 0.5f;

            // Recessed groove, same treatment as the choice slider's track
            // and the linear sliders - one consistent idea of what "a
            // channel something sits in" looks like across the whole UI.
            juce::ColourGradient trackGradient (theme.displayBackground.darker (0.12f), bounds.getX(), bounds.getY(),
                                                 theme.widgetBackground, bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill (trackGradient);
            g.fillRoundedRectangle (bounds, radius);
            g.setColour (theme.outline.withAlpha (0.6f));
            g.drawRoundedRectangle (bounds, radius, 1.0f);

            if (displayedProgress <= 0.001f)
                return;

            // The fill can never be narrower than its own corner radius or
            // it stops being a rounded shape - so the first few percent
            // are shown by fading a minimum-width pill in rather than
            // popping one into existence at full opacity.
            const auto minWidth = bounds.getHeight();
            const auto naturalWidth = bounds.getWidth() * displayedProgress;
            auto fillBounds = bounds.withWidth (juce::jmax (minWidth, naturalWidth));
            const auto emergence = juce::jlimit (0.0f, 1.0f, naturalWidth / minWidth);

            juce::ColourGradient fillGradient (theme.accent.darker (0.1f), fillBounds.getX(), fillBounds.getY(),
                                                theme.accent.brighter (0.2f), fillBounds.getRight(), fillBounds.getY(),
                                                false);
            g.setGradientFill (fillGradient);
            g.setOpacity (AbcTrainTheme::Ease::out (emergence));
            g.fillRoundedRectangle (fillBounds, radius);
            g.setOpacity (1.0f);

            // The "breathing" glow: a soft, slowly-pulsing highlight right
            // at the leading edge of the fill - only drawn once there's
            // real progress to show, so an empty bar at level 1 doesn't
            // pulse for no reason.
            if (displayedProgress > 0.02f)
            {
                // Faded in over the same first stretch as the fill, so the
                // glow doesn't switch on the instant progress crosses 2%.
                const auto glowFade = juce::jlimit (0.0f, 1.0f, (displayedProgress - 0.02f) * 25.0f);
                const auto glowAlpha = glowFade * (0.10f + 0.12f * (0.5f + 0.5f * std::sin (breathPhase)));
                const auto glowWidth = juce::jmin (20.0f, fillBounds.getWidth());
                auto glowBounds = fillBounds.removeFromRight (glowWidth);
                g.setColour (theme.textBright.withAlpha (glowAlpha));
                g.fillRoundedRectangle (glowBounds, radius);
            }
        }

    private:
        void timerCallback() override
        {
            // A full slow cycle roughly every ~3.4s (2*pi / 0.03 rad per
            // 33ms tick at 30Hz) - unhurried, "calm" pacing rather than a
            // nervous flicker.
            breathPhase += 0.03f;
            if (breathPhase > juce::MathConstants<float>::twoPi)
                breathPhase -= juce::MathConstants<float>::twoPi;

            // Nothing breathes on an empty bar, so nothing needs
            // repainting either - this timer used to redraw the widget
            // 30 times a second for a picture that never changed.
            if (displayedProgress > 0.02f)
                repaint();
        }

        float targetProgress = 0.0f;
        float displayedProgress = 0.0f;
        float breathPhase = 0.0f;

        // Animator has no default constructor (only explicit
        // Animator(shared_ptr<Impl>)) - this placeholder is never
        // started, just a valid object to overwrite the first time
        // setProgress() actually runs.
        juce::Animator currentAnimator = juce::ValueAnimatorBuilder{}.build();
        juce::VBlankAnimatorUpdater updater { this };
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Pushes the active palette into every widget that sets its own colours
    // explicitly (the LookAndFeel can't reach those). Called at construction
    // and again whenever the theme is toggled.
    void applyTheme();
    void toggleTheme();

    void timerCallback() override;          // 1 Hz, drives the Blitz clock

    void modeSelected();
    void startNewRun();
    void refreshRunStatus();

    void refreshFromGameState();
    void refreshFromProgressState();
    void refreshLocalisedText();
    void rebuildGameSelectorItems();
    // Home <-> Training. The editor used to be one flat panel with a
    // picker overlay dropped on top; these are real screens now, and
    // exactly one is visible at a time.
    enum class Screen { support, home, training };
    void showScreen (Screen);
    void rebuildHomeSections();
    void rebuildChoiceSlider();
    void choiceButtonClicked (int choiceIndex);

    // Shared tail of both answer paths (discrete index and continuous
    // value): scoring, run state, and scheduling the auto-advance.
    void afterAnswer (bool wasCorrect);

    // Buys and reveals the scope hint, or explains why it can't be
    // afforded. See SessionManager::spendHint for the price per mode.
    void requestHint();
    void refreshHintButton();

    // Puts the hint back behind its price. Called from every path that
    // starts a round, including the auto-advance - which is why it is its
    // own method rather than a few lines inside startNewRun().
    void clearHint();
    void languageSelected();

    // Declared first so it's constructed before, and destroyed after,
    // every other Component below that might still reference it during
    // teardown - see the class comment on AbcTrainLookAndFeel.
    AbcTrainLookAndFeel lookAndFeel;

    // Also declared early, before any Component that reads localised
    // text during construction. localisationProperties must outlive
    // localisation (which holds a reference to it), hence that order.
    juce::PropertiesFile localisationProperties;
    LocalisationManager localisation;

    EarTrainerProcessor& processor;

    juce::Label titleLabel;
    CompactSelector languageSelector;
    // Icon for whichever game is currently selected (see AppIcons) - kept
    // in sync with the active game in refreshFromGameState(), so a pick
    // from the card grid or a difficulty-driven change both update it.
    AppIconComponent gameIcon;

    // Opens the card grid. Replaces the plain ComboBox list of nine game
    // names, which told a player nothing about what each exercise was for
    // or how they were doing at it - see GamePickerComponent.
    // Back to Home from the training screen.
    // A labelled button, not a house glyph: "how do I get back" is the
    // question a lost player asks, and an icon they have to decode is the
    // wrong shape for the answer.
    juce::TextButton backButton;
    juce::Label currentGameLabel;
    juce::Label instructionLabel;
    juce::Label scoreLabel;

    // "До уровня 2: 20 / 100". Lives up in the exercise header rather than
    // in the crowded bottom row, where it had 84px and wrapped onto three
    // clipped lines - a progress readout that will not tell you the
    // progress. It also belongs beside the exercise's name: it is a fact
    // about this exercise, not about the run.
    juce::Label levelProgressLabel;
    juce::Label feedbackLabel;
    // No "New Round" button: rounds advance on their own, and a button
    // that only duplicates something automatic is a button to remove.
    // This one exists *only* while a Survival/Blitz run is over, which is
    // the one moment nothing advances by itself.
    juce::TextButton restartButton;

    // The scope hint: a vectorscope (where it sits in the stereo field,
    // and whether it's in phase) over a spectrum (where the energy is).
    // Hidden until bought, and reset on every new round so the previous
    // round's picture can't linger into the next one.
    // A/B: the untreated signal against the treated one. Two buttons
    // rather than one toggle, so both states are always named and which
    // one you're hearing is readable without interpreting a label that
    // changes.
    juce::TextButton beforeButton, afterButton;
    void setPlayProcessed (bool);
    void refreshBeforeAfter();

    // One labelled button carrying its own price, rather than a bare glyph
    // plus a caption beside it.
    juce::TextButton hintButton;
    static constexpr int modeRadioGroup = 8201;
    Vectorscope vectorscope;
    SpectrumAnalyzerComponent hintSpectrum;
    bool hintRevealed = false;

    // The hint panel does not exist until it is bought, and the window
    // grows to make room for it.
    //
    // It used to be reserved permanently - laid out on every round,
    // dimmed and captioned "not bought yet" - specifically so revealing a
    // hint could never resize the window under the player. That trade was
    // wrong: a training screen should hold the thing you are answering
    // with and nothing else, and a strip of grey placeholder sitting
    // there every round is worse than a window that changes size on the
    // rare occasions you actually buy one.
    static constexpr int hintRowHeight = 82;
    static constexpr int hintPanelHeight = hintRowHeight + AbcTrainTheme::Spacing::large * 2;

    // Logical layout size. The window is this multiplied by uiScale; the
    // layout itself never changes, so every size is the same design.
    static constexpr int logicalWidth = 680;
    // Derived from resized(), not guessed: 20 margin + 32 title row + 28
    // section gap + 124 exercise + 20 + 312 answer + 26 + 18 footer + 20.
    // Guessing this is what left LearnerComp with 132px of dead window
    // (decisions/023).
    // 20 margin + 32 title + 28 + 124 exercise + 20 + 318 answer + 20 +
    // 30 tool bar + 20 margin.
    static constexpr int logicalBaseHeight = 612;

    // Grows while a hint is on screen; see hintPanelHeight above.
    int getLogicalHeight() const noexcept
    {
        return logicalBaseHeight + (hintRevealed ? hintPanelHeight : 0);
    }

    void applyWindowSize();

    void setUiScale (float newScale);
    float uiScale = 1.0f;
    CompactSelector sizeSelector;

    // Practice / Survival / Blitz. A run in the latter two ends on its
    // own terms (lives or clock) and posts a score against the exercise;
    // see SessionManager.
    SessionManager session;
    // Three pills, one visibly on. A ComboBox meant the current mode was a
    // word in a well and the other two were behind a popup - so "what are
    // my options" cost a click, and the answer appeared as an OS menu that
    // belongs to no theme at all.
    juce::TextButton practiceButton, survivalButton, blitzButton;
    juce::Label runStatusLabel;

    // Kept so an auto-advance already in flight can be cancelled if the
    // player switches game/mode before it fires - otherwise a queued
    // newRound() would land on the exercise they just left.
    int pendingAdvanceId = 0;
    // Drag-to-select slider replacing the old row of separate choice
    // buttons - see decisions/015-choice-slider-and-training-sounds.md.
    ChoiceSliderComponent choiceSlider;

    // Lets a player jump straight to any level (1-10) instead of only
    // ever reaching it by accumulating points - "progress you can see
    // and control", not just a passive auto-scaling number. Still backed
    // by the same points system underneath (see
    // ProgressManager::setLevelManually), so the two never disagree.
    juce::Label streakLabel;
    juce::Label dailyChallengeLabel;

    // How many of Achievements::all() have been earned. Home screen only.

    // Height of the home screen's status row (level line + daily line).
    static constexpr int homeStatusHeight = 20;

    void showAchievementToast (const juce::String& achievementId);

    IconButton updateButton { AppIcons::Icon::download };

    // Light/dark switch. The chosen mode is stored in the same shared
    // "abcTrain" PropertiesFile the language preference uses, so it's one
    // product-wide preference rather than per-plugin or per-instance.
    IconButton themeButton { AppIcons::Icon::sun };

    // Section backdrops, computed in resized() and drawn in paint(). Held
    // as members because JUCE gives paint() no access to the layout pass,
    // and recomputing the same rectangles in both places is exactly how
    // a panel and its contents drift apart.
    juce::Rectangle<int> exerciseSection;
    juce::Rectangle<int> answerSection;
    juce::Rectangle<int> progressSection;
    juce::Rectangle<int> hintSection;

    // Overlay screen for picking which of the user's own reference-audio
    // folders (if any) the games should train on instead of pink noise -
    // see TrainingSoundsComponent/ReferenceAudioLibrary and decisions/015.
    // Has no default constructor (needs the processor), so it's
    // initialised in the constructor's member-init-list, after `processor`.
    IconButton trainingSoundsButton { AppIcons::Icon::sound };
    // Declared here, but addChildComponent()'d last in the constructor so
    // it paints over everything - the same z-order rule the lesson and
    // training-sounds overlays already had to learn (decisions/015, 017).
    AchievementToast achievementToast;

    // Theme, window size, text size, wallpaper - the settings that are set
    // once. Added after trainingSounds so it paints over it, and before
    // the toast, which paints over everything.
    IconButton settingsButton { AppIcons::Icon::settings };
    SettingsScreenComponent settingsScreen { localisation, localisationProperties };

    TrainingSoundsComponent trainingSounds;
    // The home screen lives inside a Viewport: nine trainings across four
    // categories already exceed the window, and the catalogue only grows.
    HomeScreenComponent homeScreen;

    // First launch only, and it blocks nothing - see the class comment.
    SupportScreenComponent supportScreen { localisation };
    Screen currentScreen = Screen::home;

    // Product site link, shown in a corner of every one of the four
    // editors (see decisions/016) - a dedicated page for the plugins
    // themselves is planned there later, this just points at the site
    // that exists today.
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerEditor)
};
