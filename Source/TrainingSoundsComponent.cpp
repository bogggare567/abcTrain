#include "TrainingSoundsComponent.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/audio/InstrumentLabel.h"

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

    // A loop of your own choosing, from any track on disk: pick the file,
    // drag across its waveform, save.
    fragmentButton.onClick = [this] { chooseTrackForFragment(); };
    addAndMakeVisible (fragmentButton);

    saveSelectionButton.onClick = [this] { saveSelection(); };
    cancelSelectionButton.onClick = [this] { clearSelection(); };
    addChildComponent (saveSelectionButton);
    addChildComponent (cancelSelectionButton);

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
    fragmentButton.setButtonText (text.fragmentFromTrack);
    saveSelectionButton.setButtonText (text.saveFragment);
    cancelSelectionButton.setButtonText (text.cancelSelection);
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

    const auto instrumentName = [this] (InstrumentLabel::Instrument instrument)
    {
        const auto index = (int) instrument;
        return juce::isPositiveAndBelow (index, text.instrumentNames.size())
                   ? text.instrumentNames[index]
                   : juce::String (InstrumentLabel::folderNameFor (instrument));
    };

    // A pack names itself, in two languages - or, split by instrument, each
    // part is named by its instrument (the pack's title is the heading
    // above it).
    for (const auto& category : processor.getGameManager().getReferenceAudioLibrary().getCategories())
        if (category.isPack && category.name == rawName)
        {
            if (category.packPart.isNotEmpty())
            {
                if (const auto instrument = InstrumentLabel::fromId (category.packPart))
                    return instrumentName (*instrument);

                return category.packPart;
            }

            return text.languageCode.startsWithIgnoreCase ("ru") ? category.titleRu : category.titleEn;
        }

    // A folder the importer made: its instrument, in the player's language.
    if (const auto instrument = InstrumentLabel::fromFolderName (rawName))
        return instrumentName (*instrument);

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
    pendingDelete = -1;
    rebuildRail();

    if (sourceTrack.existsAsFile())
        selectedCategory = -1;   // track mode: nothing in the rail is "this"
    else
        sourceTrack = juce::File();

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
    if (sourceTrack != juce::File())
        return &trackFiles;

    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();

    if (selectedCategory < 0 || selectedCategory >= categories.size())
        return nullptr;

    return &categories.getReference (selectedCategory).files;
}

void TrainingSoundsComponent::selectExerciseSound()
{
    sourceTrack = juce::File();
    selectionStart = selectionEnd = -1.0f;
    pendingDelete = -1;
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
    sourceTrack = juce::File();
    selectionStart = selectionEnd = -1.0f;
    pendingDelete = -1;
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

    const auto leavingTrack = sourceTrack != juce::File();
    sourceTrack = juce::File();
    pendingDelete = -1;

    // Selecting the category is selecting *rotation* - the library swaps in
    // a different clip each round. Clicking a file in the right-hand pane
    // pins one instead; clicking the category again goes back to shuffling.
    library.setActiveCategory (categories.getReference (categoryIndex).name,
                                processor.getSampleRate());

    const auto changed = selectedCategory != categoryIndex || leavingTrack;
    selectedCategory = categoryIndex;

    if (changed)
    {
        selectionStart = selectionEnd = -1.0f;
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
    constexpr int railHeadingHeight = 26;

    // The rail's buttons, bottom up: import, fragment, choose folder,
    // reveal - and the import status line above them.
    constexpr int railButtonsHeight = 10 + 34 + 6 + 28 + 4 + 28 + 4 + 28;
    constexpr int railStatusHeight = 34 + 8;

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

juce::Rectangle<int> TrainingSoundsComponent::railListBounds() const
{
    auto rail = railBounds().reduced (10, 0);
    rail.removeFromTop (48);   // the rail's own heading
    rail.removeFromBottom (railButtonsHeight + railStatusHeight);
    return rail;
}

juce::Rectangle<int> TrainingSoundsComponent::categoryRowBounds (int index) const
{
    const auto list = railListBounds();
    auto y = (float) list.getY() - railScroll;

    for (const auto& entry : railEntries)
    {
        const auto height = entry.category == noRow ? railHeadingHeight : railRowHeight;

        if (entry.category == index && entry.category != noRow)
            return { list.getX(), (int) y, list.getWidth(), height };

        y += (float) height;
    }

    return {};
}

void TrainingSoundsComponent::rebuildRail()
{
    // Exercise sound, pink noise, the built-ins; then the player's own
    // folders under one heading; then each pack under its own title, split
    // by instrument. Headings are what make forty rows readable.
    railEntries.clear();
    railEntries.push_back ({ exerciseRow, {} });
    railEntries.push_back ({ pinkNoiseRow, {} });

    const auto& categories = processor.getGameManager().getReferenceAudioLibrary().getCategories();
    const auto ru = text.languageCode.startsWithIgnoreCase ("ru");

    for (int i = 0; i < categories.size(); ++i)
        if (categories.getReference (i).name.startsWith ("Built-in"))
            railEntries.push_back ({ i, {} });

    auto headed = false;
    for (int i = 0; i < categories.size(); ++i)
    {
        const auto& c = categories.getReference (i);
        if (c.isPack || c.name.startsWith ("Built-in"))
            continue;

        if (! headed)
            railEntries.push_back ({ noRow, text.mySounds });
        headed = true;
        railEntries.push_back ({ i, {} });
    }

    juce::String lastPack;
    for (int i = 0; i < categories.size(); ++i)
    {
        const auto& c = categories.getReference (i);
        if (! c.isPack)
            continue;

        const auto packFolder = c.name.upToFirstOccurrenceOf ("/", false, false);

        if (c.packPart.isNotEmpty() && packFolder != lastPack)
            railEntries.push_back ({ noRow, ru ? c.titleRu : c.titleEn });

        lastPack = packFolder;
        railEntries.push_back ({ i, {} });
    }

    // Keep the scroll inside what is there now.
    auto content = 0;
    for (const auto& e : railEntries)
        content += e.category == noRow ? railHeadingHeight : railRowHeight;

    railScroll = juce::jlimit (0.0f, juce::jmax (0.0f, (float) (content - railListBounds().getHeight())), railScroll);
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
    area.removeFromBottom (16);   // the "drag to select" line
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

juce::Rectangle<int> TrainingSoundsComponent::rowDeleteBounds (int index) const
{
    return fileRowBounds (index).reduced (8, 0).removeFromRight (28).withSizeKeepingCentre (28, 28);
}

juce::Rectangle<int> TrainingSoundsComponent::confirmNoBounds (int index) const
{
    return fileRowBounds (index).reduced (8, 8).removeFromRight (90);
}

juce::Rectangle<int> TrainingSoundsComponent::confirmYesBounds (int index) const
{
    auto row = fileRowBounds (index).reduced (8, 8);
    row.removeFromRight (90 + 8);
    return row.removeFromRight (110);
}

const ClipPreview::Overview& TrainingSoundsComponent::overviewFor (const juce::File& file)
{
    const auto key = file.getFullPathName();

    if (auto found = overviews.find (key); found != overviews.end())
        return found->second;

    return overviews[key] = ClipPreview::readOverview (formats, file, overviewBuckets);
}

const ClipPreview::Overview& TrainingSoundsComponent::longOverviewFor (const juce::File& file)
{
    const auto key = "long:" + file.getFullPathName();

    if (auto found = overviews.find (key); found != overviews.end())
        return found->second;

    // A whole track, streamed a bucket at a time with readMaxLevels - not
    // decoded into memory whole the way a clip's overview is (a
    // five-minute stereo file would be a hundred megabytes of floats).
    ClipPreview::Overview overview;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader != nullptr && reader->sampleRate > 0.0 && reader->lengthInSamples > 0)
    {
        constexpr int buckets = 1200;
        overview.seconds = (double) reader->lengthInSamples / reader->sampleRate;
        overview.peaks.assign (buckets, 0.0f);
        auto loudest = 0.0f;
        const auto channels = (int) juce::jmin (2u, reader->numChannels);

        for (int b = 0; b < buckets; ++b)
        {
            const auto start = reader->lengthInSamples * b / buckets;
            const auto end = reader->lengthInSamples * (b + 1) / buckets;
            juce::Range<float> levels[2];
            reader->readMaxLevels (start, juce::jmax ((juce::int64) 1, end - start), levels, channels);

            auto peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax (peak, levels[ch].getEnd(), -levels[ch].getStart());

            overview.peaks[(size_t) b] = peak;
            loudest = juce::jmax (loudest, peak);
        }

        if (loudest > 0.0f)
            for (auto& p : overview.peaks)
                p /= loudest;
    }

    return overviews[key] = std::move (overview);
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
    const auto& file = (*files)[index];

    // A stretch of a long file - the selection, or twenty seconds from
    // where the waveform was clicked - rather than its first twenty seconds.
    const auto longFile = sourceTrack != juce::File();

    if (longFile || (hasSelection() && index == focusedFile))
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
            return;

        const auto seconds = (double) reader->lengthInSamples / reader->sampleRate;
        const auto startFraction = hasSelection() && from <= 0.0f ? selectionStart : juce::jmax (0.0f, from);
        const auto endFraction = hasSelection() && from <= 0.0f
                                     ? selectionEnd
                                     : (float) juce::jmin (1.0, startFraction + 20.0 / juce::jmax (0.001, seconds));
        const auto start = (juce::int64) ((double) startFraction * (double) reader->lengthInSamples);
        const auto length = (int) juce::jmax ((juce::int64) 1, (juce::int64) ((double) (endFraction - startFraction) * (double) reader->lengthInSamples));

        juce::AudioBuffer<float> source ((int) reader->numChannels, length);
        reader->read (&source, 0, length, start, true, true);

        juce::AudioBuffer<float> mono (1, length);
        mono.clear();
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            mono.addFrom (0, 0, source, ch, 0, length, 1.0f / (float) source.getNumChannels());

        const auto target = preview.getSampleRate();
        if (target > 0.0 && ! juce::approximatelyEqual (target, reader->sampleRate))
        {
            const auto ratio = reader->sampleRate / target;
            clip.setSize (1, juce::jmax (1, (int) ((double) length / ratio)));
            juce::LagrangeInterpolator interpolator;
            interpolator.process (ratio, mono.getReadPointer (0), clip.getWritePointer (0), clip.getNumSamples());
        }
        else
        {
            clip = std::move (mono);
        }

        playOffset = startFraction;
        playSpan = endFraction - startFraction;
        preview.play (std::move (clip), 0.0f);
    }
    else
    {
        if (! ClipPreview::readClip (formats, file, preview.getSampleRate(), clip))
            return;

        playOffset = 0.0f;
        playSpan = 1.0f;
        preview.play (std::move (clip), from);
    }

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

    const auto rotationQuestion = files != nullptr && ! hasSelection() && sourceTrack == juce::File();
    allClipsButton.setVisible (rotationQuestion);
    thisClipButton.setVisible (rotationQuestion);
    thisClipButton.setEnabled (hasFocus);

    saveSelectionButton.setVisible (files != nullptr && hasSelection());
    cancelSelectionButton.setVisible (files != nullptr && hasSelection());
    saveSelectionButton.setColour (juce::TextButton::buttonColourId, AbcTrainTheme::current().accent);
    saveSelectionButton.setColour (juce::TextButton::textColourOffId, AbcTrainTheme::current().windowBackground);

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

    const auto nothingSelected = ! library.getSelectedFile().existsAsFile() && sourceTrack == juce::File();

    {
        juce::Graphics::ScopedSaveState clipToList (g);
        g.reduceClipRegion (railListBounds());

        auto y = (float) railListBounds().getY() - railScroll;

        for (const auto& entry : railEntries)
        {
            if (entry.category == noRow)
            {
                const auto heading = juce::Rectangle<float> ((float) railListBounds().getX() + 12.0f, y + 8.0f,
                                                             (float) railListBounds().getWidth() - 12.0f, 16.0f);
                AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (entry.heading), heading,
                                                       AbcTrainLookAndFeel::microFont(), theme.textDim, 1.2f);
                y += (float) railHeadingHeight;
                continue;
            }

            if (entry.category == exerciseRow)
                drawRow (exerciseRow, text.exerciseSound, {}, selectedCategory < 0 && nothingSelected && library.getPreferExerciseSound());
            else if (entry.category == pinkNoiseRow)
                drawRow (pinkNoiseRow, text.pinkNoise, {}, selectedCategory < 0 && nothingSelected && ! library.getPreferExerciseSound());
            else if (juce::isPositiveAndBelow (entry.category, categories.size()))
                drawRow (entry.category, displayNameForCategory (categories.getReference (entry.category).name),
                         juce::String (categories.getReference (entry.category).files.size()), entry.category == selectedCategory);

            y += (float) railRowHeight;
        }

        // More below or above than fits: fade the edge, so the list reads
        // as one that scrolls rather than one that simply ends.
        const auto list = railListBounds().toFloat();
        const auto background = theme.windowBackground;

        if (y > list.getBottom() + 1.0f)
        {
            const auto band = list.withTop (list.getBottom() - 36.0f);
            g.setGradientFill (juce::ColourGradient (background.withAlpha (0.0f), 0.0f, band.getY(),
                                                     background, 0.0f, band.getBottom(), false));
            g.fillRect (band);
        }

        if (railScroll > 0.5f)
        {
            const auto band = list.withHeight (28.0f);
            g.setGradientFill (juce::ColourGradient (background, 0.0f, band.getY(),
                                                     background.withAlpha (0.0f), 0.0f, band.getBottom(), false));
            g.fillRect (band);
        }
    }

    // Import progress, or what the last import did, above the buttons.
    auto bottom = rail.reduced (10, 0).withTrimmedBottom (railButtonsHeight + 8);
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
    const auto& overview = sourceTrack != juce::File() ? longOverviewFor (file) : overviewFor (file);
    auto& preview = processor.getClipPreview();
    const auto playing = playingFile == focusedFile && preview.isPlaying();
    const auto progress = playing ? playOffset + playSpan * preview.getProgress() : 0.0f;

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

    // The selection behind the waveform, with its edges drawn and its
    // length written above it: what Save will cut, before any snapping.
    if (hasSelection())
    {
        const auto x0 = wave.getX() + wave.getWidth() * selectionStart;
        const auto x1 = wave.getX() + wave.getWidth() * selectionEnd;
        const auto area = juce::Rectangle<float> (x0, wave.getY(), x1 - x0, wave.getHeight());

        g.setColour (theme.accent.withAlpha (0.16f));
        g.fillRect (area);
        g.setColour (theme.accent);
        g.fillRect (area.withWidth (2.0f));
        g.fillRect (area.withX (x1 - 2.0f).withWidth (2.0f));
    }

    paintWave (g, wave, overview.peaks, theme.accent.withAlpha (hasSelection() ? 0.45f : 0.75f), progress, theme.accent);

    if (playing)
    {
        g.setColour (theme.textBright);
        g.fillRect (juce::Rectangle<float> (wave.getX() + wave.getWidth() * progress - 1.0f, wave.getY(),
                                            2.0f, wave.getHeight()));
    }

    // One line under the waveform: how to cut your own loop, or what is
    // selected.
    {
        auto hintLine = previewBounds().reduced (12).removeFromBottom (14);
        juce::String hint = text.selectHint;

        if (hasSelection())
            hint = formatTime (selectionStart * overview.seconds, true) + " - "
                 + formatTime (selectionEnd * overview.seconds, true) + "  ("
                 + juce::String ((selectionEnd - selectionStart) * overview.seconds, 1) + " s)";

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont().withHeight (11.5f));
        AbcTrainLookAndFeel::fitText (g, hint, hintLine, juce::Justification::centredRight, true);
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

        if (sourceTrack != juce::File())
        {
            title = sourceTrack.getFileNameWithoutExtension();
            caption = text.trackHint;
        }
        else if (files != nullptr)
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

            // Delete: a small cross at the row's end, on the row under the
            // mouse or the focused one, for files the library may delete.
            const auto deletable = library.canDelete ((*files)[i]) && sourceTrack == juce::File();
            inner.removeFromRight (deletable ? 34 : 0);

            if (deletable && (i == hoveredFileRow || focused) && pendingDelete != i)
            {
                const auto cross = rowDeleteBounds (i).toFloat();
                g.setColour (theme.textDim);
                g.drawRect (cross, 1.0f);
                const auto c = cross.reduced (9.0f);
                g.drawLine (c.getX(), c.getY(), c.getRight(), c.getBottom(), 1.5f);
                g.drawLine (c.getRight(), c.getY(), c.getX(), c.getBottom(), 1.5f);
            }

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

            // "Delete?" takes over the row until answered: two buttons, the
            // destructive one in the warning colour, nothing else clickable.
            if (pendingDelete == i)
            {
                g.setColour (theme.panelBackground);
                g.fillRect (row.reduced (1));
                g.setColour (theme.negative);
                g.drawRect (row, 1);

                auto question = row.reduced (14, 0);
                question.removeFromRight (confirmYesBounds (i).getWidth() + confirmNoBounds (i).getWidth() + 20);
                g.setColour (theme.textBright);
                g.setFont (AbcTrainLookAndFeel::labelFont());
                AbcTrainLookAndFeel::fitText (g, text.deleteConfirm.replace ("{{file}}", (*files)[i].getFileNameWithoutExtension()),
                                              question, juce::Justification::centredLeft, true);

                const auto yes = confirmYesBounds (i).toFloat();
                g.setColour (theme.negative);
                g.fillRect (yes);
                g.setColour (theme.windowBackground);
                g.setFont (AbcTrainLookAndFeel::labelFont());
                AbcTrainLookAndFeel::fitText (g, text.deleteYes, yes.toNearestInt(), juce::Justification::centred, false);

                const auto no = confirmNoBounds (i).toFloat();
                g.setColour (theme.outline);
                g.drawRect (no, 1.0f);
                g.setColour (theme.text);
                AbcTrainLookAndFeel::fitText (g, text.deleteNo, no.toNearestInt(), juce::Justification::centred, false);
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

    if (railListBounds().contains (event.getPosition()))
        for (const auto& entry : railEntries)
            if (entry.category != noRow && categoryRowBounds (entry.category).contains (event.getPosition()))
                category = entry.category;

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

void TrainingSoundsComponent::mouseDown (const juce::MouseEvent& event)
{
    selecting = false;
    dragStartX = event.getPosition().x;
}

void TrainingSoundsComponent::mouseDrag (const juce::MouseEvent& event)
{
    // Dragging across the big waveform selects; a click without a drag is
    // still the scrub it always was.
    const auto* files = filesForSelection();
    const auto wave = previewWaveBounds();

    if (files == nullptr || ! juce::isPositiveAndBelow (focusedFile, files->size())
        || ! wave.contains (event.getMouseDownPosition()))
        return;

    if (! selecting && std::abs (event.getPosition().x - dragStartX) < 5)
        return;

    selecting = true;
    pendingDelete = -1;

    const auto toFraction = [&wave] (int x)
    {
        return juce::jlimit (0.0f, 1.0f, (float) (x - wave.getX()) / (float) juce::jmax (1, wave.getWidth()));
    };

    const auto a = toFraction (dragStartX), b = toFraction (event.getPosition().x);
    selectionStart = juce::jmin (a, b);
    selectionEnd = juce::jmax (a, b);

    updateModeButtons();
    repaint (previewBounds());
}

void TrainingSoundsComponent::mouseUp (const juce::MouseEvent& event)
{
    const auto position = event.getPosition();

    if (selecting)
    {
        selecting = false;

        if (selectionEnd - selectionStart < 0.002f)
            clearSelection();

        return;
    }

    for (const auto& entry : railEntries)
    {
        const auto i = entry.category;

        if (i == noRow || ! railListBounds().contains (position) || ! categoryRowBounds (i).contains (position))
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

    // A row asking "delete?" answers first, and swallows anything else.
    if (pendingDelete >= 0)
    {
        const auto row = pendingDelete;
        pendingDelete = -1;

        if (confirmYesBounds (row).contains (position))
            deleteFile (row);
        else
            repaint();

        return;
    }

    if (listBounds().contains (position))
        for (int i = 0; i < files->size(); ++i)
            if ((i == hoveredFileRow || i == focusedFile) && rowDeleteBounds (i).contains (position)
                && processor.getGameManager().getReferenceAudioLibrary().canDelete ((*files)[i])
                && sourceTrack == juce::File())
            {
                stopPreview();
                pendingDelete = i;
                repaint();
                return;
            }

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
            if (i != focusedFile)
                selectionStart = selectionEnd = -1.0f;   // a selection belongs to one clip

            if (rowPlayBounds (i).contains (position) || i != playingFile)
                togglePlay (i);

            focusFile (i);
            return;
        }
}

void TrainingSoundsComponent::mouseWheelMove (const juce::MouseEvent& event,
                                               const juce::MouseWheelDetails& wheel)
{
    if (railListBounds().contains (event.getPosition()))
    {
        auto content = 0;
        for (const auto& e : railEntries)
            content += e.category == noRow ? railHeadingHeight : railRowHeight;

        railScroll = juce::jlimit (0.0f, juce::jmax (0.0f, (float) (content - railListBounds().getHeight())),
                                   railScroll - wheel.deltaY * 220.0f);
        repaint (railBounds());
        return;
    }

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

    fragmentButton.setBounds (rail.removeFromBottom (28));
    rail.removeFromBottom (4);

    chooseFolderButton.setBounds (rail.removeFromBottom (28));
    rail.removeFromBottom (4);
    revealButton.setBounds (rail.removeFromBottom (28));

    auto header = previewBounds().reduced (12).removeFromTop (34);
    auto selectionHeader = header;
    thisClipButton.setBounds (header.removeFromRight (130));
    allClipsButton.setBounds (header.removeFromRight (100));

    // Same place as the rotation switch: with a selection on the waveform,
    // what to do with it is the one question the header asks.
    saveSelectionButton.setBounds (selectionHeader.removeFromRight (170));
    selectionHeader.removeFromRight (6);
    cancelSelectionButton.setBounds (selectionHeader.removeFromRight (90));

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
        "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg;*.zip");

    juce::Component::SafePointer<TrainingSoundsComponent> safeThis (this);

    // Tracks are sliced; a pack - a .zip, or a folder with a pack.json - is
    // installed as it is (ReferenceAudioLibrary::importAndSliceMany).
    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::canSelectDirectories
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

// ---- your own fragment, deleting ---------------------------------------------

void TrainingSoundsComponent::chooseTrackForFragment()
{
    if (importRunning)
        return;

    fileChooser = std::make_unique<juce::FileChooser> (
        fragmentButton.getButtonText(),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg");

    juce::Component::SafePointer<TrainingSoundsComponent> safeThis (this);

    fileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis != nullptr && chooser.getResult().existsAsFile())
                safeThis->openTrack (chooser.getResult());
        });
}

void TrainingSoundsComponent::openTrack (const juce::File& file)
{
    stopPreview();
    sourceTrack = file;
    trackFiles.clearQuick();
    trackFiles.add (file);
    selectedCategory = -1;
    focusedFile = 0;
    fileScroll = 0.0f;
    pendingDelete = -1;
    selectionStart = selectionEnd = -1.0f;
    updateModeButtons();
    resized();
    repaint();
}

void TrainingSoundsComponent::clearSelection()
{
    selectionStart = selectionEnd = -1.0f;
    updateModeButtons();
    repaint();
}

void TrainingSoundsComponent::saveSelection()
{
    const auto* files = filesForSelection();

    if (files == nullptr || ! juce::isPositiveAndBelow (focusedFile, files->size()) || ! hasSelection())
        return;

    stopPreview();

    const auto source = (*files)[focusedFile];
    const auto& overview = sourceTrack != juce::File() ? longOverviewFor (source) : overviewFor (source);
    auto& library = processor.getGameManager().getReferenceAudioLibrary();

    const auto fragment = library.saveFragment (source, selectionStart * overview.seconds,
                                                selectionEnd * overview.seconds);

    if (! fragment.file.existsAsFile())
    {
        text.importHint = text.fragmentTooShort;
        repaint();
        return;
    }

    // Say what the snapping did - "you asked for 7.8 s, you got 4 bars" -
    // so the result is not a surprise the first time it is played.
    juce::String what;
    if (fragment.onBeatGrid && fragment.bars > 0)
        what = text.fragmentOnGrid.replace ("{{bars}}", juce::String (fragment.bars))
                                  .replace ("{{bpm}}", juce::String (fragment.bpm, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."));
    else if (fragment.onBeatGrid)
        what = text.fragmentOnBeats.replace ("{{beats}}", juce::String (fragment.beats))
                                   .replace ("{{bpm}}", juce::String (fragment.bpm, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."));
    else
        what = text.fragmentFree.replace ("{{seconds}}", juce::String (fragment.seconds, 1));

    text.importHint = text.fragmentSaved.replace ("{{what}}", what)
                                        .replace ("{{folder}}", displayNameForCategory (fragment.folderName));

    selectionStart = selectionEnd = -1.0f;

    // Stay where you were: in a track, to cut the next piece; in a
    // category, with the new clip in the list.
    const auto keepTrack = sourceTrack;
    const auto keepCategory = selectedCategory >= 0 ? selectedCategoryInfo()->name : juce::String();

    library.rescan();
    rebuildRail();

    if (keepTrack != juce::File())
    {
        openTrack (keepTrack);
    }
    else
    {
        const auto& categories = library.getCategories();
        for (int i = 0; i < categories.size(); ++i)
            if (categories.getReference (i).name == keepCategory)
            {
                selectedCategory = i;
                focusedFile = categories.getReference (i).files.indexOf (fragment.file);
            }
    }

    updateModeButtons();
    repaint();
}

void TrainingSoundsComponent::deleteFile (int index)
{
    const auto* files = filesForSelection();

    if (files == nullptr || ! juce::isPositiveAndBelow (index, files->size()))
        return;

    stopPreview();

    auto& library = processor.getGameManager().getReferenceAudioLibrary();
    const auto file = (*files)[index];
    const auto keepCategory = selectedCategory >= 0 ? selectedCategoryInfo()->name : juce::String();

    if (! library.deleteClip (file))
        return;

    text.importHint = text.deleted.replace ("{{file}}", file.getFileNameWithoutExtension());
    overviews.erase (file.getFullPathName());

    library.rescan();
    rebuildRail();

    // The same category if it still exists, the row that took this one's
    // place focused; otherwise nothing selected.
    selectedCategory = -1;
    focusedFile = -1;
    selectionStart = selectionEnd = -1.0f;

    const auto& categories = library.getCategories();
    for (int i = 0; i < categories.size(); ++i)
        if (categories.getReference (i).name == keepCategory)
        {
            selectedCategory = i;
            focusedFile = juce::jmin (index, categories.getReference (i).files.size() - 1);
        }

    updateModeButtons();
    updateStatusLabel();
    repaint();
}
