#include "HomeScreenComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"

namespace
{
    constexpr int tickHz = 60;
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
    // Starred first, then registration order. Sorting here rather than in
    // the editor keeps "what the player cares about" a property of the
    // view: nothing about the exercises themselves changes.
    std::stable_sort (newCards.begin(), newCards.end(),
                       [] (const CardInfo& a, const CardInfo& b)
                       {
                           return a.isFavourite && ! b.isFavourite;
                       });

    cards = std::move (newCards);
    hoverAmounts.assign (cards.size(), 0.0f);
    hoveredTile = -1;
    rebuildLayout();
    repaint();
}

void HomeScreenComponent::setBadges (std::vector<BadgeInfo> newBadges)
{
    // Earned first: the strip is a shelf, and a shelf shows what is on it
    // before it shows the gaps.
    std::stable_sort (newBadges.begin(), newBadges.end(),
                       [] (const BadgeInfo& a, const BadgeInfo& b)
                       {
                           if (a.earned != b.earned)
                               return a.earned;

                           return a.progress > b.progress;
                       });

    badges = std::move (newBadges);
    hoveredBadge = -1;
    rebuildLayout();
    repaint();
}

void HomeScreenComponent::setLevelCaption (juce::String caption)
{
    levelCaption = std::move (caption);
    repaint();
}

void HomeScreenComponent::setBadgeStripCaption (juce::String caption, juce::String count,
                                                 juce::String action)
{
    badgeStripCaption = std::move (caption);
    badgeStripCount = std::move (count);
    badgeStripAction = std::move (action);
    repaint();
}

void HomeScreenComponent::resized()
{
    rebuildLayout();
}

void HomeScreenComponent::rebuildLayout()
{
    using namespace AbcTrainTheme;

    tileBounds.clear();
    sections.clear();

    auto area = getLocalBounds();

    if (area.isEmpty() || cards.empty())
    {
        if (! area.isEmpty())
            badgeStrip = area.removeFromBottom (badgeStripHeight);

        return;
    }

    tileBounds.resize (cards.size());

    // --- who belongs with whom -------------------------------------------
    //
    // A new section starts wherever the family changes, which is why the
    // editor has to hand these over already grouped. Cards with no family
    // at all still lay out - they just get one nameless section, which is
    // what the screen looked like before this.
    for (size_t i = 0; i < cards.size(); ++i)
    {
        if (sections.empty() || cards[i].sectionTitle != sections.back().title)
        {
            Section section;
            section.title = cards[i].sectionTitle;
            section.subtitle = cards[i].sectionSubtitle;
            section.accent = cards[i].accent;
            section.firstCard = (int) i;
            section.numCards = 0;
            sections.push_back (std::move (section));
        }

        ++sections.back().numCards;
    }

    const auto gap = Spacing::small;

    // One card width for the whole page. It is sized so that *two
    // two-exercise families fit on one row with the between-families gap
    // between them* - not just so four cards fit, which is 12px wider and
    // was enough to push Dynamics onto its own row and the last family off
    // the bottom of the window. A four-card family then leaves that 12px
    // spare at the right, which is the right way round: cards lining up
    // column-for-column between families matters more than the last one
    // reaching the margin.
    const auto cardWidth = (area.getWidth() - sectionGap - gap * (columns - 2)) / columns;
    const auto headerBlock = sectionHeaderHeight + gap;

    auto rowY = area.getY();
    auto rowX = area.getX();
    auto rowHeight = 0;

    for (auto& section : sections)
    {
        const auto across = juce::jmin (section.numCards, columns);
        const auto rows = (section.numCards + columns - 1) / columns;
        const auto blockWidth = across * cardWidth + (across - 1) * gap;
        const auto blockHeight = headerBlock + rows * tileHeight + (rows - 1) * gap;

        // Does it fit beside what is already on this row?
        if (rowX > area.getX() && rowX + sectionGap + blockWidth > area.getRight())
        {
            rowY += rowHeight + sectionGap;
            rowX = area.getX();
            rowHeight = 0;
        }
        else if (rowX > area.getX())
        {
            rowX += sectionGap;
        }

        section.header = { rowX, rowY, blockWidth, sectionHeaderHeight };

        for (int i = 0; i < section.numCards; ++i)
        {
            const auto row = i / columns;
            const auto column = i % columns;

            tileBounds[(size_t) (section.firstCard + i)] =
                juce::Rectangle<int> (rowX + column * (cardWidth + gap),
                                       rowY + headerBlock + row * (tileHeight + gap),
                                       cardWidth, tileHeight);
        }

        rowX += blockWidth;
        rowHeight = juce::jmax (rowHeight, blockHeight);
    }

    // Achievements are laid out as one more block in the same grid, which
    // is what lets them sit *beside* a one-exercise family instead of
    // under a full row of nothing. It only starts a new row when there is
    // no useful width left on this one - two badges wide is not a shelf.
    {
        const auto minimumWidth = cardWidth * 2 + gap;
        auto blockX = rowX;
        auto blockY = rowY;

        if (rowX == area.getX() || rowX + sectionGap + minimumWidth > area.getRight())
        {
            blockY = rowX == area.getX() ? rowY : rowY + rowHeight + sectionGap;
            blockX = area.getX();
        }
        else
        {
            blockX = rowX + sectionGap;
        }

        badgeStripHeader = { blockX, blockY, area.getRight() - blockX, sectionHeaderHeight };
        badgeStrip = { blockX, blockY + sectionHeaderHeight + gap,
                        area.getRight() - blockX, tileHeight };
    }

    const auto contentWidth = (int) badges.size() * (badgeSize + Spacing::small);
    maxBadgeScroll = juce::jmax (0.0f, (float) (contentWidth - badgeStrip.getWidth() + Spacing::large));
    badgeScroll = juce::jlimit (0.0f, maxBadgeScroll, badgeScroll);
}

void HomeScreenComponent::paintSectionHeader (juce::Graphics& g, const Section& section)
{
    if (section.title.isEmpty())
        return;

    const auto& theme = AbcTrainTheme::current();
    auto area = section.header.toFloat();

    // The family's colour, as a filled square and nothing else. It is the
    // only saturated mark in the heading, so it reads as a key to the
    // outlines below it rather than as decoration.
    {
        const auto mark = area.removeFromLeft (9.0f).withSizeKeepingCentre (9.0f, 9.0f);
        g.setColour (section.accent);
        g.fillRect (mark);
        area.removeFromLeft (10.0f);
    }

    const auto titleFont = AbcTrainLookAndFeel::headingFont();
    const auto titleText = AbcTrainLookAndFeel::toCaps (section.title);
    const auto titleWidth = AbcTrainLookAndFeel::trackedTextWidth (titleText, titleFont, 2.7f);

    AbcTrainLookAndFeel::drawTrackedText (g, titleText, area.removeFromLeft (titleWidth),
                                           titleFont, theme.text, 2.7f,
                                           juce::Justification::centredLeft);

    if (section.subtitle.isNotEmpty())
    {
        area.removeFromLeft (10.0f);
        const auto subFont = AbcTrainLookAndFeel::microFont();
        const auto subText = AbcTrainLookAndFeel::toCaps (section.subtitle);

        AbcTrainLookAndFeel::drawTrackedText (
            g, subText,
            area.removeFromLeft (AbcTrainLookAndFeel::trackedTextWidth (subText, subFont, 1.68f)),
            subFont, theme.textDim, 1.68f, juce::Justification::centredLeft);
    }
}

void HomeScreenComponent::paint (juce::Graphics& g)
{
    for (const auto& section : sections)
        paintSectionHeader (g, section);

    for (size_t i = 0; i < tileBounds.size() && i < cards.size(); ++i)
        paintTile (g, cards[i], tileBounds[i],
                    i < hoverAmounts.size() ? hoverAmounts[i] : 0.0f);

    paintBadgeStrip (g);
    paintHoverTip (g);
}

void HomeScreenComponent::paintTile (juce::Graphics& g, const CardInfo& card,
                                      juce::Rectangle<int> tile, float hover)
{
    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (hover);
    const auto bounds = tile.toFloat().reduced (0.5f);

    // A card is a *drawn frame*, not a filled block, and only the one you
    // are on is filled at all. Nine filled panels shout equally, which is
    // the same as nine panels saying nothing; an outline that fills when
    // it is yours says which one is yours without a second colour, a
    // shadow or a size change.
    const auto frameColour = card.isCurrent ? card.accent
                                            : theme.outline.interpolatedWith (card.accent, 0.35f * eased);

    if (card.isCurrent)
    {
        g.setColour (card.accent.withAlpha (0.06f));
        g.fillRect (bounds);
    }
    else if (eased > 0.01f)
    {
        // Hover is the same gesture at a fraction of the strength, so
        // pointing at a card previews what selecting it will look like.
        g.setColour (card.accent.withAlpha (0.03f * eased));
        g.fillRect (bounds);
    }

    g.setColour (frameColour);
    g.drawRect (bounds, 1.0f);

    if (card.isCurrent || eased > 0.5f)
        AbcTrainLookAndFeel::drawRegistrationMarks (g, bounds,
                                                     frameColour.withAlpha (card.isCurrent ? 1.0f : eased));

    auto inner = bounds.reduced (14.0f, 13.0f);

    // --- the icon, and the level beside it -------------------------------
    auto topRow = inner.removeFromTop (24.0f);
    const auto badge = topRow.removeFromLeft (24.0f);

    // Monochrome. The family colour is carried by the frame and by the
    // segmented line at the foot of the card; nine saturated squares made
    // the catalogue louder than anything on it. The card's own eased hover
    // drives the glyph, so pointing at "Guess the Compression" makes its
    // chevrons close on the line - a caption tells you what an exercise is
    // called, this tells you what it is.
    AppIcons::drawBadged (g, card.icon, badge, theme.text, 0.75f + 0.25f * eased, eased);

    {
        const auto pending = card.promotionPending;
        auto levelBox = topRow.removeFromRight (66.0f);

        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (levelCaption),
                                               levelBox.removeFromTop (11.0f),
                                               AbcTrainLookAndFeel::microFont(),
                                               theme.textDim.withAlpha (0.8f), 1.68f,
                                               juce::Justification::centredRight);

        // Condensed rather than mono here. The mono face was carrying
        // "these digits must not jitter", which is true of a meter
        // updating sixty times a second and false of a number that
        // changes about once a week.
        g.setColour (pending ? theme.positive : theme.textBright);
        g.setFont (AbcTrainLookAndFeel::titleFont());
        g.drawText (juce::String (card.level), levelBox.removeFromTop (22.0f).toNearestInt(),
                     juce::Justification::centredRight, false);
    }

    inner.removeFromTop (7.0f);

    {
        auto nameArea = inner.removeFromTop (card.englishName.isEmpty() ? 34.0f : 22.0f);
        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::headingFont().withHeight (
                       AbcTrainLookAndFeel::headingFontHeight * AbcTrainLookAndFeel::getTextScale() * 1.16f));
        g.drawFittedText (card.name, nameArea.toNearestInt(), juce::Justification::topLeft,
                           card.englishName.isEmpty() ? 2 : 1, 0.9f);

        if (card.englishName.isNotEmpty())
            AbcTrainLookAndFeel::drawTrackedText (
                g, AbcTrainLookAndFeel::toCaps (card.englishName),
                inner.removeFromTop (16.0f), AbcTrainLookAndFeel::microFont(),
                theme.textDim.withAlpha (0.85f), 1.1f, juce::Justification::centredLeft);
    }

    if (card.statsLine.isNotEmpty())
    {
        g.setColour (theme.textDim.withAlpha (0.9f));
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText (card.statsLine, inner.removeFromTop (15.0f).toNearestInt(),
                     juce::Justification::centredLeft, false);
    }

    // --- the foot of the card: level progress, or the live promotion ----
    //
    // Segments, not a smooth fill. "Seven of ten" is a countable claim and
    // a continuous bar makes it unreadable - which matters most here,
    // because the number the bar is about is written directly above it.
    auto track = bounds.reduced (14.0f, 13.0f).removeFromBottom (3.0f);

    if (card.promotionPending)
    {
        const auto pips = juce::jmax (1, card.promotionTestLength);
        AbcTrainLookAndFeel::drawSegmentedBar (g, track, pips,
                                                (float) card.promotionStreak / (float) pips,
                                                theme.positive, theme.positive.withAlpha (0.22f));
    }
    else
    {
        AbcTrainLookAndFeel::drawSegmentedBar (g, track, 10, card.levelProgress,
                                                card.accent, theme.divider);
    }

    // --- star -------------------------------------------------------------
    // Always there now, rather than appearing on hover. Hiding it made it
    // undiscoverable: a control you cannot see is a control nobody knows
    // they have, and "pick what interests you" only works if the picking
    // is visible. Unset it is a quiet outline; hovering the tile brightens
    // it, and hovering the star itself brightens it further.
    {
        const auto star = starHitBox (tile).toFloat().reduced (3.0f);
        juce::Path starPath;
        starPath.addStar (star.getCentre(), 5, star.getWidth() * 0.24f, star.getWidth() * 0.48f);

        if (card.isFavourite)
        {
            g.setColour (theme.accentWarm);
            g.fillPath (starPath);
        }
        else
        {
            const auto lit = hoveredStar && hoveredTile >= 0
                                 && cards[(size_t) hoveredTile].gameIndex == card.gameIndex;

            g.setColour (theme.textDim.withAlpha (lit ? 0.95f : 0.3f + 0.35f * eased));
            g.strokePath (starPath, juce::PathStrokeType (1.4f));
        }
    }
}

void HomeScreenComponent::paintBadgeStrip (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    if (badgeStrip.isEmpty())
        return;

    // The heading, in the same shape as a family's - because on this page
    // that is what it is: another group of things, sitting beside the last
    // family rather than under a full row of nothing.
    {
        auto header = badgeStripHeader.toFloat();
        const auto font = AbcTrainLookAndFeel::headingFont();
        const auto caption = AbcTrainLookAndFeel::toCaps (badgeStripCaption);

        AbcTrainLookAndFeel::drawTrackedText (
            g, caption,
            header.removeFromLeft (AbcTrainLookAndFeel::trackedTextWidth (caption, font, 2.7f)),
            font, theme.text, 2.7f, juce::Justification::centredLeft);

        if (badgeStripCount.isNotEmpty())
        {
            header.removeFromLeft (12.0f);
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::labelFont());
            g.drawText (badgeStripCount, header.toNearestInt(),
                         juce::Justification::centredLeft, false);
        }

        if (badgeStripAction.isNotEmpty())
        {
            const auto actionFont = AbcTrainLookAndFeel::microFont();
            const auto action = AbcTrainLookAndFeel::toCaps (badgeStripAction);

            AbcTrainLookAndFeel::drawTrackedText (
                g, action,
                header.removeFromRight (
                    AbcTrainLookAndFeel::trackedTextWidth (action, actionFont, 1.2f) + 2.0f),
                actionFont, theme.accent, 1.2f, juce::Justification::centredRight);
        }
    }

    auto area = badgeStrip;

    // Clipped so a scrolled strip can't paint over the tiles above it.
    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (area);

    auto x = (float) area.getX() - badgeScroll;

    for (size_t i = 0; i < badges.size(); ++i)
    {
        const auto& badge = badges[i];
        const auto box = juce::Rectangle<float> (x, (float) area.getY(),
                                                  (float) badgeSize, (float) badgeSize);
        x += (float) (badgeSize + AbcTrainTheme::Spacing::small);

        if (box.getRight() < (float) area.getX() || box.getX() > (float) area.getRight())
            continue;

        const auto lit = badge.earned;
        const auto tint = badge.tint;

        // The plate sits *inside* the progress ring with a clear gap.
        // Drawing the arc over the badge - which is what the first version
        // did - left a stroke cutting across the glyph, and two things
        // overlapping is read as one broken thing.
        const auto ring = box.reduced (1.0f);
        const auto plate = box.reduced (7.0f);

        if (! lit && badge.progress > 0.005f)
        {
            juce::Path arc;
            arc.addCentredArc (ring.getCentreX(), ring.getCentreY(),
                                ring.getWidth() * 0.5f, ring.getHeight() * 0.5f, 0.0f,
                                0.0f, juce::MathConstants<float>::twoPi * badge.progress, true);
            g.setColour (tint.withAlpha (0.75f));
            g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
        else if (lit)
        {
            // Earned ones get the full ring, closed - the shape itself
            // says "complete" before the colour does.
            g.setColour (tint.withAlpha (0.9f));
            g.drawEllipse (ring, 2.2f);
        }

        AppIcons::drawBadged (g, badge.icon, plate, tint, lit ? 1.0f : 0.22f);
    }
}

void HomeScreenComponent::paintHoverTip (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    juce::String text;
    juce::Rectangle<int> anchor;

    if (hoveredBadge >= 0 && hoveredBadge < (int) badges.size())
    {
        const auto& badge = badges[(size_t) hoveredBadge];
        text = badge.name + " — " + badge.description;
        anchor = badgeStrip;
    }
    else if (hoveredTile >= 0 && hoveredTile < (int) cards.size()
             && hoveredTile < (int) tileBounds.size())
    {
        text = cards[(size_t) hoveredTile].benefit;
        anchor = tileBounds[(size_t) hoveredTile];
    }

    if (text.isEmpty())
        return;

    // Above the thing being described where there is room, below it where
    // there isn't - a tip that covers its own subject explains nothing.
    const auto width = juce::jmin (getWidth() - AbcTrainTheme::Spacing::large * 2, 320);
    const auto height = 46;

    auto box = juce::Rectangle<int> (width, height)
                   .withCentre ({ anchor.getCentreX(), anchor.getY() - height / 2 - 4 });

    if (box.getY() < 0)
        box.setY (anchor.getBottom() + 4);

    box = box.constrainedWithin (getLocalBounds());

    juce::Path card;
    card.addRoundedRectangle (box.toFloat(), AbcTrainTheme::Radius::panel);

    juce::DropShadow (theme.shadow.withAlpha (0.55f * theme.shadowStrength), 14, { 0, 3 })
        .drawForPath (g, card);

    g.setColour (theme.panelBackground.brighter (0.08f));
    g.fillPath (card);
    g.setColour (theme.outline);
    g.strokePath (card, juce::PathStrokeType (1.0f));

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    g.drawFittedText (text, box.reduced (AbcTrainTheme::Spacing::small),
                       juce::Justification::centred, 3, 0.9f);
}

juce::Rectangle<int> HomeScreenComponent::starHitBox (juce::Rectangle<int> tile) const
{
    // Beside the icon, not in the top-right corner: the corner is where
    // the level lives, and a 20px star sitting on top of a 19px number is
    // two things fighting for one space. Here it reads as a property of
    // the exercise - the thing the icon already names - and there is
    // nothing behind it to cover.
    // Derived from paintTile's own padding (14 / 13) and its 24px icon,
    // not from a second set of numbers that has to be kept in step by
    // hand - which is exactly how the star ended up five pixels off the
    // icon it sits beside the last time the card was re-laid-out.
    return juce::Rectangle<int> (22, 22)
               .withPosition (tile.getX() + 14 + 24 + 8,
                               tile.getY() + 13 + 1);
}

int HomeScreenComponent::tileIndexAt (juce::Point<int> position) const
{
    for (size_t i = 0; i < tileBounds.size(); ++i)
        if (tileBounds[i].contains (position))
            return (int) i;

    return -1;
}

int HomeScreenComponent::badgeIndexAt (juce::Point<int> position) const
{
    if (! badgeStrip.contains (position))
        return -1;

    const auto stride = badgeSize + AbcTrainTheme::Spacing::small;
    const auto offset = (float) (position.x - badgeStrip.getX()) + badgeScroll;

    if (offset < 0.0f)
        return -1;

    const auto index = (int) (offset / (float) stride);

    return index < (int) badges.size() ? index : -1;
}

void HomeScreenComponent::mouseMove (const juce::MouseEvent& event)
{
    const auto tile = tileIndexAt (event.getPosition());
    const auto badge = badgeIndexAt (event.getPosition());
    const auto star = tile >= 0 && tile < (int) tileBounds.size()
                          && starHitBox (tileBounds[(size_t) tile]).contains (event.getPosition());

    if (tile == hoveredTile && badge == hoveredBadge && star == hoveredStar)
        return;

    hoveredTile = tile;
    hoveredBadge = badge;
    hoveredStar = star;
    repaint();
}

void HomeScreenComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredTile = -1;
    hoveredBadge = -1;
    hoveredStar = false;
    repaint();
}

void HomeScreenComponent::mouseUp (const juce::MouseEvent& event)
{
    if (badgeStrip.contains (event.getPosition()))
    {
        if (onBadgeStripClicked != nullptr)
            onBadgeStripClicked();

        return;
    }

    const auto tile = tileIndexAt (event.getPosition());

    if (tile < 0 || tile >= (int) cards.size())
        return;

    const auto& card = cards[(size_t) tile];

    // The star is inside the tile, so it has to be tested first or the
    // tile would swallow every click meant for it.
    if (starHitBox (tileBounds[(size_t) tile]).contains (event.getPosition()))
    {
        if (onFavouriteToggled != nullptr)
            onFavouriteToggled (card.gameIndex, ! card.isFavourite);

        return;
    }

    if (onGameChosen != nullptr)
        onGameChosen (card.gameIndex);
}

void HomeScreenComponent::mouseWheelMove (const juce::MouseEvent& event,
                                           const juce::MouseWheelDetails& wheel)
{
    if (maxBadgeScroll <= 0.0f || ! badgeStrip.contains (event.getPosition()))
        return;

    // Either axis scrolls the strip: a mouse with only a vertical wheel
    // must still be able to reach the far end of it.
    const auto delta = (std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY);

    badgeScroll = juce::jlimit (0.0f, maxBadgeScroll, badgeScroll - delta * 220.0f);
    repaint();
}

void HomeScreenComponent::timerCallback()
{
    if (! isVisible() || hoverAmounts.empty())
        return;

    const auto step = (float) (1000.0 / (double) tickHz / AbcTrainTheme::Duration::hover);
    auto needsRepaint = false;

    for (size_t i = 0; i < hoverAmounts.size(); ++i)
    {
        const auto target = ((int) i == hoveredTile) ? 1.0f : 0.0f;
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
