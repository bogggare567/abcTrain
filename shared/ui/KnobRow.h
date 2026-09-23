#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include <functional>
#include <memory>
#include <vector>

// A row of rotary knobs bound to parameters, each with its name above in
// tracked capitals and its value below - the control half of Learner Comp
// and Learner Verb. One component instead of seven hand-wired sliders per
// editor, so the labels, the value boxes and the guide text behave the same
// in both.
class KnobRow : public juce::Component
{
public:
    struct Spec
    {
        juce::String paramId;
        juce::String label;       // already translated
        juce::String suffix;      // " ms", " dB", ":1", "%" - already translated
        int decimals = 1;
    };

    KnobRow (juce::AudioProcessorValueTreeState& state, std::vector<Spec> specs, juce::String decimal = ".")
        : apvts (state)
    {
        for (auto& spec : specs)
        {
            auto knob = std::make_unique<Knob>();
            knob->paramId = spec.paramId;
            knob->label = spec.label;

            addAndMakeVisible (knob->slider);
            knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
            knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, spec.paramId, knob->slider);

            // The value under a knob is the one number that has to be both
            // exact and readable: "20.00" said nothing a unit would not.
            // After the attachment, which installs its own text function.
            const auto suffix = spec.suffix;
            const auto decimals = spec.decimals;
            knob->slider.textFromValueFunction = [suffix, decimals, decimal] (double v)
            {
                return (decimals > 0 ? juce::String (v, decimals).replace (".", decimal)
                                     : juce::String (juce::roundToInt (v))) + suffix;
            };
            knob->slider.valueFromTextFunction = [] (const juce::String& text)
            {
                return text.replace (",", ".").retainCharacters ("-0123456789.").getDoubleValue();
            };
            knob->slider.updateText();

            auto* raw = knob.get();
            knob->slider.onDragStart = [this, raw] { if (onDragStart != nullptr) onDragStart (raw->paramId); };
            knob->slider.onDragEnd = [this, raw] { if (onDragEnd != nullptr) onDragEnd (raw->paramId); };
            knob->slider.onValueChange = [this, raw] { if (onValueChange != nullptr) onValueChange (raw->paramId); };

            knobs.push_back (std::move (knob));
        }
    }

    std::function<void (const juce::String& paramId)> onDragStart, onDragEnd, onValueChange;

    // Re-reads the palette into the value boxes, which keep the colours they
    // were given (the light theme drew white on white otherwise).
    void refreshColours()
    {
        const auto& theme = AbcTrainTheme::current();

        for (auto& k : knobs)
        {
            k->slider.setColour (juce::Slider::textBoxTextColourId, theme.textBright);
            k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        }

        repaint();
    }

    juce::Slider* sliderFor (const juce::String& paramId)
    {
        for (auto& k : knobs)
            if (k->paramId == paramId)
                return &k->slider;

        return nullptr;
    }

    static constexpr int labelHeight = 20;
    static constexpr int noteHeight = 16;

    // One short line under a knob's value saying what the number means in
    // the world - "room = 14 x 10 x 6 m" under Size. Setting any note gives
    // every column room for one.
    void setNote (const juce::String& paramId, const juce::String& note)
    {
        for (auto& k : knobs)
            if (k->paramId == paramId && k->note != note)
            {
                k->note = note;
                repaint (k->column);
            }

        if (! hasNotes && note.isNotEmpty())
        {
            hasNotes = true;
            resized();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();

        for (auto& k : knobs)
        {
            const auto area = k->column.withHeight (labelHeight).toFloat();
            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (k->label), area,
                                                  AbcTrainLookAndFeel::labelFont(), theme.textDim, 1.3f,
                                                  juce::Justification::centred);

            if (hasNotes && k->note.isNotEmpty())
            {
                g.setColour (theme.textDim);
                g.setFont (AbcTrainLookAndFeel::captionFont().withHeight (12.0f));
                g.drawFittedText (k->note, k->column.withTop (k->column.getBottom() - noteHeight),
                                  juce::Justification::centred, 1, 0.85f);
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        const auto width = area.getWidth() / juce::jmax (1, (int) knobs.size());

        for (auto& k : knobs)
        {
            k->column = area.removeFromLeft (width).reduced (AbcTrainTheme::Spacing::tight, 0);
            k->slider.setBounds (k->column.withTrimmedTop (labelHeight).withTrimmedBottom (hasNotes ? noteHeight : 0));
        }
    }

private:
    struct Knob
    {
        juce::String paramId, label, note;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::Rectangle<int> column;
    };

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<std::unique_ptr<Knob>> knobs;
    bool hasNotes = false;
};

// The teaching presets as a row of chips, the current one filled - "you
// are here, and here is what else there is". Cleared when a knob moves,
// because after that the claim is no longer true.
class PresetRow : public juce::Component
{
public:
    explicit PresetRow (juce::StringArray names)
    {
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (names[i]));
            b->onClick = [this, i] { setActive (i); if (onChosen != nullptr) onChosen (i); };
            addAndMakeVisible (b);
        }
    }

    std::function<void (int)> onChosen;

    void setActive (int index)
    {
        active = index;

        for (int i = 0; i < buttons.size(); ++i)
            AbcTrainLookAndFeel::makePrimary (*buttons[i], i == active);

        repaint();
    }

    int getActive() const noexcept { return active; }

    void resized() override
    {
        auto row = getLocalBounds();
        const auto width = row.getWidth() / juce::jmax (1, buttons.size());

        for (auto* b : buttons)
            b->setBounds (row.removeFromLeft (width).reduced (AbcTrainTheme::Spacing::tight, 0));
    }

private:
    juce::OwnedArray<juce::TextButton> buttons;
    int active = -1;
};
