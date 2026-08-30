#include "TopNavComponent.h"

namespace
{
    // Measured off the mockup (docs/design/redesign-spec.md): the bar is
    // 59 tall, the brand cell 127 wide with a divider on its right, the
    // tabs 33 tall starting at x172.
    constexpr int pagePad      = 21;
    constexpr int brandWidth   = 127;
    constexpr int markSize     = 26;
    constexpr int tabHeight    = 33;
    constexpr int tabPadding   = 16;
    constexpr int tabGap       = 2;
    constexpr int tabsLeft     = 172;

    constexpr float tabTracking   = 1.5f;
    constexpr float brandTracking = 0.8f;

    constexpr int rightCellWidth = 34;
    constexpr int rightCells     = 5;   // volume, theme, update, size, language

    constexpr int dotSize  = 5;
    constexpr int dotPitch = 8;
    constexpr int maxDots  = 5;
}

TopNavComponent::TopNavComponent()
{
    labels = { "Trainings", "Achievements", "Sounds", "Settings" };
    setInterceptsMouseClicks (true, true);
    startTimerHz (60);
}

TopNavComponent::~TopNavComponent() = default;

void TopNavComponent::setLabels (juce::StringArray itemLabels, juce::String streakCaption)
{
    if (itemLabels.size() == numItems)
        labels = std::move (itemLabels);

    streakText = std::move (streakCaption);
    repaint();
}

void TopNavComponent::setActiveItem (Item item)
{
    if (active == item)
        return;

    active = item;
    repaint();
}

void TopNavComponent::setStatus (int newStreakDays)
{
    if (streakDays == newStreakDays)
        return;

    streakDays = newStreakDays;
    repaint();
}

juce::Rectangle<int> TopNavComponent::tabBounds (int index) const
{
    if (index < 0 || index >= numItems)
        return {};

    const auto font = AbcTrainLookAndFeel::headingFont();
    auto x = tabsLeft;

    for (int i = 0; i < index; ++i)
    {
        const auto w = (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (
                            AbcTrainLookAndFeel::toCaps (labels[i]), font, tabTracking)) + tabPadding * 2;
        x += w + tabGap;
    }

    const auto width = (int) std::ceil (AbcTrainLookAndFeel::trackedTextWidth (
                            AbcTrainLookAndFeel::toCaps (labels[index]), font, tabTracking)) + tabPadding * 2;

    return { x, (getHeight() - tabHeight) / 2, width, tabHeight };
}

int TopNavComponent::tabAt (juce::Point<int> p) const
{
    for (int i = 0; i < numItems; ++i)
        if (tabBounds (i).contains (p))
            return i;

    return -1;
}

juce::Rectangle<int> TopNavComponent::rightCell (int slotFromRight) const
{
    const auto right = getWidth() - pagePad;
    const auto x = right - (slotFromRight + 1) * rightCellWidth;

    return juce::Rectangle<int> (x, (getHeight() - 24) / 2, rightCellWidth, 24).reduced (4, 0);
}

// Right to left: language, size, update, theme, volume. Language sits
// outermost because it is the one a person hunts for by eye - it is the
// only cell whose content is a word rather than a glyph.
juce::Rectangle<int> TopNavComponent::getLanguageSlot() const { return rightCell (0); }
juce::Rectangle<int> TopNavComponent::getSizeSlot()     const { return rightCell (1); }
juce::Rectangle<int> TopNavComponent::getUpdateSlot()   const { return rightCell (2); }
juce::Rectangle<int> TopNavComponent::getThemeSlot()    const { return rightCell (3); }

juce::Rectangle<int> TopNavComponent::getVolumeSlot() const
{
    // Volume is a slider, so it needs a run rather than a cell. It keeps
    // its place in the bar deliberately: it is the one control somebody
    // reaches for *during* a round, and the design mockup dropped it
    // entirely, which would have meant leaving the exercise to turn the
    // sound down.
    const auto cell = rightCell (4);
    return { cell.getRight() - 96, cell.getY(), 96, cell.getHeight() };
}

void TopNavComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto now = tabAt (e.getPosition());

    if (now != hovered)
    {
        hovered = now;
        repaint();
    }
}

void TopNavComponent::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}

void TopNavComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto index = tabAt (e.getPosition());

    if (index >= 0 && onItemChosen != nullptr)
        onItemChosen (static_cast<Item> (index));
}

void TopNavComponent::timerCallback()
{
    // Arriving slower than leaving: a highlight that lands instantly and
    // fades slowly reads as a light coming on, which is the wrong verb for
    // a pointer that is only passing through.
    const auto arrive = 0.22f;
    const auto leave  = 0.11f;

    bool changed = false;

    for (int i = 0; i < numItems; ++i)
    {
        const auto target = (i == hovered) ? 1.0f : 0.0f;
        const auto rate = target > glow[(size_t) i] ? arrive : leave;
        const auto next = glow[(size_t) i] + (target - glow[(size_t) i]) * rate;

        if (std::abs (next - glow[(size_t) i]) > 0.002f)
        {
            glow[(size_t) i] = next;
            changed = true;
        }
        else if (glow[(size_t) i] != target)
        {
            glow[(size_t) i] = target;
            changed = true;
        }
    }

    if (changed)
        repaint();
}

void TopNavComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto bounds = getLocalBounds();

    // One hairline under the whole bar. No fill: the bar is part of the
    // page, and giving it a surface of its own would make it a second
    // window sitting on the first.
    g.setColour (theme.divider);
    g.fillRect (bounds.getX(), bounds.getBottom() - 1, bounds.getWidth(), 1);

    // --- the mark and the wordmark ---------------------------------------
    {
        auto brand = bounds.withX (pagePad).withWidth (brandWidth);

        auto mark = brand.removeFromLeft (markSize)
                         .withSizeKeepingCentre (markSize, markSize);

        g.setColour (theme.accent);
        g.fillRect (mark.reduced (markSize / 4));
        g.setColour (theme.accent.withAlpha (0.35f));
        g.drawRect (mark, 1.0f);

        AbcTrainLookAndFeel::drawTrackedText (g, "abcTrain",
                                               brand.withTrimmedLeft (10).toFloat(),
                                               AbcTrainLookAndFeel::titleFont(),
                                               theme.textBright, brandTracking,
                                               juce::Justification::centredLeft);

        const auto dividerX = pagePad + brandWidth;
        g.setColour (theme.divider);
        g.fillRect (dividerX, bounds.getY(), 1, bounds.getHeight() - 1);
    }

    // --- the tabs ---------------------------------------------------------
    {
        const auto font = AbcTrainLookAndFeel::headingFont();

        for (int i = 0; i < numItems; ++i)
        {
            const auto tab = tabBounds (i);

            if (tab.getRight() > getWidth() - rightCells * rightCellWidth - pagePad)
                break;   // no room; better a missing tab than one drawn over the streak

            const auto isActive = (static_cast<int> (active) == i);
            const auto eased = AbcTrainTheme::Ease::out (glow[(size_t) i]);

            if (isActive)
            {
                g.setColour (theme.accent);
                g.fillRect (tab);
            }
            else if (eased > 0.01f)
            {
                g.setColour (theme.accent.withAlpha (0.10f * eased));
                g.fillRect (tab);
            }

            const auto colour = isActive
                                    ? theme.windowBackground
                                    : theme.textDim.interpolatedWith (theme.textBright, eased);

            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (labels[i]), tab.toFloat(),
                                                   font, colour, tabTracking,
                                                   juce::Justification::centred);
        }
    }

    // --- the streak, and its days as dots ---------------------------------
    if (streakText.isNotEmpty() && streakDays > 0)
    {
        const auto font = AbcTrainLookAndFeel::labelFont();
        const auto dotsWidth = maxDots * dotPitch - (dotPitch - dotSize);
        const auto textWidth = (int) std::ceil (
            AbcTrainLookAndFeel::trackedTextWidth (AbcTrainLookAndFeel::toCaps (streakText), font, 1.68f));

        auto right = getWidth() - pagePad - rightCells * rightCellWidth - 24;
        auto area = juce::Rectangle<int> (right - textWidth - 10 - dotsWidth, 0,
                                           textWidth + 10 + dotsWidth, getHeight());

        if (area.getX() > tabBounds (numItems - 1).getRight() + 16)
        {
            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (streakText),
                                                   area.removeFromLeft (textWidth).toFloat(),
                                                   font, theme.accentWarm, 1.68f,
                                                   juce::Justification::centredLeft);

            area.removeFromLeft (10);

            // Dots rather than a number, because a streak is a thing you
            // are trying not to break, and five squares with two lit says
            // that in a way "2" does not.
            const auto y = area.getCentreY() - dotSize / 2;

            for (int i = 0; i < maxDots; ++i)
            {
                g.setColour (i < juce::jmin (streakDays, maxDots) ? theme.accentWarm
                                                                  : theme.outline);
                g.fillRect (area.getX() + i * dotPitch, y, dotSize, dotSize);
            }
        }
    }

    // --- the divider before the app's own controls ------------------------
    {
        const auto x = getWidth() - pagePad - rightCells * rightCellWidth - 12;
        g.setColour (theme.divider);
        g.fillRect (x, bounds.getY() + 12, 1, bounds.getHeight() - 25);
    }
}
