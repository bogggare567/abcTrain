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
                                           juce::String importedNothing, juce::String importHint)
{
    importButton.setButtonText (importAndSortText);
    // Until an import has run, this line explains the button rather than
    // sitting empty.
    if (hintText.isEmpty() || hintText == previousHint)
        hintText = importHint;

    previousHint = importHint;
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
                                           inner.removeFromTop (30.0f),
                                           juce::Font (juce::FontOptions (17.0f).withStyle ("Bold")),
                                           theme.textBright, 1.2f);

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::large);
    inner.removeFromTop (44.0f);                     // the add-music row
    inner.removeFromTop ((float) AbcTrainTheme::Spacing::small);

    // One line under the button: either what is happening, or what this
    // does. The old layout spent a whole section heading plus a full file
    // path on saying where things are stored, which is not a thing anybody
    // needed to know before pressing the button.
    {
        auto row = inner.removeFromTop (16.0f);

        if (importRunning)
        {
            paintImportProgress (g, row.removeFromLeft (row.getWidth() * 0.5f)
                                        .withSizeKeepingCentre ((int) (row.getWidth() * 0.5f), 6)
                                        .toNearestInt());

            g.setColour (theme.textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (importProgressFile, row.toNearestInt(),
                         juce::Justification::centredRight, true);
        }
        else
        {
            g.setColour (theme.textDim);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (hintText, row.toNearestInt(), juce::Justification::centredLeft, true);
        }
    }

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::large);
    AbcTrainLookAndFeel::paintSectionHeading (g, inner.removeFromTop (20.0f), trainOnHeading);

    // Each row's clip count and its selected tick. The buttons draw their
    // own labels; this is what a plain button cannot say.
    {
        auto& library = processor.getGameManager().getReferenceAudioLibrary();
        const auto& categories = library.getCategories();
        const auto active = library.getActiveCategory();

        for (int i = 0; i < categoryButtons.size() && i < categories.size(); ++i)
        {
            const auto bounds = categoryButtons[i]->getBounds().toFloat();
            const auto& category = categories.getReference (i);

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (11.0f));
            g.drawText (juce::String (category.files.size()),
                         bounds.withTrimmedRight (12.0f).toNearestInt(),
                         juce::Justification::centredRight, false);

            if (category.name == active)
            {
                juce::Path tick;
                tick.startNewSubPath (bounds.getX() + 10.0f, bounds.getCentreY());
                tick.lineTo (bounds.getX() + 15.0f, bounds.getCentreY() + 5.0f);
                tick.lineTo (bounds.getX() + 23.0f, bounds.getCentreY() - 5.0f);

                g.setColour (theme.positive);
                g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
            }
        }
    }

    if (categoryButtons.isEmpty())
    {
        g.setColour (theme.textDim);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (emptyMessage, inner.toNearestInt(), juce::Justification::centredTop, 4);
    }
}

juce::Rectangle<int> TrainingSoundsComponent::cardBounds() const
{
    // One row per option, not a grid of buttons: the list has a count and
    // a tick beside each entry, which a grid of equal-width buttons cannot
    // carry. Grows with the number of categories, capped by the window.
    const auto rows = juce::jmax (1, categoryButtons.size());
    const auto wanted = AbcTrainTheme::Spacing::large * 2
                            + 30                                       // title
                            + AbcTrainTheme::Spacing::large
                            + 44                                       // add-music row
                            + AbcTrainTheme::Spacing::small + 16       // hint line
                            + AbcTrainTheme::Spacing::large + 20       // "train on"
                            + rows * (rowHeight + 2)
                            + AbcTrainTheme::Spacing::large + 34;      // footer

    return juce::Rectangle<int> (juce::jmin (getWidth() - 48, 480),
                                  juce::jmin (getHeight() - 48, wanted))
               .withCentre (getLocalBounds().getCentre());
}

void TrainingSoundsComponent::resized()
{
    using namespace AbcTrainTheme;

    auto area = cardBounds().reduced (Spacing::large);

    area.removeFromTop (30 + Spacing::large);

    // One wide primary action. "Choose folder" is still here, but as a
    // quiet secondary - almost nobody wants to answer a question about
    // storage layout, and the two used to sit side by side as equals.
    {
        auto row = area.removeFromTop (44);
        chooseFolderButton.setBounds (row.removeFromRight (120).reduced (0, 6));
        row.removeFromRight (Spacing::small);
        importButton.setBounds (row);
    }

    area.removeFromTop (Spacing::small + 16);       // the hint / progress line
    area.removeFromTop (Spacing::large + 20);       // "what to train on"

    for (auto* button : categoryButtons)
    {
        button->setBounds (area.removeFromTop (rowHeight));
        area.removeFromTop (2);
    }

    rootFolderLabel.setVisible (false);
    statusLabel.setVisible (false);

    auto footer = cardBounds().reduced (Spacing::large).removeFromBottom (34);
    closeButton.setBounds (footer.removeFromRight (110));
    footer.removeFromRight (Spacing::small);
    pinkNoiseButton.setBounds (footer.removeFromLeft (150));
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
        hintText = importedNothingText;
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

    hintText = importedClipsText.replace ("{{count}}", juce::String (clipsWritten))
                   + (parts.isEmpty() ? juce::String() : "  -  " + parts.joinIntoString (", "));

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
