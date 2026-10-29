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

    ownButton.setButtonText ("Your plugin");
    ownButton.setComponentID ("studio.own");
    ownButton.onClick = [this] { selectOwnPlugin(); };
    if (host.makeOwnPluginPanel != nullptr)
        addAndMakeVisible (ownButton);

    lessonsButton.setComponentID ("studio.lessons");
    lessonsButton.onClick = [this] { if (onLessons) onLessons(); };
    addAndMakeVisible (lessonsButton);

    refreshSwitch();
}

StudioScreenComponent::~StudioScreenComponent()
{
    close();
}

void StudioScreenComponent::open()
{
    if (ownSelected)
        showOwnPanel();
    else
        showEditorFor (selected);
}

void StudioScreenComponent::selectOwnPlugin()
{
    if (host.makeOwnPluginPanel == nullptr)
        return;

    ownSelected = true;
    refreshSwitch();
    showOwnPanel();
}

void StudioScreenComponent::showOwnPanel()
{
    dropEditor();

    if (host.setActiveEffect != nullptr)
        host.setActiveEffect (-1);

    if (ownPanel == nullptr)
        ownPanel = host.makeOwnPluginPanel();

    if (ownPanel != nullptr)
        addAndMakeVisible (*ownPanel);

    if (host.setOwnPluginActive != nullptr)
        host.setOwnPluginActive (true);

    resized();
    repaint();
}

void StudioScreenComponent::dropOwnPanel()
{
    if (host.setOwnPluginActive != nullptr)
        host.setOwnPluginActive (false);

    // Destroying the page drops both plugin instances: a plugin left loaded
    // under the trainer's menus would still hold its licence, its memory and
    // the CPU.
    ownPanel.reset();
}

void StudioScreenComponent::close()
{
    // Stop the sound first: the editor being destroyed does not stop its
    // processor, and a Studio left playing under the trainer's menus would
    // be the old "noise on the home screen" bug again.
    if (host.setActiveEffect != nullptr)
        host.setActiveEffect (-1);

    dropEditor();
    dropOwnPanel();
}

void StudioScreenComponent::select (Effect effect)
{
    if (effect == selected && editor != nullptr && ! ownSelected)
        return;

    // Coming back from «Your plugin»: the page was open, only with the own
    // panel in it instead of an editor - so isOpen() said no, and EQ / Comp
    // / Verb stayed empty until the whole Studio was reopened (the author,
    // 2026-10-24: "после своего плагина не открывается студия").
    const auto wasOpen = isOpen() || ownSelected;

    if (ownSelected)
    {
        ownSelected = false;
        dropOwnPanel();
    }

    selected = effect;
    refreshSwitch();

    if (wasOpen)
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
        const auto on = (int) selected == i && ! ownSelected;
        const auto colour = AbcTrainTheme::accentFor (familyFor (i));

        button.setToggleState (on, juce::dontSendNotification);
        // The chosen one filled in its family's colour - the same "you are
        // here" block as the tab above it, in the colour of the plugin.
        button.setColour (juce::TextButton::buttonOnColourId, colour);
        button.setColour (juce::TextButton::textColourOnId, AbcTrainTheme::current().windowBackground);
    }

    ownButton.setToggleState (ownSelected, juce::dontSendNotification);
    ownButton.setColour (juce::TextButton::buttonOnColourId, AbcTrainTheme::current().accent);
    ownButton.setColour (juce::TextButton::textColourOnId, AbcTrainTheme::current().windowBackground);
}

void StudioScreenComponent::setRotatePromptAllowed (bool allowed)
{
    rotatePromptAllowed = allowed;
    resized();
    repaint();
}

bool StudioScreenComponent::showsRotatePrompt() const
{
    return rotatePromptAllowed && getWidth() < 600 && getHeight() > getWidth();
}

void StudioScreenComponent::setLabels (juce::String studioCaption, juce::String lessonsLabel,
                                       juce::String rotate)
{
    caption = std::move (studioCaption);
    rotateMessage = std::move (rotate);
    lessonsButton.setButtonText (lessonsLabel.isNotEmpty() ? lessonsLabel : juce::String ("Lessons"));
    resized();
    repaint();
}

void StudioScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    g.fillAll (theme.windowBackground);

    if (showsRotatePrompt())
    {
        // A phone outline turning a quarter: the same picture games use,
        // because it is the same request.
        auto centre = getLocalBounds().toFloat().withSizeKeepingCentre (getWidth() - 48.0f, 220.0f);
        auto icon = centre.removeFromTop (120.0f).withSizeKeepingCentre (110.0f, 110.0f);

        g.setColour (theme.textDim.withAlpha (0.35f));
        g.drawRoundedRectangle (icon.withSizeKeepingCentre (44.0f, 76.0f), 7.0f, 2.0f);
        g.setColour (theme.accent);
        g.drawRoundedRectangle (icon.withSizeKeepingCentre (76.0f, 44.0f), 7.0f, 2.0f);

        juce::Path arc;
        arc.addCentredArc (icon.getCentreX(), icon.getCentreY(), 50.0f, 50.0f, 0.0f,
                           -0.35f * juce::MathConstants<float>::pi, -0.05f * juce::MathConstants<float>::pi, true);
        g.strokePath (arc, juce::PathStrokeType (2.0f));

        g.setColour (theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitLines (g, rotateMessage.isNotEmpty() ? rotateMessage
                                                                     : juce::String ("Turn the phone sideways: the Studio needs the width."),
                                       centre.toNearestInt(), juce::Justification::centredTop, 3, 1.0f);
        return;
    }

    if (sideSwitch())
    {
        auto rail = getLocalBounds().removeFromLeft (sideSwitchWidth);
        g.setColour (theme.divider);
        g.fillRect (rail.removeFromRight (1));
        return;
    }

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
    const auto prompt = showsRotatePrompt();

    for (auto& button : switchButtons)
        button.setVisible (! prompt);

    lessonsButton.setVisible (! prompt);

    if (editor != nullptr)
        editor->setVisible (! prompt);

    if (prompt)
        return;

    if (sideSwitch())
    {
        // Three effects and the lessons, one under the other.
        auto rail = area.removeFromLeft (sideSwitchWidth).reduced (8, 10);

        for (auto& button : switchButtons)
        {
            button.setBounds (rail.removeFromTop (44));
            rail.removeFromTop (6);
        }

        if (host.makeOwnPluginPanel != nullptr)
            ownButton.setBounds (rail.removeFromTop (44));

        lessonsButton.setBounds (rail.removeFromBottom (44));
        placeEditor (area);
        return;
    }

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

    // Your own plugin: after the three, a step apart - it is not one of
    // ours, and it is a different kind of page.
    if (host.makeOwnPluginPanel != nullptr)
        ownButton.setBounds (x + 12, bar.getY(), 150, bar.getHeight());

    // The way to the courses, at the far end of the same bar: the lessons
    // turn these plugins' knobs, so they open from where the plugins are.
    lessonsButton.setBounds (bar.removeFromRight (150).withTrimmedRight (20));

    placeEditor (area);
}

void StudioScreenComponent::placeEditor (juce::Rectangle<int> area)
{
    if (ownPanel != nullptr)
        ownPanel->setBounds (area);

    if (editor == nullptr)
        return;

    // The plugins are laid out for 820x600 and up. Below that they are
    // given that much room and drawn smaller, rather than squeezed into a
    // layout they were never built for.
    constexpr float minWidth = 820.0f, minHeight = 600.0f;
    const auto scale = juce::jmin (1.0f, (float) area.getWidth() / minWidth, (float) area.getHeight() / minHeight);

    if (scale >= 1.0f)
    {
        editor->setTransform ({});
        editor->setBounds (area);
        return;
    }

    editor->setBounds (0, 0, juce::roundToInt ((float) area.getWidth() / scale),
                       juce::roundToInt ((float) area.getHeight() / scale));
    editor->setTransform (juce::AffineTransform::scale (scale)
                              .translated ((float) area.getX(), (float) area.getY()));
}
