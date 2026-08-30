#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/i18n/LocalisationManager.h"
#include "../shared/CompactSelector.h"
#include <functional>
#include <memory>

// One place for how the app looks, instead of four controls scattered
// along the title row.
//
// The title row had grown a theme toggle, a window-size picker and a
// language picker, each a different shape, none of them labelled, all
// competing with the three that actually do something (sounds, updates,
// support). Settings that are set once belong behind a door.
//
// What is here is deliberately the ordinary set - theme, window size,
// language, text size, wallpaper. Nothing about training, nothing about
// audio: those live where they are used.
//
// **Text size is separate from window size on purpose.** The window-size
// picker scales the entire layout through an AffineTransform, so text and
// spacing grow together and the design is identical at every step. That is
// right for "this window is too small on a 4K display" and useless for "I
// can read everything except the small print". Text size scales only the
// fonts, against a layout that stays put - which is the accessibility
// knob, and the one people actually mean.
class SettingsScreenComponent : public juce::Component
{
public:
    SettingsScreenComponent (LocalisationManager&, juce::PropertiesFile&);
    ~SettingsScreenComponent() override;

    std::function<void()> onClosed;

    // Called whenever something here changes that the editor has to act on
    // (window size, or a repaint after a theme/wallpaper change).
    std::function<void()> onSettingsChanged;

    void refresh();

    // For tools/EditorSnapshots: opens one page directly. The rail is the
    // only way in from outside, and a settings page with no snapshot is a
    // settings page nobody ever looks at - which is how the licence dump
    // stayed the first thing this screen showed for so long.
    void openPageForSnapshot (int pageIndex)
    {
        selectPage (static_cast<Page> (juce::jlimit (0, numPages - 1, pageIndex)));
    }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Reads the persisted wallpaper (if any) and hands it to
    // AbcTrainLookAndFeel. Called at startup, before the first paint, so a
    // saved background is there on launch rather than after a visit here.
    static void applyStoredBackground (juce::PropertiesFile&);

    static constexpr const char* textScaleKey = "textScale";

    // Reads the saved typeface and hands it to AbcTrainLookAndFeel.
    // Called at startup alongside applyStoredBackground, before the
    // first paint - a font applied only after a visit to Settings is
    // a font nobody sees on launch.
    static void applyStoredTypeface (juce::PropertiesFile&);

    static constexpr const char* backgroundPathKey = "backgroundImage";
    static constexpr const char* backgroundScrimKey = "backgroundScrim";

    // How a round ends. Two questions the app used to answer for you:
    //
    //  - **When to show the review.** Always, only when you missed, or
    //    never. Somebody drilling for speed does not want a sentence
    //    after every correct answer; somebody learning wants it every
    //    time. Neither is the default for the other.
    //  - **Whether the next round starts by itself.** Auto-advance is
    //    right at speed and wrong when you want to sit with what you just
    //    heard - and there was no way to say so.
    //
    // Both live in the same shared PropertiesFile as the language and the
    // theme, since they are preferences about the person rather than about
    // one session.
    enum class Review { always, onMiss, never };

    static constexpr const char* reviewModeKey  = "reviewMode";
    static constexpr const char* autoAdvanceKey = "autoAdvanceRounds";

    static Review getReviewMode (juce::PropertiesFile&);
    static bool getAutoAdvance (juce::PropertiesFile&);

private:
    juce::Rectangle<int> cardBounds() const;
    void chooseBackground();
    void clearBackground();

    // The side rail: About / Appearance / Background, and room for the
    // pages that are coming (see docs/roadmap.md). A settings screen that
    // is a single flat card stops working the moment it has more than one
    // subject in it, and this one already has three.
    enum class Page { about, appearance, training, background };
    static constexpr int numPages = 4;

    void selectPage (Page);
    void paintSideMenu (juce::Graphics&, juce::Rectangle<int>);
    juce::Rectangle<int> sideMenuBounds() const;
    juce::Rectangle<int> pageBounds() const;

    Page currentPage = Page::about;
    int hoveredMenuRow = -1;

    // The licence, shown in full rather than linked: a licence you have to
    // leave the app to read is a licence nobody reads.
    juce::TextEditor licenceView;

    LocalisationManager& localisation;
    juce::PropertiesFile& properties;

    juce::Label textScaleLabel, typefaceLabel, screensaverLabel, backgroundLabel, scrimLabel;
    CompactSelector typefaceSelector;
    CompactSelector screensaverSelector;
    juce::Slider textScaleSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider scrimSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextButton chooseBackgroundButton, clearBackgroundButton, closeButton;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::String headingAppearance, headingBackground;

    // Segmented choices, built from plain TextButtons: the look-and-feel
    // already draws an unselected button as a hairline frame and a
    // selected one as a filled block, which *is* a segmented control.
    juce::Label reviewLabel, advanceLabel;
    juce::OwnedArray<juce::TextButton> reviewButtons, advanceButtons;

    void refreshTrainingButtons();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsScreenComponent)
};
