#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// A one-or-two-glyph indicator that opens a menu when clicked: "RU", "M".
//
// It replaces a juce::ComboBox in the places where the *current value* is
// the only thing worth showing and the list is a rare, deliberate visit -
// language and window size. A ComboBox draws a bordered well and a
// permanent arrow, which reads as a form field the eye keeps returning
// to. Language and size are set once and then forgotten, so they should
// sit in the title row the way a status indicator does: small, quiet,
// legible, and obviously clickable only when you go looking for it.
//
// This is not a ComboBox subclass. ComboBox's drawing goes through
// LookAndFeel::drawComboBox, which is shared with every other combo box
// in the four plugins (game selector, mode, level, reverb type) - all of
// which genuinely *are* form fields and should keep looking like one.
// Overriding that method would have restyled them too.
class CompactSelector : public juce::Component
{
public:
    CompactSelector() = default;

    // `shortLabel` is what the indicator shows when this item is selected;
    // `label` is what the menu lists. They differ for language, where the
    // menu has to say "Русский" but the indicator only has room for "RU" -
    // and where a two-letter code is in fact the clearer indicator, since
    // it stays legible in every script.
    void addItem (const juce::String& label, int itemId, const juce::String& shortLabel = {});
    void clearItems();

    void setSelectedId (int itemId, juce::NotificationType = juce::sendNotification);
    int getSelectedId() const noexcept { return selectedId; }

    // Optional prefix drawn ahead of the value in the dim text colour
    // ("size M"). Empty by default - the value alone is usually enough.
    void setCaption (const juce::String& newCaption);

    std::function<void()> onChange;

    // Width this selector needs for its widest item, so callers can lay it
    // out without guessing. Includes the caption and the chevron.
    int getPreferredWidth() const;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Item { juce::String label, shortLabel; int id = 0; };

    juce::String labelForId (int itemId) const;
    void showMenu();

    std::vector<Item> items;
    int selectedId = 0;
    juce::String caption;
    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompactSelector)
};
