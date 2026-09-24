#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include <vector>

// The panel beside the EQ curve: one real job on the chosen instrument, as
// steps, each ticked when the curve does it; or, with no instrument
// chosen, what the range under the pointer sounds like.
//
// It sits beside the curve rather than over it, so the thing being taught
// and the thing being turned are both in view - the module panel covers
// the analysis while it runs; this never covers anything.
class LessonPanel : public juce::Component
{
public:
    enum class State { done, current, later };

    struct Step
    {
        juce::String text;
        State state = State::later;
    };

    void setContent (juce::String newCaption, juce::String newTitle, std::vector<Step> newSteps,
                     juce::String newBody, juce::String newFootnote)
    {
        if (newCaption == caption && newTitle == title && newBody == body && newFootnote == footnote
            && sameSteps (newSteps))
            return;

        caption = std::move (newCaption);
        title = std::move (newTitle);
        steps = std::move (newSteps);
        body = std::move (newBody);
        footnote = std::move (newFootnote);
        repaint();
    }

    void setAccentColour (juce::Colour c) { accent = c; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto bounds = getLocalBounds().toFloat();

        g.setColour (theme.panelBackground.withAlpha (0.7f));
        g.fillRect (bounds);
        g.setColour (theme.outline);
        g.drawRect (bounds.reduced (0.5f), 1.0f);

        auto area = getLocalBounds().reduced (14, 12);

        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (caption), area.removeFromTop (16).toFloat(),
                                              AbcTrainLookAndFeel::microFont(), accent, 1.4f);
        area.removeFromTop (6);

        // Title: as many lines as it needs, up to three.
        {
            const auto font = AbcTrainLookAndFeel::headingFont().withHeight (19.0f);
            juce::AttributedString text;
            text.append (title, font, theme.textBright);
            juce::TextLayout layout;
            layout.createLayout (text, (float) area.getWidth());
            const auto h = juce::jmin (70, (int) std::ceil (layout.getHeight()));
            layout.draw (g, area.removeFromTop (h).toFloat());
        }

        area.removeFromTop (10);

        auto footArea = area.removeFromBottom (footnote.isNotEmpty() ? 36 : 0);

        const auto bodyFont = AbcTrainLookAndFeel::captionFont().withHeight (13.5f);

        for (const auto& step : steps)
        {
            juce::AttributedString text;
            const auto colour = step.state == State::current ? theme.textBright
                              : step.state == State::done ? theme.textDim : theme.text;
            text.append (step.text, bodyFont, colour);
            juce::TextLayout layout;
            layout.createLayout (text, (float) area.getWidth() - 22.0f);
            const auto h = (int) std::ceil (layout.getHeight());

            if (h > area.getHeight())
                break;

            auto row = area.removeFromTop (h);
            auto mark = row.removeFromLeft (22).toFloat().withHeight (16.0f);

            if (step.state == State::done)
            {
                juce::Path tick;
                const auto c = mark.getCentre().translated (-4.0f, 0.0f);
                tick.startNewSubPath (c.x - 4.0f, c.y);
                tick.lineTo (c.x - 1.0f, c.y + 3.0f);
                tick.lineTo (c.x + 4.0f, c.y - 4.0f);
                g.setColour (theme.positive);
                g.strokePath (tick, juce::PathStrokeType (1.6f));
            }
            else
            {
                g.setColour (step.state == State::current ? accent : theme.textDim);
                g.setFont (bodyFont);
                AbcTrainLookAndFeel::fitText (g, step.state == State::current ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92"))
                                                         : juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")),
                            mark.toNearestInt(), juce::Justification::centredLeft, false);
            }

            layout.draw (g, row.toFloat());
            area.removeFromTop (8);
        }

        if (body.isNotEmpty())
        {
            g.setColour (theme.text);
            g.setFont (bodyFont);
            AbcTrainLookAndFeel::fitLines (g, body, area, juce::Justification::topLeft, 6, 1.0f);
        }

        if (footnote.isNotEmpty())
        {
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont().withHeight (12.0f));
            AbcTrainLookAndFeel::fitLines (g, footnote, footArea, juce::Justification::bottomLeft, 3, 0.9f);
        }
    }

private:
    bool sameSteps (const std::vector<Step>& other) const
    {
        if (other.size() != steps.size())
            return false;

        for (size_t i = 0; i < steps.size(); ++i)
            if (other[i].text != steps[i].text || other[i].state != steps[i].state)
                return false;

        return true;
    }

    juce::String caption, title, body, footnote;
    std::vector<Step> steps;
    juce::Colour accent { AbcTrainTheme::current().accent };
};
