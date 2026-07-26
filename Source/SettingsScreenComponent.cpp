#include "SettingsScreenComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"

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

    refresh();
}

SettingsScreenComponent::~SettingsScreenComponent() = default;

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
    return juce::Rectangle<int> (juce::jmin (getWidth() - 48, 460),
                                  juce::jmin (getHeight() - 48, 300))
               .withCentre (getLocalBounds().getCentre());
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
    g.setColour (theme.outline);
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    auto inner = card.reduced ((float) AbcTrainTheme::Spacing::large);

    AbcTrainLookAndFeel::drawTrackedText (g, localisation.getText ("ui.settings"),
                                           inner.removeFromTop (26.0f),
                                           juce::Font (juce::FontOptions (17.0f).withStyle ("Bold")),
                                           theme.textBright, 1.2f);

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::medium);
    AbcTrainLookAndFeel::paintSectionHeading (g, inner.removeFromTop (24.0f), headingAppearance);

    inner.removeFromTop (32.0f + (float) AbcTrainTheme::Spacing::medium);
    AbcTrainLookAndFeel::paintSectionHeading (g, inner.removeFromTop (24.0f), headingBackground);
}

void SettingsScreenComponent::resized()
{
    using namespace AbcTrainTheme;

    auto area = cardBounds().reduced (Spacing::large);

    area.removeFromTop (26 + Spacing::medium);
    area.removeFromTop (24);                       // "Appearance"

    {
        auto row = area.removeFromTop (32);
        textScaleLabel.setBounds (row.removeFromLeft (130));
        textScaleSlider.setBounds (row);
    }

    area.removeFromTop (Spacing::medium);
    area.removeFromTop (24);                       // "Background"

    {
        auto row = area.removeFromTop (32);
        backgroundLabel.setBounds (row.removeFromLeft (130));
        clearBackgroundButton.setBounds (row.removeFromRight (110));
        row.removeFromRight (Spacing::small);
        chooseBackgroundButton.setBounds (row);
    }

    area.removeFromTop (Spacing::small);

    {
        auto row = area.removeFromTop (32);
        scrimLabel.setBounds (row.removeFromLeft (130));
        scrimSlider.setBounds (row);
    }

    closeButton.setBounds (cardBounds().reduced (Spacing::large)
                               .removeFromBottom (34).removeFromRight (110));
}
