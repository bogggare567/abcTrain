#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include <functional>

// A caption in tracked capitals and a row of chips, one of them chosen:
// "MATERIAL  [DRUM LOOP] VOCAL BASS", "START FROM  ...", "TYPE  ...".
//
// The approved Learner layouts (docs/design/approved-2026-09) put every
// choice a player makes about *what* is playing or *where to start* on
// one of these, level with the thing it changes. A dropdown hides the
// alternatives until opened; a chip row is the list of alternatives, which
// is what somebody learning a tool needs to see.
//
// The chosen chip is outlined in the family colour over a tint of it - not
// filled solid. A solid block is the one primary action of a screen
// (AbcTrainLookAndFeel::makePrimary) and a row of options is not that.
class ChipRow : public juce::Component
{
public:
    ChipRow() = default;

    void setCaption (juce::String newCaption)
    {
        caption = std::move (newCaption);
        resized();
        repaint();
    }

    void setItems (const juce::StringArray& labels)
    {
        chips.clear();

        for (int i = 0; i < labels.size(); ++i)
        {
            auto* chip = chips.add (new Chip (*this, i, labels[i]));
            addAndMakeVisible (chip);
        }

        resized();
        repaint();
    }

    int getNumItems() const noexcept { return chips.size(); }

    void setChosen (int index)
    {
        chosen = index;
        repaint();

        for (auto* c : chips)
            c->repaint();
    }

    int getChosen() const noexcept { return chosen; }

    void setAccent (juce::Colour colour)
    {
        accent = colour;
        repaint();

        for (auto* c : chips)
            c->repaint();
    }

    // Chips can be marked, e.g. a lesson already done. Drawn as a small
    // tick before the label.
    void setMarked (int index, bool shouldBeMarked)
    {
        if (juce::isPositiveAndBelow (index, chips.size()))
        {
            chips[index]->marked = shouldBeMarked;
            chips[index]->repaint();
        }
    }

    std::function<void (int)> onChosen;

    // The width this row would like at its natural chip widths.
    int getPreferredWidth() const
    {
        auto width = captionWidth() + (caption.isNotEmpty() ? 14 : 0);

        for (auto* c : chips)
            width += c->preferredWidth() + gap;

        return width;
    }

    void paint (juce::Graphics& g) override
    {
        if (caption.isEmpty())
            return;

        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (caption),
                                              getLocalBounds().withWidth (captionWidth()).toFloat(),
                                              AbcTrainLookAndFeel::microFont(),
                                              AbcTrainTheme::current().textDim, 1.4f);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (caption.isNotEmpty())
            area.removeFromLeft (captionWidth() + 14);

        // Natural widths when they fit; shrunk evenly when they do not.
        auto natural = 0;
        for (auto* c : chips)
            natural += c->preferredWidth() + gap;

        const auto squeeze = natural > area.getWidth() && natural > 0
                                 ? (float) area.getWidth() / (float) natural : 1.0f;

        for (auto* c : chips)
        {
            const auto w = (int) ((float) c->preferredWidth() * squeeze);
            c->setBounds (area.removeFromLeft (w));
            area.removeFromLeft ((int) ((float) gap * squeeze));
        }
    }

private:
    static constexpr int gap = 6;

    int captionWidth() const
    {
        if (caption.isEmpty())
            return 0;

        return (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (AbcTrainLookAndFeel::toCaps (caption),
                                                                        AbcTrainLookAndFeel::microFont(), 1.4f));
    }

    struct Chip : public juce::Component
    {
        Chip (ChipRow& ownerToUse, int indexToUse, juce::String textToShow)
            : owner (ownerToUse), index (indexToUse), text (std::move (textToShow))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTitle (text);
        }

        int preferredWidth() const
        {
            return (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (AbcTrainLookAndFeel::toCaps (text),
                                                                            AbcTrainLookAndFeel::microFont(), 1.2f))
                   + 28 + (marked ? 14 : 0);
        }

        void paint (juce::Graphics& g) override
        {
            const auto& theme = AbcTrainTheme::current();
            const auto on = owner.chosen == index;
            const auto hover = isMouseOver (true);
            auto box = getLocalBounds().toFloat().reduced (0.5f);

            g.setColour (on ? owner.accent.withAlpha (0.2f)
                            : theme.widgetBackground.withAlpha (hover ? 0.9f : 0.55f));
            g.fillRect (box);
            g.setColour (on ? owner.accent : theme.outline.withAlpha (hover ? 1.0f : 0.8f));
            g.drawRect (box, 1.0f);

            auto label = getLocalBounds();

            if (marked)
            {
                auto tickBox = label.removeFromLeft (22).toFloat().withTrimmedLeft (10.0f);
                juce::Path tick;
                const auto c = tickBox.getCentre();
                tick.startNewSubPath (c.x - 4.0f, c.y);
                tick.lineTo (c.x - 1.0f, c.y + 3.0f);
                tick.lineTo (c.x + 4.0f, c.y - 3.5f);
                g.setColour (theme.positive);
                g.strokePath (tick, juce::PathStrokeType (1.6f));
            }

            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (text), label.toFloat(),
                                                  AbcTrainLookAndFeel::microFont(),
                                                  on ? theme.textBright : theme.text, 1.2f,
                                                  juce::Justification::centred);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! getLocalBounds().contains (e.getPosition()))
                return;

            owner.setChosen (index);

            if (owner.onChosen != nullptr)
                owner.onChosen (index);
        }

        ChipRow& owner;
        int index;
        juce::String text;
        bool marked = false;
    };

    juce::String caption;
    juce::OwnedArray<Chip> chips;
    int chosen = -1;
    juce::Colour accent { AbcTrainTheme::current().accent };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChipRow)
};
