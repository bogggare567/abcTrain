#include "CompactSelector.h"
#include "AbcTrainLookAndFeel.h"
#include "AbcTrainTheme.h"

namespace
{
    juce::Font valueFont()
    {
        return juce::Font (juce::FontOptions (12.0f).withStyle ("Bold"));
    }

    constexpr float tracking = 0.8f;
    constexpr int chevronWidth = 9;
    constexpr int gap = 5;
}

void CompactSelector::addItem (const juce::String& label, int itemId,
                               const juce::String& shortLabel)
{
    items.push_back ({ label, shortLabel.isNotEmpty() ? shortLabel : label, itemId });
}

void CompactSelector::clearItems()
{
    items.clear();
    selectedId = 0;
}

void CompactSelector::setSelectedId (int itemId, juce::NotificationType notification)
{
    if (itemId == selectedId)
        return;

    selectedId = itemId;
    repaint();

    if (notification != juce::dontSendNotification && onChange != nullptr)
        onChange();
}

void CompactSelector::setCaption (const juce::String& newCaption)
{
    caption = newCaption;
    repaint();
}

juce::String CompactSelector::labelForId (int itemId) const
{
    for (const auto& item : items)
        if (item.id == itemId)
            return item.shortLabel;

    return {};
}

int CompactSelector::getPreferredWidth() const
{
    const auto font = valueFont();
    auto widest = 0.0f;

    for (const auto& item : items)
        widest = juce::jmax (widest, AbcTrainLookAndFeel::trackedTextWidth (item.shortLabel, font, tracking));

    if (caption.isNotEmpty())
        widest += AbcTrainLookAndFeel::trackedTextWidth (caption + " ", font, tracking);

    return (int) std::ceil (widest) + gap + chevronWidth + 8;
}

void CompactSelector::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    auto bounds = getLocalBounds().toFloat();

    // No well, no border at rest. Hover is the entire affordance: the
    // indicator warms up and a faint plate appears under it, which is
    // enough to say "this is clickable" without it competing with the
    // title row's real controls for attention the rest of the time.
    if (hovered)
    {
        g.setColour (theme.widgetBackground.withAlpha (0.75f));
        g.fillRoundedRectangle (bounds, (float) AbcTrainTheme::Radius::small);
    }

    auto text = bounds.reduced (4.0f, 0.0f);
    auto chevronArea = text.removeFromRight ((float) chevronWidth);
    text.removeFromRight ((float) gap);

    const auto font = valueFont();
    auto x = text.getX();

    if (caption.isNotEmpty())
    {
        const auto captionWidth = AbcTrainLookAndFeel::trackedTextWidth (caption, font, tracking);
        AbcTrainLookAndFeel::drawTrackedText (g, caption, text.withWidth (captionWidth), font,
                                              theme.textDim.withAlpha (0.7f), tracking);
        x += captionWidth + 4.0f;
    }

    AbcTrainLookAndFeel::drawTrackedText (g, labelForId (selectedId), text.withLeft (x), font,
                                          hovered ? theme.textBright : theme.text, tracking);

    // A chevron rather than JUCE's filled triangle: two strokes read as
    // lighter than a solid arrowhead at this size, which is the point.
    juce::Path chevron;
    const auto cx = chevronArea.getCentreX();
    const auto cy = chevronArea.getCentreY() + 1.0f;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);

    g.setColour ((hovered ? theme.text : theme.textDim).withAlpha (0.8f));
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void CompactSelector::mouseEnter (const juce::MouseEvent&)
{
    hovered = true;
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    repaint();
}

void CompactSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}

void CompactSelector::mouseDown (const juce::MouseEvent&)
{
    showMenu();
}

void CompactSelector::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    for (const auto& item : items)
        menu.addItem (item.id, item.label, true, item.id == selectedId);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                  .withMinimumWidth (juce::jmax (getWidth(), 96)),
                        [safeThis = juce::Component::SafePointer<CompactSelector> (this)] (int chosenId)
                        {
                            if (safeThis != nullptr && chosenId != 0)
                                safeThis->setSelectedId (chosenId);
                        });
}
