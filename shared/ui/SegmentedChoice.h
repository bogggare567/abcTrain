#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include <functional>
#include <vector>

// A row of square segments, one lit: "2 | 3 | 4", "OFF | ON".
//
// The settings pages are mostly small closed sets - three numbers, on or
// off - and a dropdown hides a three-way choice behind a click while a
// segmented row shows all of it and the current one at once. Square,
// tracked caps, the same grammar as the mode pills on the training screen.
class SegmentedChoice : public juce::Component
{
public:
    SegmentedChoice() { setRepaintsOnMouseActivity (true); }

    // Values and what to print for each, in order.
    void setOptions (std::vector<int> newValues, juce::StringArray newLabels)
    {
        values = std::move (newValues);
        labels = std::move (newLabels);
        repaint();
    }

    void setValue (int newValue, juce::NotificationType notify = juce::dontSendNotification)
    {
        if (value == newValue)
            return;

        value = newValue;
        repaint();

        if (notify != juce::dontSendNotification && onChange != nullptr)
            onChange (value);
    }

    int getValue() const noexcept { return value; }

    // Tracked capitals by default, like every label in this grammar. Off
    // for labels carrying units, where "DB" and "Ч" would be wrong.
    void setUppercase (bool shouldBeUppercase) { uppercase = shouldBeUppercase; repaint(); }

    std::function<void (int)> onChange;

    int getPreferredWidth() const
    {
        const auto font = AbcTrainLookAndFeel::labelFont();
        int w = 0;

        for (const auto& l : labels)
            w += juce::jmax (44, juce::roundToInt (AbcTrainLookAndFeel::trackedTextWidth (shown (l), font, 1.2f)) + 24);

        return w;
    }

    // The fill of the chosen cell; transparent means the palette's accent.
    // A Learner plugin passes its family colour.
    void setAccent (juce::Colour c) { accentOverride = c; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto accent = accentOverride.isTransparent() ? theme.accent : accentOverride;
        const auto font = AbcTrainLookAndFeel::labelFont();
        const auto cells = cellBounds();
        const auto mouse = getMouseXYRelative();

        for (size_t i = 0; i < cells.size(); ++i)
        {
            const auto r = cells[i].toFloat();
            const auto selected = values[i] == value;
            const auto hover = isMouseOver() && cells[i].contains (mouse) && isEnabled();

            if (selected)
            {
                g.setColour (accent.withAlpha (isEnabled() ? 0.85f : 0.35f));
                g.fillRect (r);
            }
            else if (hover)
            {
                g.setColour (theme.widgetBackground);
                g.fillRect (r);
            }

            const auto textColour = selected ? AbcTrainLookAndFeel::labelColourOn (accent)
                                             : (isEnabled() ? theme.text : theme.textDim);

            // A label wider than its cell gives up its tracking, then its
            // size, down to three quarters - never its neighbour's space
            // ("85 dB(A)90 dB(A)" on a narrow window).
            const auto text = shown (labels[(int) i]);
            auto cellFont = font;
            auto tracking = 1.2f;
            const auto room = r.getWidth() - 6.0f;

            if (AbcTrainLookAndFeel::trackedTextWidth (text, cellFont, tracking) > room)
            {
                tracking = 0.0f;
                const auto natural = AbcTrainLookAndFeel::trackedTextWidth (text, cellFont, 0.0f);

                if (natural > room && natural > 0.0f)
                    cellFont = cellFont.withHeight (cellFont.getHeight() * juce::jmax (0.75f, room / natural));
            }

            AbcTrainLookAndFeel::drawTrackedText (g, text, r, cellFont, textColour, tracking,
                                                  juce::Justification::centred);
        }

        g.setColour (theme.outline);
        g.drawRect (getLocalBounds().toFloat(), 1.0f);

        for (size_t i = 1; i < cells.size(); ++i)
            g.fillRect ((float) cells[i].getX(), 0.0f, 1.0f, (float) getHeight());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! isEnabled())
            return;

        const auto cells = cellBounds();

        for (size_t i = 0; i < cells.size(); ++i)
            if (cells[i].contains (e.getPosition()))
                setValue (values[i], juce::sendNotification);
    }

private:
    juce::Colour accentOverride;
    juce::String shown (const juce::String& label) const
    {
        return uppercase ? AbcTrainLookAndFeel::toCaps (label) : label;
    }

    std::vector<juce::Rectangle<int>> cellBounds() const
    {
        std::vector<juce::Rectangle<int>> cells;

        if (values.empty())
            return cells;

        // Widths in proportion to the labels, so "OFF | 90 MIN" does not
        // squeeze the long one to fit the short one.
        const auto font = AbcTrainLookAndFeel::labelFont();
        std::vector<float> widths;
        float total = 0.0f;

        for (const auto& l : labels)
        {
            const auto w = juce::jmax (44.0f, AbcTrainLookAndFeel::trackedTextWidth (shown (l), font, 1.2f) + 24.0f);
            widths.push_back (w);
            total += w;
        }

        float x = 0.0f;

        for (auto w : widths)
        {
            const auto next = x + w * (float) getWidth() / total;
            cells.push_back ({ juce::roundToInt (x), 0, juce::roundToInt (next) - juce::roundToInt (x), getHeight() });
            x = next;
        }

        return cells;
    }

    std::vector<int> values;
    juce::StringArray labels;
    int value = 0;
    bool uppercase = true;
};
