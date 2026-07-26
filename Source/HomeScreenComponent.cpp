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

void HomeScreenComponent::setBadgeStripCaption (juce::String caption)
{
    badgeStripCaption = std::move (caption);
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

    auto area = getLocalBounds();

    if (area.isEmpty())
        return;

    const auto rows = (int) ((cards.size() + columns - 1) / columns);

    if (rows <= 0)
    {
        badgeStrip = area.removeFromBottom (badgeStripHeight);
        return;
    }

    const auto gap = Spacing::small;

    // The grid takes what it needs and no more; the badge strip follows it
    // immediately rather than being pinned to the bottom edge. Pinning it
    // left a 200px hole between the two whenever the window was taller
    // than the tiles required.
    const auto rowHeight = juce::jlimit (tileHeight, tileHeightCeiling,
                                          (area.getHeight() - badgeStripHeight - Spacing::medium
                                            - gap * (rows - 1)) / rows);

    for (auto row = 0; row < rows; ++row)
    {
        auto rowArea = area.removeFromTop (rowHeight);
        area.removeFromTop (gap);

        const auto columnWidth = (rowArea.getWidth() - gap * (columns - 1)) / columns;

        for (auto column = 0; column < columns; ++column)
        {
            const auto index = row * columns + column;

            if (index >= (int) cards.size())
                break;

            tileBounds.push_back (rowArea.removeFromLeft (columnWidth));
            rowArea.removeFromLeft (gap);
        }
    }

    area.removeFromTop (Spacing::medium);
    badgeStrip = area.removeFromTop (badgeStripHeight);

    const auto contentWidth = (int) badges.size() * (badgeSize + Spacing::small);
    maxBadgeScroll = juce::jmax (0.0f, (float) (contentWidth - badgeStrip.getWidth() + Spacing::large));
    badgeScroll = juce::jlimit (0.0f, maxBadgeScroll, badgeScroll);
}

void HomeScreenComponent::paint (juce::Graphics& g)
{
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
    const auto bounds = tile.toFloat().reduced (1.0f);

    juce::Path shape;
    shape.addRoundedRectangle (bounds, AbcTrainTheme::Radius::panel);

    if (eased > 0.01f)
        juce::DropShadow (theme.shadow.withAlpha (0.45f * theme.shadowStrength * eased),
                           10 + (int) (6.0f * eased), { 0, 2 }).drawForPath (g, shape);

    juce::ColourGradient fill (theme.panelBackground.brighter (0.03f + 0.05f * eased),
                                bounds.getX(), bounds.getY(),
                                theme.panelBackground, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (fill);
    g.fillPath (shape);

    g.setColour (card.isCurrent ? theme.accent.withAlpha (0.85f)
                                : theme.outline.withAlpha (0.5f + 0.4f * eased));
    g.strokePath (shape, juce::PathStrokeType (card.isCurrent ? 1.6f : 1.0f));

    auto inner = bounds.reduced ((float) AbcTrainTheme::Spacing::small);

    // --- the coloured badge, and the level beside it ---------------------
    auto topRow = inner.removeFromTop (32.0f);
    const auto badge = topRow.removeFromLeft (32.0f);

    // Monochrome. The family colour is still carried by the thin progress
    // line at the bottom of the tile, where it is a hint; nine saturated
    // squares made the catalogue louder than anything on it.
    AppIcons::drawBadged (g, card.icon, badge, theme.text, 0.75f + 0.25f * eased);

    // The level, with the word "Level" over it.
    //
    // A bare "1" in the corner answered no question anybody was asking -
    // one what? A small caption above the number costs 11px and removes
    // the guess entirely.
    {
        const auto pending = card.promotionPending;
        auto levelBox = topRow.removeFromRight (58.0f);

        AbcTrainLookAndFeel::drawTrackedText (g, levelCaption.toUpperCase(),
                                               levelBox.removeFromTop (11.0f),
                                               AbcTrainLookAndFeel::captionFont(),
                                               theme.textDim.withAlpha (0.75f), 1.4f,
                                               juce::Justification::centredRight);

        g.setColour (pending ? theme.positive : theme.textBright);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (19.0f));
        g.drawText (juce::String (card.level), levelBox.removeFromTop (21.0f).toNearestInt(),
                     juce::Justification::centredRight, false);
    }

    inner.removeFromTop (4.0f);

    auto nameArea = inner.removeFromTop (30.0f);
    g.setColour (theme.textBright);
    g.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
    g.drawFittedText (card.name, nameArea.toNearestInt(), juce::Justification::topLeft, 2, 0.85f);

    if (card.statsLine.isNotEmpty())
    {
        g.setColour (theme.textDim.withAlpha (0.8f));
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (11.0f));
        g.drawText (card.statsLine, inner.removeFromTop (14.0f).toNearestInt(),
                     juce::Justification::centredLeft, false);
    }

    // --- the bottom line: either level progress, or the live test -------
    auto track = bounds.reduced ((float) AbcTrainTheme::Spacing::small)
                     .removeFromBottom (4.0f);

    g.setColour (theme.displayBackground.withAlpha (0.8f));
    g.fillRoundedRectangle (track, 2.0f);

    if (card.promotionPending)
    {
        // A promotion is live: the bar stops reporting distance and starts
        // reporting the test, in discrete pips, because "3 of 5 in a row"
        // is a countable thing and a smooth bar would hide that.
        const auto pips = juce::jmax (1, card.promotionTestLength);
        const auto pipWidth = (track.getWidth() - 2.0f * (float) (pips - 1)) / (float) pips;

        for (auto i = 0; i < pips; ++i)
        {
            const auto pip = track.withWidth (pipWidth)
                                  .withX (track.getX() + (float) i * (pipWidth + 2.0f));

            g.setColour (i < card.promotionStreak ? theme.positive
                                                  : theme.positive.withAlpha (0.22f));
            g.fillRoundedRectangle (pip, 2.0f);
        }
    }
    else if (card.levelProgress > 0.001f)
    {
        g.setColour (card.accent.withAlpha (0.8f));
        g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * card.levelProgress)),
                                 2.0f);
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

    auto area = badgeStrip;

    AbcTrainLookAndFeel::drawTrackedText (g, badgeStripCaption.toUpperCase(),
                                           area.removeFromTop (16).toFloat(),
                                           AbcTrainLookAndFeel::captionFont(),
                                           theme.textDim, 1.6f);

    area.removeFromTop (AbcTrainTheme::Spacing::tight);

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
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
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
    return juce::Rectangle<int> (22, 22)
               .withPosition (tile.getX() + AbcTrainTheme::Spacing::small + 36,
                               tile.getY() + AbcTrainTheme::Spacing::small + 5);
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
