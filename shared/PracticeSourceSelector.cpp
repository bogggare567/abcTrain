#include "PracticeSourceSelector.h"

namespace
{
    // The indicator has room for a word, not a category name. "Built-in
    // Percussive" becomes "Percussive" - the "Built-in" prefix is what
    // every built-in category has in common, so it is the part that
    // distinguishes nothing.
    juce::String shortLabelFor (const juce::String& categoryName)
    {
        auto trimmed = categoryName.trim();

        if (trimmed.startsWithIgnoreCase ("Built-in "))
            trimmed = trimmed.substring (9);

        return trimmed.length() > 11 ? trimmed.substring (0, 10) + juce::String::charToString (0x2026)
                                     : trimmed;
    }
}

PracticeSourceSelector::PracticeSourceSelector (ReferenceAudioLibrary& libraryToUse,
                                                PracticeAudioSource& sourceToDrive,
                                                juce::PropertiesFile& propertiesFile,
                                                std::function<double()> sampleRateProvider)
    : library (libraryToUse), source (sourceToDrive), properties (propertiesFile),
      getSampleRate (std::move (sampleRateProvider))
{
    selector.setCaption ("source");
    selector.onChange = [this] { applySelection(); };
    addAndMakeVisible (selector);

    refresh();
}

void PracticeSourceSelector::refresh()
{
    library.rescan();

    selector.clearItems();
    selector.addItem ("Host audio", 1, "host");

    const auto& categories = library.getCategories();

    for (int i = 0; i < categories.size(); ++i)
        selector.addItem (categories[i].name + " (" + juce::String (categories[i].files.size()) + ")",
                          i + 2, shortLabelFor (categories[i].name));

    // Restore by *name*, not by index: the list is rebuilt from whatever
    // folders exist, so an index saved last week can point at a different
    // category this week - or at nothing.
    const auto saved = properties.getValue (selectedCategoryKey);
    auto restoredId = 1;

    if (saved.isNotEmpty())
        for (int i = 0; i < categories.size(); ++i)
            if (categories[i].name == saved)
                restoredId = i + 2;

    selector.setSelectedId (restoredId, juce::dontSendNotification);
    applySelection();
}

void PracticeSourceSelector::applySelection()
{
    const auto id = selector.getSelectedId();
    const auto& categories = library.getCategories();
    const auto index = id - 2;

    if (index < 0 || index >= categories.size())
    {
        source.setEnabled (false);
        properties.setValue (selectedCategoryKey, juce::String());
        properties.saveIfNeeded();
        return;
    }

    const auto sampleRate = getSampleRate != nullptr ? getSampleRate() : 0.0;

    // A host that has not called prepareToPlay yet reports 0, and loading
    // at that rate would resample the clip into nothing. Enable anyway and
    // let prepareToPlay's own load pick it up: the alternative is a control
    // that silently does nothing when pressed before playback starts.
    library.setActiveCategory (categories[index].name, sampleRate > 0.0 ? sampleRate : 44100.0);
    source.setEnabled (true);

    properties.setValue (selectedCategoryKey, categories[index].name);
    properties.saveIfNeeded();
}

void PracticeSourceSelector::resized()
{
    selector.setBounds (getLocalBounds());
}
