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

TrainingSoundsComponent::~TrainingSoundsComponent() = default;

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

    // Sets the *category*, not one file. The library then swaps in a
    // different clip each round - see advanceToRandomClip. Picking one
    // file and looping it all session was the fastest way to stop hearing
    // the material.
    library.setActiveCategory (categories.getReference (categoryIndex).name,
                                processor.getSampleRate());
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

    inner.removeFromTop (36.0f + (float) AbcTrainTheme::Spacing::small);

    // While an import runs, the row that normally shows the library path
    // shows how far along it is and which file is being worked on.
    if (importRunning)
    {
        auto row = inner.removeFromTop (18.0f);

        paintImportProgress (g, row.removeFromLeft (row.getWidth() * 0.55f)
                                    .withSizeKeepingCentre ((int) (row.getWidth() * 0.55f), 8)
                                    .toNearestInt());

        g.setColour (theme.textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (importProgressFile, row.toNearestInt(),
                     juce::Justification::centredRight, true);
    }
    else
    {
        inner.removeFromTop (18.0f);
    }

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::medium);
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
    // Hidden while importing - the progress bar takes this row (paint()).
    rootFolderLabel.setBounds (area.removeFromTop (18));
    rootFolderLabel.setVisible (! importRunning);

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


// Runs the slicing off the message thread. Owns nothing the UI owns.
class TrainingSoundsComponent::ImportJob : public juce::Thread
{
public:
    ImportJob (TrainingSoundsComponent& ownerToUse, juce::Array<juce::File> filesToImport)
        : juce::Thread ("abcTrain import"), owner (ownerToUse), files (std::move (filesToImport))
    {
    }

    ~ImportJob() override
    {
        // Waits, rather than killing: the worker is midway through writing
        // a WAV, and a half-written file in the library is worse than a
        // moment's delay closing the window.
        stopThread (4000);
    }

    void run() override
    {
        juce::Component::SafePointer<TrainingSoundsComponent> safeOwner (&owner);

        const auto written = owner.processor.getGameManager().getReferenceAudioLibrary()
                                 .importAndSliceMany (
                                     files,
                                     [safeOwner] (float progress, juce::String fileName)
                                     {
                                         if (safeOwner == nullptr)
                                             return;

                                         safeOwner->importProgress.store (progress);

                                         juce::MessageManager::callAsync ([safeOwner, fileName]
                                         {
                                             if (safeOwner != nullptr)
                                             {
                                                 safeOwner->importProgressFile = fileName;
                                                 safeOwner->repaint();
                                             }
                                         });
                                     },
                                     [this] { return threadShouldExit(); });

        juce::MessageManager::callAsync ([safeOwner, written]
        {
            if (safeOwner != nullptr)
                safeOwner->finishImport (written);
        });
    }

private:
    TrainingSoundsComponent& owner;
    juce::Array<juce::File> files;
};

void TrainingSoundsComponent::importAndSort()
{
    if (importRunning)
        return;

    // Files, not a folder, and as many as you like - the way every other
    // "add your music" dialog on the machine works. Choosing a folder made
    // the player answer a question about storage layout before they could
    // find out whether the feature was any good.
    fileChooser = std::make_unique<juce::FileChooser> (
        importButton.getButtonText(),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac;*.mp3");

    juce::Component::SafePointer<TrainingSoundsComponent> safeThis (this);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::canSelectMultipleItems,
        [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            const auto chosen = chooser.getResults();

            if (chosen.isEmpty())
                return;

            safeThis->startImport (chosen);
        });
}

void TrainingSoundsComponent::startImport (const juce::Array<juce::File>& files)
{
    importJob.reset();

    importRunning = true;
    importProgress.store (0.0f);
    importProgressFile = {};

    importButton.setEnabled (false);
    chooseFolderButton.setEnabled (false);

    importJob = std::make_unique<ImportJob> (*this, files);
    importJob->startThread();

    repaint();
}

void TrainingSoundsComponent::finishImport (int clipsWritten)
{
    importRunning = false;
    importButton.setEnabled (true);
    chooseFolderButton.setEnabled (true);

    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    library.rescan();

    if (clipsWritten <= 0)
    {
        statusLabel.setText (importedNothingText, juce::dontSendNotification);
        refresh();
        return;
    }

    // What was added, broken down - "47 clips" says less than the shape of
    // the library it just built, and the shape is what tells you whether
    // the material you imported was any use.
    juce::StringArray parts;

    for (const auto& category : library.getCategories())
        if (! category.name.startsWith ("Built-in") && ! category.files.isEmpty())
            parts.add (juce::String (category.files.size()) + " " + category.name.toLowerCase());

    statusLabel.setText (importedClipsText.replace ("{{count}}", juce::String (clipsWritten))
                             + (parts.isEmpty() ? juce::String()
                                                : "  -  " + parts.joinIntoString (", ")),
                          juce::dontSendNotification);

    refresh();
}

void TrainingSoundsComponent::paintImportProgress (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();
    const auto bounds = area.toFloat();

    g.setColour (theme.displayBackground);
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    const auto progress = juce::jlimit (0.0f, 1.0f, importProgress.load());

    if (progress > 0.001f)
    {
        g.setColour (theme.accent);
        g.fillRoundedRectangle (bounds.withWidth (juce::jmax (bounds.getHeight(),
                                                              bounds.getWidth() * progress)),
                                 bounds.getHeight() * 0.5f);
    }

    g.setColour (theme.outline.withAlpha (0.6f));
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);
}
