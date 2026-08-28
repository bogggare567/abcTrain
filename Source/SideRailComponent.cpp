#include "SideRailComponent.h"

namespace
{
    constexpr int brandHeight = 30;
    constexpr int rowHeight = 34;
    constexpr int statusHeight = 56;
    constexpr int controlsHeight = 30;

    AppIcons::Icon iconFor (SideRailComponent::Item item)
    {
        switch (item)
        {
            case SideRailComponent::Item::achievements: return AppIcons::Icon::award;
            case SideRailComponent::Item::sounds:       return AppIcons::Icon::sound;
            case SideRailComponent::Item::settings:     return AppIcons::Icon::settings;
            case SideRailComponent::Item::trainings:
            default:                                    return AppIcons::Icon::home;
        }
    }
}

SideRailComponent::SideRailComponent()
{
    labels = { "Trainings", "Achievements", "Sounds", "Settings" };

    setInterceptsMouseClicks (true, true);
    startTimerHz (60);
}

SideRailComponent::~SideRailComponent() { stopTimer(); }

void SideRailComponent::setLabels (juce::StringArray itemLabels, juce::String levelCaption,
                                    juce::String streakCaption)
{
    if (itemLabels.size() == numItems)
        labels = std::move (itemLabels);

    levelText = std::move (levelCaption);
    streakText = std::move (streakCaption);
    repaint();
}

void SideRailComponent::setActiveItem (Item item)
{
    if (active == item)
        return;

    active = item;
    repaint();
}

void SideRailComponent::setStatus (int newLevel, float progress, int newStreakDays)
{
    level = newLevel;
    levelProgress = juce::jlimit (0.0f, 1.0f, progress);
    streakDays = newStreakDays;
    repaint();
}

juce::Rectangle<int> SideRailComponent::rowBounds (int index) const
{
    using namespace AbcTrainTheme;

    auto area = getLocalBounds().reduced (Spacing::medium, Spacing::medium);
    area.removeFromTop (brandHeight + Spacing::medium);

    return area.removeFromTop (rowHeight * numItems)
               .withHeight (rowHeight)
               .translated (0, rowHeight * index);
}

int SideRailComponent::rowAt (juce::Point<int> p) const
{
    for (int i = 0; i < numItems; ++i)
        if (rowBounds (i).contains (p))
            return i;

    return -1;
}

void SideRailComponent::mouseMove (const juce::MouseEvent& e)
{
    const auto now = rowAt (e.getPosition());

    if (now != hovered)
    {
        hovered = now;
        setMouseCursor (now >= 0 ? juce::MouseCursor::PointingHandCursor
                                 : juce::MouseCursor::NormalCursor);
    }
}

void SideRailComponent::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void SideRailComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto row = rowAt (e.getPosition());

    if (row >= 0 && onItemChosen != nullptr)
        onItemChosen ((Item) row);
}

void SideRailComponent::timerCallback()
{
    // The same asymmetry WidgetStateRegistry uses: arriving is quicker
    // than leaving, which is what reads as the highlight having weight
    // rather than being a state flag drawn twice.
    auto changed = false;

    for (int i = 0; i < numItems; ++i)
    {
        const auto target = (i == hovered) ? 1.0f : 0.0f;
        const auto rate = target > glow[(size_t) i] ? 0.22f : 0.11f;
        const auto next = glow[(size_t) i] + (target - glow[(size_t) i]) * rate;

        if (std::abs (next - glow[(size_t) i]) > 0.001f)
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

    if (std::abs (shownProgress - levelProgress) > 0.0005f)
    {
        shownProgress += (levelProgress - shownProgress) * 0.14f;
        changed = true;
    }

    if (changed)
        repaint();
}

void SideRailComponent::paint (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = AbcTrainTheme::current();

    // A step down from the page rather than a border. Every panel in this
    // app used to be outlined, and when everything has a border a border
    // stops separating anything.
    g.setColour (theme.windowBackground.contrasting (0.028f));
    g.fillRect (getLocalBounds());

    g.setColour (theme.divider.withAlpha (0.85f));
    g.fillRect (getWidth() - 1, 0, 1, getHeight());

    auto area = getLocalBounds().reduced (Spacing::medium, Spacing::medium);

    // --- brand -----------------------------------------------------------
    {
        auto brand = area.removeFromTop (brandHeight);
        auto mark = brand.removeFromLeft (22).withSizeKeepingCentre (22, 22).toFloat();

        g.setColour (AbcTrainTheme::accentFor (Family::frequency).withAlpha (0.9f));
        g.fillRoundedRectangle (mark, 6.0f);

        brand.removeFromLeft (Spacing::small);
        AbcTrainLookAndFeel::drawTrackedText (g, "abcTrain", brand.toFloat(),
                                               AbcTrainLookAndFeel::headingFont(),
                                               theme.textBright, 0.6f);
        area.removeFromTop (Spacing::medium);
    }

    // --- navigation ------------------------------------------------------
    for (int i = 0; i < numItems; ++i)
    {
        const auto row = rowBounds (i).toFloat().reduced (0.0f, 1.0f);
        const auto selected = ((int) active == i);
        const auto lit = glow[(size_t) i];

        if (selected || lit > 0.002f)
        {
            g.setColour (selected ? theme.widgetBackground
                                  : theme.widgetBackground.withAlpha (0.55f * lit));
            g.fillRoundedRectangle (row, Radius::button);
        }

        // A selected row also carries a short accent stub at its left
        // edge. Colour alone would be the only signal otherwise, and it
        // has to survive both themes and a colour-blind reader.
        if (selected)
        {
            g.setColour (AbcTrainTheme::accentFor (Family::frequency));
            g.fillRoundedRectangle (row.getX() + 1.0f, row.getCentreY() - 8.0f, 2.5f, 16.0f, 1.25f);
        }

        auto content = row.reduced (Spacing::medium, 0.0f);
        auto glyph = content.removeFromLeft (17.0f).withSizeKeepingCentre (17.0f, 17.0f);

        const auto tint = selected ? theme.textBright
                                    : theme.textDim.interpolatedWith (theme.text, lit);

        AppIcons::draw (g, iconFor ((Item) i), glyph, tint);

        content.removeFromLeft (Spacing::medium);
        g.setColour (tint);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (labels[i], content.toNearestInt(), juce::Justification::centredLeft, true);
    }

    // --- everything below is pinned to the bottom -------------------------
    auto indicatorRow = area.removeFromBottom (24);
    area.removeFromBottom (Spacing::small);
    auto bottom = area.removeFromBottom (controlsHeight);
    area.removeFromBottom (Spacing::small);
    auto volumeRow = area.removeFromBottom (controlsHeight);
    area.removeFromBottom (Spacing::small);
    auto status = area.removeFromBottom (statusHeight);

    juce::ignoreUnused (bottom, volumeRow, indicatorRow);

    // --- level and streak, seen rather than read --------------------------
    {
        auto card = status.toFloat();
        g.setColour (theme.panelBackground.withAlpha (0.75f));
        g.fillRoundedRectangle (card, Radius::button);

        auto inner = status.reduced (Spacing::small, Spacing::small);

        AbcTrainLookAndFeel::drawTrackedText (g, levelText + " " + juce::String (level),
                                               inner.removeFromTop (13).toFloat(),
                                               AbcTrainLookAndFeel::captionFont(),
                                               theme.textDim, 1.0f);

        inner.removeFromTop (4);

        auto track = inner.removeFromTop (4).toFloat();
        g.setColour (theme.displayBackground);
        g.fillRoundedRectangle (track, 2.0f);

        if (shownProgress > 0.004f)
        {
            const auto accent = AbcTrainTheme::accentFor (Family::frequency);
            juce::ColourGradient fill (accent.darker (0.1f), track.getX(), 0.0f,
                                        accent.brighter (0.2f), track.getRight(), 0.0f, false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * shownProgress)), 2.0f);
        }

        inner.removeFromTop (5);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.drawText (streakText.replace ("{{days}}", juce::String (streakDays)),
                     inner.removeFromTop (13), juce::Justification::centredLeft, true);
    }
}

// The three slots the editor fills. Derived from the same arithmetic
// paint() uses, so the background and the widgets on it cannot disagree.
static juce::Rectangle<int> bottomStrip (juce::Rectangle<int> local, int which)
{
    using namespace AbcTrainTheme;

    auto area = local.reduced (Spacing::medium, Spacing::medium);

    auto indicators = area.removeFromBottom (24);
    area.removeFromBottom (Spacing::small);
    auto controls = area.removeFromBottom (controlsHeight);
    area.removeFromBottom (Spacing::small);
    auto volume = area.removeFromBottom (controlsHeight);

    switch (which)
    {
        case 0: return volume.withSizeKeepingCentre (volume.getWidth(), 22);

        case 3:
        {
            auto size = indicators.removeFromLeft (34);
            return size.withSizeKeepingCentre (34, 22);
        }

        case 4:
        {
            indicators.removeFromLeft (34 + Spacing::small);
            return indicators.withSizeKeepingCentre (indicators.getWidth(), 22);
        }

        default: break;
    }

    auto theme = controls.removeFromLeft (controlsHeight);
    controls.removeFromLeft (Spacing::tight);
    auto update = controls.removeFromLeft (controlsHeight);

    return which == 1 ? theme : update;
}

juce::Rectangle<int> SideRailComponent::getVolumeSlot()   const { return bottomStrip (getLocalBounds(), 0); }
juce::Rectangle<int> SideRailComponent::getThemeSlot()    const { return bottomStrip (getLocalBounds(), 1); }
juce::Rectangle<int> SideRailComponent::getUpdateSlot()   const { return bottomStrip (getLocalBounds(), 2); }
juce::Rectangle<int> SideRailComponent::getSizeSlot()     const { return bottomStrip (getLocalBounds(), 3); }
juce::Rectangle<int> SideRailComponent::getLanguageSlot() const { return bottomStrip (getLocalBounds(), 4); }
