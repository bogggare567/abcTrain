#include "PluginEditor.h"
#include "ReverbGuide.h"
#include "VocalSpaceLesson.h"
#include "BrightVsDarkTailLesson.h"
#include "../../shared/Version.h"
#include "../../shared/i18n/LocalisationManager.h"
#include <array>
#include <memory>

namespace
{
    struct KnobSpec
    {
        const char* paramId;
        const char* label;
    };

    const std::array<KnobSpec, 6> knobSpecs {{
        { "decay",    "Decay" },
        { "preDelay", "Pre-Delay" },
        { "size",     "Size" },
        { "damping",  "Damping" },
        { "dryWet",   "Dry/Wet" },
        { "width",    "Width" }
    }};

    constexpr const char* themeModeKey = "themeMode";
}

LearnerVerbEditor::LearnerVerbEditor (LearnerVerbProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      lessonController (p.apvts, buildVocalSpaceLesson()),
      tailLessonController (p.apvts, buildBrightVsDarkTailLesson()),
      // Same shared "abcTrain" settings folder the language preference
      // uses, so light/dark is one product-wide choice.
      themeProperties (LocalisationManager::makeDefaultOptions())
{
    AbcTrainTheme::setMode (themeProperties.getValue (themeModeKey, "dark") == "light"
                                ? AbcTrainTheme::Mode::light
                                : AbcTrainTheme::Mode::dark);
    lookAndFeel.refreshFromTheme();

    setLookAndFeel (&lookAndFeel);

    // Drawn by paint() with letter-spacing rather than via the Label.
    titleLabel.setText ("Learner Verb", juce::dontSendNotification);
    titleLabel.setVisible (false);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

    pluginIcon.setIcon (AppIcons::Icon::learnerVerb);
    addAndMakeVisible (pluginIcon);


    addAndMakeVisible (spectrum);

    addAndMakeVisible (waveform);

    inputPeakLabel.setJustificationType (juce::Justification::centred);
    inputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (inputPeakLabel);

    outputPeakLabel.setJustificationType (juce::Justification::centred);
    outputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (outputPeakLabel);

    typeSelector.addItem ("Room", 1);
    typeSelector.addItem ("Hall", 2);
    typeSelector.addItem ("Plate", 3);
    typeSelector.addItem ("Spring", 4);
    addAndMakeVisible (typeSelector);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, LearnerVerbProcessor::typeParamId, typeSelector);

    typeSelector.onChange = [this]
    {
        guideTooltip.setText (ReverbGuide::describe ("type"));
    };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        const auto& spec = knobSpecs[i];

        knob.nameLabel.setText (spec.label, juce::dontSendNotification);
        knob.nameLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (knob.nameLabel);

        addAndMakeVisible (knob.slider);

        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, spec.paramId, knob.slider);

        const juce::String paramId (spec.paramId);
        knob.slider.onDragStart = [this, paramId]
        {
            guideTooltip.setText (ReverbGuide::describe (paramId));
        };
        knob.slider.onDragEnd = [this]
        {
            guideTooltip.setText ({});
        };
    }

    for (int i = 0; i < (int) ReverbGuide::presets.size(); ++i)
    {
        auto* button = presetButtons.add (new juce::TextButton (ReverbGuide::presets[(size_t) i].name));
        button->onClick = [this, i] { processor.applyPreset (i); };
        addAndMakeVisible (button);
    }

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, LearnerVerbProcessor::bypassParamId, bypassButton);
    addAndMakeVisible (bypassButton);

    processor.setWaveformDisplay (&waveform);
    processor.setSpectrumAnalyzer (&spectrum);

    lessonSelector.addItem ("Space for Vocals", 1);
    lessonSelector.addItem ("Bright vs. Dark Tail", 2);
    lessonSelector.onChange = [this]
    {
        // Two lessons, one picker (see decisions/017) - only the selected
        // one is ever shown/started; the other stays hidden.
        if (lessonSelector.getSelectedId() == 1)
            lessonController.showAndStart();
        else if (lessonSelector.getSelectedId() == 2)
            tailLessonController.showAndStart();
    };
    addAndMakeVisible (lessonSelector);

    updateButton.onClick = [this]
    {
        // See CLAUDE.md's Update-checking section: without this,
        // clicking "Updates" gave no visible reaction at all whenever no
        // newer release was found (or this repo simply had no releases
        // yet), since checkForUpdatesAsync's callback deliberately never
        // fires on failure. Now every click gets some visible outcome:
        // "Checking...", then the update prompt, a brief "Up to date",
        // or - if nothing came back within a few seconds - "Couldn't
        // check".
        juce::Component::SafePointer<LearnerVerbEditor> safeThis (this);
        auto handled = std::make_shared<bool> (false);

        updateButton.setEnabled (false);
        updateButton.setButtonText ("Checking...");

        UpdateChecker::checkForUpdatesAsync (CurrentVersion::string, [safeThis, handled] (bool foundNewer, UpdateChecker::ReleaseInfo release)
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);

            if (! foundNewer)
            {
                safeThis->updateButton.setButtonText ("Up to date");
                juce::Timer::callAfterDelay (2500, [safeThis]
                {
                    if (safeThis != nullptr)
                        safeThis->updateButton.setButtonText ("Updates");
                });
                return;
            }

            safeThis->updateButton.setButtonText ("Updates");

            const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
                juce::MessageBoxIconType::InfoIcon,
                "Update Available",
                "Version " + release.tagName + " is available - you're on " + juce::String (CurrentVersion::string) + ".",
                "Open Release Page", "Later",
                safeThis.getComponent());

            juce::AlertWindow::showAsync (options, [release] (int result)
            {
                // makeOptionsOkCancel adds two buttons; per AlertWindow's
                // documented N-button result mapping, button[0] ("Open
                // Release Page") returns 1, button[1] ("Later") returns 0.
                if (result == 1)
                    juce::URL (release.htmlUrl).launchInDefaultBrowser();
            });
        });

        juce::Timer::callAfterDelay (6000, [safeThis, handled]
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);
            safeThis->updateButton.setButtonText ("Couldn't check");

            juce::Timer::callAfterDelay (2500, [safeThis]
            {
                if (safeThis != nullptr)
                    safeThis->updateButton.setButtonText ("Updates");
            });
        });
    };
    addAndMakeVisible (updateButton);

    soundkorbLink.setFont (AbcTrainLookAndFeel::monoFont().withHeight (13.0f), false,
                            juce::Justification::centredRight);
    addAndMakeVisible (soundkorbLink);

    // After the controls (so it floats above the visualisation it covers)
    // but before the lesson overlays, which must stay on top of everything.
    addAndMakeVisible (guideTooltip);

    // Added last, after every other child, so a shown lesson overlay
    // actually covers the title-row buttons/link instead of them poking
    // through on top of it - the same z-order fix as decisions/015's
    // Training Sounds overlay and decisions/016's soundkorb.ru link.
    addChildComponent (lessonController);
    lessonController.onClosed = [this] { resized(); };

    addChildComponent (tailLessonController);
    tailLessonController.onClosed = [this] { resized(); };

    startTimerHz (30);
    // Taller for the section panels' own padding/captions; the guide text
    // no longer needs a permanent strip (it floats on demand instead).
    setSize (780, 760);

    applyTheme();
}

void LearnerVerbEditor::applyTheme()
{
    const auto& theme = AbcTrainTheme::current();

    inputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    outputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, theme.accent);

    for (auto& knob : knobs)
        knob.nameLabel.setColour (juce::Label::textColourId, theme.textDim);

    themeButton.setButtonText (theme.mode == AbcTrainTheme::Mode::light ? "Dark" : "Light");
    repaint();
}

void LearnerVerbEditor::toggleTheme()
{
    const auto newMode = AbcTrainTheme::getMode() == AbcTrainTheme::Mode::light
                             ? AbcTrainTheme::Mode::dark
                             : AbcTrainTheme::Mode::light;

    AbcTrainTheme::setMode (newMode);
    themeProperties.setValue (themeModeKey, newMode == AbcTrainTheme::Mode::light ? "light" : "dark");

    lookAndFeel.refreshFromTheme();
    applyTheme();

    for (auto* child : getChildren())
        child->repaint();
}

LearnerVerbEditor::~LearnerVerbEditor()
{
    processor.setWaveformDisplay (nullptr);
    processor.setSpectrumAnalyzer (nullptr);
    setLookAndFeel (nullptr);
}

void LearnerVerbEditor::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    AbcTrainLookAndFeel::paintSectionPanel (g, analysisSection.toFloat(), "Analysis");
    AbcTrainLookAndFeel::paintSectionPanel (g, controlSection.toFloat(), "Reverb");

    AbcTrainLookAndFeel::paintDisplayWell (g, spectrum.getBounds().toFloat().expanded (1.0f));
    AbcTrainLookAndFeel::paintDisplayWell (g, waveform.getBounds().toFloat().expanded (1.0f));

    AbcTrainLookAndFeel::drawTrackedText (
        g, titleLabel.getText(),
        juce::Rectangle<float> (52.0f, (float) AbcTrainTheme::Spacing::medium,
                                 (float) getWidth() * 0.4f, 32.0f),
        AbcTrainLookAndFeel::titleFont(), theme.textBright, 1.8f,
        juce::Justification::centredLeft);
}

void LearnerVerbEditor::resized()
{
    using namespace AbcTrainTheme;

    lessonController.setBounds (getLocalBounds());
    tailLessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (Spacing::large);

    auto titleRow = area.removeFromTop (32);
    lessonSelector.setBounds (titleRow.removeFromRight (156));
    titleRow.removeFromRight (Spacing::small);
    themeButton.setBounds (titleRow.removeFromRight (62));
    titleRow.removeFromRight (Spacing::small);
    updateButton.setBounds (titleRow.removeFromRight (76));
    titleRow.removeFromRight (Spacing::small);
    bypassButton.setBounds (titleRow.removeFromRight (96));
    pluginIcon.setBounds (titleRow.removeFromLeft (28));

    area.removeFromTop (Spacing::section);

    // --- analysis section: spectrum, waveform, peak readouts ---
    analysisSection = area.removeFromTop (368);
    {
        auto inner = analysisSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        spectrum.setBounds (inner.removeFromTop (140).reduced (1));
        inner.removeFromTop (Spacing::medium);
        waveform.setBounds (inner.removeFromTop (150).reduced (1));
        inner.removeFromTop (Spacing::small);

        auto meterRow = inner.removeFromTop (20);
        inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 2));
        outputPeakLabel.setBounds (meterRow);
    }

    area.removeFromTop (Spacing::medium);

    // --- control section: type, knobs, presets ---
    controlSection = area.removeFromTop (222);
    {
        auto inner = controlSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        typeSelector.setBounds (inner.removeFromTop (26).withSizeKeepingCentre (220, 26));
        inner.removeFromTop (Spacing::small);

        auto knobRow = inner.removeFromTop (108);
        const auto knobWidth = knobRow.getWidth() / (int) knobs.size();
        for (auto& knob : knobs)
        {
            auto column = knobRow.removeFromLeft (knobWidth).reduced (Spacing::tight);
            knob.nameLabel.setBounds (column.removeFromTop (18));
            knob.slider.setBounds (column);
        }

        inner.removeFromTop (Spacing::small);

        auto presetRow = inner.removeFromTop (32);
        const auto presetWidth = presetRow.getWidth() / juce::jmax (1, presetButtons.size());
        for (auto* button : presetButtons)
            button->setBounds (presetRow.removeFromLeft (presetWidth).reduced (Spacing::tight, 0));
    }

    soundkorbLink.setBounds (area.removeFromBottom (18).removeFromRight (130));

    // The guide card floats over the lower part of the analysis section:
    // close to the controls being dragged, without covering them.
    guideTooltip.setBounds (analysisSection.reduced (Spacing::large, 0)
                                            .withHeight (66)
                                            .withY (analysisSection.getBottom() - 78));
}

void LearnerVerbEditor::timerCallback()
{
    const auto sr = processor.getSampleRate();
    spectrum.setSampleRate (sr > 0.0 ? sr : 44100.0);

    inputPeakLabel.setText ("In: "
                                 + juce::String (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -60.0f), 1)
                                 + " dB",
                             juce::dontSendNotification);

    outputPeakLabel.setText ("Out: "
                                  + juce::String (juce::Decibels::gainToDecibels (waveform.getOutputPeak(), -60.0f), 1)
                                  + " dB",
                              juce::dontSendNotification);
}
