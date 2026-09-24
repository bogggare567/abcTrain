#include "AchievementsScreenComponent.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

namespace
{
    constexpr int tickHz = 60;

    constexpr float headerHeight   = 64.0f;
    constexpr float captionHeight  = 34.0f;
    constexpr float medalCellWidth = 184.0f;
    constexpr float medalCellHeight = 206.0f;
    constexpr float medalDiameter  = 108.0f;
    constexpr float stampSize      = 70.0f;
    constexpr float stampGap       = 8.0f;
    constexpr float sectionGap     = 22.0f;
}

AchievementsScreenComponent::AchievementsScreenComponent()
{
    setOpaque (true);

    closeButton.onClick = [this]
    {
        setVisible (false);

        if (onClosed != nullptr)
            onClosed();
    };

    // Not shown: this is a page reached from a tab, and another tab is the
    // way out (ADR 034). Kept as a child so nothing that reaches for it
    // finds a null.
    addChildComponent (closeButton);

    startTimerHz (tickHz);
}

AchievementsScreenComponent::~AchievementsScreenComponent()
{
    stopTimer();
}

void AchievementsScreenComponent::setEntries (std::vector<Entry> newEntries)
{
    // Milestones first; inside each layer, earned first, then whatever is
    // closest - the gap you are three rounds from is worth more attention
    // than the one that needs a year.
    std::stable_sort (newEntries.begin(), newEntries.end(),
                       [] (const Entry& a, const Entry& b)
                       {
                           if (a.milestone != b.milestone)
                               return a.milestone;

                           if (a.earned != b.earned)
                               return a.earned;

                           return a.progress > b.progress;
                       });

    entries = std::move (newEntries);
    hoverAmounts.assign (entries.size(), 0.0f);
    hovered = -1;
    scrollOffset = 0.0f;

    layout();
    repaint();
}

void AchievementsScreenComponent::setStrings (juce::String title, juce::String subtitle,
                                               juce::String milestones, juce::String stamps)
{
    titleText = std::move (title);
    subtitleText = std::move (subtitle);
    milestonesText = std::move (milestones);
    stampsText = std::move (stamps);
    repaint();
}

juce::Rectangle<int> AchievementsScreenComponent::cardBounds() const
{
    // A page, not a card (ADR 034).
    return getLocalBounds();
}

void AchievementsScreenComponent::layout()
{
    cells.assign (entries.size(), {});

    const auto area = cardBounds().toFloat().reduced ((float) AbcTrainTheme::Spacing::large);
    auto y = area.getY() + headerHeight;

    // --- milestones: as many medals across as fit, centred as a block ---
    const auto numMilestones = (int) std::count_if (entries.begin(), entries.end(),
                                                    [] (const Entry& e) { return e.milestone; });

    milestoneCaption = { area.getX(), y, area.getWidth(), captionHeight };
    y += captionHeight;

    const auto medalCols = juce::jlimit (2, 6, (int) (area.getWidth() / medalCellWidth));
    const auto medalWidth = area.getWidth() / (float) medalCols;

    int m = 0, s = 0;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (! entries[i].milestone)
            continue;

        const auto col = m % medalCols;
        const auto row = m / medalCols;
        cells[i] = { area.getX() + (float) col * medalWidth,
                     y + (float) row * medalCellHeight,
                     medalWidth, medalCellHeight };
        ++m;
    }

    y += (float) ((numMilestones + medalCols - 1) / medalCols) * medalCellHeight + sectionGap;

    // --- stamps: a tight grid, left-aligned like text ----------------------
    stampCaption = { area.getX(), y, area.getWidth(), captionHeight };
    y += captionHeight;

    const auto stampCols = juce::jmax (1, (int) ((area.getWidth() + stampGap) / (stampSize + stampGap)));

    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].milestone)
            continue;

        const auto col = s % stampCols;
        const auto row = s / stampCols;
        cells[i] = { area.getX() + (float) col * (stampSize + stampGap),
                     y + (float) row * (stampSize + stampGap),
                     stampSize, stampSize };
        ++s;
    }

    const auto stampRows = (s + stampCols - 1) / stampCols;
    y += (float) stampRows * (stampSize + stampGap);

    contentHeight = y - (float) cardBounds().getY() + (float) AbcTrainTheme::Spacing::large;
    maxScroll = juce::jmax (0.0f, contentHeight - (float) getHeight());
    scrollOffset = juce::jlimit (0.0f, maxScroll, scrollOffset);
}

void AchievementsScreenComponent::resized()
{
    layout();
}

// ============================================================ drawings ===

void AchievementsScreenComponent::drawArt (juce::Graphics& g, Art art,
                                           juce::Rectangle<float> r, juce::Colour c)
{
    // Every drawing is the subject's own picture, in the vocabulary a
    // sound engineer already reads: a filter's bell, a compressor's knee,
    // an impulse response, a pan pot. Strokes scale with the box so a
    // medal in a toast and a medal on the page are the same drawing.
    const auto stroke = juce::jmax (1.5f, r.getWidth() * 0.045f);
    const juce::PathStrokeType pen (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const auto faint = c.withAlpha (c.getFloatAlpha() * 0.35f);

    const auto x = [&] (float t) { return r.getX() + r.getWidth() * t; };
    const auto y = [&] (float t) { return r.getY() + r.getHeight() * t; };

    switch (art)
    {
        case Art::eq:
        {
            g.setColour (faint);
            g.drawLine (x (0.0f), y (0.78f), x (1.0f), y (0.78f), stroke * 0.6f);

            juce::Path bell;
            bell.startNewSubPath (x (0.0f), y (0.78f));
            for (int i = 1; i <= 48; ++i)
            {
                const auto t = (float) i / 48.0f;
                const auto d = (t - 0.5f) / 0.14f;
                bell.lineTo (x (t), y (0.78f - 0.58f * std::exp (-0.5f * d * d)));
            }
            g.setColour (c);
            g.strokePath (bell, pen);
            break;
        }

        case Art::compression:
        {
            g.setColour (faint);
            g.drawLine (x (0.05f), y (0.95f), x (0.95f), y (0.05f), stroke * 0.6f);   // 1:1

            juce::Path curve;
            curve.startNewSubPath (x (0.05f), y (0.95f));
            curve.lineTo (x (0.45f), y (0.55f));
            curve.quadraticTo (x (0.55f), y (0.45f), x (0.65f), y (0.42f));
            curve.lineTo (x (0.95f), y (0.33f));
            g.setColour (c);
            g.strokePath (curve, pen);
            g.fillEllipse (x (0.55f) - stroke * 1.4f, y (0.46f) - stroke * 1.4f, stroke * 2.8f, stroke * 2.8f);
            break;
        }

        case Art::reverb:
        {
            g.setColour (c);
            for (int i = 0; i < 9; ++i)
            {
                const auto t = 0.06f + 0.11f * (float) i;
                const auto h = 0.8f * std::exp (-0.33f * (float) i);
                g.fillRect (juce::Rectangle<float> (x (t), y (0.88f - h), stroke * 1.1f, r.getHeight() * h));
            }
            break;
        }

        case Art::pan:
        {
            juce::Path arc;
            arc.addCentredArc (x (0.5f), y (0.82f), r.getWidth() * 0.44f, r.getWidth() * 0.44f,
                               0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            g.setColour (faint);
            g.strokePath (arc, pen);

            for (int i = -2; i <= 2; ++i)
            {
                const auto a = (float) i * juce::MathConstants<float>::pi / 5.0f;
                const auto rr = r.getWidth() * 0.44f;
                g.drawLine (x (0.5f) + std::sin (a) * rr * 0.86f, y (0.82f) - std::cos (a) * rr * 0.86f,
                            x (0.5f) + std::sin (a) * rr, y (0.82f) - std::cos (a) * rr, stroke * 0.7f);
            }

            g.setColour (c);
            g.drawLine (x (0.5f), y (0.82f), x (0.5f), y (0.26f), stroke * 1.2f);
            g.fillEllipse (x (0.5f) - stroke * 1.6f, y (0.82f) - stroke * 1.6f, stroke * 3.2f, stroke * 3.2f);
            break;
        }

        case Art::delay:
        {
            g.setColour (faint);
            g.drawLine (x (0.02f), y (0.5f), x (0.98f), y (0.5f), stroke * 0.6f);

            for (int i = 0; i < 4; ++i)
            {
                const auto size = r.getWidth() * (0.2f - 0.04f * (float) i);
                g.setColour (c.withAlpha (c.getFloatAlpha() * (1.0f - 0.2f * (float) i)));
                g.fillRect (juce::Rectangle<float> (size, size)
                                .withCentre ({ x (0.12f + 0.26f * (float) i), y (0.5f) }));
            }
            break;
        }

        case Art::distortion:
        {
            g.setColour (faint);
            juce::Path sine;
            juce::Path clipped;
            for (int i = 0; i <= 64; ++i)
            {
                const auto t = (float) i / 64.0f;
                const auto v = std::sin (t * juce::MathConstants<float>::twoPi * 1.5f);
                const auto pt = juce::Point<float> (x (t), y (0.5f - 0.38f * v));
                const auto pc = juce::Point<float> (x (t), y (0.5f - 0.38f * juce::jlimit (-0.55f, 0.55f, v * 1.4f)));
                if (i == 0) { sine.startNewSubPath (pt); clipped.startNewSubPath (pc); }
                else        { sine.lineTo (pt);          clipped.lineTo (pc); }
            }
            g.strokePath (sine, juce::PathStrokeType (stroke * 0.6f));
            g.setColour (c);
            g.strokePath (clipped, pen);
            break;
        }

        case Art::width:
        {
            const juce::Point<float> apex (x (0.5f), y (0.9f));
            g.setColour (faint);
            g.drawLine ({ apex, { x (0.36f), y (0.12f) } }, stroke * 0.7f);
            g.drawLine ({ apex, { x (0.64f), y (0.12f) } }, stroke * 0.7f);

            g.setColour (c);
            g.drawLine ({ apex, { x (0.04f), y (0.2f) } }, stroke);
            g.drawLine ({ apex, { x (0.96f), y (0.2f) } }, stroke);

            juce::Path arc;
            arc.addCentredArc (apex.x, apex.y, r.getWidth() * 0.62f, r.getWidth() * 0.62f, 0.0f,
                               -0.9f, 0.9f, true);
            g.strokePath (arc, pen);
            break;
        }

        case Art::gain:
        {
            g.setColour (faint);
            g.fillRect (juce::Rectangle<float> (x (0.18f), y (0.36f), r.getWidth() * 0.22f, r.getHeight() * 0.54f));
            g.setColour (c);
            g.fillRect (juce::Rectangle<float> (x (0.6f), y (0.26f), r.getWidth() * 0.22f, r.getHeight() * 0.64f));

            // the bracket between the two tops: the whole subject is that gap
            g.drawLine (x (0.44f), y (0.36f), x (0.56f), y (0.36f), stroke * 0.7f);
            g.drawLine (x (0.44f), y (0.26f), x (0.56f), y (0.26f), stroke * 0.7f);
            g.drawLine (x (0.5f), y (0.26f), x (0.5f), y (0.36f), stroke * 0.7f);
            break;
        }

        case Art::range:
        {
            static const float heights[] { 0.35f, 0.5f, 0.62f, 0.86f, 0.62f, 0.45f, 0.3f };
            const auto w = r.getWidth() / 7.0f;
            for (int i = 0; i < 7; ++i)
            {
                g.setColour (i == 3 ? c : faint);
                g.fillRect (juce::Rectangle<float> (r.getX() + w * (float) i + w * 0.15f,
                                                    y (0.9f - heights[i]), w * 0.7f,
                                                    r.getHeight() * heights[i]));
            }
            break;
        }

        case Art::allFive:
        case Art::allNine:
        {
            using AbcTrainTheme::Family;
            const Family families[] { Family::frequency, Family::dynamics, Family::space, Family::character };
            const auto whole = art == Art::allNine;
            const auto radius = r.getWidth() * 0.4f;

            for (int q = 0; q < 4; ++q)
            {
                const auto start = (float) q * juce::MathConstants<float>::halfPi + 0.08f;
                const auto end = start + (juce::MathConstants<float>::halfPi - 0.16f) * (whole ? 1.0f : 0.55f);
                juce::Path arc;
                arc.addCentredArc (r.getCentreX(), r.getCentreY(), radius, radius, 0.0f, start, end, true);

                g.setColour (AbcTrainTheme::accentFor (families[q]).withAlpha (c.getFloatAlpha()));
                g.strokePath (arc, juce::PathStrokeType (stroke * 1.8f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::butt));
            }

            g.setColour (c);
            const auto dot = whole ? stroke * 3.2f : stroke * 2.0f;
            g.fillRect (juce::Rectangle<float> (dot, dot).withCentre (r.getCentre()));
            break;
        }

        case Art::month:
        {
            const auto cell = r.getWidth() / 6.0f;
            for (int row = 0; row < 5; ++row)
                for (int col = 0; col < 6; ++col)
                {
                    const auto index = row * 6 + col;
                    g.setColour (index < 30 ? c : faint);
                    g.fillRect (juce::Rectangle<float> (r.getX() + (float) col * cell + cell * 0.18f,
                                                        r.getY() + r.getHeight() * 0.08f
                                                            + (float) row * cell + cell * 0.18f,
                                                        cell * 0.64f, cell * 0.64f));
                }
            break;
        }
    }
}

// ============================================================ painting ===

void AchievementsScreenComponent::paintSectionCaption (juce::Graphics& g, const juce::String& text,
                                                       juce::Rectangle<float> area)
{
    const auto& theme = AbcTrainTheme::current();
    const auto font = AbcTrainLookAndFeel::headingFont();
    const auto caps = AbcTrainLookAndFeel::toCaps (text);
    const auto width = AbcTrainLookAndFeel::trackedTextWidth (caps, font, 2.7f);

    AbcTrainLookAndFeel::drawTrackedText (g, caps, area.withWidth (width), font, theme.text, 2.7f,
                                           juce::Justification::centredLeft);

    g.setColour (theme.divider);
    g.fillRect (juce::Rectangle<float> (area.getX() + width + 14.0f, area.getCentreY(),
                                        juce::jmax (0.0f, area.getWidth() - width - 14.0f), 1.0f));
}

void AchievementsScreenComponent::paintMedal (juce::Graphics& g, const Entry& entry,
                                              juce::Rectangle<float> cell, float hover)
{
    const auto& theme = AbcTrainTheme::current();
    const auto lift = AbcTrainTheme::Ease::out (hover);

    auto disc = juce::Rectangle<float> (medalDiameter, medalDiameter)
                    .withCentre ({ cell.getCentreX(), cell.getY() + 8.0f + medalDiameter * 0.5f - 3.0f * lift });

    // The plate.
    g.setColour (entry.earned ? theme.displayBackground : theme.displayBackground.withAlpha (0.6f));
    g.fillEllipse (disc.reduced (6.0f));

    if (entry.earned)
    {
        // A glow under an earned medal - the one soft effect on the page,
        // kept for the thing that was earned.
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (entry.tint.withAlpha (0.07f / (float) i));
            g.drawEllipse (disc.expanded ((float) i * 3.0f), 3.0f);
        }

        g.setColour (entry.tint);
        g.drawEllipse (disc.reduced (1.5f), 3.0f);
        g.setColour (entry.tint.withAlpha (0.45f));
        g.drawEllipse (disc.reduced (8.0f), 1.0f);
    }
    else
    {
        g.setColour (theme.outline);
        g.drawEllipse (disc.reduced (1.5f), 1.5f);

        // How far along: an arc from twelve o'clock, in the medal's colour.
        if (entry.progress > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc (disc.getCentreX(), disc.getCentreY(),
                               disc.getWidth() * 0.5f - 1.5f, disc.getHeight() * 0.5f - 1.5f,
                               0.0f, 0.0f, juce::MathConstants<float>::twoPi * entry.progress, true);
            g.setColour (entry.tint.withAlpha (0.8f));
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::butt));
        }
    }

    drawArt (g, entry.art, disc.reduced (medalDiameter * 0.27f),
             entry.earned ? entry.tint : theme.textDim.withAlpha (0.55f));

    // Name and what it asks.
    auto text = cell.withTop (disc.getBottom() + 10.0f).reduced (8.0f, 0.0f);

    g.setColour (entry.earned ? theme.textBright : theme.text);
    g.setFont (AbcTrainLookAndFeel::headingFont());
    AbcTrainLookAndFeel::fitLines (g, entry.name, text.removeFromTop (24.0f).toNearestInt(),
                       juce::Justification::centred, 1, 0.8f);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    AbcTrainLookAndFeel::fitLines (g, entry.description, text.removeFromTop (40.0f).toNearestInt(),
                       juce::Justification::centredTop, 2, 0.85f);
}

void AchievementsScreenComponent::paintStamp (juce::Graphics& g, const Entry& entry,
                                              juce::Rectangle<float> cell, float hover)
{
    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (hover);

    if (entry.earned)
    {
        g.setColour (entry.tint.withAlpha (0.14f + 0.06f * eased));
        g.fillRect (cell);
        g.setColour (entry.tint);
        g.drawRect (cell, 1.0f);
    }
    else
    {
        g.setColour (theme.outline.withAlpha (0.8f + 0.2f * eased));
        g.drawRect (cell, 1.0f);

        // Progress as a bar along the bottom edge: small, but a stamp you
        // are 80% of the way to should not look like one you have never
        // touched.
        if (entry.progress > 0.01f)
        {
            g.setColour (entry.tint.withAlpha (0.7f));
            g.fillRect (cell.withTop (cell.getBottom() - 3.0f).withWidth (cell.getWidth() * entry.progress));
        }
    }

    auto inner = cell.reduced (5.0f);
    AppIcons::draw (g, entry.icon, inner.removeFromTop (22.0f).withSizeKeepingCentre (18.0f, 18.0f),
                    entry.earned ? entry.tint : theme.textDim.withAlpha (0.6f));

    g.setColour (entry.earned ? theme.textBright : theme.textDim);
    g.setFont (AbcTrainLookAndFeel::microFont());
    AbcTrainLookAndFeel::fitLines (g, entry.name, inner.toNearestInt(), juce::Justification::centred, 2, 0.75f);
}

void AchievementsScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::translation (0.0f, -scrollOffset));

    const auto area = cardBounds().toFloat().reduced ((float) AbcTrainTheme::Spacing::large);

    {
        auto header = area.withHeight (headerHeight);
        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::titleFont());
        AbcTrainLookAndFeel::fitText (g, titleText, header.removeFromTop (30.0f).toNearestInt(), juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitText (g, subtitleText, header.removeFromTop (22.0f).toNearestInt(), juce::Justification::centredLeft, true);
    }

    paintSectionCaption (g, milestonesText, milestoneCaption.withTrimmedBottom (8.0f));
    paintSectionCaption (g, stampsText, stampCaption.withTrimmedBottom (8.0f));

    for (size_t i = 0; i < entries.size() && i < cells.size(); ++i)
    {
        const auto hover = i < hoverAmounts.size() ? hoverAmounts[i] : 0.0f;
        if (entries[i].milestone)
            paintMedal (g, entries[i], cells[i], hover);
        else
            paintStamp (g, entries[i], cells[i], hover);
    }

    g.restoreState();
    g.saveState();

    // A tooltip for the hovered stamp: its square has room for a name but
    // not for what it asks.
    if (hovered >= 0 && hovered < (int) entries.size() && ! entries[(size_t) hovered].milestone)
    {
        const auto& entry = entries[(size_t) hovered];
        const auto cell = cells[(size_t) hovered].translated (0.0f, -scrollOffset);
        const auto text = entry.name + "  ·  " + entry.description;
        const auto font = AbcTrainLookAndFeel::labelFont();
        const auto width = AbcTrainLookAndFeel::trackedTextWidth (text, font, 0.0f) + 24.0f;

        auto tip = juce::Rectangle<float> (width, 30.0f)
                       .withCentre ({ cell.getCentreX(), cell.getY() - 20.0f });
        tip = tip.withX (juce::jlimit (4.0f, (float) getWidth() - width - 4.0f, tip.getX()));

        g.setColour (theme.panelBackground);
        g.fillRect (tip);
        g.setColour (theme.outline);
        g.drawRect (tip, 1.0f);
        g.setColour (theme.textBright);
        g.setFont (font);
        AbcTrainLookAndFeel::fitText (g, text, tip.toNearestInt(), juce::Justification::centred, false);
    }

    // The fade is the scrollbar: it appears only where there is more.
    if (scrollOffset < maxScroll - 1.0f)
    {
        const auto fade = 40.0f;
        const auto bottom = (float) getHeight();
        g.setGradientFill ({ theme.windowBackground, 0.0f, bottom,
                             theme.windowBackground.withAlpha (0.0f), 0.0f, bottom - fade, false });
        g.fillRect (getLocalBounds().toFloat().withTop (bottom - fade));
    }
}

void AchievementsScreenComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position.translated (0.0f, scrollOffset);
    auto index = -1;

    for (size_t i = 0; i < cells.size(); ++i)
        if (cells[i].contains (p))
            index = (int) i;

    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void AchievementsScreenComponent::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void AchievementsScreenComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scrollOffset = juce::jlimit (0.0f, maxScroll, scrollOffset - wheel.deltaY * 120.0f);
    repaint();
}

void AchievementsScreenComponent::timerCallback()
{
    auto moving = false;

    for (size_t i = 0; i < hoverAmounts.size(); ++i)
    {
        const auto target = (int) i == hovered ? 1.0f : 0.0f;
        auto& value = hoverAmounts[i];

        if (std::abs (value - target) > 0.001f)
        {
            value += (target - value) * 0.25f;
            moving = true;
        }
        else
        {
            value = target;
        }
    }

    if (moving)
        repaint();
}
