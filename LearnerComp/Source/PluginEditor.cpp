#include "PluginEditor.h"
#include "ParameterGuide.h"
#include "VocalCompressionLesson.h"
#include "BusGlueLesson.h"
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

    const std::array<KnobSpec, 7> knobSpecs {{
        { "threshold", "Threshold" },
        { "ratio",     "Ratio" },
        { "attack",    "Attack" },
        { "release",   "Release" },
        { "knee",      "Knee" },
        { "makeup",    "Makeup" },
        { "dryWet",    "Dry/Wet" }
    }};

    constexpr const char* themeModeKey = "themeMode";
}

LearnerCompEditor::LearnerCompEditor (LearnerCompProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      lessonController (p.apvts, buildVocalCompressionLesson()),
      busGlueLessonController (p.apvts, buildBusGlueLesson()),
      // Same shared "abcTrain" settings folder the language preference
      // uses, so light/dark is one product-wide choice rather than a
      // per-plugin one.
      themeProperties (LocalisationManager::makeDefaultOptions())
{
    AbcTrainTheme::setMode (themeProperties.getValue (themeModeKey, "dark") == "light"
                                ? AbcTrainTheme::Mode::light
                                : AbcTrainTheme::Mode::dark);
    accent = AbcTrainTheme::accentFor (AbcTrainTheme::Family::dynamics);
    lookAndFeel.refreshFromTheme (accent);

    setLookAndFeel (&lookAndFeel);

    // Drawn by paint() with letter-spacing rather than via the Label.
    titleLabel.setText ("Learner Comp", juce::dontSendNotification);
    titleLabel.setVisible (false);

    themeButton.onClick = [this] { toggleTheme(); };
    addAndMakeVisible (themeButton);

    addAndMakeVisible (practiceSelector);

    pluginIcon.setIcon (AppIcons::Icon::learnerComp);
    addAndMakeVisible (pluginIcon);

    addAndMakeVisible (spectrum);

    addAndMakeVisible (waveform);

    addAndMakeVisible (gainReductionMeter);

    inputPeakLabel.setJustificationType (juce::Justification::centred);
    inputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (inputPeakLabel);

    outputPeakLabel.setJustificationType (juce::Justification::centred);
    outputPeakLabel.setFont (AbcTrainLookAndFeel::monoFont());
    addAndMakeVisible (outputPeakLabel);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        const auto& spec = knobSpecs[i];

        knob.nameLabel.setText (spec.label, juce::dontSendNotification);
        knob.nameLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (knob.nameLabel);

        addAndMakeVisible (knob.slider);

        // Rebuild the value box now that this slider has a parent, and so
        // resolves to this editor's LookAndFeel rather than JUCE's default.
        // A Slider creates its text box in its own constructor - as a
        // member, long before setLookAndFeel() - so it keeps the default
        // LookAndFeel's bordered, filled field no matter what colours the
        // theme sets later. This has to come *after* addAndMakeVisible:
        // calling it first resolves getLookAndFeel() to the default one
        // again and changes nothing, which is exactly what the first
        // attempt at this fix did.
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);

        knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, spec.paramId, knob.slider);

        const juce::String paramId (spec.paramId);
        knob.slider.onDragStart = [this, paramId]
        {
            guideTooltip.setText (CompressorGuide::describe (paramId));
        };
        knob.slider.onDragEnd = [this]
        {
            // Empty text animates the card out rather than leaving a
            // permanent "drag a knob" strip taking up layout space.
            guideTooltip.setText ({});
        };
    }

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, LearnerCompProcessor::bypassParamId, bypassButton);
    addAndMakeVisible (bypassButton);

    for (int i = 0; i < (int) CompressorGuide::presets.size(); ++i)
    {
        auto* button = presetButtons.add (new juce::TextButton (CompressorGuide::presets[(size_t) i].name));
        button->onClick = [this, i]
        {
            processor.applyPreset (i);

            // The knobs moving is the "what changed"; this is the "why".
            guideTooltip.setText (juce::String (CompressorGuide::presets[(size_t) i].name) + " - "
                                   + CompressorGuide::presets[(size_t) i].what, 9000);
        };
        addAndMakeVisible (button);
    }

    processor.setWaveformDisplay (&waveform);
    processor.setSpectrumAnalyzer (&spectrum);

    lessonSelector.addItem ("Vocal Compression", 1);
    lessonSelector.addItem ("Bus Glue Compression", 2);
    lessonSelector.onChange = [this]
    {
        // Two lessons, one picker (see decisions/017) - only the selected
        // one is ever shown/started; the other stays hidden.
        if (lessonSelector.getSelectedId() == 1)
            lessonController.showAndStart();
        else if (lessonSelector.getSelectedId() == 2)
            busGlueLessonController.showAndStart();
    };
    // Nothing is selected until a lesson is picked, and an unlabelled
    // empty dropdown in the title row was the result - caught by rendering
    // the editor (tools/EditorSnapshots). Re-cleared on close below, so
    // picking the same lesson twice in a row still starts it.
    lessonSelector.setTextWhenNothingSelected ("Lessons");
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
        juce::Component::SafePointer<LearnerCompEditor> safeThis (this);
        auto handled = std::make_shared<bool> (false);

        updateButton.setEnabled (false);
        // The button is now an icon with no room for text, so the outcome
        // goes where there is room: the guide card that already floats in
        // over the visualisation. ADR 014's rule still holds - every click
        // gets a visible outcome - it just gets a better-looking one.
        guideTooltip.setText ("Checking for updates...");

        UpdateChecker::checkForUpdatesAsync (CurrentVersion::string, [safeThis, handled] (bool foundNewer, UpdateChecker::ReleaseInfo release)
        {
            if (safeThis == nullptr || *handled)
                return;
            *handled = true;

            safeThis->updateButton.setEnabled (true);

            if (! foundNewer)
            {
                safeThis->guideTooltip.setText ("You're on the latest version ("
                                                 + juce::String (CurrentVersion::string) + ").", 4000);
                return;
            }

            safeThis->guideTooltip.setText ({});

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
            safeThis->guideTooltip.setText ("Couldn't reach the update server. "
                                             "Check your connection and try again.", 5000);
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
    lessonController.onClosed = [this]
    {
        // Back to "Lessons" so the same one can be started again.
        lessonSelector.setSelectedId (0, juce::dontSendNotification);
        resized();
    };

    addChildComponent (busGlueLessonController);
    busGlueLessonController.onClosed = [this]
    {
        // Back to "Lessons" so the same one can be started again.
        lessonSelector.setSelectedId (0, juce::dontSendNotification);
        resized();
    };

    startTimerHz (30);
    // Taller than before: the two section panels carry their own padding
    // and captions, and the guide text no longer occupies a permanent
    // strip (it floats over the visualisation on demand instead).
    // 20 + 32 title + 28 + 460 analysis + 12 + 186 controls + 20 margin.
    // Derived from resized() rather than guessed, which is how 132px of
    // empty window got here in the first place.
    setSize (840, 758);

    applyTheme();
}

void LearnerCompEditor::applyTheme()
{
    const auto& theme = AbcTrainTheme::current();

    inputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    outputPeakLabel.setColour (juce::Label::textColourId, theme.textDim);
    soundkorbLink.setColour (juce::HyperlinkButton::textColourId, theme.accent);

    for (auto& knob : knobs)
        knob.nameLabel.setColour (juce::Label::textColourId, theme.textDim);

    // The glyph cross-fades rather than cutting, so the toggle reads as
    // one control changing state (see IconButton).
    themeButton.setIcon (theme.mode == AbcTrainTheme::Mode::light ? AppIcons::Icon::moon
                                                                  : AppIcons::Icon::sun);

    spectrum.setAccentColour (accent);
    waveform.setAccentColour (accent);
    pluginIcon.setIconColour (accent);
    repaint();
}

void LearnerCompEditor::toggleTheme()
{
    const auto newMode = AbcTrainTheme::getMode() == AbcTrainTheme::Mode::light
                             ? AbcTrainTheme::Mode::dark
                             : AbcTrainTheme::Mode::light;

    AbcTrainTheme::setMode (newMode);
    themeProperties.setValue (themeModeKey, newMode == AbcTrainTheme::Mode::light ? "light" : "dark");

    // accentFor() returns a different value per mode, so it has to be
    // asked again rather than reused from construction.
    accent = AbcTrainTheme::accentFor (AbcTrainTheme::Family::dynamics);
    lookAndFeel.refreshFromTheme (accent);
    applyTheme();

    for (auto* child : getChildren())
        child->repaint();
}

LearnerCompEditor::~LearnerCompEditor()
{
    processor.setWaveformDisplay (nullptr);
    processor.setSpectrumAnalyzer (nullptr);
    setLookAndFeel (nullptr);
}

void LearnerCompEditor::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    // Each plugin's own room, the same idea EarTrainer applies per
    // exercise - and the same four family colours, so the trainer and the
    // plugin that teaches the same skill read as one subject.
    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat(), accent);

    AbcTrainLookAndFeel::paintSectionPanel (g, analysisSection.toFloat(), "Analysis");
    AbcTrainLookAndFeel::paintSectionPanel (g, controlSection.toFloat(), "Compressor");

    // Recessed wells behind the two data displays, so they read as cut
    // into the panel while the controls sit on top of it.
    AbcTrainLookAndFeel::paintDisplayWell (g, spectrum.getBounds().toFloat().expanded (1.0f));
    AbcTrainLookAndFeel::paintDisplayWell (g, waveform.getBounds().toFloat().expanded (1.0f));

    AbcTrainLookAndFeel::drawTrackedText (
        g, titleLabel.getText(),
        juce::Rectangle<float> (52.0f, (float) AbcTrainTheme::Spacing::medium,
                                 (float) getWidth() * 0.4f, 32.0f),
        AbcTrainLookAndFeel::titleFont(), theme.textBright, 1.8f,
        juce::Justification::centredLeft);
}

void LearnerCompEditor::paintOverChildren (juce::Graphics& g)
{
    if (bypassVeil <= 0.004f || analysisSection.isEmpty())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (bypassVeil);
    const auto area = analysisSection.toFloat().reduced (AbcTrainTheme::Spacing::medium);

    // Desaturating the analysis rather than hiding it: you still want to
    // see the signal going past, you just need to be able to tell at a
    // glance that nothing is being done to it.
    g.setColour (theme.windowBackground.withAlpha (0.62f * eased));
    g.fillRoundedRectangle (area, AbcTrainTheme::Radius::well);

    AbcTrainLookAndFeel::drawTrackedText (g, "BYPASSED", area.withHeight (20.0f)
                                                              .withY (area.getCentreY() - 10.0f),
                                           AbcTrainLookAndFeel::captionFont(),
                                           theme.textDim.withAlpha (eased), 3.0f,
                                           juce::Justification::centred);
}

void LearnerCompEditor::resized()
{
    using namespace AbcTrainTheme;

    lessonController.setBounds (getLocalBounds());
    busGlueLessonController.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (Spacing::large);

    auto titleRow = area.removeFromTop (32);
    lessonSelector.setBounds (titleRow.removeFromRight (156));
    titleRow.removeFromRight (Spacing::small);
    themeButton.setBounds (titleRow.removeFromRight (30).withSizeKeepingCentre (30, 30));
    titleRow.removeFromRight (Spacing::tight);
    updateButton.setBounds (titleRow.removeFromRight (30).withSizeKeepingCentre (30, 30));
    titleRow.removeFromRight (Spacing::small);
    bypassButton.setBounds (titleRow.removeFromRight (96));
    titleRow.removeFromRight (Spacing::small);
    practiceSelector.setBounds (titleRow.removeFromRight (practiceSelector.getPreferredWidth())
                                    .withSizeKeepingCentre (practiceSelector.getPreferredWidth(), 24));
    pluginIcon.setBounds (titleRow.removeFromLeft (28));

    area.removeFromTop (Spacing::section);

    // --- analysis section: spectrum, waveform, meters ---
    // 460, not the 390 this used to be. The old value was 14px short of
    // what the rows inside it ask for, and removeFromTop clamps to the
    // height available rather than overflowing - so the gain-reduction
    // meter silently got 32px instead of 46 and drew as a token circle.
    // Caught by rendering the editor (tools/EditorSnapshots); the window
    // had 132px of dead space underneath at the same time, which is where
    // the extra height comes from rather than from a bigger window.
    analysisSection = area.removeFromTop (460);
    {
        auto inner = analysisSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

        spectrum.setBounds (inner.removeFromTop (170).reduced (1));
        inner.removeFromTop (Spacing::medium);
        waveform.setBounds (inner.removeFromTop (170).reduced (1));
        inner.removeFromTop (Spacing::medium);

        auto meterRow = inner.removeFromTop (52);
        inputPeakLabel.setBounds (meterRow.removeFromLeft (meterRow.getWidth() / 3));
        outputPeakLabel.setBounds (meterRow.removeFromRight (meterRow.getWidth() / 2));
        gainReductionMeter.setBounds (meterRow.reduced (Spacing::small, 0));
    }

    area.removeFromTop (Spacing::medium);

    // --- control section: knobs and presets ---
    controlSection = area.removeFromTop (186);
    {
        auto inner = controlSection.reduced (Spacing::medium);
        inner.removeFromTop (Spacing::large);

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
    // close to the knobs being dragged, without covering them.
    guideTooltip.setBounds (analysisSection.reduced (Spacing::large, 0)
                                            .withHeight (66)
                                            .withY (analysisSection.getBottom() - 78));
}

void LearnerCompEditor::timerCallback()
{
    // Bypass used to change nothing on screen, so the only way to know
    // whether you were hearing the plugin was to look at the checkbox.
    // Eased on this timer rather than a second one - 30 Hz over ~260 ms is
    // eight frames, plenty for a fade.
    {
        const auto target = processor.apvts.getRawParameterValue (LearnerCompProcessor::bypassParamId)->load() > 0.5f
                                ? 1.0f : 0.0f;

        if (! juce::approximatelyEqual (bypassVeil, target))
        {
            const auto step = (float) (1000.0 / 30.0 / AbcTrainTheme::Duration::release);

            bypassVeil = std::abs (target - bypassVeil) <= step
                             ? target
                             : bypassVeil + (target > bypassVeil ? step : -step);
            repaint();
        }
    }

    const auto sr = processor.getSampleRate();
    spectrum.setSampleRate (sr > 0.0 ? sr : 44100.0);

    gainReductionMeter.setGainReductionDb (waveform.getCurrentHighlightAmount());

    inputPeakLabel.setText ("In: "
                                 + juce::String (juce::Decibels::gainToDecibels (waveform.getInputPeak(), -60.0f), 1)
                                 + " dB",
                             juce::dontSendNotification);

    outputPeakLabel.setText ("Out: "
                                  + juce::String (juce::Decibels::gainToDecibels (waveform.getOutputPeak(), -60.0f), 1)
                                  + " dB",
                              juce::dontSendNotification);
}
