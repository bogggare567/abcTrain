#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"
#include "ABCompare.h"
#include "AppIcons.h"
#include "GuideTooltip.h"
#include "ModuleProgress.h"
#include "ModuleScreenComponent.h"
#include "PracticeSourceSelector.h"
#include "UpdateWindow.h"
#include "WindowFit.h"
#include "i18n/LocalisationManager.h"
#include <functional>
#include <memory>

// Everything the three Learner plugins have in common, once (ADR 037).
//
// The Comp and Verb editors were ~95% the same file - title row, theme,
// language, update flow, bypass veil, module panel, lesson overlays, site
// link - and every fix had to be made three times, which is how LearnerEQ
// ended up with a module panel that could not check anything and Comp and
// Verb with a violet one nobody chose. A subclass now supplies only what
// is actually its own: the analysis section (spectrum, waveform, meters),
// the controls under it, and its modules.
//
// Layout, top to bottom: the title row (icon, name, family, practice
// source, A/B, bypass, update, theme, modules), the analysis section which takes
// whatever height is left, the control section at a fixed height, and the
// site link. The module panel lives over the analysis section and never
// over the controls - you answer a check with the plugin's own knob.
class LearnerEditorBase : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    // What the base needs from a processor, without knowing its type.
    struct Services
    {
        juce::AudioProcessorValueTreeState& apvts;
        juce::String bypassParamId;
        juce::PropertiesFile& libraryProperties;     // practice source and module progress
        ReferenceAudioLibrary& practiceLibrary;
        PracticeAudioSource& practiceSource;
        std::function<void (const juce::String&, float)> setCheckOverride;
        std::function<void()> clearCheckOverride;
    };

    struct Identity
    {
        juce::String title;                // "ABC Learner Comp"
        AbcTrainTheme::Family family;
        AppIcons::Icon icon;
        juce::String controlsCaptionKey;   // i18n key for the control section
    };

    LearnerEditorBase (juce::AudioProcessor&, Services, Identity);
    ~LearnerEditorBase() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // For tools/EditorSnapshots.
    void openModuleShelfForSnapshot()                     { moduleScreen.openShelf(); moduleScreen.completeAnimation(); }
    void openModuleCheckForSnapshot (int index)           { moduleScreen.openCheckForSnapshot (index); }
    void openModuleResultForSnapshot (int index, bool ok) { moduleScreen.openResultForSnapshot (index, ok); }

protected:
    // Called by the subclass at the end of its constructor, once its own
    // components exist: modules, then the overlays are added last so they
    // paint over everything, then the window size.
    void finishSetup (std::vector<TrainingModule::Definition> modules,
                      int width, int height);

    // Short windows (a laptop, ADR 038): the controls take their compact
    // metrics - smaller knobs, tighter rows - and the analysis section may
    // shrink below its preferred height. Subclasses read this in
    // controlsContentHeight() and layoutControls().
    bool isCompact() const noexcept { return getHeight() < designSize.y - 40; }
    int knobRowHeight() const noexcept { return isCompact() ? 104 : 132; }
    int presetRowHeight() const noexcept { return isCompact() ? 28 : 32; }
    int rowGap() const noexcept { return isCompact() ? 6 : 8; }

    // Heights the subclass needs; the analysis section gets at least its
    // own and everything left over.
    virtual int analysisContentHeight() const = 0;
    virtual int controlsContentHeight() const = 0;

    // Inner areas, already inset for the section caption.
    virtual void layoutAnalysis (juce::Rectangle<int>) = 0;
    virtual void layoutControls (juce::Rectangle<int>) = 0;

    // The palette changed (or the editor just opened): push the accent
    // into whatever draws itself.
    virtual void themeChanged() {}

    // 30 Hz, for meters and readouts.
    virtual void tick() {}

    // i18n with an English fallback, for the per-plugin text (knob names,
    // guide sentences, preset names, module content).
    juce::String t (const juce::String& key, const juce::String& fallback) const;

    // "," or "." as this language writes a decimal.
    juce::String decimalPoint() const;

    // A number the way this language writes it, with a unit key's text.
    juce::String formatNumber (double value, int decimals) const
    {
        return juce::String (value, decimals).replace (".", decimalPoint());
    }

    // "In  -12,3 dB": a peak readout in this language's decimals and units.
    juce::String peakText (const char* key, const char* fallback, float gain) const
    {
        return t (key, fallback) + "  " + formatNumber (juce::Decibels::gainToDecibels (gain, -60.0f), 1)
             + " " + t ("unit.dB", "dB");
    }

    void showGuide (const juce::String& text, int autoDismissMs = 0) { guideTooltip.setText (text, autoDismissMs); }

    // Declared first so it outlives every child (see AbcTrainLookAndFeel).
    AbcTrainLookAndFeel lookAndFeel;

    // The product-wide settings file - theme, language, text size - the
    // same one the trainer writes. The plugins used to read the language
    // from the practice-library file, where it never was.
    juce::PropertiesFile settingsFile;
    LocalisationManager localisation { settingsFile };

    Services services;
    Identity identity;
    juce::Colour accent;

    juce::Rectangle<int> analysisSection, controlSection;

    GuideTooltip guideTooltip;
    ModuleProgress moduleProgress;
    ModuleScreenComponent moduleScreen;

private:
    void timerCallback() override;
    void applyTheme();
    void toggleTheme();
    void checkForUpdates();
    ModuleScreenComponent::Strings moduleStrings() const;

    AppIconComponent pluginIcon;
    juce::TextButton bypassButton;
    juce::TextButton slotA { "A" }, slotB { "B" };
    ABCompare abCompare;
    void refreshSlots();
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    IconButton modulesButton { AppIcons::Icon::modules };
    IconButton updateButton { AppIcons::Icon::download };
    IconButton themeButton { AppIcons::Icon::sun };
    PracticeSourceSelector practiceSelector;
    juce::HyperlinkButton soundkorbLink { "soundkorb.ru", juce::URL ("https://soundkorb.ru") };
    UpdateWindow updateWindow;

    float bypassVeil = 0.0f;
    juce::Point<int> designSize { 900, 830 };
    int familyLimit = 10000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEditorBase)
};
