#include "SettingsScreenComponent.h"
#include "../shared/IdleScreensaver.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include "../shared/Version.h"
#include <BrandBinaryData.h>

SettingsScreenComponent::SettingsScreenComponent (LocalisationManager& localisationToUse,
                                                   juce::PropertiesFile& propertiesToUse)
    : localisation (localisationToUse), properties (propertiesToUse)
{
    setOpaque (true);

    // 0.8 to 1.4: below 0.8 the layout can no longer hold the text it was
    // designed around, and above 1.4 two-line labels start colliding. A
    // range you cannot break the app with is worth more than a range that
    // goes to eleven.
    textScaleSlider.setRange (0.8, 1.4, 0.05);
    textScaleSlider.setValue (properties.getDoubleValue (textScaleKey, 1.0),
                               juce::dontSendNotification);
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

    // A font list, not a font *picker*: no previews, no weights, no sizes.
    // The one real question is "does this one read better to you", and the
    // answer is visible the instant it is chosen, because the screen behind
    // the menu redraws in it.
    {
        const auto names = AbcTrainLookAndFeel::availableTypefaceNames();
        const auto saved = properties.getValue (AbcTrainLookAndFeel::typefaceKey, "System");

        for (int i = 0; i < names.size(); ++i)
            typefaceSelector.addItem (names[i], i + 1,
                                       names[i] == "System" ? juce::String ("Aa")
                                                            : names[i].substring (0, 2));

        typefaceSelector.setSelectedId (juce::jmax (1, names.indexOf (saved) + 1),
                                         juce::dontSendNotification);

        typefaceSelector.onChange = [this]
        {
            const auto chosen = AbcTrainLookAndFeel::availableTypefaceNames()
                                    [typefaceSelector.getSelectedId() - 1];

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

    typefaceLabel.setText ("Typeface", juce::dontSendNotification);
    addAndMakeVisible (typefaceLabel);

    // Off first, because "make it stop" is the request somebody arrives
    // here with.
    {
        const std::pair<const char*, int> options[] {
            { "Off", 0 }, { "1 min", 60 }, { "3 min", 180 }, { "5 min", 300 }, { "10 min", 600 }
        };

        const auto saved = properties.getIntValue (IdleScreensaver::idleSecondsKey,
                                                    IdleScreensaver::defaultIdleSeconds);
        auto selected = 3;

        for (int i = 0; i < 5; ++i)
        {
            screensaverSelector.addItem (options[(size_t) i].first, i + 1,
                                          options[(size_t) i].second == 0
                                              ? juce::String ("off")
                                              : juce::String (options[(size_t) i].second / 60) + "m");

            if (options[(size_t) i].second == saved)
                selected = i + 1;
        }

        screensaverSelector.setSelectedId (selected, juce::dontSendNotification);

        screensaverSelector.onChange = [this]
        {
            const int seconds[] = { 0, 60, 180, 300, 600 };
            const auto chosen = seconds[juce::jlimit (0, 4, screensaverSelector.getSelectedId() - 1)];

            properties.setValue (IdleScreensaver::idleSecondsKey, chosen);
            properties.saveIfNeeded();

            if (onSettingsChanged != nullptr)
                onSettingsChanged();
        };

        addAndMakeVisible (screensaverSelector);
    }

    screensaverLabel.setText ("Screensaver", juce::dontSendNotification);
    addAndMakeVisible (screensaverLabel);

    scrimSlider.setRange (0.0, 0.9, 0.05);
    scrimSlider.setValue (properties.getDoubleValue (backgroundScrimKey, 0.55),
                           juce::dontSendNotification);
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
    addAndMakeVisible (chooseBackgroundButton);

    clearBackgroundButton.onClick = [this] { clearBackground(); };
    addAndMakeVisible (clearBackgroundButton);

    closeButton.onClick = [this]
    {
        setVisible (false);

        if (onClosed != nullptr)
            onClosed();
    };
    addAndMakeVisible (closeButton);

    for (auto* label : { &textScaleLabel, &backgroundLabel, &scrimLabel })
    {
        label->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    }

    licenceView.setMultiLine (true);
    licenceView.setReadOnly (true);
    licenceView.setScrollbarsShown (true);
    licenceView.setCaretVisible (false);
    licenceView.setFont (AbcTrainLookAndFeel::captionFont());
    licenceView.setText (juce::String::fromUTF8 (BrandBinaryData::LICENSE,
                                                  BrandBinaryData::LICENSESize));
    addAndMakeVisible (licenceView);

    selectPage (Page::about);
    refresh();
}

SettingsScreenComponent::~SettingsScreenComponent() = default;

void SettingsScreenComponent::applyStoredTypeface (juce::PropertiesFile& properties)
{
    AbcTrainLookAndFeel::setTypefaceName (
        properties.getValue (AbcTrainLookAndFeel::typefaceKey, "System"));
}

void SettingsScreenComponent::applyStoredBackground (juce::PropertiesFile& properties)
{
    const juce::File file (properties.getValue (backgroundPathKey));

    if (! file.existsAsFile())
    {
        AbcTrainLookAndFeel::setCustomBackground ({}, 0.55f);
        return;
    }

    // A file that has since been deleted or moved simply turns the
    // wallpaper off rather than leaving a broken state - people move their
    // pictures around, and losing one should not mean an app that will not
    // draw.
    AbcTrainLookAndFeel::setCustomBackground (juce::ImageFileFormat::loadFrom (file),
                                               (float) properties.getDoubleValue (backgroundScrimKey, 0.55));
}

void SettingsScreenComponent::refresh()
{
    const auto& theme = AbcTrainTheme::current();

    headingAppearance = localisation.getText ("ui.settingsAppearance");
    headingBackground = localisation.getText ("ui.settingsBackground");

    textScaleLabel.setText (localisation.getText ("ui.textSize"), juce::dontSendNotification);
    backgroundLabel.setText (localisation.getText ("ui.backgroundImage"), juce::dontSendNotification);
    scrimLabel.setText (localisation.getText ("ui.backgroundDim"), juce::dontSendNotification);

    chooseBackgroundButton.setButtonText (localisation.getText ("ui.chooseImage"));
    clearBackgroundButton.setButtonText (localisation.getText ("ui.clearImage"));
    closeButton.setButtonText (localisation.getText ("ui.close"));

    for (auto* label : { &textScaleLabel, &backgroundLabel, &scrimLabel })
        label->setColour (juce::Label::textColourId, theme.text);

    // Nothing to dim if there is no picture.
    const auto hasImage = AbcTrainLookAndFeel::customBackground().isValid();
    scrimSlider.setEnabled (hasImage);
    clearBackgroundButton.setEnabled (hasImage);

    repaint();
}

void SettingsScreenComponent::chooseBackground()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        localisation.getText ("ui.chooseImage"),
        juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
        "*.png;*.jpg;*.jpeg");

    // The OS picker can outlive this component if the editor is closed
    // while it is open - same SafePointer guard the sounds window uses.
    juce::Component::SafePointer<SettingsScreenComponent> safeThis (this);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto file = chooser.getResult();

            if (! file.existsAsFile())
                return;

            const auto image = juce::ImageFileFormat::loadFrom (file);

            if (! image.isValid())
                return;

            safeThis->properties.setValue (backgroundPathKey, file.getFullPathName());
            safeThis->properties.saveIfNeeded();

            AbcTrainLookAndFeel::setCustomBackground (image,
                                                       (float) safeThis->scrimSlider.getValue());
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

juce::Rectangle<int> SettingsScreenComponent::cardBounds() const
{
    return juce::Rectangle<int> (juce::jlimit (460, getWidth() - 80,
                                                juce::roundToInt ((float) getWidth() * 0.64f)),
                                  juce::jlimit (380, getHeight() - 80,
                                                juce::roundToInt ((float) getHeight() * 0.66f)))
               .withCentre (getLocalBounds().getCentre());
}

juce::Rectangle<int> SettingsScreenComponent::sideMenuBounds() const
{
    return cardBounds().removeFromLeft (150);
}

juce::Rectangle<int> SettingsScreenComponent::pageBounds() const
{
    return cardBounds().withTrimmedLeft (150).reduced (AbcTrainTheme::Spacing::large);
}

void SettingsScreenComponent::selectPage (Page page)
{
    currentPage = page;

    const auto appearance = page == Page::appearance;
    const auto background = page == Page::background;

    textScaleLabel.setVisible (appearance);
    textScaleSlider.setVisible (appearance);
    typefaceLabel.setVisible (appearance);
    typefaceSelector.setVisible (appearance);
    screensaverLabel.setVisible (appearance);
    screensaverSelector.setVisible (appearance);

    for (auto* c : { (juce::Component*) &backgroundLabel, (juce::Component*) &scrimLabel,
                     (juce::Component*) &chooseBackgroundButton,
                     (juce::Component*) &clearBackgroundButton, (juce::Component*) &scrimSlider })
    {
        c->setVisible (background);
    }

    licenceView.setVisible (page == Page::about);

    resized();
    repaint();
}

void SettingsScreenComponent::paintSideMenu (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();

    g.setColour (theme.windowBackground.withAlpha (0.5f));
    g.fillRect (area);

    g.setColour (theme.divider);
    g.drawVerticalLine (area.getRight() - 1, (float) area.getY(), (float) area.getBottom());

    auto row = area.reduced (AbcTrainTheme::Spacing::small,
                              AbcTrainTheme::Spacing::large);

    const juce::String labels[] { localisation.getText ("ui.about"),
                                   localisation.getText ("ui.settingsAppearance"),
                                   localisation.getText ("ui.settingsBackground") };

    for (int i = 0; i < 3; ++i)
    {
        const auto bounds = row.removeFromTop (34);
        row.removeFromTop (2);

        const auto selected = (int) currentPage == i;

        if (selected || hoveredMenuRow == i)
        {
            g.setColour (selected ? theme.accent.withAlpha (0.18f)
                                  : theme.widgetBackground.withAlpha (0.6f));
            g.fillRoundedRectangle (bounds.toFloat().reduced (2.0f, 0.0f),
                                     (float) AbcTrainTheme::Radius::small);
        }

        if (selected)
        {
            g.setColour (theme.accent);
            g.fillRoundedRectangle (bounds.toFloat().withWidth (3.0f).reduced (0.0f, 6.0f), 1.5f);
        }

        g.setColour (selected ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (labels[i], bounds.withTrimmedLeft (14), juce::Justification::centredLeft, true);
    }
}

void SettingsScreenComponent::mouseMove (const juce::MouseEvent& event)
{
    auto row = sideMenuBounds().reduced (AbcTrainTheme::Spacing::small,
                                          AbcTrainTheme::Spacing::large);
    auto found = -1;

    for (int i = 0; i < 3; ++i)
    {
        if (row.removeFromTop (34).contains (event.getPosition()))
            found = i;

        row.removeFromTop (2);
    }

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
    auto row = sideMenuBounds().reduced (AbcTrainTheme::Spacing::small,
                                          AbcTrainTheme::Spacing::large);

    for (int i = 0; i < 3; ++i)
    {
        if (row.removeFromTop (34).contains (event.getPosition()))
        {
            selectPage ((Page) i);
            return;
        }

        row.removeFromTop (2);
    }
}

void SettingsScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    const auto card = cardBounds().toFloat();

    juce::Path shape;
    shape.addRoundedRectangle (card, AbcTrainTheme::Radius::panel);

    juce::DropShadow (theme.shadow.withAlpha (0.6f * theme.shadowStrength), 24, { 0, 6 })
        .drawForPath (g, shape);

    g.setColour (theme.panelBackground);
    g.fillPath (shape);

    // The side rail is clipped to the card's rounded corner, so it does
    // not paint square edges over it.
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (shape);
        paintSideMenu (g, sideMenuBounds());
    }

    g.setColour (theme.outline);
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    auto page = pageBounds();

    const juce::String heading = currentPage == Page::about        ? localisation.getText ("ui.about")
                               : currentPage == Page::appearance   ? localisation.getText ("ui.settingsAppearance")
                                                                   : localisation.getText ("ui.settingsBackground");

    AbcTrainLookAndFeel::drawTrackedText (g, heading,
                                           page.removeFromTop (26).toFloat(),
                                           AbcTrainLookAndFeel::headingFont(),
                                           theme.textBright, 1.2f);

    page.removeFromTop (AbcTrainTheme::Spacing::small);

    if (currentPage == Page::about)
    {
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText ("abcTrain " + juce::String (CurrentVersion::string),
                     page.removeFromTop (18), juce::Justification::centredLeft, false);

        page.removeFromTop (AbcTrainTheme::Spacing::small);
    }
}

void SettingsScreenComponent::resized()
{
    using namespace AbcTrainTheme;

    auto page = pageBounds();
    page.removeFromTop (26 + Spacing::small);

    if (currentPage == Page::about)
    {
        page.removeFromTop (18 + Spacing::small);
        licenceView.setBounds (page.withTrimmedBottom (40));
    }
    else if (currentPage == Page::appearance)
    {
        auto row = page.removeFromTop (32);
        textScaleLabel.setBounds (row.removeFromLeft (120));
        textScaleSlider.setBounds (row);

        page.removeFromTop (AbcTrainTheme::Spacing::small);

        auto fontRow = page.removeFromTop (32);
        typefaceLabel.setBounds (fontRow.removeFromLeft (120));
        typefaceSelector.setBounds (fontRow.removeFromLeft (
            juce::jmax (90, typefaceSelector.getPreferredWidth()))
                .withSizeKeepingCentre (juce::jmax (90, typefaceSelector.getPreferredWidth()), 24));

        page.removeFromTop (AbcTrainTheme::Spacing::small);

        auto saverRow = page.removeFromTop (32);
        screensaverLabel.setBounds (saverRow.removeFromLeft (120));
        const auto saverWidth = juce::jmax (90, screensaverSelector.getPreferredWidth());
        screensaverSelector.setBounds (saverRow.removeFromLeft (saverWidth)
                                           .withSizeKeepingCentre (saverWidth, 24));
    }
    else
    {
        auto row = page.removeFromTop (32);
        backgroundLabel.setBounds (row.removeFromLeft (120));
        clearBackgroundButton.setBounds (row.removeFromRight (100));
        row.removeFromRight (Spacing::small);
        chooseBackgroundButton.setBounds (row);

        page.removeFromTop (Spacing::small);

        auto scrimRow = page.removeFromTop (32);
        scrimLabel.setBounds (scrimRow.removeFromLeft (120));
        scrimSlider.setBounds (scrimRow);
    }

    closeButton.setBounds (pageBounds().removeFromBottom (32).removeFromRight (100));
}
