#include "TrainingSoundsComponent.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"

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
    // Not shown. This is a page reached from a tab in the bar above, and
    // the way out of it is another tab - a "Close" button in the corner of
    // a full page is a second exit for a door that is already open, and it
    // read as the leftover of the dialogue this used to be. Kept as a
    // child (rather than deleted) so the key handler and tests that reach
    // for it still have something to reach for.
    addChildComponent (closeButton);

    formats.registerBasicFormats();

    // "All clips" is rotation through the category, "This clip only" pins
    // the clip drawn large. Two segments of one switch, because they are
    // one question.
    allClipsButton.setClickingTogglesState (false);
    thisClipButton.setClickingTogglesState (false);
    allClipsButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    thisClipButton.setConnectedEdges (juce::Button::ConnectedOnLeft);

    allClipsButton.onClick = [this]
    {
        if (selectedCategory >= 0)
            selectCategory (selectedCategory);
    };

    thisClipButton.onClick = [this] { pinFile (focusedFile); };

    addChildComponent (allClipsButton);
    addChildComponent (thisClipButton);
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
    allClipsButton.setButtonText (text.allClips);
    thisClipButton.setButtonText (text.thisClip);
    closeButton.setButtonText (text.close);

    updateStatusLabel();
    resized();
    repaint();
}

juce::String TrainingSoundsComponent::displayNameForCategory (const juce::String& rawName) const
{
    if (rawName == "Built-in Percussive") return text.builtInPercussive;
    if (rawName == "Built-in Sustained")  return text.builtInSustained;

    // A pack names itself, in two languages.
    for (const auto& category : processor.getGameManager().getReferenceAudioLibrary().getCategories())
        if (category.isPack && category.name == rawName)
            return text.languageCode.startsWithIgnoreCase ("ru") ? category.titleRu : category.titleEn;

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
    focusedFile = -1;

    if (const auto* files = filesForSelection())
        focusedFile = juce::jmax (0, files->indexOf (library.getSelectedFile()));

    updateModeButtons();
    updateStatusLabel();
    resized();
    repaint();
}

const ReferenceAudioLibrary::Category* TrainingSoundsComponent::selectedCategoryInfo() const
{
    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    if (selectedCategory < 0 || selectedCategory >= categories.size())
        return nullptr;

    return &categories.getReference (selectedCategory);
}

const juce::Array<juce::File>* TrainingSoundsComponent::filesForSelection() const
{
    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    if (selectedCategory < 0 || selectedCategory >= categories.size())
        return nullptr;

    return &categories.getReference (selectedCategory).files;
}

void TrainingSoundsComponent::selectExerciseSound()
{
    processor.getGameManager().getReferenceAudioLibrary().clearSelection();
    processor.getGameManager().setPreferExerciseSound (true);
    selectedCategory = -1;
    focusedFile = -1;
    fileScroll = 0.0f;
    updateModeButtons();
    updateStatusLabel();
    repaint();
}

void TrainingSoundsComponent::selectPinkNoise()
{
    processor.getGameManager().getReferenceAudioLibrary().clearSelection();
    processor.getGameManager().setPreferExerciseSound (false);
    selectedCategory = -1;
    focusedFile = -1;
    fileScroll = 0.0f;
    updateModeButtons();
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

    updateModeButtons();
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

    const auto changed = selectedCategory != categoryIndex;
    selectedCategory = categoryIndex;

    if (changed)
    {
        fileScroll = 0.0f;
        focusedFile = categories.getReference (categoryIndex).files.isEmpty() ? -1 : 0;
    }

    updateModeButtons();
    updateStatusLabel();
    repaint();
}

void TrainingSoundsComponent::updateStatusLabel()
{
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto selected = library.getSelectedFile();

    if (! selected.existsAsFile())
    {
        statusLabel.setText (library.getPreferExerciseSound() ? text.trainingOnExerciseSound
                                                              : text.trainingOnPinkNoise,
                             juce::dontSendNotification);
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
    constexpr int railWidth = 220;
    constexpr int railRowHeight = 36;
    constexpr int titleHeight = 44;
    constexpr int previewHeight = 156;
    constexpr int fileRowHeight = 46;
    constexpr int fileRowGap = 6;
    constexpr int footerHeight = 30;
    constexpr int overviewBuckets = 480;

    juce::String formatTime (double seconds, bool fine)
    {
        seconds = juce::jmax (0.0, seconds);
        const auto minutes = (int) (seconds / 60.0);
        const auto rest = seconds - minutes * 60.0;

        if (fine)
            return juce::String (minutes) + ":" + (rest < 10.0 ? "0" : "") + juce::String (rest, 1);

        return juce::String (minutes) + ":" + juce::String ((int) rest).paddedLeft ('0', 2);
    }
}

juce::Rectangle<int> TrainingSoundsComponent::railBounds() const
{
    return cardBounds().withWidth (railWidth);
}

juce::Rectangle<int> TrainingSoundsComponent::filePaneBounds() const
{
    return cardBounds().withTrimmedLeft (railWidth).reduced (AbcTrainTheme::Spacing::large);
}

juce::Rectangle<int> TrainingSoundsComponent::categoryRowBounds (int index) const
{
    auto rail = railBounds().reduced (10, 0);
    rail.removeFromTop (48);   // the rail's own heading

    return { rail.getX(), rail.getY() + (index - exerciseRow) * railRowHeight, rail.getWidth(), railRowHeight };
}

juce::Rectangle<int> TrainingSoundsComponent::previewBounds() const
{
    auto pane = filePaneBounds();
    pane.removeFromTop (titleHeight);
    return pane.removeFromTop (previewHeight);
}

juce::Rectangle<int> TrainingSoundsComponent::previewPlayBounds() const
{
    return previewBounds().reduced (12).removeFromTop (34).removeFromLeft (34);
}

juce::Rectangle<int> TrainingSoundsComponent::previewWaveBounds() const
{
    auto area = previewBounds().reduced (12);
    area.removeFromTop (34 + 8);
    return area;
}

juce::Rectangle<int> TrainingSoundsComponent::listBounds() const
{
    auto pane = filePaneBounds();
    pane.removeFromTop (titleHeight + previewHeight + AbcTrainTheme::Spacing::medium);
    pane.removeFromBottom (footerHeight);
    return pane;
}

juce::Rectangle<int> TrainingSoundsComponent::fileRowBounds (int index) const
{
    const auto list = listBounds();
    return { list.getX(), list.getY() + (int) ((float) (index * (fileRowHeight + fileRowGap)) - fileScroll),
             list.getWidth(), fileRowHeight };
}

juce::Rectangle<int> TrainingSoundsComponent::rowPlayBounds (int index) const
{
    return fileRowBounds (index).reduced (10, 0).removeFromLeft (30).withSizeKeepingCentre (30, 30);
}

const ClipPreview::Overview& TrainingSoundsComponent::overviewFor (const juce::File& file)
{
    const auto key = file.getFullPathName();

    if (auto found = overviews.find (key); found != overviews.end())
        return found->second;

    return overviews[key] = ClipPreview::readOverview (formats, file, overviewBuckets);
}

void TrainingSoundsComponent::focusFile (int index)
{
    focusedFile = index;
    updateModeButtons();
    repaint();
}

void TrainingSoundsComponent::togglePlay (int index, float from)
{
    const auto* files = filesForSelection();

    if (files == nullptr || index < 0 || index >= files->size())
        return;

    auto& preview = processor.getClipPreview();

    if (playingFile == index && preview.isPlaying() && from <= 0.0f)
    {
        stopPreview();
        return;
    }

    juce::AudioBuffer<float> clip;

    if (! ClipPreview::readClip (formats, (*files)[index], preview.getSampleRate(), clip))
        return;

    preview.play (std::move (clip), from);
    playingFile = index;
    focusFile (index);
    startTimerHz (30);
}

void TrainingSoundsComponent::stopPreview()
{
    processor.getClipPreview().stop();
    playingFile = -1;
    stopTimer();
    repaint();
}

void TrainingSoundsComponent::timerCallback()
{
    if (! processor.getClipPreview().isPlaying())
    {
        stopPreview();
        return;
    }

    repaint (previewBounds());
    repaint (listBounds());
}

void TrainingSoundsComponent::visibilityChanged()
{
    // Leaving the page silences it: a preview carrying on under the
    // exercise list is a sound with no visible source.
    if (! isVisible())
        stopPreview();
}

void TrainingSoundsComponent::updateModeButtons()
{
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto* files = filesForSelection();
    const auto hasFocus = files != nullptr && juce::isPositiveAndBelow (focusedFile, files->size());
    const auto pinnedHere = hasFocus && library.isPinned() && library.getSelectedFile() == (*files)[focusedFile];

    allClipsButton.setVisible (files != nullptr);
    thisClipButton.setVisible (files != nullptr);
    thisClipButton.setEnabled (hasFocus);

    allClipsButton.setToggleState (files != nullptr && ! pinnedHere
                                       && library.getSelectedFile().existsAsFile() && ! library.isPinned(),
                                   juce::dontSendNotification);
    thisClipButton.setToggleState (pinnedHere, juce::dontSendNotification);

    const auto& theme = AbcTrainTheme::current();

    for (auto* b : { &allClipsButton, &thisClipButton })
    {
        b->setColour (juce::TextButton::buttonOnColourId, theme.accent);
        b->setColour (juce::TextButton::textColourOnId, theme.windowBackground);
    }
}

void TrainingSoundsComponent::paintRail (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto& categories = library.getCategories();

    const auto rail = railBounds();
    g.setColour (theme.displayBackground.withAlpha (0.35f));
    g.fillRect (rail);
    g.setColour (theme.divider);
    g.drawVerticalLine (rail.getRight() - 1, (float) rail.getY(), (float) rail.getBottom());

    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (text.trainOnSection),
                                           rail.reduced (22, 0).withTrimmedTop (14).withHeight (20).toFloat(),
                                           AbcTrainLookAndFeel::microFont(), theme.textDim, 1.4f);

    const auto drawRow = [&] (int index, const juce::String& name, const juce::String& detail, bool selected)
    {
        const auto row = categoryRowBounds (index);

        if (index == hoveredCategoryRow || selected)
        {
            g.setColour (selected ? theme.accent.withAlpha (0.2f) : theme.widgetBackground.withAlpha (0.5f));
            g.fillRect (row);
        }

        auto label = row.reduced (12, 0);

        g.setColour (selected ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        AbcTrainLookAndFeel::fitText (g, name, label.removeFromLeft (label.getWidth() - 30), juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
        AbcTrainLookAndFeel::fitText (g, detail, label, juce::Justification::centredRight, false);
    };

    const auto nothingSelected = ! library.getSelectedFile().existsAsFile();
    drawRow (exerciseRow, text.exerciseSound, {}, selectedCategory < 0 && nothingSelected && library.getPreferExerciseSound());
    drawRow (pinkNoiseRow, text.pinkNoise, {}, selectedCategory < 0 && nothingSelected && ! library.getPreferExerciseSound());

    for (int i = 0; i < categories.size(); ++i)
        drawRow (i, displayNameForCategory (categories.getReference (i).name),
                 juce::String (categories.getReference (i).files.size()), i == selectedCategory);

    // Import progress, or what the last import did, above the buttons.
    auto bottom = rail.reduced (10, 0).withTrimmedBottom (10 + 34 + 6 + 28 + 4 + 28 + 8);
    auto status = bottom.removeFromBottom (34);

    if (importRunning)
    {
        paintImportProgress (g, status.removeFromTop (6));
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        AbcTrainLookAndFeel::fitText (g, importProgressFile, status, juce::Justification::centredLeft, true);
    }
    else if (text.importHint.isNotEmpty() && text.importHint != previousHint)
    {
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        AbcTrainLookAndFeel::fitLines (g, text.importHint, status, juce::Justification::bottomLeft, 2, 0.9f);
    }
}

void TrainingSoundsComponent::paintWave (juce::Graphics& g, juce::Rectangle<float> area,
                                          const std::vector<float>& peaks, juce::Colour colour,
                                          float playedFraction, juce::Colour playedColour)
{
    if (peaks.empty() || area.getWidth() < 2.0f)
        return;

    const auto columns = juce::jmax (1, (int) area.getWidth());
    const auto centre = area.getCentreY();
    const auto half = area.getHeight() * 0.5f;

    juce::Path played, rest;

    for (int x = 0; x < columns; ++x)
    {
        const auto from = (size_t) ((double) x / columns * (double) peaks.size());
        const auto to = juce::jmax (from + 1, (size_t) ((double) (x + 1) / columns * (double) peaks.size()));

        auto peak = 0.0f;
        for (auto i = from; i < juce::jmin (to, peaks.size()); ++i)
            peak = juce::jmax (peak, peaks[i]);

        const auto h = juce::jmax (0.5f, peak * half);
        const auto bar = juce::Rectangle<float> (area.getX() + (float) x, centre - h, 1.0f, h * 2.0f);

        ((float) x / (float) columns < playedFraction ? played : rest).addRectangle (bar);
    }

    g.setColour (colour);
    g.fillPath (rest);
    g.setColour (playedColour);
    g.fillPath (played);
}

void TrainingSoundsComponent::paintPreview (juce::Graphics& g, juce::Rectangle<int> box)
{
    const auto& theme = AbcTrainTheme::current();
    const auto* files = filesForSelection();

    g.setColour (theme.displayBackground);
    g.fillRect (box);
    g.setColour (theme.outline);
    g.drawRect (box, 1);

    if (files == nullptr || ! juce::isPositiveAndBelow (focusedFile, files->size()))
        return;

    const auto& file = (*files)[focusedFile];
    const auto& overview = overviewFor (file);
    auto& preview = processor.getClipPreview();
    const auto playing = playingFile == focusedFile && preview.isPlaying();
    const auto progress = playing ? preview.getProgress() : 0.0f;

    // Play / stop, a filled square in the accent colour like the mock-up.
    {
        const auto button = previewPlayBounds().toFloat();
        g.setColour (theme.accent);
        g.fillRect (button);
        g.setColour (theme.windowBackground);

        if (playing)
        {
            g.fillRect (button.withSizeKeepingCentre (10.0f, 10.0f));
        }
        else
        {
            juce::Path triangle;
            const auto c = button.getCentre();
            triangle.addTriangle (c.x - 4.0f, c.y - 6.0f, c.x - 4.0f, c.y + 6.0f, c.x + 6.0f, c.y);
            g.fillPath (triangle);
        }
    }

    auto header = box.reduced (12).removeFromTop (34);
    header.removeFromLeft (34 + 12);

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::headingFont());
    const auto name = file.getFileNameWithoutExtension();
    const auto nameWidth = juce::jmin (header.getWidth() / 2,
                                       (int) juce::GlyphArrangement::getStringWidth (AbcTrainLookAndFeel::headingFont(), name) + 4);
    AbcTrainLookAndFeel::fitText (g, name, header.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);
    header.removeFromLeft (10);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
    AbcTrainLookAndFeel::fitText (g, formatTime (progress * overview.seconds, true) + " / " + formatTime (overview.seconds, true),
                header, juce::Justification::centredLeft, false);

    const auto wave = previewWaveBounds().toFloat();
    paintWave (g, wave, overview.peaks, theme.accent.withAlpha (0.75f), progress, theme.accent);

    if (playing)
    {
        g.setColour (theme.textBright);
        g.fillRect (juce::Rectangle<float> (wave.getX() + wave.getWidth() * progress - 1.0f, wave.getY(),
                                            2.0f, wave.getHeight()));
    }
}

void TrainingSoundsComponent::paintFilePane (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto* files = filesForSelection();
    const auto pane = filePaneBounds();

    // Title: what is selected, and in small type how this page works.
    {
        auto row = pane.withHeight (titleHeight - 8);
        juce::String title, caption;

        if (files != nullptr)
        {
            title = displayNameForCategory (selectedCategoryInfo()->name);
            caption = text.clipsCaption.replace ("{{n}}", juce::String (files->size()));
        }
        else
        {
            title = library.getPreferExerciseSound() ? text.exerciseSound : text.pinkNoise;
        }

        const auto font = AbcTrainLookAndFeel::displayFont().withHeight (26.0f);
        const auto width = (int) juce::GlyphArrangement::getStringWidth (font, title) + 14;

        g.setColour (theme.textBright);
        g.setFont (font);
        AbcTrainLookAndFeel::fitText (g, title, row.removeFromLeft (juce::jmin (width, row.getWidth() / 2)),
                    juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        AbcTrainLookAndFeel::fitText (g, caption, row, juce::Justification::centredLeft, true);
    }

    if (files == nullptr)
    {
        // Two different empty states: nothing picked yet, or nothing there.
        auto body = pane.withTrimmedTop (titleHeight);
        g.setColour (theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitLines (g, statusLabel.getText(), body.removeFromTop (48), juce::Justification::topLeft, 3);

        body.removeFromTop (AbcTrainTheme::Spacing::medium);
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        AbcTrainLookAndFeel::fitLines (g, library.getCategories().isEmpty() ? text.empty : text.pickCategory,
                          body.removeFromTop (40), juce::Justification::topLeft, 3);
        return;
    }

    paintPreview (g, previewBounds());

    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (listBounds());

        auto& preview = processor.getClipPreview();
        const auto pinnedFile = library.isPinned() ? library.getSelectedFile() : juce::File();

        for (int i = 0; i < files->size(); ++i)
        {
            const auto row = fileRowBounds (i);

            if (! row.intersects (listBounds()))
                continue;

            const auto focused = (i == focusedFile);
            const auto pinned = pinnedFile == (*files)[i];

            g.setColour (focused ? theme.accent.withAlpha (0.12f)
                                 : theme.panelBackground.withAlpha (i == hoveredFileRow ? 0.9f : 0.6f));
            g.fillRect (row);
            g.setColour (focused ? theme.accent.withAlpha (0.8f) : theme.outline.withAlpha (0.7f));
            g.drawRect (row, 1);

            auto inner = row.reduced (10, 0);
            const auto playBox = rowPlayBounds (i).toFloat();
            inner.removeFromLeft (30 + 12);

            const auto playing = playingFile == i && preview.isPlaying();
            g.setColour (theme.outline);
            g.drawRect (playBox, 1.0f);
            g.setColour (theme.text);

            if (playing)
            {
                g.fillRect (playBox.withSizeKeepingCentre (8.0f, 8.0f));
            }
            else
            {
                juce::Path triangle;
                const auto c = playBox.getCentre();
                triangle.addTriangle (c.x - 3.0f, c.y - 4.5f, c.x - 3.0f, c.y + 4.5f, c.x + 4.5f, c.y);
                g.fillPath (triangle);
            }

            const auto& overview = overviewFor ((*files)[i]);

            // Name, then the small waveform, then length and credit.
            auto nameBox = inner.removeFromLeft (juce::jmin (150, inner.getWidth() / 4));
            g.setColour (focused || pinned ? theme.textBright : theme.text);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            AbcTrainLookAndFeel::fitText (g, (*files)[i].getFileNameWithoutExtension(), nameBox, juce::Justification::centredLeft, true);

            auto creditBox = inner.removeFromRight (juce::jmin (120, inner.getWidth() / 5));
            auto timeBox = inner.removeFromRight (52);
            inner.removeFromRight (10);

            paintWave (g, inner.reduced (0, 12).toFloat(), overview.peaks,
                       (focused ? theme.accent : theme.textDim).withAlpha (focused ? 0.85f : 0.45f),
                       playing ? preview.getProgress() : 0.0f, theme.accent);

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
            AbcTrainLookAndFeel::fitText (g, formatTime (overview.seconds, false), timeBox, juce::Justification::centred, false);

            if (const auto* category = selectedCategoryInfo(); category != nullptr && i < category->clips.size())
            {
                const auto& credit = category->clips.getReference (i).credit;

                if (credit.author.isNotEmpty() || credit.license.isNotEmpty())
                {
                    g.setFont (AbcTrainLookAndFeel::captionFont().withHeight (11.5f));
                    AbcTrainLookAndFeel::fitLines (g, credit.license + (credit.author.isNotEmpty() ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7\n")) + credit.author : juce::String()),
                                      creditBox, juce::Justification::centredLeft, 2, 0.9f);
                }
            }

            if (pinned)
            {
                g.setColour (theme.positive);
                g.fillRect (row.withWidth (3));
            }
        }
    }

    // The footer says what will play in a round - the one fact this whole
    // page exists to change.
    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    AbcTrainLookAndFeel::fitText (g, statusLabel.getText(), pane.withTop (pane.getBottom() - footerHeight + 8),
                juce::Justification::centredLeft, true);
}

void TrainingSoundsComponent::paint (juce::Graphics& g)
{
    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    paintRail (g);
    paintFilePane (g);
}

juce::Rectangle<int> TrainingSoundsComponent::cardBounds() const
{
    // A page, not a card: it fills everything under the bar above.
    return getLocalBounds();
}

void TrainingSoundsComponent::mouseMove (const juce::MouseEvent& event)
{
    auto category = noRow;
    auto file = -1;

    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    for (int i = exerciseRow; i < categories.size(); ++i)
        if (categoryRowBounds (i).contains (event.getPosition()))
            category = i;

    if (const auto* files = filesForSelection())
        for (int i = 0; i < files->size(); ++i)
            if (fileRowBounds (i).contains (event.getPosition()) && listBounds().contains (event.getPosition()))
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
    if (hoveredCategoryRow != noRow || hoveredFileRow != -1)
    {
        hoveredCategoryRow = noRow;
        hoveredFileRow = -1;
        repaint();
    }
}

void TrainingSoundsComponent::mouseUp (const juce::MouseEvent& event)
{
    const auto position = event.getPosition();
    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    for (int i = exerciseRow; i < categories.size(); ++i)
    {
        if (! categoryRowBounds (i).contains (position))
            continue;

        stopPreview();

        if (i == exerciseRow)
            selectExerciseSound();
        else if (i == pinkNoiseRow)
            selectPinkNoise();
        else
            selectCategory (i);

        return;
    }

    const auto* files = filesForSelection();

    if (files == nullptr)
        return;

    if (previewPlayBounds().contains (position))
    {
        togglePlay (focusedFile);
        return;
    }

    // Clicking the big waveform plays from there - the scrub every
    // sample browser has.
    if (previewWaveBounds().contains (position) && juce::isPositiveAndBelow (focusedFile, files->size()))
    {
        const auto wave = previewWaveBounds();
        togglePlay (focusedFile, juce::jmax (0.0001f, (float) (position.x - wave.getX()) / (float) wave.getWidth()));
        return;
    }

    if (! listBounds().contains (position))
        return;

    // "Click to hear": a click anywhere on a row focuses it and plays it;
    // the play button on the row toggles.
    for (int i = 0; i < files->size(); ++i)
        if (fileRowBounds (i).contains (position))
        {
            if (rowPlayBounds (i).contains (position) || i != playingFile)
                togglePlay (i);

            focusFile (i);
            return;
        }
}

void TrainingSoundsComponent::mouseWheelMove (const juce::MouseEvent& event,
                                               const juce::MouseWheelDetails& wheel)
{
    if (! listBounds().contains (event.getPosition()))
        return;

    const auto* files = filesForSelection();
    const auto contentHeight = files != nullptr ? (float) (files->size() * (fileRowHeight + fileRowGap)) : 0.0f;

    maxFileScroll = juce::jmax (0.0f, contentHeight - (float) listBounds().getHeight());
    fileScroll = juce::jlimit (0.0f, maxFileScroll, fileScroll - wheel.deltaY * 220.0f);
    repaint();
}

void TrainingSoundsComponent::resized()
{
    using namespace AbcTrainTheme;

    auto rail = railBounds().reduced (10);
    importButton.setBounds (rail.removeFromBottom (34));
    rail.removeFromBottom (6);

    chooseFolderButton.setBounds (rail.removeFromBottom (28));
    rail.removeFromBottom (4);
    revealButton.setBounds (rail.removeFromBottom (28));

    auto header = previewBounds().reduced (12).removeFromTop (34);
    thisClipButton.setBounds (header.removeFromRight (130));
    allClipsButton.setBounds (header.removeFromRight (100));

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

        const auto onProgress = [safeOwner] (float progress, juce::String fileName)
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
        };

        auto& library = owner.processor.getGameManager().getReferenceAudioLibrary();
        const auto shouldStop = [this] { return threadShouldExit(); };

        const auto written = library.importAndSliceMany (files, onProgress, shouldStop);

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
    chooseFilesToImport();
}

void TrainingSoundsComponent::chooseFilesToImport()
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
            parts.add (juce::String (category.files.size()) + " " + displayNameForCategory (category.name).toLowerCase());

    text.importHint = text.importedClips.replace ("{{count}}", juce::String (clipsWritten))
                   + (parts.isEmpty() ? juce::String() : "  -  " + parts.joinIntoString (", "));

    refresh();
}

void TrainingSoundsComponent::paintImportProgress (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& theme = AbcTrainTheme::current();
    const auto bounds = area.toFloat();

    g.setColour (theme.displayBackground);
    g.fillRect (bounds);

    const auto progress = juce::jlimit (0.0f, 1.0f, importProgress.load());

    if (progress > 0.001f)
    {
        g.setColour (theme.accent);
        g.fillRect (bounds.withWidth (juce::jmax (bounds.getHeight(),
                                                  bounds.getWidth() * progress)));
    }

    g.setColour (theme.outline.withAlpha (0.6f));
    g.drawRect (bounds, 1.0f);
}
