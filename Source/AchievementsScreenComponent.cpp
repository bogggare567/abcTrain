#include "AchievementsScreenComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"

namespace
{
    constexpr int tickHz = 60;
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
    addAndMakeVisible (closeButton);

    startTimerHz (tickHz);
}

AchievementsScreenComponent::~AchievementsScreenComponent()
{
    stopTimer();
}

void AchievementsScreenComponent::setEntries (std::vector<Entry> newEntries)
{
    // Earned first, then whatever is closest. A shelf shows what is on it
    // before it shows the gaps - and among the gaps, the one you are three
    // rounds away from is worth more of your attention than the one that
    // needs a year.
    std::stable_sort (newEntries.begin(), newEntries.end(),
                       [] (const Entry& a, const Entry& b)
                       {
                           if (a.earned != b.earned)
                               return a.earned;

                           return a.progress > b.progress;
                       });

    entries = std::move (newEntries);
    hoverAmounts.assign (entries.size(), 0.0f);
    hoveredRow = -1;
    scrollOffset = 0.0f;

    resized();
    repaint();
}

void AchievementsScreenComponent::setStrings (juce::String title, juce::String subtitle,
                                               juce::String close)
{
    titleText = std::move (title);
    subtitleText = std::move (subtitle);
    closeButton.setButtonText (close);
    repaint();
}

juce::Rectangle<int> AchievementsScreenComponent::cardBounds() const
{
    // A fraction of the window with a floor - see RunResultsComponent
    // for why these stopped being fixed widths. The shelf also takes more
    // *height* than the others: it is the one panel whose whole point is
    // that you can see how much is still ahead of you.
    return juce::Rectangle<int> (juce::jlimit (460, getWidth() - 64,
                                                juce::roundToInt ((float) getWidth() * 0.64f)),
                                  juce::jlimit (440, getHeight() - 64,
                                                juce::roundToInt ((float) getHeight() * 0.80f)))
               .withCentre (getLocalBounds().getCentre());
}

juce::Rectangle<int> AchievementsScreenComponent::listBounds() const
{
    using namespace AbcTrainTheme;

    return cardBounds().reduced (Spacing::large)
               .withTrimmedTop (26 + 18 + Spacing::medium)
               .withTrimmedBottom (34 + Spacing::small);
}

void AchievementsScreenComponent::paintEntry (juce::Graphics& g, const Entry& entry,
                                               juce::Rectangle<int> area, float hover)
{
    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (hover);
    const auto bounds = area.toFloat().reduced (0.0f, 1.0f);

    if (eased > 0.01f)
    {
        g.setColour (theme.widgetBackground.withAlpha (0.5f * eased));
        g.fillRoundedRectangle (bounds, (float) AbcTrainTheme::Radius::small);
    }

    auto row = bounds.reduced ((float) AbcTrainTheme::Spacing::small, 0.0f);

    // The badge, at the same size and with the same ring the home strip
    // uses - one idea of what an achievement looks like, in both places.
    const auto badgeBox = row.removeFromLeft (42.0f).withSizeKeepingCentre (38.0f, 38.0f);

    if (entry.earned)
    {
        g.setColour (entry.tint.withAlpha (0.9f));
        g.drawEllipse (badgeBox, 2.0f);
    }
    else if (entry.progress > 0.005f)
    {
        juce::Path arc;
        arc.addCentredArc (badgeBox.getCentreX(), badgeBox.getCentreY(),
                            badgeBox.getWidth() * 0.5f, badgeBox.getHeight() * 0.5f, 0.0f,
                            0.0f, juce::MathConstants<float>::twoPi * entry.progress, true);
        g.setColour (entry.tint.withAlpha (0.7f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    AppIcons::drawBadged (g, entry.icon, badgeBox.reduced (6.0f), entry.tint,
                           entry.earned ? 1.0f : 0.25f);

    row.removeFromLeft ((float) AbcTrainTheme::Spacing::medium);

    // The tier, right-aligned, and for a locked one the progress instead -
    // "14 / 20" says what to do next in a way a percentage does not.
    auto rightColumn = row.removeFromRight (76.0f);

    g.setColour (entry.earned ? entry.tint : theme.textDim.withAlpha (0.8f));
    g.setFont (AbcTrainLookAndFeel::captionFont());
    g.drawText (entry.earned ? entry.tierName
                             : juce::String (juce::roundToInt (entry.progress * 100.0f)) + "%",
                 rightColumn.toNearestInt(), juce::Justification::centredRight, false);

    auto text = row;

    g.setColour (entry.earned ? theme.textBright : theme.text.withAlpha (0.75f));
    g.setFont (AbcTrainLookAndFeel::headingFont());
    g.drawText (entry.name, text.removeFromTop (18.0f).toNearestInt(),
                 juce::Justification::centredLeft, true);

    g.setColour (theme.textDim.withAlpha (entry.earned ? 0.9f : 0.7f));
    g.setFont (AbcTrainLookAndFeel::captionFont());
    g.drawText (entry.description, text.removeFromTop (16.0f).toNearestInt(),
                 juce::Justification::centredLeft, true);
}

void AchievementsScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    const auto card = cardBounds().toFloat();

    juce::Path shape;
    shape.addRoundedRectangle (card, AbcTrainTheme::Radius::panel);

    juce::DropShadow (theme.shadow.withAlpha (0.6f * theme.shadowStrength), 24, { 0, 6 })
        .drawForPath (g, shape);

    g.setColour (theme.panelBackground);
    g.fillPath (shape);
    g.setColour (theme.outline);
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    auto header = card.reduced ((float) AbcTrainTheme::Spacing::large).toNearestInt();

    AbcTrainLookAndFeel::drawTrackedText (g, titleText, header.removeFromTop (26).toFloat(),
                                           AbcTrainLookAndFeel::headingFont(),
                                           theme.textBright, 1.2f);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    g.drawText (subtitleText, header.removeFromTop (18), juce::Justification::centredLeft, true);

    // The list scrolls inside its own clip, so rows cannot paint over the
    // heading above or the button below.
    const auto list = listBounds();
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (list);

    auto y = (float) list.getY() - scrollOffset;

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const juce::Rectangle<int> row (list.getX(), (int) y, list.getWidth(), rowHeight);
        y += (float) rowHeight;

        if (row.getBottom() < list.getY() || row.getY() > list.getBottom())
            continue;

        paintEntry (g, entries[i], row, i < hoverAmounts.size() ? hoverAmounts[i] : 0.0f);

        if (i + 1 < entries.size())
        {
            g.setColour (theme.divider.withAlpha (0.4f));
            g.drawHorizontalLine (row.getBottom(), (float) row.getX() + 50.0f,
                                   (float) row.getRight());
        }
    }

    // A row sliced in half by the card's edge reads as a rendering fault,
    // not as "there is more below" - so fade the list into the panel at
    // whichever end still has content past it. The fade is the scrollbar:
    // it appears only when there is somewhere to go, which is also the
    // only time it means anything.
    const auto fadeHeight = 26.0f;
    const auto fadeInto = theme.panelBackground;

    if (scrollOffset > 1.0f)
    {
        g.setGradientFill ({ fadeInto, (float) list.getX(), (float) list.getY(),
                             fadeInto.withAlpha (0.0f), (float) list.getX(),
                             (float) list.getY() + fadeHeight, false });
        g.fillRect (list.withHeight ((int) fadeHeight));
    }

    if (scrollOffset < maxScroll - 1.0f)
    {
        g.setGradientFill ({ fadeInto, (float) list.getX(), (float) list.getBottom(),
                             fadeInto.withAlpha (0.0f), (float) list.getX(),
                             (float) list.getBottom() - fadeHeight, false });
        g.fillRect (list.withTop (list.getBottom() - (int) fadeHeight));
    }
}

void AchievementsScreenComponent::resized()
{
    using namespace AbcTrainTheme;

    closeButton.setBounds (cardBounds().reduced (Spacing::large)
                               .removeFromBottom (34).removeFromRight (110));

    const auto contentHeight = (float) ((int) entries.size() * rowHeight);
    maxScroll = juce::jmax (0.0f, contentHeight - (float) listBounds().getHeight());
    scrollOffset = juce::jlimit (0.0f, maxScroll, scrollOffset);
}

void AchievementsScreenComponent::mouseMove (const juce::MouseEvent& event)
{
    const auto list = listBounds();
    auto found = -1;

    if (list.contains (event.getPosition()))
    {
        const auto index = (int) (((float) (event.y - list.getY()) + scrollOffset) / (float) rowHeight);

        if (index >= 0 && index < (int) entries.size())
            found = index;
    }

    if (found != hoveredRow)
    {
        hoveredRow = found;
        repaint();
    }
}

void AchievementsScreenComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredRow = -1;
    repaint();
}

void AchievementsScreenComponent::mouseWheelMove (const juce::MouseEvent&,
                                                   const juce::MouseWheelDetails& wheel)
{
    if (maxScroll <= 0.0f)
        return;

    scrollOffset = juce::jlimit (0.0f, maxScroll, scrollOffset - wheel.deltaY * 240.0f);
    repaint();
}

void AchievementsScreenComponent::timerCallback()
{
    if (! isVisible() || hoverAmounts.empty())
        return;

    const auto step = (float) (1000.0 / (double) tickHz / AbcTrainTheme::Duration::hover);
    auto needsRepaint = false;

    for (size_t i = 0; i < hoverAmounts.size(); ++i)
    {
        const auto target = ((int) i == hoveredRow) ? 1.0f : 0.0f;
        auto& value = hoverAmounts[i];

        if (std::abs (target - value) <= step)
        {
            if (! juce::approximatelyEqual (value, target))
            {
                value = target;
                needsRepaint = true;
            }

            continue;
        }

        value += target > value ? step : -step;
        needsRepaint = true;
    }

    if (needsRepaint)
        repaint();
}
