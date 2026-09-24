#include "shared/learning/CompanionPanel.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

namespace
{
    constexpr int tabsHeight = 40;
    constexpr int guideHeight = 116;
    constexpr int hearingHeight = 168;

    juce::String fill (juce::String text, int n)
    {
        return text.replace ("{{n}}", juce::String (n));
    }
}

CompanionPanel::CompanionPanel()
{
    setOpaque (true);
}

void CompanionPanel::setStrings (Strings newStrings)
{
    text = std::move (newStrings);
    repaint();
}

void CompanionPanel::setAccent (juce::Colour c)
{
    accent = c;
    repaint();
}

void CompanionPanel::attach (juce::Component& modules, juce::Component* lesson)
{
    modulesView = &modules;
    lessonView = lesson;

    addChildComponent (modules);

    if (lessonView != nullptr)
        addChildComponent (*lessonView);

    // Open on the lesson when there is one: it is what the plugin was
    // showing beside the curve a moment ago.
    if (lessonView != nullptr)
        showLesson();
    else
        showModules();
}

void CompanionPanel::detach()
{
    if (modulesView != nullptr)
        removeChildComponent (modulesView);

    if (lessonView != nullptr)
        removeChildComponent (lessonView);

    modulesView = nullptr;
    lessonView = nullptr;
}

void CompanionPanel::showModules()
{
    lessonShown = false;

    if (modulesView != nullptr)
        modulesView->setVisible (true);

    if (lessonView != nullptr)
        lessonView->setVisible (false);

    resized();
    repaint();
}

void CompanionPanel::showLesson()
{
    if (lessonView == nullptr)
        return showModules();

    lessonShown = true;
    lessonView->setVisible (true);

    if (modulesView != nullptr)
        modulesView->setVisible (false);

    resized();
    repaint();
}

void CompanionPanel::setGuide (const juce::String& newGuide)
{
    if (newGuide == guide)
        return;

    guide = newGuide;
    repaint (guideArea());
}

void CompanionPanel::setHearing (const CompanionHearing& h)
{
    const auto changed = h.fromApp != hearing.fromApp || h.sessionMinutes != hearing.sessionMinutes
                      || std::abs (h.weekFraction - hearing.weekFraction) > 0.001
                      || h.minutesUntilBreak != hearing.minutesUntilBreak || h.calibrated != hearing.calibrated
                      || juce::roundToInt (h.levelDbA) != juce::roundToInt (hearing.levelDbA);
    hearing = h;

    if (changed)
        repaint (hearingArea());
}

juce::Rectangle<int> CompanionPanel::tabsArea() const
{
    return getLocalBounds().removeFromTop (tabsHeight);
}

juce::Rectangle<int> CompanionPanel::tabBounds (int index) const
{
    auto row = tabsArea().reduced (AbcTrainTheme::Spacing::medium, 6);
    const auto count = lessonView != nullptr ? 2 : 1;
    const auto width = juce::jmin (150, row.getWidth() / count);
    return row.withWidth (width).translated (index * (width + 4), 0);
}

juce::Rectangle<int> CompanionPanel::hearingArea() const
{
    return getLocalBounds().removeFromBottom (hearingHeight);
}

juce::Rectangle<int> CompanionPanel::guideArea() const
{
    return getLocalBounds().withTrimmedBottom (hearingHeight).removeFromBottom (guideHeight);
}

juce::Rectangle<int> CompanionPanel::bodyArea() const
{
    return getLocalBounds().withTrimmedTop (tabsHeight).withTrimmedBottom (hearingHeight + guideHeight);
}

void CompanionPanel::resized()
{
    const auto body = bodyArea();

    if (modulesView != nullptr)
        modulesView->setBounds (body);

    if (lessonView != nullptr)
        lessonView->setBounds (body.reduced (AbcTrainTheme::Spacing::small));
}

void CompanionPanel::mouseUp (const juce::MouseEvent& e)
{
    if (! tabsArea().contains (e.getPosition()))
        return;

    if (tabBounds (0).contains (e.getPosition()))
        lessonView != nullptr ? showLesson() : showModules();
    else if (lessonView != nullptr && tabBounds (1).contains (e.getPosition()))
        showModules();
}

void CompanionPanel::paint (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();

    g.fillAll (theme.panelBackground);

    // Tabs: the lesson first when there is one, then the modules.
    {
        const juce::String labels[] { lessonView != nullptr ? text.lesson : text.modules, text.modules };
        const auto count = lessonView != nullptr ? 2 : 1;

        for (int i = 0; i < count; ++i)
        {
            const auto on = lessonView != nullptr ? (i == 0) == lessonShown : true;
            auto box = tabBounds (i).toFloat();

            if (on)
            {
                g.setColour (accent.withAlpha (0.18f));
                g.fillRect (box);
                g.setColour (accent);
                g.fillRect (box.removeFromBottom (2.0f));
            }

            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (labels[i]), tabBounds (i).toFloat(),
                                                  AbcTrainLookAndFeel::labelFont(), on ? theme.textBright : theme.textDim,
                                                  1.3f, juce::Justification::centred);
        }

        g.setColour (theme.divider);
        g.fillRect (0, tabsHeight - 1, getWidth(), 1);
    }

    // Under the pointer.
    {
        auto area = guideArea();
        g.setColour (theme.divider);
        g.fillRect (area.removeFromTop (1));
        area = area.reduced (Spacing::medium, Spacing::small);

        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (text.underPointer),
                                              area.removeFromTop (18).toFloat(), AbcTrainLookAndFeel::microFont(),
                                              theme.textDim, 1.4f);
        area.removeFromTop (4);

        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.setColour (guide.isNotEmpty() ? theme.text : theme.textDim);
        AbcTrainLookAndFeel::fitLines (g, guide.isNotEmpty() ? guide : text.underPointerEmpty, area,
                                       juce::Justification::topLeft, 4, 0.85f);
    }

    paintHearing (g, hearingArea());
}

void CompanionPanel::paintHearing (juce::Graphics& g, juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();

    g.setColour (theme.divider);
    g.fillRect (area.removeFromTop (1));
    area = area.reduced (Spacing::medium, Spacing::small);

    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (text.hearing),
                                          area.removeFromTop (18).toFloat(), AbcTrainLookAndFeel::microFont(),
                                          theme.textDim, 1.4f);
    area.removeFromTop (6);

    // One line: a label on the left, a value on the right, a bar under it -
    // the shape of a usage meter, which everybody already reads.
    const auto meter = [&g, &theme, &area] (const juce::String& label, const juce::String& value,
                                             float fraction, juce::Colour colour)
    {
        auto row = area.removeFromTop (20);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.setColour (theme.text);
        AbcTrainLookAndFeel::fitText (g, label, row.removeFromLeft (row.getWidth() / 2), juce::Justification::centredLeft, true);
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.5f));
        AbcTrainLookAndFeel::fitText (g, value, row, juce::Justification::centredRight, true);

        auto bar = area.removeFromTop (8).toFloat();
        g.setColour (theme.displayBackground);
        g.fillRect (bar);
        g.setColour (theme.outline);
        g.drawRect (bar, 1.0f);
        g.setColour (colour);
        g.fillRect (bar.reduced (1.0f).withWidth ((bar.getWidth() - 2.0f) * juce::jlimit (0.0f, 1.0f, fraction)));
        area.removeFromTop (8);
    };

    if (! hearing.fromApp)
    {
        // A plugin in a DAW sees only its own window being open.
        meter (text.pluginTime, fill (text.minutes, hearing.sessionMinutes),
               (float) hearing.sessionMinutes / 60.0f, accent);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.setColour (theme.textDim);
        AbcTrainLookAndFeel::fitLines (g, text.pluginNote, area, juce::Justification::topLeft, 2, 0.9f);
        return;
    }

    auto sessionValue = fill (text.minutes, hearing.sessionMinutes);
    if (hearing.levelDbA > 1.0)
        sessionValue << "  ~" << juce::roundToInt (hearing.levelDbA) << " dB(A)";

    meter (text.session, sessionValue, (float) hearing.sessionMinutes / 60.0f, accent);

    if (hearing.calibrated)
        meter (text.week, juce::String (juce::roundToInt (hearing.weekFraction * 100.0)) + "%",
               (float) hearing.weekFraction, hearing.weekFraction >= 0.5 ? theme.accentWarm : accent);

    if (hearing.minutesUntilBreak >= 0)
        meter (text.untilBreak, fill (text.inMinutes, hearing.minutesUntilBreak),
               1.0f - (float) hearing.minutesUntilBreak / 60.0f, theme.accentWarm);

    if (! hearing.calibrated)
    {
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.setColour (theme.textDim);
        AbcTrainLookAndFeel::fitLines (g, text.notCalibrated, area, juce::Justification::topLeft, 2, 0.9f);
    }
}

// ---------------------------------------------------------------------------

CompanionWindow::CompanionWindow (const juce::String& title, CompanionPanel& panel, juce::Colour background)
    : DocumentWindow (title, background, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    setContentNonOwned (&panel, false);
    setResizable (true, false);
    setResizeLimits (300, 460, 900, 1400);
}

void CompanionWindow::closeButtonPressed()
{
    if (onCloseRequested != nullptr)
        onCloseRequested();
}
