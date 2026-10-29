#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/SegmentedChoice.h"
#include "Hotkeys.h"
#include <functional>

/**
    Settings > Keys (the Set-PC-Keys board): every shortcut in one table -
    action, key, second key, where it works - with a filter by place, a
    count of clashes and a way back to the standard set.

    A click on a key cell arms it ("press a key"); the next key pressed is
    the new one. Esc leaves it as it was, Backspace or Delete clears it.
    A key that another action in the same place already uses is drawn in
    the "wrong" colour, in both cells, and counted in the header - it is
    allowed (the player may be half-way through swapping two keys), never
    silently taken from the other action.
*/
class HotkeysEditor : public juce::Component
{
public:
    struct Strings
    {
        juce::String all, action, key, second, where, defaults, clashes, noClashes, press, note;
        std::function<juce::String (const char*)> text;   // action titles and scope names
    };

    explicit HotkeysEditor (Hotkeys::Map& mapToEdit) : map (mapToEdit), table (*this)
    {
        filter.onChange = [this] (int) { table.rebuild(); resized(); repaint(); };
        addAndMakeVisible (filter);

        defaultsButton.onClick = [this]
        {
            map.resetAll();
            table.armed = {};
            table.repaint();
            repaint();
        };
        addAndMakeVisible (defaultsButton);

        viewport.setViewedComponent (&table, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void setStrings (Strings s)
    {
        strings = std::move (s);
        filter.setOptions ({ -1, 0, 1, 2 }, { strings.all,
                                                strings.text (Hotkeys::scopeKey (Hotkeys::Scope::training)),
                                                strings.text (Hotkeys::scopeKey (Hotkeys::Scope::seminar)),
                                                strings.text (Hotkeys::scopeKey (Hotkeys::Scope::studio)) });
        if (! stringsSet)
            filter.setValue (-1);
        stringsSet = true;
        defaultsButton.setButtonText (strings.defaults);
        table.rebuild();
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (30);
        defaultsButton.setBounds (head.removeFromRight (180));
        head.removeFromRight (12);
        clashArea = head.removeFromRight (180);
        head.removeFromRight (12);
        filter.setBounds (head.withWidth (juce::jmin (head.getWidth(), juce::jmax (360, filter.getPreferredWidth()))));

        r.removeFromTop (12);
        noteArea = r.removeFromBottom (24);
        r.removeFromBottom (6);
        viewport.setBounds (r);
        table.setSize (r.getWidth() - (table.getHeightNeeded() > r.getHeight() ? viewport.getScrollBarThickness() : 0),
                       juce::jmax (r.getHeight(), table.getHeightNeeded()));
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto clashes = map.clashCount();
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.setColour (clashes > 0 ? theme.negative : theme.textDim);
        AbcTrainLookAndFeel::fitText (g, clashes > 0 ? strings.clashes.replace ("{{n}}", juce::String (clashes)) : strings.noClashes,
                                      clashArea, juce::Justification::centredRight);
        g.setColour (theme.textDim);
        AbcTrainLookAndFeel::fitText (g, strings.note, noteArea, juce::Justification::centredLeft);
    }

private:
    struct Table : public juce::Component
    {
        explicit Table (HotkeysEditor& o) : owner (o) { setWantsKeyboardFocus (true); }

        struct Line { bool caption = false; Hotkeys::Scope scope {}; const Hotkeys::Action* action = nullptr; };
        std::vector<Line> lines;
        struct Armed { juce::String id; int slot = -1; } armed;

        static constexpr int lineHeight = 34, captionHeight = 40, headerHeight = 28;

        void rebuild()
        {
            lines.clear();
            const auto only = owner.filter.getValue();

            for (int s = 0; s < 3; ++s)
            {
                if (only >= 0 && only != s)
                    continue;

                lines.push_back ({ true, (Hotkeys::Scope) s, nullptr });
                for (const auto& a : Hotkeys::actions())
                    if ((int) a.scope == s)
                        lines.push_back ({ false, a.scope, &a });
            }
        }

        int getHeightNeeded() const
        {
            int h = headerHeight;
            for (const auto& l : lines)
                h += l.caption ? captionHeight : lineHeight;
            return h + 8;
        }

        // Columns: action | key | second | where.
        void columns (juce::Rectangle<int> r, juce::Rectangle<int>& act, juce::Rectangle<int>& k1,
                      juce::Rectangle<int>& k2, juce::Rectangle<int>& where) const
        {
            const auto keyW = juce::jlimit (110, 170, r.getWidth() / 6);
            act = r.removeFromLeft (r.getWidth() - 2 * keyW - 20 - juce::jmin (260, r.getWidth() / 4));
            r.removeFromLeft (10);
            k1 = r.removeFromLeft (keyW);
            r.removeFromLeft (10);
            k2 = r.removeFromLeft (keyW);
            r.removeFromLeft (16);
            where = r;
        }

        void paint (juce::Graphics& g) override
        {
            const auto& theme = AbcTrainTheme::current();
            auto r = getLocalBounds().reduced (12, 0);
            juce::Rectangle<int> act, k1, k2, where;

            auto head = r.removeFromTop (headerHeight);
            columns (head, act, k1, k2, where);
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::microFont());
            AbcTrainLookAndFeel::fitText (g, owner.strings.action.toUpperCase(), act, juce::Justification::centredLeft);
            AbcTrainLookAndFeel::fitText (g, owner.strings.key.toUpperCase(), k1, juce::Justification::centredLeft);
            AbcTrainLookAndFeel::fitText (g, owner.strings.second.toUpperCase(), k2, juce::Justification::centredLeft);
            AbcTrainLookAndFeel::fitText (g, owner.strings.where.toUpperCase(), where, juce::Justification::centredLeft);
            g.setColour (theme.divider);
            g.fillRect (r.getX(), r.getY() - 1, r.getWidth(), 1);

            for (const auto& l : lines)
            {
                if (l.caption)
                {
                    auto cap = r.removeFromTop (captionHeight);
                    g.setColour (theme.accent);
                    g.setFont (AbcTrainLookAndFeel::microFont());
                    AbcTrainLookAndFeel::fitText (g, owner.strings.text (Hotkeys::scopeKey (l.scope)).toUpperCase(),
                                                  cap.withTrimmedTop (14), juce::Justification::centredLeft);
                    continue;
                }

                auto line = r.removeFromTop (lineHeight);
                columns (line, act, k1, k2, where);

                g.setColour (theme.text);
                g.setFont (AbcTrainLookAndFeel::bodyFont());
                AbcTrainLookAndFeel::fitText (g, owner.strings.text (l.action->titleKey), act, juce::Justification::centredLeft);

                paintKey (g, k1.withSizeKeepingCentre (k1.getWidth(), 26), *l.action, 0);
                paintKey (g, k2.withSizeKeepingCentre (k2.getWidth(), 26), *l.action, 1);

                g.setColour (theme.textDim);
                g.setFont (AbcTrainLookAndFeel::labelFont());
                AbcTrainLookAndFeel::fitText (g, owner.strings.text (l.action->scope == Hotkeys::Scope::training ? "keys.where.training"
                                                                    : l.action->scope == Hotkeys::Scope::seminar ? "keys.where.seminar"
                                                                                                                 : "keys.where.studio"),
                                              where, juce::Justification::centredLeft);

                g.setColour (theme.divider);
                g.fillRect (line.getX(), line.getBottom() - 1, line.getWidth(), 1);
            }
        }

        void paintKey (juce::Graphics& g, juce::Rectangle<int> cell, const Hotkeys::Action& a, int slot) const
        {
            const auto& theme = AbcTrainTheme::current();
            const auto isArmed = armed.id == a.id && armed.slot == slot;
            const auto key = owner.map.get (a.id, slot);
            const auto clash = owner.map.clashes (a.id, slot);

            if (isArmed)
            {
                g.setColour (theme.accentWarm.withAlpha (0.18f));
                g.fillRect (cell);
                g.setColour (theme.accentWarm);
                g.drawRect (cell, 1);
                g.setFont (AbcTrainLookAndFeel::labelFont());
                AbcTrainLookAndFeel::fitText (g, owner.strings.press, cell.reduced (6, 0), juce::Justification::centred);
                return;
            }

            if (! key.isValid())
            {
                g.setColour (theme.outline.withAlpha (0.6f));
                g.drawRect (cell, 1);
                g.setColour (theme.textDim);
                g.setFont (AbcTrainLookAndFeel::labelFont());
                AbcTrainLookAndFeel::fitText (g, juce::String::fromUTF8 ("\xe2\x80\x94"), cell, juce::Justification::centred);
                return;
            }

            // A keycap: a filled block, the key's name in the mono face.
            auto cap = cell.withWidth (juce::jmin (cell.getWidth(), 34 + 9 * Hotkeys::Map::label (key).length()));
            g.setColour (clash ? theme.negative.withAlpha (0.25f) : theme.widgetBackground);
            g.fillRect (cap);
            g.setColour (clash ? theme.negative : theme.outline);
            g.drawRect (cap, 1);
            g.setColour (clash ? theme.negative : theme.textBright);
            g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (14.0f));
            AbcTrainLookAndFeel::fitText (g, Hotkeys::Map::label (key), cap.reduced (6, 0), juce::Justification::centred);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            auto r = getLocalBounds().reduced (12, 0);
            r.removeFromTop (headerHeight);
            juce::Rectangle<int> act, k1, k2, where;

            for (const auto& l : lines)
            {
                auto line = r.removeFromTop (l.caption ? captionHeight : lineHeight);
                if (l.caption || ! line.contains (e.getPosition()))
                    continue;

                columns (line, act, k1, k2, where);
                const auto slot = k1.contains (e.getPosition()) ? 0 : k2.contains (e.getPosition()) ? 1 : -1;
                armed = slot >= 0 ? Armed { l.action->id, slot } : Armed {};
                if (slot >= 0)
                    grabKeyboardFocus();
                repaint();
                return;
            }
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (armed.slot < 0)
                return false;

            if (key == juce::KeyPress::escapeKey)
            {
                armed = {};
            }
            else
            {
                const auto clear = key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey;
                owner.map.set (armed.id, armed.slot, clear ? juce::KeyPress() : key);
                armed = {};
            }

            repaint();
            owner.repaint();
            return true;
        }

        void focusLost (FocusChangeType) override { armed = {}; repaint(); }

        HotkeysEditor& owner;
    };

    Hotkeys::Map& map;
    Strings strings;
    bool stringsSet = false;
    SegmentedChoice filter;
    juce::TextButton defaultsButton;
    juce::Viewport viewport;
    Table table;
    juce::Rectangle<int> clashArea, noteArea;
};
