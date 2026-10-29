#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/**
    Seminars that have been held, kept on this computer for going over the
    mistakes afterwards (Sem-Archive board, 2026-10-24).

    One JSON file per seminar in the app's own folder: its title and date,
    whether it was anonymous, how many took part, and per round what was
    played (the exercise, the answer, the sound), how many answered, how
    many were right and who missed. Nothing leaves the computer unless the
    presenter exports it.

    An anonymous seminar is stored as it was shown: numbers, never names.

    The sound is kept by name, not as audio: the round's exact setting is
    not stored, so a round cannot be played again from here - "repeat the
    mistakes" runs the same exercises again with fresh rounds instead.
*/
class SeminarArchive
{
public:
    struct Round
    {
        int game = -1;               // GameManager index
        juce::String exercise, answer, sound;
        int answered = 0, right = 0;
        juce::StringArray missed;    // who answered wrong (numbers when anonymous)
    };

    struct Session
    {
        juce::String title;
        juce::Time when;
        bool anonymous = false;
        int people = 0;
        std::vector<Round> rounds;
        juce::File file;             // where it is stored (set by list / save)

        int answeredTotal() const;
        int rightTotal() const;
    };

    static juce::File folder();

    /** Writes a new file; returns it (or an invalid File). */
    static juce::File save (const Session&);

    /** Newest first. */
    static std::vector<Session> list();

    static juce::String toCsv (const Session&);

    static juce::var toVar (const Session&);
    static Session fromVar (const juce::var&);
};
