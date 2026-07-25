#include "GamePickerComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include <cmath>

namespace
{
    constexpr int tickHz = 60;
    constexpr int gridGap = AbcTrainTheme::Spacing::medium;
}

GamePickerComponent::GamePickerComponent()
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

GamePickerComponent::~GamePickerComponent()
{
    stopTimer();
}

void GamePickerComponent::setCards (std::vector<CardInfo> newCards)
{
    cards = std::move (newCards);
    hoverAmounts.assign (cards.size(), 0.0f);
    hoveredIndex = -1;
    resized();
    repaint();
}

void GamePickerComponent::timerCallback()
{
    if (! isVisible())
        return;

    const auto step = (float) (1000.0 / (double) tickHz / AbcTrainTheme::Duration::hover);
    bool needsRepaint = false;

    for (size_t i = 0; i < hoverAmounts.size(); ++i)
    {
        const auto target = ((int) i == hoveredIndex) ? 1.0f : 0.0f;
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

juce::Rectangle<int> GamePickerComponent::boundsForCard (int index) const
{
    auto area = getLocalBounds().reduced (AbcTrainTheme::Spacing::large);
    area.removeFromTop (44);                                  // heading
    area.removeFromBottom (36 + AbcTrainTheme::Spacing::medium); // close button row

    const auto columnWidth = (area.getWidth() - gridGap * (columns - 1)) / columns;
    const auto row = index / columns;
    const auto column = index % columns;

    return { area.getX() + column * (columnWidth + gridGap),
             area.getY() + row * (cardHeight + gridGap),
             columnWidth,
             cardHeight };
}

int GamePickerComponent::cardIndexAt (juce::Point<int> position) const
{
    for (int i = 0; i < (int) cards.size(); ++i)
        if (boundsForCard (i).contains (position))
            return i;

    return -1;
}

void GamePickerComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto index = cardIndexAt (e.getPosition());
    if (index != hoveredIndex)
    {
        hoveredIndex = index;
        setMouseCursor (index >= 0 ? juce::MouseCursor::PointingHandCursor
                                   : juce::MouseCursor::NormalCursor);
    }
}

void GamePickerComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredIndex = -1;
}

void GamePickerComponent::mouseUp (const juce::MouseEvent& e)
{
    const auto index = cardIndexAt (e.getPosition());
    if (index >= 0 && onGameChosen != nullptr)
        onGameChosen (index);
}

void GamePickerComponent::paintCard (juce::Graphics& g, int index, juce::Rectangle<float> bounds)
{
    const auto& theme = AbcTrainTheme::current();
    const auto& card = cards[(size_t) index];
    const auto hover = AbcTrainTheme::Ease::out (hoverAmounts[(size_t) index]);

    // The card lifts toward the viewer under the pointer: it rises 2px,
    // its shadow grows, and its surface brightens. Same "physical" idea as
    // the buttons, at a larger scale.
    const auto lifted = bounds.translated (0.0f, -2.0f * hover);

    juce::Path shape;
    shape.addRoundedRectangle (lifted, AbcTrainTheme::Radius::panel);

    juce::DropShadow shadow (theme.shadow.withAlpha ((0.20f + 0.22f * hover) * theme.shadowStrength),
                              (int) (7.0f + 8.0f * hover), { 0, (int) (2.0f + 2.0f * hover) });
    shadow.drawForPath (g, shape);

    const auto surface = theme.widgetBackground.brighter (0.05f * hover);
    juce::ColourGradient fill (surface.brighter (0.05f), lifted.getX(), lifted.getY(),
                                surface.darker (0.04f), lifted.getX(), lifted.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (lifted, AbcTrainTheme::Radius::panel);

    // The currently-selected training keeps a persistent accent border, so
    // "where am I" is readable without hovering anything.
    g.setColour (card.isCurrent ? theme.accent.withAlpha (0.9f)
                                : theme.outline.withAlpha (0.8f + 0.2f * hover));
    g.drawRoundedRectangle (lifted, AbcTrainTheme::Radius::panel, card.isCurrent ? 1.6f : 1.0f);

    auto inner = lifted.reduced ((float) AbcTrainTheme::Spacing::medium);

    // Icon and name share the top row; the benefit text then spans the
    // *full* card width below them rather than staying in a column beside
    // the icon. Found by running it: indenting the body text past the icon
    // left so little width that both the longer game names and every
    // benefit line truncated with an ellipsis.
    auto headerRow = inner.removeFromTop (22.0f);
    const auto iconBox = headerRow.removeFromLeft (22.0f);
    AppIcons::draw (g, card.icon, iconBox,
                    card.isCurrent ? theme.accent : theme.textDim.brighter (0.3f * hover));

    headerRow.removeFromLeft ((float) AbcTrainTheme::Spacing::small);

    g.setColour (theme.textBright);
    // 13px rather than 14: the longest game name ("Guess the Gain
    // Change" / "Угадай изменение громкости") was the one that still
    // ellipsised at 14 in the narrowest column.
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText (card.name, headerRow, juce::Justification::centredLeft, true);

    inner.removeFromTop (5.0f);

    // Stats pinned to the bottom, benefit filling whatever is left - so a
    // long benefit line can't push the record off the card.
    if (card.statsLine.isNotEmpty())
    {
        const auto statsArea = inner.removeFromBottom (14.0f);
        g.setColour (theme.textDim.withAlpha (0.8f));
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.5f));
        g.drawText (card.statsLine, statsArea, juce::Justification::centredLeft, true);
    }

    // The benefit line - the whole reason a card beats a dropdown entry.
    g.setColour (theme.textDim);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText (card.benefit, inner.toNearestInt(), juce::Justification::topLeft, 4);
}

void GamePickerComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    auto headingArea = getLocalBounds().reduced (AbcTrainTheme::Spacing::large)
                            .removeFromTop (34).toFloat();
    AbcTrainLookAndFeel::drawTrackedText (g, heading, headingArea,
                                           AbcTrainLookAndFeel::titleFont(),
                                           theme.textBright, 1.6f,
                                           juce::Justification::centredLeft);

    for (int i = 0; i < (int) cards.size(); ++i)
        paintCard (g, i, boundsForCard (i).toFloat());
}

void GamePickerComponent::resized()
{
    auto area = getLocalBounds().reduced (AbcTrainTheme::Spacing::large);
    closeButton.setBounds (area.removeFromBottom (36).removeFromRight (110));
}
