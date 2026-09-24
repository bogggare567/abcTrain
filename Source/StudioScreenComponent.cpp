#include "StudioScreenComponent.h"
#include "shared/learning/LearnerEditorBase.h"

#include "shared/ui/AbcTrainLookAndFeel.h"

namespace
{
    // The names on real plugins, like the answer names in the trainer:
    // not translated.
    const char* const effectNames[] = { "EQ", "Comp", "Verb" };

    AbcTrainTheme::Family familyFor (int index)
    {
        switch (index)
        {
            case 0:  return AbcTrainTheme::Family::frequency;
            case 1:  return AbcTrainTheme::Family::dynamics;
            default: return AbcTrainTheme::Family::space;
        }
    }
}

StudioScreenComponent::StudioScreenComponent (Host hostToUse)
    : host (std::move (hostToUse))
{
    for (int i = 0; i < numEffects; ++i)
    {
        auto& button = switchButtons[(size_t) i];
        button.setButtonText (effectNames[i]);
        button.setComponentID (juce::String ("studio.") + effectNames[i]);
        button.onClick = [this, i] { select ((Effect) i); };
        addAndMakeVisible (button);
    }

    refreshSwitch();
}

StudioScreenComponent::~StudioScreenComponent()
{
    close();
}

void StudioScreenComponent::open()
{
    showEditorFor (selected);
}

void StudioScreenComponent::close()
{
    // Stop the sound first: the editor being destroyed does not stop its
    // processor, and a Studio left playing under the trainer's menus would
    // be the old "noise on the home screen" bug again.
    if (host.setActiveEffect != nullptr)
        host.setActiveEffect (-1);

    dropEditor();
}

void StudioScreenComponent::select (Effect effect)
{
    if (effect == selected && editor != nullptr)
        return;

    selected = effect;
    refreshSwitch();

    if (isOpen())
        showEditorFor (effect);
}

void StudioScreenComponent::showEditorFor (Effect effect)
{
    // The old editor goes before the new one exists: two editors of the
    // same kind alive at once would both register their displays with
    // processors, and the one being destroyed would unregister the other's.
    dropEditor();

    if (host.processorFor == nullptr)
        return;

    auto& processor = host.processorFor ((int) effect);
    editor.reset (processor.getActiveEditor() == nullptr ? processor.createEditorAndMakeActive() : nullptr);

    if (editor != nullptr)
    {
        // The plugin's editor is a window of its own in a DAW and brings a
        // resize corner; here it is a page, sized by the app.
        editor->setResizable (false, false);

        if (auto* learner = dynamic_cast<LearnerEditorBase*> (editor.get()))
        {
            learner->setEmbedded (true);
            learner->setHearingProvider (host.hearing);
        }

        addAndMakeVisible (*editor);
    }

    if (host.setActiveEffect != nullptr)
        host.setActiveEffect ((int) effect);

    resized();
    repaint();
}

void StudioScreenComponent::dropEditor()
{
    // Whoever deletes an editor has to tell its processor first - that is
    // the host's job, and here this screen is the host. Without it the
    // processor kept pointing at the dead editor, refused to make a new
    // one, and the second visit to a plugin was an empty page.
    if (editor != nullptr)
        editor->getAudioProcessor()->editorBeingDeleted (editor.get());

    editor.reset();
}

void StudioScreenComponent::refreshSwitch()
{
    for (int i = 0; i < numEffects; ++i)
    {
        auto& button = switchButtons[(size_t) i];
        const auto on = (int) selected == i;
        const auto colour = AbcTrainTheme::accentFor (familyFor (i));

        button.setToggleState (on, juce::dontSendNotification);
        // The chosen one filled in its family's colour - the same "you are
        // here" block as the tab above it, in the colour of the plugin.
        button.setColour (juce::TextButton::buttonOnColourId, colour);
        button.setColour (juce::TextButton::textColourOnId, AbcTrainTheme::current().windowBackground);
    }
}

void StudioScreenComponent::setLabels (juce::String studioCaption)
{
    caption = std::move (studioCaption);
    repaint();
}

void StudioScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    g.fillAll (theme.windowBackground);

    auto bar = getLocalBounds().removeFromTop (switchHeight);
    g.setColour (theme.divider);
    g.fillRect (bar.removeFromBottom (1));

    if (caption.isNotEmpty())
        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (caption),
                                              bar.withTrimmedLeft (20).withWidth (160).toFloat(),
                                              AbcTrainLookAndFeel::captionFont(), theme.textDim, 1.6f);
}

void StudioScreenComponent::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop (switchHeight).reduced (0, 9);

    // The switch sits where the caption ends, so the three names line up
    // with the tabs above rather than floating in the middle of the bar.
    bar.removeFromLeft (180);

    // Overlapping by a pixel so neighbours share one border and the three
    // read as one segmented control.
    auto x = bar.getX();

    for (auto& button : switchButtons)
    {
        button.setBounds (x, bar.getY(), 96, bar.getHeight());
        x += 95;
    }

    if (editor != nullptr)
        editor->setBounds (area);
}
