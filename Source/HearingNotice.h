#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include <functional>

// A slim strip under the navigation bar for what HearingGuard has to say:
// "an hour without a break", "your ears are tired", "half the week's
// dose". One sentence and at most two buttons.
//
// A strip, not a dialogue: it never stops a round, never takes focus and
// never blocks the answer - H.870 asks that the listener be *told*, and a
// modal box in the middle of an exercise would be punishing the person
// for having ears. It stays until answered or until the next one replaces
// it.
class HearingNotice : public juce::Component
{
public:
    HearingNotice()
    {
        addAndMakeVisible (primary);
        addAndMakeVisible (secondary);

        primary.onClick = [this] { if (onPrimary != nullptr) onPrimary(); dismiss(); };
        secondary.onClick = [this] { if (onSecondary != nullptr) onSecondary(); dismiss(); };
    }

    void show (const juce::String& message, const juce::String& primaryText,
               const juce::String& secondaryText = {}, bool isWarning = false)
    {
        text = message;
        warning = isWarning;
        primary.setButtonText (primaryText);
        secondary.setButtonText (secondaryText);
        secondary.setVisible (secondaryText.isNotEmpty());
        setVisible (true);
        resized();
        repaint();

        if (onShown != nullptr)
            onShown();
    }

    void dismiss()
    {
        setVisible (false);

        if (onDismissed != nullptr)
            onDismissed();
    }

    std::function<void()> onPrimary, onSecondary, onShown, onDismissed;

    static constexpr int height = 46;

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto accent = warning ? theme.accentWarm : theme.positive;

        g.setColour (theme.panelBackground);
        g.fillRect (getLocalBounds());
        g.setColour (accent.withAlpha (0.12f));
        g.fillRect (getLocalBounds());
        g.setColour (accent);
        g.fillRect (0, 0, 4, getHeight());
        g.setColour (theme.divider);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        auto area = getLocalBounds().withTrimmedLeft (20).withTrimmedRight (buttonsWidth() + 24);

        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (label), area.removeFromLeft (90).toFloat(),
                                              AbcTrainLookAndFeel::microFont(), accent, 1.4f);

        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawFittedText (text, area, juce::Justification::centredLeft, 2, 0.9f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12, 8);
        primary.setBounds (area.removeFromRight (150));

        if (secondary.isVisible())
        {
            area.removeFromRight (8);
            secondary.setBounds (area.removeFromRight (130));
        }
    }

    void setLabel (const juce::String& newLabel) { label = newLabel; repaint(); }

private:
    int buttonsWidth() const { return 150 + (secondary.isVisible() ? 138 : 0); }

    juce::String text, label;
    bool warning = false;
    juce::TextButton primary, secondary;
};
