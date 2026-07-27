#include "TrainingSoundsComponent.h"
#include "../shared/AbcTrainTheme.h"
#include "../shared/AbcTrainLookAndFeel.h"

TrainingSoundsComponent::TrainingSoundsComponent (EarTrainerProcessor& processorToControl)
    : processor (processorToControl)
{
    setOpaque (true);

    titleLabel.setText ("Choose Training Sounds", juce::dontSendNotification);
    // Drawn by paint() with letter-spacing, like every other heading here.
    titleLabel.setVisible (false);

    chooseFolderButton.onClick = [this]
    {
        // FileChooser::launchAsync's callback can outlive this component
        // if the editor is closed while the OS picker is still open -
        // the lambda's SafePointer null-checks before touching `this`.
        fileChooser = std::make_unique<juce::FileChooser> (
            "Choose a folder of your own reference audio",
            processor.getGameManager().getReferenceAudioLibrary().getRootFolder(),
            "*");

        juce::Component::SafePointer<TrainingSoundsComponent> safeThis (this);

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [safeThis] (const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr)
                    return;

                const auto chosen = chooser.getResult();
                if (chosen == juce::File())
                    return;

                safeThis->processor.getGameManager().getReferenceAudioLibrary().setRootFolder (chosen);
                safeThis->refresh();
            });
    };
    addAndMakeVisible (chooseFolderButton);

    importButton.onClick = [this] { importAndSort(); };
    addAndMakeVisible (importButton);

    rootFolderLabel.setJustificationType (juce::Justification::centred);
    rootFolderLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (rootFolderLabel);

    pinkNoiseButton.onClick = [this]
    {
        processor.getGameManager().getReferenceAudioLibrary().clearSelection();
        updateStatusLabel();
    };
    addAndMakeVisible (pinkNoiseButton);

    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    closeButton.onClick = [this]
    {
        setVisible (false);
        if (onClosed != nullptr)
            onClosed();
    };
    addAndMakeVisible (closeButton);
}

void TrainingSoundsComponent::setStrings (juce::String title, juce::String sourceSection,
                                           juce::String trainOnSection, juce::String chooseFolder,
                                           juce::String pinkNoise, juce::String close,
                                           juce::String emptyText, juce::String importAndSortText,
                                           juce::String importing, juce::String importedClips,
                                           juce::String importedNothing)
{
    importButton.setButtonText (importAndSortText);
    importingText = std::move (importing);
    importedClipsText = std::move (importedClips);
    importedNothingText = std::move (importedNothing);

    titleLabel.setText (title, juce::dontSendNotification);
    sourceHeading = std::move (sourceSection);
    trainOnHeading = std::move (trainOnSection);
    chooseFolderButton.setButtonText (chooseFolder);
    pinkNoiseButton.setButtonText (pinkNoise);
    closeButton.setButtonText (close);
    emptyMessage = std::move (emptyText);
    resized();
    repaint();
}

void TrainingSoundsComponent::refresh()
{
    // Re-read the palette here, not in the constructor. Colours captured
    // once at construction survive a theme switch unchanged - which is
    // how dim text ended up invisible: a dark-theme grey left sitting on
    // a near-white light-theme panel, and the reverse.
    const auto& theme = AbcTrainTheme::current();
    rootFolderLabel.setColour (juce::Label::textColourId, theme.textDim);
    statusLabel.setColour (juce::Label::textColourId, theme.textDim);
    titleLabel.setColour (juce::Label::textColourId, theme.textBright);

    categoryButtons.clear();

    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    library.rescan();

    rootFolderLabel.setText ("Folder: " + library.getRootFolder().getFullPathName(), juce::dontSendNotification);

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
    const auto& theme = AbcTrainTheme::current();

    // A dimmed backdrop with a real card on top, instead of the whole
    // window filled edge to edge and outlined in a 2px accent rectangle.
    // The old version read as an error dialog: a hard coloured border
    // around a column of identical full-width buttons, with the folder
    // path in 11px underneath.
    // Opaque. The first version dimmed the screen to 82% and left the home
    // screen legible underneath, so the card floated on a blurry mess of
    // tiles - reported as "нет фона никакого". setOpaque(true) is already
    // set on this component, and a translucent fill under an opaque
    // component is a contradiction JUCE will happily draw.
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

    AbcTrainLookAndFeel::drawTrackedText (g, titleLabel.getText(),
                                           inner.removeFromTop (26.0f),
                                           juce::Font (juce::FontOptions (17.0f).withStyle ("Bold")),
                                           theme.textBright, 1.2f,
                                           juce::Justification::centredLeft);

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::medium);

    // Section headings, the same treatment the training screen uses, so
    // the two screens are visibly the same product.
    AbcTrainLookAndFeel::paintSectionHeading (g, inner.removeFromTop (24.0f), sourceHeading);

    inner.removeFromTop (36.0f + (float) AbcTrainTheme::Spacing::small + 18.0f
                          + (float) AbcTrainTheme::Spacing::medium);
    AbcTrainLookAndFeel::paintSectionHeading (g, inner.removeFromTop (24.0f), trainOnHeading);

    if (categoryButtons.isEmpty())
    {
        auto empty = cardBounds().reduced (AbcTrainTheme::Spacing::large)
                         .withTrimmedTop (150);

        g.setColour (theme.textDim);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (emptyMessage, empty, juce::Justification::centredTop, 4);
    }
}

juce::Rectangle<int> TrainingSoundsComponent::cardBounds() const
{
    // Grows with the number of categories, up to what the window can hold.
    const auto rows = (categoryButtons.size() + categoryColumns - 1) / categoryColumns;
    const auto wanted = AbcTrainTheme::Spacing::large * 2 + 26 + AbcTrainTheme::Spacing::medium
                            + 24 + 36 + AbcTrainTheme::Spacing::small + 18
                            + AbcTrainTheme::Spacing::medium + 24
                            + juce::jmax (1, rows) * (categoryTileHeight + AbcTrainTheme::Spacing::small)
                            + AbcTrainTheme::Spacing::medium + 34;

    return juce::Rectangle<int> (juce::jmin (getWidth() - 48, 520),
                                  juce::jmin (getHeight() - 48, wanted))
               .withCentre (getLocalBounds().getCentre());
}

void TrainingSoundsComponent::resized()
{
    using namespace AbcTrainTheme;

    auto area = cardBounds().reduced (Spacing::large);

    area.removeFromTop (26 + Spacing::medium);   // title
    area.removeFromTop (24);                     // "Where the sounds come from"

    {
        auto row = area.removeFromTop (36);
        importButton.setBounds (row.removeFromRight (row.getWidth() / 2 - 4));
        row.removeFromRight (8);
        chooseFolderButton.setBounds (row);
    }

    area.removeFromTop (Spacing::small);
    rootFolderLabel.setBounds (area.removeFromTop (18));

    area.removeFromTop (Spacing::medium);
    area.removeFromTop (24);                     // "What to train on"

    // A grid, not a column. Nine full-width buttons stacked vertically is
    // a list of commands; a grid of tiles is a set of things to choose
    // between, which is what this actually is.
    {
        auto grid = area.removeFromTop (
            juce::jmax (1, (categoryButtons.size() + categoryColumns - 1) / categoryColumns)
                * (categoryTileHeight + Spacing::small));

        auto index = 0;

        while (index < categoryButtons.size())
        {
            auto row = grid.removeFromTop (categoryTileHeight);
            grid.removeFromTop (Spacing::small);

            const auto columnWidth = (row.getWidth() - Spacing::small * (categoryColumns - 1))
                                         / categoryColumns;

            for (auto column = 0; column < categoryColumns && index < categoryButtons.size(); ++column)
            {
                categoryButtons[index++]->setBounds (row.removeFromLeft (columnWidth));
                row.removeFromLeft (Spacing::small);
            }
        }
    }

    auto bottomRow = cardBounds().reduced (Spacing::large).removeFromBottom (34);
    closeButton.setBounds (bottomRow.removeFromRight (110));
    bottomRow.removeFromRight (Spacing::small);
    pinkNoiseButton.setBounds (bottomRow.removeFromLeft (150));
    statusLabel.setBounds (bottomRow.reduced (Spacing::small, 0));
}


void TrainingSoundsComponent::importAndSort()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        importButton.getButtonText(),
        processor.getGameManager().getReferenceAudioLibrary().getRootFolder(),
        "*.wav;*.aiff;*.aif;*.flac;*.mp3");

    juce::Component::SafePointer<TrainingSoundsComponent> safeThis (this);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::canSelectDirectories,
        [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto chosen = chooser.getResult();

            if (chosen == juce::File())
                return;

            // Say something before the wait, not after it. Decoding and
            // analysing a folder of full-length tracks takes real seconds,
            // and a window that simply freezes reads as a crash.
            safeThis->statusLabel.setText (safeThis->importingText, juce::dontSendNotification);
            safeThis->repaint();

            juce::MessageManager::callAsync ([safeThis, chosen]
            {
                if (safeThis == nullptr)
                    return;

                const auto written = safeThis->processor.getGameManager()
                                         .getReferenceAudioLibrary().importAndSlice (chosen);

                safeThis->statusLabel.setText (
                    written > 0 ? safeThis->importedClipsText.replace ("{{count}}", juce::String (written))
                                : safeThis->importedNothingText,
                    juce::dontSendNotification);

                safeThis->refresh();
            });
        });
}
