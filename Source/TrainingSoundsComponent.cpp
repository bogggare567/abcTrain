#include "TrainingSoundsComponent.h"

TrainingSoundsComponent::TrainingSoundsComponent (EarTrainerProcessor& processorToControl)
    : processor (processorToControl)
{
    setOpaque (true);

    titleLabel.setText ("Choose Training Sounds", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);

    pinkNoiseButton.onClick = [this]
    {
        processor.getGameManager().getReferenceAudioLibrary().clearSelection();
        updateStatusLabel();
    };
    addAndMakeVisible (pinkNoiseButton);

    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0a0b0));
    addAndMakeVisible (statusLabel);

    closeButton.onClick = [this]
    {
        setVisible (false);
        if (onClosed != nullptr)
            onClosed();
    };
    addAndMakeVisible (closeButton);
}

void TrainingSoundsComponent::refresh()
{
    categoryButtons.clear();

    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    library.rescan();

    const auto maxLevelReached = processor.getProgressManager().getMaxLevelReached();
    const auto& categories = library.getCategories();

    for (int i = 0; i < categories.size(); ++i)
    {
        const auto& category = categories.getReference (i);
        const bool unlocked = maxLevelReached > i;

        auto* button = categoryButtons.add (new juce::TextButton (
            unlocked ? category.name
                     : category.name + " (reach level " + juce::String (i + 1) + ")"));
        button->setEnabled (unlocked);
        button->onClick = [this, i] { selectCategory (i); };
        addAndMakeVisible (button);
    }

    updateStatusLabel();
    resized();
}

void TrainingSoundsComponent::selectCategory (int categoryIndex)
{
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto& categories = library.getCategories();

    if (categoryIndex < 0 || categoryIndex >= categories.size())
        return;

    const auto& files = categories.getReference (categoryIndex).files;
    if (files.isEmpty())
        return;

    const auto& chosen = files.getReference (random.nextInt (files.size()));
    library.selectFile (chosen, processor.getSampleRate());
    updateStatusLabel();
}

void TrainingSoundsComponent::updateStatusLabel()
{
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto selected = library.getSelectedFile();

    statusLabel.setText (selected.existsAsFile()
                              ? "Training on: " + selected.getFileNameWithoutExtension()
                              : "Training on: Pink Noise (default)",
                          juce::dontSendNotification);
}

void TrainingSoundsComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));
    g.setColour (juce::Colour (0xff5b9bd5).withAlpha (0.6f));
    g.drawRect (getLocalBounds(), 2);

    if (categoryButtons.isEmpty())
    {
        g.setColour (juce::Colour (0xffa0a0b0));
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawFittedText ("No categories found under " + processor.getGameManager().getReferenceAudioLibrary().getRootFolder().getFullPathName()
                               + " - add subfolders of your own audio files there, one subfolder per category.",
                           getLocalBounds().reduced (24).withTrimmedTop (80), juce::Justification::centredTop, 4);
    }
}

void TrainingSoundsComponent::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);
    pinkNoiseButton.setBounds (area.removeFromTop (32));
    area.removeFromTop (8);

    auto bottomRow = area.removeFromBottom (32);
    closeButton.setBounds (bottomRow.removeFromRight (100));
    statusLabel.setBounds (bottomRow.reduced (8, 0));
    area.removeFromBottom (8);

    for (auto* button : categoryButtons)
    {
        button->setBounds (area.removeFromTop (32));
        area.removeFromTop (6);
    }
}
