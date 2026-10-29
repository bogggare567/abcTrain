#pragma once

#include "shared/learning/CompanionPanel.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include "shared/ui/AbcTrainTheme.h"

#include <array>
#include <functional>
#include <memory>

// The Studio tab: Learner EQ, Comp and Verb inside the abcTrain app
// (ADR 041).
//
// The processors belong to EarTrainerProcessor, which runs whichever one
// this screen says is active; this component only owns the editor of the
// one on show. Editors are made when the tab opens and dropped when it
// closes - three analysers repainting at 30 Hz behind a page nobody is
// looking at would be the most expensive thing the app does.
//
// The editor is the plugin's own, unchanged, so the Studio and the
// plugin in a DAW cannot drift apart: one editor, two places to open it.
class StudioScreenComponent : public juce::Component
{
public:
    enum class Effect { eq = 0, comp = 1, verb = 2 };
    static constexpr int numEffects = 3;

    struct Host
    {
        // Which processor the app should run; -1 for none.
        std::function<void (int)> setActiveEffect;

        // The processor whose editor the screen shows.
        std::function<juce::AudioProcessor& (int)> processorFor;

        // What the Learner's companion window shows as "Hearing today".
        std::function<CompanionHearing()> hearing;

        // «Свой плагин» (ADR 065): the page for the player's own installed
        // plugins. Absent (nullptr) where the app cannot host plugins - iOS.
        std::function<std::unique_ptr<juce::Component>()> makeOwnPluginPanel;
        std::function<void (bool)> setOwnPluginActive;
    };

    explicit StudioScreenComponent (Host);
    ~StudioScreenComponent() override;

    // Opens the page on `effect` and starts it playing; close() stops the
    // sound and drops the editor. The editor calls these as the tab comes
    // and goes, rather than relying on visibility, so a page that is
    // merely covered by an overlay keeps playing.
    void open();
    void close();
    bool isOpen() const noexcept { return editor != nullptr; }

    void select (Effect);
    Effect getSelected() const noexcept { return selected; }

    void setLabels (juce::String studioCaption, juce::String lessonsLabel = {},
                    juce::String rotateMessage = {});

    // On a phone held upright the plugins do not fit: the page asks for
    // the phone to be turned instead of showing them at a third of their
    // size (ADR 060). Off on a desktop, where a narrow window just scales.
    void setOwnPluginLabel (const juce::String& label) { ownButton.setButtonText (label); }
    void selectOwnPlugin();
    juce::Component* getOwnPluginPanel() const noexcept { return ownPanel.get(); }
    void setRotatePromptAllowed (bool allowed);
    bool showsRotatePrompt() const;

    // The course lessons window (ADR 052), opened from the right end of
    // the switch bar.
    std::function<void()> onLessons;

    // The editor on show, if any - the lessons window lights a frequency
    // band on Learner EQ's spectrum through it.
    juce::AudioProcessorEditor* getEditor() const noexcept { return editor.get(); }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int switchHeight = 52;

private:
    void showEditorFor (Effect);
    void placeEditor (juce::Rectangle<int>);
    void dropEditor();
    void refreshSwitch();

    Host host;
    Effect selected = Effect::eq;
    juce::String caption, rotateMessage;
    bool rotatePromptAllowed = false;

    // A landscape phone is short: the switch goes down the left edge as a
    // column and the plugin is scaled to the rest.
    bool sideSwitch() const { return getHeight() < 500 && getWidth() > getHeight(); }
    static constexpr int sideSwitchWidth = 112;

    std::array<juce::TextButton, numEffects> switchButtons;
    juce::TextButton ownButton;
    bool ownSelected = false;
    std::unique_ptr<juce::Component> ownPanel;
    void showOwnPanel();
    void dropOwnPanel();
    juce::TextButton lessonsButton;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioScreenComponent)
};
