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

    void refresh();

    enum class Page { training, hearing, appearance, background, about };
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

    juce::Rectangle<int> sideMenuBounds() const;
    juce::Rectangle<int> pageBounds() const;
    juce::Rectangle<int> modeSwitchBounds() const;
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

    SegmentedChoice modeSwitch;

    // Training
    SegmentedChoice stepRule, answerPause, survivalLives, blitzSeconds, blitzPenalty, hints, allModes;
    juce::TextButton resetButton;

    // Hearing
    SegmentedChoice hearingOn, breakMinutes, fatigueHint, weeklyLimit;
    juce::TextButton calibrationNoiseButton, calibrationSaveButton, calibrationClearButton;
    juce::Slider calibrationSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Component calibrationRow;
    SegmentedChoice exposureHours, exposureLevel;
    juce::TextButton exposureAddButton;
    juce::Component exposureRow;
    juce::String hearingStatusText;
    bool noisePlaying = false;

    // Appearance
    juce::Slider textScaleSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    CompactSelector typefaceSelector, screensaverSelector;

    // Background
    juce::TextButton chooseBackgroundButton, clearBackgroundButton;
    juce::Component backgroundButtons;
    juce::Slider scrimSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    // About
    juce::TextEditor licenceView;
    juce::TextButton licenceToggle;
    bool licenceExpanded = false;

    juce::TextButton closeButton;

    std::vector<Row> rows;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsScreenComponent)
};
