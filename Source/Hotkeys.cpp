#include "Hotkeys.h"

namespace Hotkeys
{
    const std::vector<Action>& actions()
    {
        // Order here is order on the page. Seminar keys default to what a
        // presenter's clicker sends (Page Down / Page Up) with the arrows
        // as the second key.
        static const std::vector<Action> list {
            { "train.toggle",    "keys.train.toggle",    Scope::training, "spacebar",            "" },
            { "train.a",         "keys.train.a",         Scope::training, "1",                   "a" },
            { "train.b",         "keys.train.b",         Scope::training, "2",                   "b" },
            { "train.left",      "keys.train.left",      Scope::training, "cursor left",         "" },
            { "train.right",     "keys.train.right",     Scope::training, "cursor right",        "" },
            { "train.fineLeft",  "keys.train.fineLeft",  Scope::training, "shift + cursor left", "" },
            { "train.fineRight", "keys.train.fineRight", Scope::training, "shift + cursor right", "" },
            { "train.answer",    "keys.train.answer",    Scope::training, "return",              "" },
            { "train.hint",      "keys.train.hint",      Scope::training, "h",                   "" },
            { "train.help",      "keys.train.help",      Scope::training, "F1",                  "" },

            { "sem.next",        "keys.sem.next",        Scope::seminar,  "page down",           "cursor right" },
            { "sem.play",        "keys.sem.play",        Scope::seminar,  "spacebar",            "" },
            { "sem.reveal",      "keys.sem.reveal",      Scope::seminar,  "r",                   "" },
            { "sem.projector",   "keys.sem.projector",   Scope::seminar,  "p",                   "" },

            { "own.a",           "keys.own.a",           Scope::studio,   "q",                   "" },
            { "own.b",           "keys.own.b",           Scope::studio,   "w",                   "" },
            { "own.dry",         "keys.own.dry",         Scope::studio,   "e",                   "" },
            { "own.next",        "keys.own.next",        Scope::studio,   "n",                   "" },
        };
        return list;
    }

    const char* scopeKey (Scope s)
    {
        switch (s)
        {
            case Scope::seminar: return "keys.scope.seminar";
            case Scope::studio:  return "keys.scope.studio";
            case Scope::training:
            default:             return "keys.scope.training";
        }
    }

    static const Action* find (const juce::String& id)
    {
        for (const auto& a : actions())
            if (id == a.id)
                return &a;
        return nullptr;
    }

    juce::KeyPress Map::get (const juce::String& id, int slot) const
    {
        const auto* a = find (id);
        if (a == nullptr)
            return {};

        const auto stored = properties.getValue (entry (id, slot));

        if (stored == "-")
            return {};

        const juce::String text = stored.isNotEmpty() ? stored : juce::String (slot == 0 ? a->key : a->alt);
        return text.isEmpty() ? juce::KeyPress() : juce::KeyPress::createFromDescription (text);
    }

    void Map::set (const juce::String& id, int slot, const juce::KeyPress& key)
    {
        properties.setValue (entry (id, slot), key.isValid() ? key.getTextDescription() : juce::String ("-"));
        properties.saveIfNeeded();
    }

    void Map::resetAll()
    {
        for (const auto& a : actions())
            for (int slot = 0; slot < 2; ++slot)
                properties.removeValue (entry (a.id, slot));
        properties.saveIfNeeded();
    }

    juce::String Map::actionFor (const juce::KeyPress& key, Scope scope) const
    {
        for (const auto& a : actions())
            if (a.scope == scope)
                for (int slot = 0; slot < 2; ++slot)
                    if (const auto k = get (a.id, slot); k.isValid() && k == key)
                        return a.id;
        return {};
    }

    bool Map::clashes (const juce::String& id, int slot) const
    {
        const auto* me = find (id);
        const auto key = get (id, slot);

        if (me == nullptr || ! key.isValid())
            return false;

        for (const auto& a : actions())
            if (a.scope == me->scope)
                for (int s = 0; s < 2; ++s)
                    if (! (id == a.id && s == slot) && get (a.id, s) == key)
                        return true;
        return false;
    }

    int Map::clashCount() const
    {
        int n = 0;
        for (const auto& a : actions())
            for (int slot = 0; slot < 2; ++slot)
                n += clashes (a.id, slot) ? 1 : 0;
        return n / 2;   // each clash is two cells
    }

    juce::String Map::label (const juce::KeyPress& key)
    {
        if (! key.isValid())
            return {};

        juce::String text;
        const auto mods = key.getModifiers();
       #if JUCE_MAC
        if (mods.isCommandDown())  text << juce::String::fromUTF8 ("\xe2\x8c\x98 ");
       #else
        if (mods.isCommandDown())  text << "Ctrl ";
       #endif
        if (mods.isAltDown())      text << juce::String::fromUTF8 ("\xe2\x8c\xa5 ");
        if (mods.isShiftDown())    text << juce::String::fromUTF8 ("\xe2\x87\xa7 ");

        const auto code = key.getKeyCode();
        if (code == juce::KeyPress::spaceKey)          text << "Space";
        else if (code == juce::KeyPress::returnKey)    text << "Enter";
        else if (code == juce::KeyPress::escapeKey)    text << "Esc";
        else if (code == juce::KeyPress::leftKey)      text << juce::String::fromUTF8 ("\xe2\x86\x90");
        else if (code == juce::KeyPress::rightKey)     text << juce::String::fromUTF8 ("\xe2\x86\x92");
        else if (code == juce::KeyPress::upKey)        text << juce::String::fromUTF8 ("\xe2\x86\x91");
        else if (code == juce::KeyPress::downKey)      text << juce::String::fromUTF8 ("\xe2\x86\x93");
        else if (code == juce::KeyPress::pageDownKey)  text << "PgDn";
        else if (code == juce::KeyPress::pageUpKey)    text << "PgUp";
        else
        {
            // The description without its modifiers: "F1", "H", "1".
            auto d = juce::KeyPress (code).getTextDescription();
            text << (d.length() == 1 ? d.toUpperCase() : d);
        }

        return text.trim();
    }
}
