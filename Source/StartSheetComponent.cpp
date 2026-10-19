#include "StartSheetComponent.h"

#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

namespace
{
    using LnF = AbcTrainLookAndFeel;

    constexpr int pad = 16;
    constexpr int headerHeight = 52;
    constexpr int levelHeight = 52;
    constexpr int captionHeight = 20;
    constexpr int modeHeight = 58;
    constexpr int modeGap = 6;
    constexpr int soundHeight = 46;
    constexpr int startHeight = 54;
    constexpr int gap = 14;
}

StartSheetComponent::StartSheetComponent()
{
    setWantsKeyboardFocus (true);
    startButton.onClick = [this]
    {
        const auto mode = selected;
        setVisible (false);

        if (onStart != nullptr)
            onStart (mode);
    };
    LnF::makePrimary (startButton);
    addAndMakeVisible (startButton);
}

void StartSheetComponent::show (Content newContent, int mode)
{
    content = std::move (newContent);
    selected = juce::jlimit (0, 2, mode);

    if (content.modes[(size_t) selected].locked)
        selected = 0;

    startButton.setButtonText (content.startText);
    setVisible (true);
    toFront (true);
    resized();
    repaint();
}

void StartSheetComponent::dismiss()
{
    if (! isVisible())
        return;

    setVisible (false);

    if (onDismissed != nullptr)
        onDismissed();
}

int StartSheetComponent::contentHeight (int width) const
{
    // The description is the one part whose height depends on the words.
    juce::AttributedString text;
    text.append (content.description, LnF::bodyFont());
    juce::TextLayout layout;
    layout.createLayout (text, (float) juce::jmax (100, width - 2 * pad));
    const auto descriptionHeight = juce::jlimit (22, 110, (int) std::ceil (layout.getHeight()) + 2);

    return pad + headerHeight + gap + descriptionHeight + gap + levelHeight + gap
         + captionHeight + 8 + 3 * modeHeight + 2 * modeGap + gap
         + soundHeight + gap + startHeight + pad;
}

juce::Rectangle<int> StartSheetComponent::panelBounds() const
{
    if (asSheet())
    {
        const auto height = juce::jmin (getHeight() - 24, contentHeight (getWidth()) + 10);
        return getLocalBounds().removeFromBottom (height);
    }

    if (twoColumns())
        return getLocalBounds().reduced (24, 8).withSizeKeepingCentre (juce::jmin (getWidth() - 48, 860),
                                                                       juce::jmin (getHeight() - 16, 400));

    const auto width = juce::jmin (getWidth() - 48, 520);
    const auto height = juce::jmin (getHeight() - 24, contentHeight (width));
    return juce::Rectangle<int> (width, height).withCentre (getLocalBounds().getCentre());
}

StartSheetComponent::Rows StartSheetComponent::rowsIn (juce::Rectangle<int> panel) const
{
    Rows r;
    auto area = panel.reduced (pad);

    if (twoColumns())
    {
        auto right = area.removeFromRight ((area.getWidth() - pad) / 2);
        area.removeFromRight (pad);

        // Left: what it is, the level, the sound.
        r.header = area.removeFromTop (headerHeight);
        area.removeFromTop (10);
        r.sound = area.removeFromBottom (soundHeight);
        area.removeFromBottom (8);
        r.level = area.removeFromBottom (levelHeight);
        area.removeFromBottom (8);
        r.description = area;

        // Right: the choice and Start.
        right.removeFromBottom (startHeight + 10);
        r.choose = right.removeFromTop (captionHeight);
        right.removeFromTop (6);
        const auto h = juce::jmin (modeHeight, (right.getHeight() - 2 * modeGap) / 3);
        for (int i = 0; i < 3; ++i)
        {
            r.modes[(size_t) i] = right.removeFromTop (h);
            right.removeFromTop (modeGap);
        }

        return r;
    }

    if (asSheet())
        area.removeFromTop (10);   // the grab handle

    // Bottom up, so a short screen squeezes the description, not Start.
    auto bottom = area.removeFromBottom (startHeight);
    juce::ignoreUnused (bottom);
    area.removeFromBottom (gap);
    r.sound = area.removeFromBottom (soundHeight);
    area.removeFromBottom (gap);

    for (int i = 2; i >= 0; --i)
    {
        r.modes[(size_t) i] = area.removeFromBottom (modeHeight);
        if (i > 0)
            area.removeFromBottom (modeGap);
    }

    area.removeFromBottom (8);
    r.choose = area.removeFromBottom (captionHeight);
    area.removeFromBottom (gap);
    r.level = area.removeFromBottom (levelHeight);
    area.removeFromBottom (gap);

    r.header = area.removeFromTop (headerHeight);
    area.removeFromTop (gap);
    r.description = area;
    return r;
}

juce::Rectangle<int> StartSheetComponent::startBounds() const
{
    auto panel = panelBounds().reduced (pad);

    if (twoColumns())
        return panel.removeFromRight ((panel.getWidth() - pad) / 2).removeFromBottom (startHeight);

    return panel.removeFromBottom (startHeight);
}

void StartSheetComponent::resized()
{
    startButton.setBounds (startBounds());
}

void StartSheetComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    // Whatever was on screen stays visible under a dim: the sheet is a
    // question about the next step, not a new place.
    g.fillAll (theme.windowBackground.withAlpha (0.72f));

    auto panel = panelBounds();
    g.setColour (theme.panelBackground);
    g.fillRect (panel);
    g.setColour (content.accent.withAlpha (0.9f));

    if (asSheet())
    {
        g.fillRect (panel.removeFromTop (1));
        g.setColour (theme.textDim.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<int> (36, 4).withCentre ({ panel.getCentreX(), panel.getY() + 9 }));
    }
    else
    {
        g.drawRect (panel, 1);
    }

    const auto rows = rowsIn (panelBounds());

    // --- the exercise -----------------------------------------------------
    {
        auto header = rows.header;
        auto box = header.removeFromLeft (headerHeight);
        g.setColour (content.accent);
        g.drawRect (box, 1);
        AppIcons::draw (g, content.icon, box.reduced (12).toFloat(), content.accent);
        header.removeFromLeft (14);

        g.setColour (theme.textBright);
        g.setFont (LnF::titleFont());
        LnF::fitText (g, content.title, header.removeFromTop (30), juce::Justification::centredLeft, true);
        LnF::drawTrackedText (g, LnF::toCaps (content.family), header.toFloat(), LnF::microFont(),
                              theme.textDim, 1.6f);
    }

    g.setColour (theme.text);
    g.setFont (LnF::bodyFont());
    LnF::fitLines (g, content.description, rows.description, juce::Justification::topLeft, 5, 0.9f);

    // --- where the staircase is ------------------------------------------
    {
        auto level = rows.level;
        g.setColour (theme.divider);
        g.fillRect (level.removeFromTop (1));
        g.fillRect (level.removeFromBottom (1));

        auto value = level.removeFromRight (juce::jmin (150, level.getWidth() / 2));
        g.setColour (theme.textBright);
        g.setFont (LnF::monoFont().withHeight (16.0f));
        LnF::fitText (g, content.levelText, value, juce::Justification::centredRight, true);

        auto left = level.reduced (0, 9);
        LnF::drawTrackedText (g, LnF::toCaps (content.levelCaption), left.removeFromTop (16).toFloat(),
                              LnF::microFont(), theme.textDim, 1.6f);
        LnF::drawSegmentedBar (g, left.removeFromTop (14).withWidth (juce::jmin (200, left.getWidth() - 12))
                                      .toFloat().withSizeKeepingCentre ((float) juce::jmin (200, left.getWidth() - 12), 4.0f),
                               content.levelOf,
                               (float) content.levelSteps / (float) juce::jmax (1, content.levelOf),
                               content.accent, theme.outline, 3.0f);
    }

    LnF::drawTrackedText (g, LnF::toCaps (content.chooseCaption), rows.choose.toFloat(),
                          LnF::labelFont(), theme.textDim, 2.0f);

    // --- the three modes ---------------------------------------------------
    for (int i = 0; i < 3; ++i)
    {
        const auto& mode = content.modes[(size_t) i];
        auto row = rows.modes[(size_t) i];
        const auto on = i == selected;
        const auto alpha = mode.locked ? 0.45f : 1.0f;

        if (on)
        {
            g.setColour (theme.accentWarm.withAlpha (0.10f));
            g.fillRect (row);
        }

        g.setColour ((on ? theme.accentWarm : theme.outline).withAlpha (alpha));
        g.drawRect (row, 1);

        auto inner = row.reduced (12, 8);
        auto dot = inner.removeFromLeft (12).withSizeKeepingCentre (12, 12);

        if (on)
            g.fillRect (dot);
        else
            g.drawRect (dot, 1);

        inner.removeFromLeft (12);
        LnF::drawTrackedText (g, LnF::toCaps (mode.name), inner.removeFromTop (22).toFloat(),
                              LnF::headingFont().withHeight (18.0f), theme.textBright.withAlpha (alpha), 1.4f);
        g.setColour (theme.textDim.withAlpha (alpha));
        g.setFont (LnF::captionFont());
        LnF::fitText (g, mode.detail, inner, juce::Justification::centredLeft, true);
    }

    // --- what it will be played on ----------------------------------------
    {
        auto sound = rows.sound;
        g.setColour (theme.divider);
        g.fillRect (sound.removeFromTop (1));
        g.fillRect (sound.removeFromBottom (1));

        auto chevron = sound.removeFromRight (20).toFloat().withSizeKeepingCentre (8.0f, 12.0f);
        juce::Path p;
        p.startNewSubPath (chevron.getX(), chevron.getY());
        p.lineTo (chevron.getRight(), chevron.getCentreY());
        p.lineTo (chevron.getX(), chevron.getBottom());
        g.setColour (theme.textDim);
        g.strokePath (p, juce::PathStrokeType (1.6f));

        auto caption = sound.removeFromLeft (90);
        LnF::drawTrackedText (g, LnF::toCaps (content.soundCaption), caption.toFloat(), LnF::labelFont(),
                              theme.textDim, 2.0f);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, content.soundValue, sound.withTrimmedRight (8), juce::Justification::centredRight, true);
    }
}

void StartSheetComponent::mouseUp (const juce::MouseEvent& event)
{
    if (! event.mouseWasClicked())
        return;

    const auto p = event.getPosition();

    if (! panelBounds().contains (p))
    {
        dismiss();
        return;
    }

    const auto rows = rowsIn (panelBounds());

    for (int i = 0; i < 3; ++i)
        if (rows.modes[(size_t) i].contains (p))
        {
            if (content.modes[(size_t) i].locked)
            {
                if (onLockedPressed != nullptr)
                    onLockedPressed (i);
                return;
            }

            selected = i;
            repaint();
            return;
        }

    if (rows.sound.contains (p) && onSound != nullptr)
    {
        setVisible (false);
        onSound();
    }
}

bool StartSheetComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        startButton.triggerClick();
        return true;
    }

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const auto step = key == juce::KeyPress::upKey ? -1 : 1;

        for (int k = 1; k <= 3; ++k)
        {
            const auto next = (selected + step * k + 6) % 3;

            if (! content.modes[(size_t) next].locked)
            {
                selected = next;
                repaint();
                break;
            }
        }

        return true;
    }

    return false;
}
