#include "HomeScreenComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"

namespace
{
    constexpr int tickHz = 60;
    constexpr int maxLevel = 10;
}

HomeScreenComponent::HomeScreenComponent()
{
    setInterceptsMouseClicks (true, false);
    startTimerHz (tickHz);
}

HomeScreenComponent::~HomeScreenComponent()
{
    stopTimer();
}

void HomeScreenComponent::setCards (std::vector<CardInfo> newCards)
{
    // Starred first, then the order the editor grouped them in.
    std::stable_sort (newCards.begin(), newCards.end(),
                       [] (const CardInfo& a, const CardInfo& b)
                       {
                           return a.isFavourite && ! b.isFavourite;
                       });

    cards = std::move (newCards);
    hoverAmounts.assign (cards.size(), 0.0f);
    hoveredRow = -1;
    rebuildLayout();
    repaint();
}

void HomeScreenComponent::resized()
{
    rebuildLayout();
}

void HomeScreenComponent::rebuildLayout()
{
    sections.clear();
    rowBounds.assign (cards.size(), {});

    const auto area = getLocalBounds();
    if (area.isEmpty() || cards.empty())
        return;

    // Count the sections first: the row height is whatever lets every row
    // fit without scrolling, clamped so a tall window does not turn nine
    // rows into nine slabs and a short one does not crush them.
    int numSections = 0;
    for (size_t i = 0; i < cards.size(); ++i)
        if (i == 0 || cards[i].sectionTitle != cards[i - 1].sectionTitle)
            ++numSections;

    const auto chrome = numSections * headerHeight + (numSections - 1) * sectionGap;
    const auto rowHeight = juce::jlimit (minRowHeight, maxRowHeight,
                                         (area.getHeight() - chrome) / juce::jmax (1, (int) cards.size()));

    auto y = area.getY();

    for (size_t i = 0; i < cards.size(); ++i)
    {
        if (i == 0 || cards[i].sectionTitle != cards[i - 1].sectionTitle)
        {
            if (i > 0)
                y += sectionGap;

            Section section;
            section.title = cards[i].sectionTitle;
            section.subtitle = cards[i].sectionSubtitle;
            section.count = cards[i].sectionCount;
            section.accent = cards[i].accent;
            section.header = { area.getX(), y, area.getWidth(), headerHeight };
            sections.push_back (std::move (section));
            y += headerHeight;
        }

        rowBounds[i] = { area.getX(), y, area.getWidth(), rowHeight };
        y += rowHeight;
    }
}

void HomeScreenComponent::paintSectionHeader (juce::Graphics& g, const Section& section)
{
    if (section.title.isEmpty())
        return;

    const auto& theme = AbcTrainTheme::current();
    auto area = section.header.toFloat().withTrimmedLeft (12.0f);

    {
        const auto mark = area.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 10.0f);
        g.setColour (section.accent);
        g.fillRect (mark);
        area.removeFromLeft (12.0f);
    }

    if (section.count.isNotEmpty())
    {
        const auto countFont = AbcTrainLookAndFeel::labelFont();
        AbcTrainLookAndFeel::drawTrackedText (
            g, section.count,
            area.removeFromRight (AbcTrainLookAndFeel::trackedTextWidth (section.count, countFont, 0.0f) + 12.0f),
            countFont, theme.textDim, 0.0f, juce::Justification::centredLeft);
    }

    const auto titleFont = AbcTrainLookAndFeel::headingFont();
    const auto titleText = AbcTrainLookAndFeel::toCaps (section.title);
    const auto titleWidth = AbcTrainLookAndFeel::trackedTextWidth (titleText, titleFont, 2.7f);

    AbcTrainLookAndFeel::drawTrackedText (g, titleText, area.removeFromLeft (titleWidth),
                                           titleFont, theme.text, 2.7f,
                                           juce::Justification::centredLeft);

    if (section.subtitle.isNotEmpty())
    {
        area.removeFromLeft (12.0f);
        const auto subFont = AbcTrainLookAndFeel::microFont();
        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (section.subtitle), area,
                                               subFont, theme.textDim, 1.68f,
                                               juce::Justification::centredLeft);
    }

    // The hairline under the heading - the only rule on this screen, so
    // the families read as four blocks rather than one long list.
    g.setColour (theme.divider);
    g.fillRect (section.header.toFloat().removeFromBottom (1.0f));
}

void HomeScreenComponent::paintRuler (juce::Graphics& g, const CardInfo& card, juce::Rectangle<float> track)
{
    const auto& theme = AbcTrainTheme::current();

    // Ten segments, because there are ten steps and "seven of ten" is a
    // countable claim. The fill is the record, which never drops.
    AbcTrainLookAndFeel::drawSegmentedBar (g, track, maxLevel,
                                            (float) card.bestLevel / (float) maxLevel,
                                            card.accent, theme.displayBackground, 2.0f);

    const auto xForLevel = [&] (int level)
    {
        return track.getX() + track.getWidth() * (float) juce::jlimit (0, maxLevel, level) / (float) maxLevel;
    };

    // The next rung of the ladder, dashed: the distance to the next name
    // is a length you can see, which is the goal-gradient doing its work.
    const auto rung = juce::jlimit (1, 5, (juce::jlimit (1, maxLevel, card.bestLevel) + 1) / 2);
    if (rung < 5)
    {
        const auto x = xForLevel (2 * rung + 1);
        g.setColour (theme.accentWarm);
        for (auto yy = track.getY() - 5.0f; yy < track.getBottom() + 5.0f; yy += 4.0f)
            g.fillRect (x - 1.0f, yy, 2.0f, 2.0f);
    }

    // Where the staircase stands today, as a white tick. Only drawn when
    // it is below the record - at the record the fill already says it.
    if (card.level < card.bestLevel)
    {
        const auto x = xForLevel (card.level);
        g.setColour (theme.textBright);
        g.fillRect (x - 1.5f, track.getY() - 4.0f, 3.0f, track.getHeight() + 8.0f);
    }
}

void HomeScreenComponent::paintRow (juce::Graphics& g, const CardInfo& card,
                                     juce::Rectangle<int> bounds, float hover)
{
    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (hover);
    auto row = bounds.toFloat();

    // The exercise you are on is the one drawn frame on the page, with the
    // marks - every other row is quiet.
    if (card.isCurrent)
    {
        g.setColour (card.accent.withAlpha (0.07f));
        g.fillRect (row.reduced (0.0f, 2.0f));
        g.setColour (card.accent.withAlpha (0.8f));
        g.drawRect (row.reduced (0.0f, 2.0f), 1.0f);
        AbcTrainLookAndFeel::drawRegistrationMarks (g, row.reduced (0.0f, 2.0f), card.accent, 3.0f, 2.5f);
    }
    else if (eased > 0.001f)
    {
        g.setColour (theme.widgetBackground.withAlpha (0.55f * eased));
        g.fillRect (row.reduced (0.0f, 2.0f));
    }

    auto inner = row.reduced (12.0f, 0.0f);

    // --- star ---
    {
        const auto star = inner.removeFromLeft (20.0f).withSizeKeepingCentre (18.0f, 18.0f);
        const auto lit = card.isFavourite || (hoveredRow >= 0 && hoveredStar
                                              && rowBounds[(size_t) hoveredRow] == bounds);
        AppIcons::draw (g, AppIcons::Icon::award, star,
                        card.isFavourite ? theme.accentWarm
                                         : theme.textDim.withAlpha (lit ? 0.9f : 0.35f));
        inner.removeFromLeft (10.0f);
    }

    // --- icon ---
    {
        const auto icon = inner.removeFromLeft (28.0f).withSizeKeepingCentre (26.0f, 26.0f);
        AppIcons::draw (g, card.icon, icon, card.isCurrent ? card.accent : theme.text);
        inner.removeFromLeft (14.0f);
    }

    // --- the number, on the right, as on a meter ---
    auto right = inner.removeFromRight (juce::jlimit (170.0f, 230.0f, inner.getWidth() * 0.2f));
    {
        const auto bothLines = right.getHeight() >= 44.0f;
        auto numberArea = bothLines ? right.withTrimmedBottom (right.getHeight() * 0.44f) : right;

        g.setColour (theme.textBright);
        g.setFont (card.levelIsNumber
                       ? AbcTrainLookAndFeel::monoFont().withHeight (18.0f * AbcTrainLookAndFeel::getTextScale())
                       : AbcTrainLookAndFeel::titleFont());
        g.drawFittedText (card.levelText, numberArea.toNearestInt(),
                           bothLines ? juce::Justification::bottomRight : juce::Justification::centredRight,
                           1, 0.75f);

        if (bothLines)
        {
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            g.drawFittedText (card.rankText,
                               right.withTop (numberArea.getBottom() + 2.0f).toNearestInt(),
                               juce::Justification::topRight, 1, 0.85f);
        }
    }

    inner.removeFromRight (20.0f);

    // --- name and its record in words ---
    auto nameColumn = inner.removeFromLeft (juce::jlimit (210.0f, 320.0f, inner.getWidth() * 0.38f));
    {
        const auto twoLines = nameColumn.getHeight() >= 44.0f;
        auto top = twoLines ? nameColumn.withTrimmedBottom (nameColumn.getHeight() * 0.44f) : nameColumn;

        g.setColour (card.isCurrent || eased > 0.5f ? theme.textBright : theme.text);
        g.setFont (AbcTrainLookAndFeel::titleFont());
        g.drawFittedText (card.name, top.toNearestInt(),
                           twoLines ? juce::Justification::bottomLeft : juce::Justification::centredLeft,
                           1, 0.8f);

        if (twoLines)
        {
            auto second = card.statsLine;
            if (card.englishName.isNotEmpty())
                second = card.englishName + "  ·  " + second;

            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            g.drawFittedText (second, nameColumn.withTop (top.getBottom() + 2.0f).toNearestInt(),
                               juce::Justification::topLeft, 1, 0.85f);
        }
    }

    inner.removeFromLeft (16.0f);

    // --- the ruler ---
    paintRuler (g, card, inner.withSizeKeepingCentre (inner.getWidth(), 10.0f));
}

void HomeScreenComponent::paint (juce::Graphics& g)
{
    for (const auto& section : sections)
        paintSectionHeader (g, section);

    for (size_t i = 0; i < rowBounds.size() && i < cards.size(); ++i)
        paintRow (g, cards[i], rowBounds[i], i < hoverAmounts.size() ? hoverAmounts[i] : 0.0f);
}

juce::Rectangle<int> HomeScreenComponent::starHitBox (juce::Rectangle<int> row) const
{
    return row.withTrimmedLeft (4).withWidth (36);
}

int HomeScreenComponent::rowIndexAt (juce::Point<int> p) const
{
    for (size_t i = 0; i < rowBounds.size(); ++i)
        if (rowBounds[i].contains (p))
            return (int) i;

    return -1;
}

void HomeScreenComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto index = rowIndexAt (e.getPosition());
    const auto onStar = index >= 0 && starHitBox (rowBounds[(size_t) index]).contains (e.getPosition());

    if (index != hoveredRow || onStar != hoveredStar)
    {
        hoveredRow = index;
        hoveredStar = onStar;
        setMouseCursor (index >= 0 ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void HomeScreenComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredRow = -1;
    hoveredStar = false;
    repaint();
}

void HomeScreenComponent::mouseUp (const juce::MouseEvent& e)
{
    const auto index = rowIndexAt (e.getPosition());
    if (index < 0 || ! e.mouseWasClicked())
        return;

    const auto& card = cards[(size_t) index];

    if (starHitBox (rowBounds[(size_t) index]).contains (e.getPosition()))
    {
        if (onFavouriteToggled != nullptr)
            onFavouriteToggled (card.gameIndex, ! card.isFavourite);
        return;
    }

    if (onGameChosen != nullptr)
        onGameChosen (card.gameIndex);
}

void HomeScreenComponent::timerCallback()
{
    // Eased hover per row; stops asking for repaints once everything has
    // settled, so an idle home screen costs nothing.
    auto moving = false;

    for (size_t i = 0; i < hoverAmounts.size(); ++i)
    {
        const auto target = (int) i == hoveredRow ? 1.0f : 0.0f;
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
