#include "TrainingSoundsComponent.h"
#include "../shared/AbcTrainTheme.h"
#include "../shared/AbcTrainLookAndFeel.h"

TrainingSoundsComponent::TrainingSoundsComponent (EarTrainerProcessor& processorToControl)
    : processor (processorToControl)
{
    setOpaque (true);

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
    rootFolderLabel.setFont (AbcTrainLookAndFeel::captionFont());
    addAndMakeVisible (rootFolderLabel);

    // Reveals the library folder in Finder/Explorer. "Where did my import
    // go" was unanswerable from this screen, and the answer is a place, so
    // the honest control is one that takes you there.
    revealButton.onClick = [this]
    {
        processor.getGameManager().getReferenceAudioLibrary().getRootFolder()
            .revealToUser();
    };
    addAndMakeVisible (revealButton);

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

void TrainingSoundsComponent::setStrings (Strings strings)
{
    // The import hint doubles as the result line after an import has run,
    // so it is only reset while it is still saying what the button does.
    const auto hintWasStock = text.importHint.isEmpty()
                                  || text.importHint == previousHint;

    previousHint = strings.importHint;
    const auto keptHint = text.importHint;

    text = std::move (strings);

    if (! hintWasStock)
        text.importHint = keptHint;

    importButton.setButtonText (text.importAndSort);
    titleLabel.setText (text.title, juce::dontSendNotification);
    chooseFolderButton.setButtonText (text.chooseFolder);
    revealButton.setButtonText (text.openFolder);
    closeButton.setButtonText (text.close);

    updateStatusLabel();
    resized();
    repaint();
}

juce::String TrainingSoundsComponent::displayNameForCategory (const juce::String& rawName) const
{
    if (rawName == "Built-in Percussive") return text.builtInPercussive;
    if (rawName == "Built-in Sustained")  return text.builtInSustained;

    // Anything else is a folder somebody made. Its name is theirs.
    return rawName;
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

    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    library.rescan();

    rootFolderLabel.setText (library.getRootFolder().getFullPathName(), juce::dontSendNotification);

    // Whatever the library currently says is active decides which rail row
    // is lit, so opening this screen shows the truth rather than a default.
    selectedCategory = -1;
    const auto& categories = library.getCategories();

    for (int i = 0; i < categories.size(); ++i)
        if (categories.getReference (i).name == library.getActiveCategory())
            selectedCategory = i;

    // A pinned file, or a selection saved by an older build, can leave a
    // file active with no category name to match it. Find the category it
    // lives in rather than showing nothing selected at all.
    if (selectedCategory < 0 && library.getSelectedFile().existsAsFile())
        for (int i = 0; i < categories.size() && selectedCategory < 0; ++i)
            if (categories.getReference (i).files.contains (library.getSelectedFile()))
                selectedCategory = i;

    fileScroll = 0.0f;
    updateStatusLabel();
    resized();
    repaint();
}

const juce::Array<juce::File>* TrainingSoundsComponent::filesForSelection() const
{
    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    if (selectedCategory < 0 || selectedCategory >= categories.size())
        return nullptr;

    return &categories.getReference (selectedCategory).files;
}

void TrainingSoundsComponent::selectPinkNoise()
{
    processor.getGameManager().getReferenceAudioLibrary().clearSelection();
    selectedCategory = -1;
    fileScroll = 0.0f;
    updateStatusLabel();
    repaint();
}

void TrainingSoundsComponent::pinFile (int fileIndex)
{
    const auto* files = filesForSelection();

    if (files == nullptr || fileIndex < 0 || fileIndex >= files->size())
        return;

    processor.getGameManager().getReferenceAudioLibrary()
        .pinFile ((*files)[fileIndex], processor.getSampleRate());

    updateStatusLabel();
    repaint();
}

void TrainingSoundsComponent::selectCategory (int categoryIndex)
{
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto& categories = library.getCategories();

    if (categoryIndex < 0 || categoryIndex >= categories.size())
        return;

    // Selecting the category is selecting *rotation* - the library swaps in
    // a different clip each round. Clicking a file in the right-hand pane
    // pins one instead; clicking the category again goes back to shuffling.
    library.setActiveCategory (categories.getReference (categoryIndex).name,
                                processor.getSampleRate());

    selectedCategory = categoryIndex;
    fileScroll = 0.0f;
    updateStatusLabel();
    repaint();
}

void TrainingSoundsComponent::updateStatusLabel()
{
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto selected = library.getSelectedFile();

    if (! selected.existsAsFile())
    {
        statusLabel.setText (text.trainingOnPinkNoise, juce::dontSendNotification);
        return;
    }

    // Pinned and shuffling are different states and the screen has to say
    // which one it is in - otherwise a filename on screen looks like a
    // promise that this is the clip you will hear next round.
    const auto category = library.getActiveCategory();

    const auto file = selected.getFileNameWithoutExtension();

    statusLabel.setText (library.isPinned() || category.isEmpty()
                              ? text.trainingOnFile.replace ("{{file}}", file)
                              : text.shuffling
                                    .replace ("{{category}}", displayNameForCategory (category))
                                    .replace ("{{file}}", file),
                          juce::dontSendNotification);
}

namespace
{
    constexpr int railWidth = 196;
    constexpr int headerHeight = 30;
    constexpr int actionRowHeight = 44;
    constexpr int hintHeight = 16;
    constexpr int footerHeight = 34;
}

juce::Rectangle<int> TrainingSoundsComponent::railBounds() const
{
    using namespace AbcTrainTheme;

    auto area = cardBounds().reduced (Spacing::large);
    area.removeFromTop (headerHeight + Spacing::large + actionRowHeight
                        + Spacing::small + hintHeight + Spacing::large);
    area.removeFromBottom (footerHeight + Spacing::small);

    return area.removeFromLeft (railWidth);
}

juce::Rectangle<int> TrainingSoundsComponent::filePaneBounds() const
{
    using namespace AbcTrainTheme;

    auto area = cardBounds().reduced (Spacing::large);
    area.removeFromTop (headerHeight + Spacing::large + actionRowHeight
                        + Spacing::small + hintHeight + Spacing::large);
    area.removeFromBottom (footerHeight + Spacing::small);
    area.removeFromLeft (railWidth + Spacing::medium);

    return area;
}

juce::Rectangle<int> TrainingSoundsComponent::categoryRowBounds (int index) const
{
    auto rail = railBounds();
    rail.removeFromTop (18);   // the rail's own heading

    return { rail.getX(), rail.getY() + (index + 1) * rowHeight, rail.getWidth(), rowHeight - 2 };
}

juce::Rectangle<int> TrainingSoundsComponent::fileRowBounds (int index) const
{
    auto pane = filePaneBounds();
    pane.removeFromTop (18);

    return { pane.getX(), pane.getY() + (int) ((float) index * rowHeight - fileScroll),
             pane.getWidth(), rowHeight - 2 };
}

void TrainingSoundsComponent::paintRail (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto& categories = library.getCategories();

    AbcTrainLookAndFeel::paintSectionHeading (g, railBounds().removeFromTop (18).toFloat(),
                                               text.trainOnSection);

    const auto drawRow = [&] (int index, const juce::String& name, const juce::String& detail,
                              bool selected)
    {
        const auto row = categoryRowBounds (index);

        if (index == hoveredCategoryRow || selected)
        {
            g.setColour (selected ? theme.accent.withAlpha (0.22f)
                                   : theme.widgetBackground.withAlpha (0.5f));
            g.fillRoundedRectangle (row.toFloat(), AbcTrainTheme::Radius::button);
        }

        auto text = row.reduced (10, 0);

        g.setColour (selected ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText (name, text.removeFromLeft (text.getWidth() - 34),
                     juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (11.0f));
        g.drawText (detail, text, juce::Justification::centredRight, false);
    };

    drawRow (-1, text.pinkNoise, {}, ! library.getSelectedFile().existsAsFile());

    for (int i = 0; i < categories.size(); ++i)
        drawRow (i, displayNameForCategory (categories.getReference (i).name),
                  juce::String (categories.getReference (i).files.size()),
                  i == selectedCategory && library.getSelectedFile().existsAsFile());
}

void TrainingSoundsComponent::paintFilePane (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto* files = filesForSelection();
    const auto pane = filePaneBounds();

    AbcTrainLookAndFeel::paintSectionHeading (g, pane.toFloat().withHeight (18.0f),
                                               files == nullptr ? juce::String() : text.clipsHeading);

    if (files == nullptr)
    {
        // Two different empty states. "There is nothing here" and "you have
        // not picked anything yet" are not the same problem, and telling
        // someone to import music when they already have four categories on
        // the left is how a screen loses their trust.
        const auto anything = ! library.getCategories().isEmpty();

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawFittedText (anything ? text.pickCategory : text.empty,
                           pane.withTrimmedTop (18), juce::Justification::centredTop, 4);
        return;
    }

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (pane.withTrimmedTop (18));

    const auto pinnedFile = library.isPinned() ? library.getSelectedFile() : juce::File();

    for (int i = 0; i < files->size(); ++i)
    {
        const auto row = fileRowBounds (i);

        if (! row.intersects (pane))
            continue;

        const auto isPinned = pinnedFile == (*files)[i];

        if (i == hoveredFileRow || isPinned)
        {
            g.setColour (isPinned ? theme.accent.withAlpha (0.22f)
                                   : theme.widgetBackground.withAlpha (0.5f));
            g.fillRoundedRectangle (row.toFloat(), AbcTrainTheme::Radius::button);
        }

        auto text = row.reduced (10, 0);

        if (isPinned)
        {
            juce::Path tick;
            const auto box = text.removeFromLeft (18).toFloat();
            tick.startNewSubPath (box.getX(), box.getCentreY());
            tick.lineTo (box.getX() + 4.0f, box.getCentreY() + 4.0f);
            tick.lineTo (box.getX() + 11.0f, box.getCentreY() - 5.0f);
            g.setColour (theme.positive);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
        else
        {
            text.removeFromLeft (18);
        }

        g.setColour (isPinned ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText ((*files)[i].getFileNameWithoutExtension(), text,
                     juce::Justification::centredLeft, true);
    }
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
                                           inner.removeFromTop ((float) headerHeight),
                                           AbcTrainLookAndFeel::headingFont(),
                                           theme.textBright, 1.2f);

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::large);
    inner.removeFromTop ((float) actionRowHeight);
    inner.removeFromTop ((float) AbcTrainTheme::Spacing::small);

    {
        auto row = inner.removeFromTop ((float) hintHeight);

        if (importRunning)
        {
            paintImportProgress (g, row.removeFromLeft (row.getWidth() * 0.5f)
                                        .withSizeKeepingCentre ((int) (row.getWidth() * 0.5f), 6)
                                        .toNearestInt());

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            g.drawText (importProgressFile, row.toNearestInt(),
                         juce::Justification::centredRight, true);
        }
        else
        {
            // Where the files actually are. Not a section heading and not a
            // question - just the answer, because "where did my import go"
            // was unanswerable from this screen.
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            g.drawText (rootFolderLabel.getText(), row.toNearestInt(),
                         juce::Justification::centredLeft, true);
        }
    }

    paintRail (g);
    paintFilePane (g);

    // A hairline between the two panes, so the eye reads them as one screen
    // with two jobs rather than two lists that happen to be adjacent.
    {
        const auto rail = railBounds();
        g.setColour (theme.divider.withAlpha (0.6f));
        g.drawVerticalLine (rail.getRight() + AbcTrainTheme::Spacing::medium / 2,
                             (float) rail.getY(), (float) rail.getBottom());
    }

    // The footer says what is playing, which is the one fact this whole
    // screen exists to change.
    {
        auto footer = cardBounds().reduced (AbcTrainTheme::Spacing::large)
                          .removeFromBottom (footerHeight);
        footer.removeFromRight (110 + AbcTrainTheme::Spacing::small + 110);

        g.setColour (theme.text);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText (statusLabel.getText(), footer, juce::Justification::centredLeft, true);
    }
}

juce::Rectangle<int> TrainingSoundsComponent::cardBounds() const
{
    // Two panes need width; the old single column was 480 and could not
    // have held a filename beside a category name.
    return juce::Rectangle<int> (juce::jlimit (560, getWidth() - 80,
                                                juce::roundToInt ((float) getWidth() * 0.72f)),
                                  juce::jlimit (440, getHeight() - 80,
                                                juce::roundToInt ((float) getHeight() * 0.72f)))
               .withCentre (getLocalBounds().getCentre());
}

void TrainingSoundsComponent::mouseMove (const juce::MouseEvent& event)
{
    auto category = -2;
    auto file = -1;

    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    for (int i = -1; i < categories.size(); ++i)
        if (categoryRowBounds (i).contains (event.getPosition()))
            category = i;

    if (const auto* files = filesForSelection())
        for (int i = 0; i < files->size(); ++i)
            if (fileRowBounds (i).contains (event.getPosition())
                && filePaneBounds().contains (event.getPosition()))
                file = i;

    if (category != hoveredCategoryRow || file != hoveredFileRow)
    {
        hoveredCategoryRow = category;
        hoveredFileRow = file;
        repaint();
    }
}

void TrainingSoundsComponent::mouseExit (const juce::MouseEvent&)
{
    if (hoveredCategoryRow != -2 || hoveredFileRow != -1)
    {
        hoveredCategoryRow = -2;
        hoveredFileRow = -1;
        repaint();
    }
}

void TrainingSoundsComponent::mouseUp (const juce::MouseEvent& event)
{
    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    for (int i = -1; i < categories.size(); ++i)
    {
        if (! categoryRowBounds (i).contains (event.getPosition()))
            continue;

        if (i < 0)
            selectPinkNoise();
        else
            selectCategory (i);

        return;
    }

    if (const auto* files = filesForSelection())
        for (int i = 0; i < files->size(); ++i)
            if (fileRowBounds (i).contains (event.getPosition())
                && filePaneBounds().contains (event.getPosition()))
            {
                pinFile (i);
                return;
            }
}

void TrainingSoundsComponent::mouseWheelMove (const juce::MouseEvent& event,
                                               const juce::MouseWheelDetails& wheel)
{
    if (! filePaneBounds().contains (event.getPosition()))
        return;

    const auto* files = filesForSelection();
    const auto contentHeight = files != nullptr ? (float) (files->size() * rowHeight) : 0.0f;

    maxFileScroll = juce::jmax (0.0f, contentHeight - (float) (filePaneBounds().getHeight() - 18));
    fileScroll = juce::jlimit (0.0f, maxFileScroll, fileScroll - wheel.deltaY * 220.0f);
    repaint();
}

void TrainingSoundsComponent::resized()
{
    using namespace AbcTrainTheme;

    auto area = cardBounds().reduced (Spacing::large);
    area.removeFromTop (headerHeight + Spacing::large);

    {
        auto row = area.removeFromTop (actionRowHeight);
        chooseFolderButton.setBounds (row.removeFromRight (120).reduced (0, 6));
        row.removeFromRight (Spacing::small);
        revealButton.setBounds (row.removeFromRight (110).reduced (0, 6));
        row.removeFromRight (Spacing::small);
        importButton.setBounds (row);
    }

    rootFolderLabel.setVisible (false);
    statusLabel.setVisible (false);

    auto footer = cardBounds().reduced (Spacing::large).removeFromBottom (footerHeight);
    closeButton.setBounds (footer.removeFromRight (110));
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
        text.importHint = text.importedNothing;
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

    text.importHint = text.importedClips.replace ("{{count}}", juce::String (clipsWritten))
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
