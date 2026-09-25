#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LocalRoom.h"
#include <functional>
#include <vector>

// The invitees of a "list only" seminar, edited like a small spreadsheet.
//
// It replaced one multi-line text box where people were typed as
// "name, e-mail" per line (the screenshot of 2026-09-25: an empty box,
// three buttons, nothing saying what goes where). Now:
//
// - one row per person, a Name field and an E-mail field side by side, the
//   personal code already made on the right;
// - there is always one empty row at the bottom: typing in it is adding a
//   person, so there is no "Add" to find;
// - Enter moves on (name -> e-mail -> next person's name), like a form;
// - pasting several lines - from Excel, Google Sheets, a mail, a chat -
//   into any field fills that many rows; the e-mail is recognised by its
//   "@", whichever column it was in;
// - a malformed address is outlined, not refused: the list is also used
//   offline, where the e-mail is only a note.
class InviteListComponent : public juce::Component
{
public:
    struct Strings
    {
        juce::String name { "Name" }, mail { "E-mail" }, code { "Code" };
        juce::String namePlaceholder { "Ivan Petrov" }, mailPlaceholder { "ivan@school.ru" };
        juce::String remove { "Remove" };
    };

    InviteListComponent();
    ~InviteListComponent() override;

    void setStrings (const Strings&);

    // Everybody with a name or an e-mail, each with a unique code.
    std::vector<LocalRoom::Invitee> getInvitees() const;
    void setInvitees (const std::vector<LocalRoom::Invitee>&);

    // Lines of "name, e-mail" / tab-separated / e-mail only: appended.
    void appendFromText (const juce::String&);

    int getNumPeople() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onChanged;

    static bool looksLikeMail (const juce::String&);

    // Pasted text as people: (name, e-mail) per line, header rows and
    // blank lines dropped. What appendFromText() fills the rows with.
    static std::vector<std::pair<juce::String, juce::String>> parsePeople (const juce::String&);

private:
    class Row;
    friend class Row;

    void ensureTrailingEmptyRow();
    void rowChanged (Row*);
    void removeRow (Row*);
    void focusNext (Row*, bool fromMail);
    void layoutRows();

    Strings text;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Row> rows;
    juce::Random random;
    static constexpr int rowHeight = 38, headerHeight = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InviteListComponent)
};
