#include "SettingsScreenComponent.h"
#include "shared/ui/IdleScreensaver.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/updates/Version.h"
#include <BrandBinaryData.h>

namespace
{
    constexpr int railWidth = 190;
    constexpr int menuRowHeight = 36;
    constexpr int rowHeight = 62;
    constexpr int titleColumn = 330;
    constexpr int controlHeight = 30;

    juce::String n (int value) { return juce::String (value); }
}

SettingsScreenComponent::SettingsScreenComponent (LocalisationManager& localisationToUse,
                                                   juce::PropertiesFile& propertiesToUse)
    : localisation (localisationToUse), properties (propertiesToUse)
{
    setOpaque (true);

    // ---- mode --------------------------------------------------------
    modeSwitch.onChange = [this] (int value)
    {
        settings.setMode (value == 1 ? TrainerSettings::Mode::pro : TrainerSettings::Mode::beginner);
        syncControlsFromSettings();

        if (onTrainerSettingsChanged != nullptr)
            onTrainerSettingsChanged();
    };
    addAndMakeVisible (modeSwitch);

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
    addAndMakeVisible (calibrationRow);

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
            typefaceSelector.addItem (names[i], i + 1,
                                       names[i] == "System" ? juce::String ("Aa") : names[i].substring (0, 2));

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
            screensaverSelector.addItem (seconds[i] == 0 ? localisation.getText ("set.off") : localisation.getText ("set.minutes", { { "n", n (seconds[i] / 60) } }), i + 1,
                                          seconds[i] == 0 ? juce::String ("off") : n (seconds[i] / 60) + "m");

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

    // Not shown: this is a page reached from a tab, and the way out is
    // another tab. Kept as a child for the key handler and the tests.
    closeButton.onClick = [this]
    {
        setVisible (false);

        if (onClosed != nullptr)
            onClosed();
    };
    addChildComponent (closeButton);

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
        { "set.stepRule.title",     "set.stepRule.hint",     &stepRule,       0, true,  Page::training },
        { "set.answerPause.title",  "set.answerPause.hint",  &answerPause,    0, true,  Page::training },
        { "set.hints.title",        "set.hints.hint",        &hints,          0, true,  Page::training },
        { "set.allModes.title",     "set.allModes.hint",     &allModes,       0, true,  Page::training },
        { "set.lives.title",        "set.lives.hint",        &survivalLives,  0, true,  Page::training },
        { "set.blitz.title",        "set.blitz.hint",        &blitzSeconds,   0, true,  Page::training },
        { "set.penalty.title",      "set.penalty.hint",      &blitzPenalty,   0, true,  Page::training },

        { "set.hearingOn.title",    "set.hearingOn.hint",    &hearingOn,      0, false, Page::hearing },
        { "set.break.title",        "set.break.hint",        &breakMinutes,   0, true,  Page::hearing },
        { "set.fatigue.title",      "set.fatigue.hint",      &fatigueHint,    0, true,  Page::hearing },
        { "set.calib.title",        "set.calib.hint",        &calibrationRow, 0, false, Page::hearing },
        { "set.weekly.title",       "set.weekly.hint",       &weeklyLimit,    0, true,  Page::hearing },
        { "set.exposure.title",     "set.exposure.hint",     &exposureRow,    0, true,  Page::hearing },

        { "ui.textSize",            "set.textSize.hint",     &textScaleSlider,     0,   false, Page::appearance },
        { "set.typeface.title",     "set.typeface.hint",     &typefaceSelector,    140, false, Page::appearance },
        { "set.screensaver.title",  "set.screensaver.hint",  &screensaverSelector, 140, false, Page::appearance },

        { "ui.backgroundImage",     "set.background.hint",   &backgroundButtons, 0, false, Page::background },
        { "ui.backgroundDim",       "set.backgroundDim.hint", &scrimSlider,      0, false, Page::background },
    };
}

void SettingsScreenComponent::syncControlsFromSettings()
{
    const auto pro = settings.getMode() == TrainerSettings::Mode::pro;
    modeSwitch.setValue (pro ? 1 : 0);

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

juce::String SettingsScreenComponent::hintFor (const Row& row) const
{
    // A few hints say what the *current* value does, which is the whole
    // point of having them.
    if (row.control == &stepRule)
        return localisation.getText ("set.stepRule.hint" + n (stepRule.getValue()));

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

    modeSwitch.setOptions ({ 0, 1 }, { t ("set.mode.beginner"), t ("set.mode.pro") });

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
    closeButton.setButtonText (t ("ui.close"));

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
    return getLocalBounds().removeFromLeft (railWidth);
}

juce::Rectangle<int> SettingsScreenComponent::modeSwitchBounds() const
{
    return sideMenuBounds().reduced (AbcTrainTheme::Spacing::medium, 0)
                           .withTrimmedTop (AbcTrainTheme::Spacing::large + 20)
                           .withHeight (controlHeight);
}

juce::Rectangle<int> SettingsScreenComponent::pageBounds() const
{
    return getLocalBounds().withTrimmedLeft (railWidth).reduced (AbcTrainTheme::Spacing::large + 8,
                                                                 AbcTrainTheme::Spacing::large);
}

int SettingsScreenComponent::menuRowAt (juce::Point<int> p) const
{
    auto area = sideMenuBounds().withTrimmedTop (modeSwitchBounds().getBottom() + 58)
                                .reduced (AbcTrainTheme::Spacing::small, 0);

    for (int i = 0; i < 5; ++i)
    {
        if (area.removeFromTop (menuRowHeight).contains (p))
            return i;

        area.removeFromTop (2);
    }

    return -1;
}

void SettingsScreenComponent::selectPage (Page page)
{
    currentPage = page;

    for (auto& row : rows)
        row.control->setVisible (row.page == page);

    resetButton.setVisible (page == Page::training && settings.getMode() == TrainerSettings::Mode::pro);
    licenceView.setVisible (page == Page::about);
    licenceToggle.setVisible (page == Page::about);

    syncControlsFromSettings();
    visibilityChanged();
}

void SettingsScreenComponent::resized()
{
    modeSwitch.setBounds (modeSwitchBounds());

    auto page = pageBounds();
    page.removeFromTop (30 + AbcTrainTheme::Spacing::small);   // heading

    if (currentPage == Page::training || currentPage == Page::hearing)
        page.removeFromTop (26);                                // the mode note / status line

    const auto contentWidth = juce::jmin (page.getWidth(), 980);

    for (auto& row : rows)
    {
        if (row.page != currentPage)
            continue;

        row.bounds = page.removeFromTop (rowHeight).withWidth (contentWidth);

        auto controlArea = row.bounds.withTrimmedLeft (titleColumn);
        const auto width = row.controlWidth > 0 ? row.controlWidth : controlArea.getWidth();
        const auto box = controlArea.removeFromLeft (width).withSizeKeepingCentre (width, controlHeight)
                                    .withY (row.bounds.getY() + 6);

        if (auto* seg = dynamic_cast<SegmentedChoice*> (row.control))
            seg->setBounds (box.withWidth (juce::jmin (box.getWidth(), juce::jmax (seg->getPreferredWidth(), 220))));
        else
            row.control->setBounds (box);
    }

    // Composite rows lay out their own children.
    {
        auto r = calibrationRow.getLocalBounds();
        const auto w = (float) r.getWidth();
        calibrationNoiseButton.setBounds (r.removeFromLeft (juce::jmin (170, (int) (w * 0.34f))));
        r.removeFromLeft (8);
        calibrationClearButton.setBounds (r.removeFromRight (juce::jmin (110, (int) (w * 0.22f))));
        r.removeFromRight (8);
        calibrationSaveButton.setBounds (r.removeFromRight (juce::jmin (120, (int) (w * 0.24f))));
        r.removeFromRight (8);
        calibrationSlider.setBounds (r);
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
        chooseBackgroundButton.setBounds (r.removeFromLeft (200));
        r.removeFromLeft (8);
        clearBackgroundButton.setBounds (r.removeFromLeft (120));
    }

    if (currentPage == Page::training)
        resetButton.setBounds (page.removeFromTop (12 + controlHeight).removeFromBottom (controlHeight)
                                   .withX (page.getX() + titleColumn).withWidth (240));

    if (currentPage == Page::about)
    {
        page.removeFromTop (22);
        auto body = page.removeFromTop (juce::jmin (page.getHeight() - 44, 420))
                        .removeFromLeft (juce::jmin (page.getWidth(), 760));
        licenceView.setBounds (body);
        licenceToggle.setBounds (body.getX(), body.getBottom() + AbcTrainTheme::Spacing::small, 240, controlHeight);
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

    // The mode, first: it decides what everything else means.
    const auto modeBox = modeSwitchBounds();
    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (localisation.getText ("set.mode.title")),
                                          modeBox.translated (0, -22).withHeight (18).toFloat(),
                                          AbcTrainLookAndFeel::microFont(), theme.textDim, 1.4f);

    const auto pro = settings.getMode() == TrainerSettings::Mode::pro;
    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    g.drawFittedText (localisation.getText (pro ? "set.mode.hintPro" : "set.mode.hintBeginner"),
                      modeBox.translated (0, controlHeight + 6).withHeight (36), juce::Justification::topLeft, 2, 1.0f);

    const juce::String labels[] { localisation.getText ("set.page.training"),
                                   localisation.getText ("set.page.hearing"),
                                   localisation.getText ("ui.settingsAppearance"),
                                   localisation.getText ("ui.settingsBackground"),
                                   localisation.getText ("ui.about") };

    auto rowArea = area.withTrimmedTop (modeBox.getBottom() + 58).reduced (AbcTrainTheme::Spacing::small, 0);

    for (int i = 0; i < 5; ++i)
    {
        const auto bounds = rowArea.removeFromTop (menuRowHeight);
        rowArea.removeFromTop (2);

        const auto selected = (int) currentPage == i;

        if (selected || hoveredMenuRow == i)
        {
            g.setColour (selected ? theme.accent.withAlpha (0.18f) : theme.widgetBackground.withAlpha (0.6f));
            g.fillRect (bounds.toFloat().reduced (2.0f, 0.0f));
        }

        if (selected)
        {
            g.setColour (theme.accent);
            g.fillRect (bounds.toFloat().withWidth (3.0f).reduced (0.0f, 6.0f));
        }

        g.setColour (selected ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (labels[i], bounds.withTrimmedLeft (14), juce::Justification::centredLeft, true);
    }
}

void SettingsScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());
    g.setColour (theme.panelBackground);
    g.fillRect (getLocalBounds());

    paintSideMenu (g, sideMenuBounds());

    auto page = pageBounds();

    const juce::String heading = currentPage == Page::training   ? localisation.getText ("set.page.training")
                               : currentPage == Page::hearing    ? localisation.getText ("set.page.hearing")
                               : currentPage == Page::appearance ? localisation.getText ("ui.settingsAppearance")
                               : currentPage == Page::background ? localisation.getText ("ui.settingsBackground")
                                                                 : localisation.getText ("ui.about");

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::titleFont());
    g.drawText (heading, page.removeFromTop (30), juce::Justification::centredLeft, false);
    page.removeFromTop (AbcTrainTheme::Spacing::small);

    const auto pro = settings.getMode() == TrainerSettings::Mode::pro;

    if (currentPage == Page::training)
    {
        g.setColour (pro ? theme.textDim : theme.accentWarm);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (localisation.getText (pro ? "set.training.notePro" : "set.training.noteBeginner"),
                    page.removeFromTop (26), juce::Justification::centredLeft, true);
    }
    else if (currentPage == Page::hearing)
    {
        g.setColour (theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (hearingStatusText, page.removeFromTop (26), juce::Justification::centredLeft, true);
    }
    else if (currentPage == Page::about)
    {
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText ("abcTrain " + juce::String (CurrentVersion::string), page.removeFromTop (18),
                    juce::Justification::centredLeft, false);
    }

    // Rows: title and what it does on the left, hairline between rows.
    for (const auto& row : rows)
    {
        if (row.page != currentPage)
            continue;

        const auto enabled = row.control->isEnabled();
        auto rowBounds = row.bounds;
        auto text = rowBounds.removeFromLeft (titleColumn - 24);

        g.setColour (enabled ? theme.textBright : theme.textDim);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (localisation.getText (row.titleKey), text.removeFromTop (24).withTrimmedTop (4),
                    juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.drawFittedText (hintFor (row), text.withTrimmedBottom (4), juce::Justification::topLeft, 2, 0.9f);

        g.setColour (theme.divider);
        g.fillRect (row.bounds.getX(), row.bounds.getBottom() - 1, row.bounds.getWidth(), 1);
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
    const auto found = menuRowAt (event.getPosition());

    if (found >= 0)
    {
        selectPage ((Page) found);
        repaint();
    }
}
