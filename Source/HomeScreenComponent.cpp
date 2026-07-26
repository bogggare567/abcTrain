#include "HomeScreenComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include <cmath>

namespace
{
    constexpr int tickHz = 60;
    constexpr int gridGap = AbcTrainTheme::Spacing::small;
    constexpr float starSize = 18.0f;
}

HomeScreenComponent::HomeScreenComponent()
{
    setOpaque (true);
    startTimerHz (tickHz);
}

HomeScreenComponent::~HomeScreenComponent()
{
    stopTimer();
}

void HomeScreenComponent::setHeader (juce::String title, juce::String subtitle)
{
    headerTitle = std::move (title);
    headerSubtitle = std::move (subtitle);
    repaint();
}

void HomeScreenComponent::setSections (std::vector<Section> newSections)
{
    sections = std::move (newSections);
    hoveredCard = -1;
    rebuildLayout();
    repaint();
}

void HomeScreenComponent::rebuildLayout()
{
    laidOutCards.clear();
    laidOutSections.clear();

    // Width comes from the component (the Viewport sets it); height is an
    // *output* of this pass, so it must not be read from getLocalBounds().
    // Deliberately tall: removeFromTop() is clamped by the rectangle's own
    // height, so starting from a 1px-high rect silently trimmed the header
    // reserve to 1px and dropped the first section on top of the title.
    auto area = juce::Rectangle<int> (0, 0, getWidth(), 1 << 20)
                    .withTrimmedLeft (AbcTrainTheme::Spacing::large)
                    .withTrimmedRight (AbcTrainTheme::Spacing::large);
    area.removeFromTop (AbcTrainTheme::Spacing::large);
    area.removeFromTop (52);   // header

    const auto columnWidth = (area.getWidth() - gridGap * (columns - 1)) / columns;
    auto y = area.getY();

    for (int s = 0; s < (int) sections.size(); ++s)
    {
        const auto& section = sections[(size_t) s];
        if (section.cards.empty())
            continue;

        laidOutSections.push_back ({ { area.getX(), y, area.getWidth(), sectionTitleHeight }, s });
        y += sectionTitleHeight;

        for (int c = 0; c < (int) section.cards.size(); ++c)
        {
            const auto column = c % columns;
            const auto row = c / columns;

            laidOutCards.push_back ({ { area.getX() + column * (columnWidth + gridGap),
                                        y + row * (cardHeight + gridGap),
                                        columnWidth, cardHeight },
                                      s, c });
        }

        const auto rows = ((int) section.cards.size() + columns - 1) / columns;
        y += rows * (cardHeight + gridGap) + AbcTrainTheme::Spacing::medium;
    }

    contentHeight = y + AbcTrainTheme::Spacing::large;
    hoverAmounts.assign (laidOutCards.size(), 0.0f);
}

void HomeScreenComponent::resized()
{
    rebuildLayout();
}

void HomeScreenComponent::timerCallback()
{
    if (! isVisible() || hoverAmounts.empty())
        return;

    const auto step = (float) (1000.0 / (double) tickHz / AbcTrainTheme::Duration::hover);
    bool needsRepaint = false;

    for (size_t i = 0; i < hoverAmounts.size(); ++i)
    {
        const auto target = ((int) i == hoveredCard) ? 1.0f : 0.0f;
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

juce::Rectangle<float> HomeScreenComponent::starBoundsFor (juce::Rectangle<float> cardBounds) const
{
    return { cardBounds.getRight() - starSize - (float) AbcTrainTheme::Spacing::small,
             cardBounds.getY() + (float) AbcTrainTheme::Spacing::small,
             starSize, starSize };
}

int HomeScreenComponent::cardAt (juce::Point<int> position) const
{
    for (int i = 0; i < (int) laidOutCards.size(); ++i)
        if (laidOutCards[(size_t) i].bounds.contains (position))
            return i;

    return -1;
}

void HomeScreenComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto index = cardAt (e.getPosition());
    const auto overStar = index >= 0
                          && starBoundsFor (laidOutCards[(size_t) index].bounds.toFloat())
                                 .expanded (4.0f).contains (e.position);

    if (index != hoveredCard || overStar != hoveredStar)
    {
        hoveredCard = index;
        hoveredStar = overStar;
        setMouseCursor (index >= 0 ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void HomeScreenComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredCard = -1;
    hoveredStar = false;
}

void HomeScreenComponent::mouseUp (const juce::MouseEvent& e)
{
    const auto index = cardAt (e.getPosition());
    if (index < 0)
        return;

    const auto& laidOut = laidOutCards[(size_t) index];
    const auto& card = sections[(size_t) laidOut.sectionIndex].cards[(size_t) laidOut.cardIndex];

    // The star is a control *inside* the card, so it has to be tested
    // before the card's own click - otherwise starring a training would
    // also start it, which is exactly the opposite of what a shortlist is
    // for.
    if (starBoundsFor (laidOut.bounds.toFloat()).expanded (4.0f).contains (e.position))
    {
        if (onFavouriteToggled != nullptr)
            onFavouriteToggled (card.gameIndex, ! card.isFavourite);
        return;
    }

    if (onGameChosen != nullptr)
        onGameChosen (card.gameIndex);
}

void HomeScreenComponent::paintCard (juce::Graphics& g, const CardInfo& card,
                                      juce::Rectangle<float> bounds, float hoverRaw)
{
    const auto& theme = AbcTrainTheme::current();
    const auto hover = AbcTrainTheme::Ease::out (hoverRaw);
    const auto lifted = bounds.translated (0.0f, -2.0f * hover);

    juce::Path shape;
    shape.addRoundedRectangle (lifted, AbcTrainTheme::Radius::panel);

    juce::DropShadow shadow (theme.shadow.withAlpha ((0.18f + 0.20f * hover) * theme.shadowStrength),
                              (int) (6.0f + 8.0f * hover), { 0, (int) (2.0f + 2.0f * hover) });
    shadow.drawForPath (g, shape);

    const auto surface = theme.widgetBackground.brighter (0.05f * hover);
    juce::ColourGradient fill (surface.brighter (0.05f), lifted.getX(), lifted.getY(),
                                surface.darker (0.04f), lifted.getX(), lifted.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (lifted, AbcTrainTheme::Radius::panel);

    g.setColour (card.isCurrent ? theme.accent.withAlpha (0.9f)
                                : theme.outline.withAlpha (0.8f + 0.2f * hover));
    g.drawRoundedRectangle (lifted, AbcTrainTheme::Radius::panel, card.isCurrent ? 1.6f : 1.0f);

    // ---- star ----
    const auto star = starBoundsFor (lifted);
    {
        juce::Path starPath;
        starPath.addStar (star.getCentre(), 5, star.getWidth() * 0.28f, star.getWidth() * 0.5f);

        if (card.isFavourite)
        {
            g.setColour (theme.accentWarm);
            g.fillPath (starPath);
        }
        else
        {
            g.setColour (theme.textDim.withAlpha (0.35f + 0.35f * hover));
            g.strokePath (starPath, juce::PathStrokeType (1.2f));
        }
    }

    auto inner = lifted.reduced ((float) AbcTrainTheme::Spacing::medium);
    inner.removeFromRight (starSize);   // keep text clear of the star

    auto headerRow = inner.removeFromTop (20.0f);
    const auto iconBox = headerRow.removeFromLeft (20.0f);
    AppIcons::draw (g, card.icon, iconBox,
                    card.isCurrent ? theme.accent : theme.textDim.brighter (0.3f * hover));

    headerRow.removeFromLeft ((float) AbcTrainTheme::Spacing::small);
    g.setColour (theme.textBright);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (card.name, headerRow, juce::Justification::centredLeft, true);

    inner.removeFromTop (4.0f);

    if (card.statsLine.isNotEmpty())
    {
        const auto statsArea = inner.removeFromBottom (13.0f);
        g.setColour (theme.textDim.withAlpha (0.8f));
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.0f));
        g.drawText (card.statsLine, statsArea, juce::Justification::centredLeft, true);
    }

    g.setColour (theme.textDim);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawFittedText (card.benefit, inner.toNearestInt(), juce::Justification::topLeft, 3);
}

void HomeScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    auto headerArea = juce::Rectangle<int> (AbcTrainTheme::Spacing::large,
                                             AbcTrainTheme::Spacing::large,
                                             juce::jmax (0, getWidth() - 2 * AbcTrainTheme::Spacing::large),
                                             46).toFloat();

    AbcTrainLookAndFeel::drawTrackedText (g, headerTitle, headerArea.removeFromTop (28.0f),
                                           AbcTrainLookAndFeel::titleFont(),
                                           theme.textBright, 1.6f,
                                           juce::Justification::centredLeft);

    g.setColour (theme.textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (headerSubtitle, headerArea, juce::Justification::centredLeft, true);

    for (const auto& laidOut : laidOutSections)
    {
        const auto& section = sections[(size_t) laidOut.sectionIndex];
        auto titleArea = laidOut.titleBounds.toFloat();

        AbcTrainLookAndFeel::drawTrackedText (g, section.title.toUpperCase(),
                                               titleArea.withTrimmedBottom (6.0f),
                                               AbcTrainLookAndFeel::captionFont(),
                                               theme.textDim.withAlpha (0.8f), 1.3f,
                                               juce::Justification::bottomLeft);

        const auto lineY = titleArea.getBottom() - 3.0f;
        g.setColour (theme.divider);
        g.drawLine (titleArea.getX(), lineY, titleArea.getRight(), lineY, 1.0f);
    }

    for (int i = 0; i < (int) laidOutCards.size(); ++i)
    {
        const auto& laidOut = laidOutCards[(size_t) i];
        const auto& card = sections[(size_t) laidOut.sectionIndex].cards[(size_t) laidOut.cardIndex];
        const auto hover = i < (int) hoverAmounts.size() ? hoverAmounts[(size_t) i] : 0.0f;

        paintCard (g, card, laidOut.bounds.toFloat(), hover);
    }
}
