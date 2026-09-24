#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/i18n/LocalisationManager.h"
#include "shared/ui/CompactSelector.h"
#include "shared/ui/SegmentedChoice.h"
#include "TrainerSettings.h"
#include <functional>
#include <memory>

// Every setting the trainer has, on one page per subject (ADR 036).
//
// **Beginner / Pro** sits at the top of the side rail, because it decides
// what the rest of the page means. In Beginner every training rule is the
// standard one; the rows are still shown - greyed, holding the defaults -
// so that what *could* be tuned is visible, and one click away, without
// being a thing to fiddle with in the first week. Pro enables them.
// Nothing is lost switching back: TrainerSettings keeps the Pro values and
// simply stops honouring them.
//
// Each row is: what it is (one line), what it does to you (one dim line),
// and the control on the right. The dim line is the one that matters -
// "3 in a row" means nothing until it says "settles near 79% correct".
//
// Pages: Training, Hearing, Appearance, Background, About. Appearance and
// Background are the same for everyone; they are about the person, not
// the game.
class SettingsScreenComponent : public juce::Component,
                                private juce::Timer
{
public:
    SettingsScreenComponent (LocalisationManager&, juce::PropertiesFile&);
    ~SettingsScreenComponent() override;

    std::function<void()> onClosed;

    // Called whenever something here changes that the editor has to act on
    // (window size, or a repaint after a theme/wallpaper change).
    std::function<void()> onSettingsChanged;

    // A training or hearing setting changed: the editor re-applies
    // TrainerSettings to the session, the staircase and the guard.
    std::function<void()> onTrainerSettingsChanged;

    // Hearing hooks, wired by the editor to the processor and HearingGuard.
    std::function<void (bool)> onCalibrationNoise;
    std::function<void (double hours, double levelDbA)> onAddExposure;

    // "34% of the week · last second 76 dB(A)" - asked once a second while
    // the Hearing page is open.
    std::function<juce::String()> hearingStatus;

    // Who made each sound the trainer can play, and under what licence -
    // shown on the About page under the product's own licence. The editor
    // builds it from the sound library's packs (ADR 040).
    std::function<juce::String()> soundCredits;

    // What the top bar's right corner used to hold (ADR 042): the editor
    // owns the theme, the language and the update check; this page only
    // offers them.
    std::function<void (bool dark)> onThemeChosen;
    std::function<void (juce::String languageCode)> onLanguageChosen;
    std::function<void()> onCheckForUpdates;

    // The standalone's audio device dialog; null where there is none (the
    // snapshot tools), and the button is greyed.
    std::function<void()> onAudioDevice;

    // "Checking...", "Up to date", "Couldn't check" - shown under the
    // "Check now" button. Empty clears it.
    void setUpdateStatus (const juce::String&);

    // Read by the editor at launch. On unless the player turned it off.
    static constexpr const char* autoUpdateKey = "autoUpdateCheck";

    // Live: the tab on or off (off - the app does not know the server's
    // address at all), and signing in, which the Live page runs.
    static constexpr const char* liveTabKey = "liveTab";
    std::function<void (bool)> onLiveTabChanged;
    std::function<void()> onSignIn;

    // The account (ADR 045): signed in or not, and sync. The editor pushes
    // the state in; the rows call back.
    void setAccountState (bool signedIn, const juce::String& nick, bool syncOn, const juce::String& syncStatus);
    std::function<void()> onSignOut, onSyncNow;
    std::function<void (bool)> onSyncChanged;

    void refresh();

    enum class Page { training, hearing, appearance, background, live, about };
    static constexpr int numPages = 6;
    void selectPage (Page);
    Page getPage() const noexcept { return currentPage; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    static void applyStoredBackground (juce::PropertiesFile&);
    static void applyStoredTypeface (juce::PropertiesFile&);

    static constexpr const char* textScaleKey = "textScale";
    static constexpr const char* backgroundPathKey = "backgroundImage";
    static constexpr const char* backgroundScrimKey = "backgroundScrim";

private:
    using Id = TrainerSettings::Id;

    // One row of a settings page.
    struct Row
    {
        juce::String titleKey, hintKey;
        juce::Component* control = nullptr;
        int controlWidth = 0;              // 0 = take the rest of the row
        bool proOnly = false;
        Page page = Page::training;
        juce::Rectangle<int> bounds;       // laid out in resized()
    };

    void buildRows();
    void timerCallback() override;
    void syncControlsFromSettings();
    juce::String hintFor (const Row&) const;
    bool rowIsOffered (const Row&) const;

    juce::Rectangle<int> sideMenuBounds() const;
    juce::Rectangle<int> pageBounds() const;
    juce::Rectangle<int> modeSwitchBounds() const;
    int menuTop() const;
    void paintSideMenu (juce::Graphics&, juce::Rectangle<int>);
    int menuRowAt (juce::Point<int>) const;

    void chooseBackground();
    void clearBackground();

    void refreshLicenceView();
    static juce::String licenceText (bool full);

    LocalisationManager& localisation;
    juce::PropertiesFile& properties;
    TrainerSettings settings { properties };

    Page currentPage = Page::training;
    int hoveredMenuRow = -1;

    // Pro mode as one switch at the foot of the rail (the author's call):
    // it is a setting you choose once, not a page you move between.
    juce::ToggleButton modeSwitch;

    // Training
    SegmentedChoice stepRule, answerPause, survivalLives, blitzSeconds, blitzPenalty, hints, allModes;
    juce::TextButton resetButton;

    // Hearing
    SegmentedChoice hearingOn, breakMinutes, fatigueHint, weeklyLimit;
    juce::TextButton calibrationNoiseButton, calibrationSaveButton, calibrationClearButton;
    juce::TextButton calibrationDown { juce::String::fromUTF8 ("\xe2\x88\x92") }, calibrationUp { "+" };
    juce::Slider calibrationSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Component calibrationRow;
    SegmentedChoice exposureHours, exposureLevel;
    juce::TextButton exposureAddButton;
    juce::Component exposureRow;
    juce::String hearingStatusText;
    bool noisePlaying = false;

    // Appearance
    SegmentedChoice themeChoice;
    CompactSelector languageChoice;
    juce::Slider textScaleSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    CompactSelector typefaceSelector, screensaverSelector;

    // Background
    juce::TextButton chooseBackgroundButton, clearBackgroundButton;
    juce::Component backgroundButtons;
    juce::Slider scrimSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // About
    juce::TextEditor licenceView;
    juce::TextButton licenceToggle;

    // Feedback without telemetry: opens a prefilled GitHub issue with the
    // version and system filled in - the person decides what to send.
    juce::TextButton feedbackButton;

    // "Offer beta versions" - read by every plugin's update check
    // (UpdateChecker::betaOptInKey in the shared settings file).
    juce::TextButton betaToggle;
    void refreshBetaToggle();

    // Updates: on their own (default) or only when asked; and asking.
    SegmentedChoice autoUpdateChoice;
    juce::TextButton checkNowButton;
    juce::String updateStatusText;

    // Live
    SegmentedChoice liveTabChoice;
    juce::TextButton accountButton;
    SegmentedChoice syncChoice;
    juce::TextButton syncNowButton;
    bool accountSignedIn = false;
    juce::String accountNick, syncStatusText;

    // Hearing: which output the sound goes to.
    juce::TextButton audioDeviceButton;

public:
    // The prefilled issue URL, public so a test can check what is sent.
    static juce::URL feedbackUrl (const juce::String& version, const juce::String& system);

private:
    bool licenceExpanded = false;

    juce::TextButton closeButton;

    std::vector<Row> rows;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsScreenComponent)
};
