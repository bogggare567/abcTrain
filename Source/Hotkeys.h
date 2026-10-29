#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/**
    Keyboard shortcuts, one table for the whole app (Set-PC-Keys board,
    2026-10-24).

    Every action has a place where it works (scope), a key and an optional
    second key. The pair is stored per user in the settings file, one entry
    per slot: "hotkey.<id>.<slot>" holding the key's description, or "-"
    when the player cleared it - a missing entry means "the default".

    Two actions conflict only inside one scope: Space can play/stop in a
    seminar and switch A/B in training, because those two screens are
    never in front at the same time.

    The trainer's keys also work in a battle: a battle round is the
    trainer's own screen.
*/
namespace Hotkeys
{
    enum class Scope { training, seminar, studio };

    struct Action
    {
        const char* id;
        const char* titleKey;   // i18n
        Scope scope;
        const char* key;        // juce::KeyPress description, "" for none
        const char* alt;
    };

    const std::vector<Action>& actions();
    const char* scopeKey (Scope);   // i18n key of the scope's name

    class Map
    {
    public:
        explicit Map (juce::PropertiesFile& file) : properties (file) {}

        juce::KeyPress get (const juce::String& id, int slot) const;
        void set (const juce::String& id, int slot, const juce::KeyPress&);   // invalid KeyPress clears
        void resetAll();

        /** The action this key starts in this scope, or empty. */
        juce::String actionFor (const juce::KeyPress&, Scope) const;

        /** Another action in the same scope uses the same key. */
        bool clashes (const juce::String& id, int slot) const;
        int clashCount() const;

        /** "Space", "⇧ ←", "F1" - short, for a key cell. */
        static juce::String label (const juce::KeyPress&);

    private:
        static juce::String entry (const juce::String& id, int slot) { return "hotkey." + id + "." + juce::String (slot); }
        juce::PropertiesFile& properties;
    };
}
