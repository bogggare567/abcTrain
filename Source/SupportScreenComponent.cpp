#include "SupportScreenComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include "../shared/AppIcons.h"

SupportScreenComponent::SupportScreenComponent()
{
    setOpaque (true);

    donateButton.setButtonText ("Support the project");
    donateButton.onClick = []
    {
        juce::URL ("https://soundkorb.ru").launchInDefaultBrowser();
    };
    addAndMakeVisible (donateButton);

    starButton.setButtonText ("Star on GitHub");
    starButton.onClick = []
    {
        juce::URL ("https://github.com/bogggare567/abcTrain").launchInDefaultBrowser();
    };
    addAndMakeVisible (starButton);

    continueButton.setButtonText ("Continue");
    continueButton.onClick = [this]
    {
        if (onDismissed != nullptr)
            onDismissed();
    };
    addAndMakeVisible (continueButton);

    addAndMakeVisible (repoLink);
}

void SupportScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    auto area = getLocalBounds().reduced (AbcTrainTheme::Spacing::large * 2);
    area.removeFromTop (40);

    // The plugin's own icon, large - the first thing seen should be the
    // thing itself, not a wall of text.
    const auto iconBox = area.removeFromTop (72).withSizeKeepingCentre (64, 64).toFloat();
    AppIcons::draw (g, AppIcons::Icon::eq, iconBox, theme.accent);

    area.removeFromTop (AbcTrainTheme::Spacing::large);

    AbcTrainLookAndFeel::drawTrackedText (g, "abcTrain", area.removeFromTop (34).toFloat(),
                                           AbcTrainLookAndFeel::titleFont(),
                                           theme.textBright, 2.0f,
                                           juce::Justification::centred);

    area.removeFromTop (AbcTrainTheme::Spacing::small);

    g.setColour (theme.textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawFittedText ("Ear training for mixing engineers, and three teaching plugins "
                      "that process your own audio.\n\n"
                      "Free, open source, and always will be. It is built and maintained "
                      "in the open - if it is useful to you, a donation keeps it moving, "
                      "and a star helps other people find it.\n\n"
                      "Neither is required. Nothing here is locked.",
                      area.removeFromTop (120), juce::Justification::centredTop, 8);
}

void SupportScreenComponent::resized()
{
    auto area = getLocalBounds().reduced (AbcTrainTheme::Spacing::large * 2);
    area.removeFromTop (40 + 72 + AbcTrainTheme::Spacing::large + 34
                        + AbcTrainTheme::Spacing::small + 120 + AbcTrainTheme::Spacing::large);

    auto row = area.removeFromTop (38).withSizeKeepingCentre (
                   juce::jmin (area.getWidth(), 360), 38);
    donateButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
    row.removeFromLeft (8);
    starButton.setBounds (row);

    area.removeFromTop (AbcTrainTheme::Spacing::medium);
    continueButton.setBounds (area.removeFromTop (34).withSizeKeepingCentre (160, 34));

    repoLink.setBounds (getLocalBounds().removeFromBottom (40).reduced (AbcTrainTheme::Spacing::large, 10));
}
