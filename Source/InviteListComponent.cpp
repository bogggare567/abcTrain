#include "InviteListComponent.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

namespace
{
    using LnF = AbcTrainLookAndFeel;

    // Splits one pasted line into (name, e-mail): tab first (a spreadsheet),
    // then ';' (a CSV from Excel in Russian locales), then ','. The part
    // with an "@" is the e-mail, whichever side it was on.
    std::pair<juce::String, juce::String> splitLine (const juce::String& line)
    {
        juce::StringArray parts;

        for (const auto* separator : { "\t", ";", "," })
        {
            if (line.contains (separator))
            {
                parts = juce::StringArray::fromTokens (line, separator, "\"");
                break;
            }
        }

        if (parts.isEmpty())
            parts.add (line);

        for (auto& p : parts)
            p = p.trim().unquoted().trim();

        parts.removeEmptyStrings();
        juce::String name, mail;

        for (const auto& p : parts)
        {
            if (mail.isEmpty() && p.containsChar ('@') && ! p.containsChar (' '))
                mail = p;
            else
                name = name.isEmpty() ? p : name + " " + p;
        }

        // "Ivan Petrov <ivan@school.ru>" - the way mail programs copy.
        if (mail.isEmpty() && name.containsChar ('<') && name.containsChar ('@'))
        {
            mail = name.fromFirstOccurrenceOf ("<", false, false).upToFirstOccurrenceOf (">", false, false).trim();
            name = name.upToFirstOccurrenceOf ("<", false, false).trim();
        }

        return { name, mail };
    }
}

bool InviteListComponent::looksLikeMail (const juce::String& s)
{
    const auto at = s.indexOfChar ('@');
    return at > 0 && s.lastIndexOfChar ('.') > at + 1 && ! s.endsWithChar ('.') && ! s.containsAnyOf (" ,;<>");
}

// ---- one row ------------------------------------------------------------

class InviteListComponent::Row : public juce::Component
{
public:
    struct Field : public juce::TextEditor
    {
        explicit Field (Row& r) : row (r) {}

        // Several lines pasted into one field: they are several people.
        void insertTextAtCaret (const juce::String& t) override
        {
            if (t.containsAnyOf ("\r\n"))
            {
                row.owner.appendFromText (t);
                return;
            }

            juce::TextEditor::insertTextAtCaret (t);
        }

        Row& row;
    };

    explicit Row (InviteListComponent& o) : owner (o), name (*this), mail (*this)
    {
        const auto& theme = AbcTrainTheme::current();

        for (auto* e : { &name, &mail })
        {
            e->setFont (LnF::bodyFont());
            e->setIndents (8, 8);
            e->setColour (juce::TextEditor::backgroundColourId, theme.displayBackground);
            e->setColour (juce::TextEditor::textColourId, theme.textBright);
            e->setColour (juce::TextEditor::outlineColourId, theme.outline);
            e->setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
            e->onTextChange = [this] { owner.rowChanged (this); };
            addAndMakeVisible (e);
        }

        mail.setInputRestrictions (120);
        name.setInputRestrictions (60);
        name.onReturnKey = [this] { owner.focusNext (this, false); };
        mail.onReturnKey = [this] { owner.focusNext (this, true); };
        mail.onFocusLost = [this] { refreshMailOutline(); };

        removeButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xc3\x97")));
        removeButton.onClick = [this] { owner.removeRow (this); };
        addAndMakeVisible (removeButton);
    }

    bool isEmpty() const { return name.getText().trim().isEmpty() && mail.getText().trim().isEmpty(); }

    void refreshMailOutline()
    {
        const auto& theme = AbcTrainTheme::current();
        const auto m = mail.getText().trim();
        mail.setColour (juce::TextEditor::outlineColourId,
                        m.isEmpty() || looksLikeMail (m) ? theme.outline : theme.negative);
        mail.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        auto r = getLocalBounds();
        r.removeFromRight (36);
        auto codeBox = r.removeFromRight (80);

        g.setColour (code.isNotEmpty() && ! isEmpty() ? theme.textBright : theme.textDim);
        g.setFont (LnF::monoFont().withHeight (17.0f));
        LnF::fitText (g, isEmpty() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")) : code, codeBox,
                      juce::Justification::centred, false);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 3);
        removeButton.setBounds (r.removeFromRight (32).withSizeKeepingCentre (30, 30));
        r.removeFromRight (84);   // the code, painted
        const auto half = (r.getWidth() - 8) * 45 / 100;
        name.setBounds (r.removeFromLeft (half));
        r.removeFromLeft (8);
        mail.setBounds (r);
    }

    InviteListComponent& owner;
    Field name, mail;
    juce::TextButton removeButton;
    juce::String code;
};

// ---- the list -----------------------------------------------------------

InviteListComponent::InviteListComponent()
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);
    ensureTrailingEmptyRow();
}

InviteListComponent::~InviteListComponent() = default;

void InviteListComponent::setStrings (const Strings& s)
{
    text = s;
    const auto& theme = AbcTrainTheme::current();

    for (auto* row : rows)
    {
        row->name.setTextToShowWhenEmpty (text.namePlaceholder, theme.textDim);
        row->mail.setTextToShowWhenEmpty (text.mailPlaceholder, theme.textDim);
        row->removeButton.setTooltip (text.remove);
    }

    repaint();
}

int InviteListComponent::getNumPeople() const
{
    int n = 0;
    for (auto* row : rows)
        n += row->isEmpty() ? 0 : 1;
    return n;
}

std::vector<LocalRoom::Invitee> InviteListComponent::getInvitees() const
{
    std::vector<LocalRoom::Invitee> out;

    for (auto* row : rows)
        if (! row->isEmpty())
            out.push_back ({ row->name.getText().trim(), row->mail.getText().trim(), row->code });

    return out;
}

void InviteListComponent::setInvitees (const std::vector<LocalRoom::Invitee>& people)
{
    rows.clear();
    content.removeAllChildren();

    for (const auto& p : people)
    {
        auto* row = rows.add (new Row (*this));
        content.addAndMakeVisible (row);
        row->name.setText (p.name, false);
        row->mail.setText (p.mail, false);
        row->code = p.code;
        row->refreshMailOutline();
    }

    ensureTrailingEmptyRow();
    setStrings (text);
    layoutRows();
}

std::vector<std::pair<juce::String, juce::String>> InviteListComponent::parsePeople (const juce::String& pasted)
{
    std::vector<std::pair<juce::String, juce::String>> people;
    juce::StringArray lines;
    lines.addLines (pasted);
    lines.trim();
    lines.removeEmptyStrings();

    for (const auto& line : lines)
    {
        const auto [name, mail] = splitLine (line);

        // A header row copied along with the table: no address in it, and
        // it names the columns.
        if (mail.isEmpty())
        {
            const auto lower = line.toLowerCase();
            const auto nameWord = juce::String (juce::CharPointer_UTF8 ("\xd0\xb8\xd0\xbc\xd1\x8f"));     // имя
            const auto mailWord = juce::String (juce::CharPointer_UTF8 ("\xd0\xbf\xd0\xbe\xd1\x87\xd1\x82")); // почт

            if (lower.startsWith ("name") || lower.startsWith (nameWord) || lower.contains ("e-mail")
                || lower.contains ("email") || lower.contains (mailWord))
                continue;
        }

        if (name.isNotEmpty() || mail.isNotEmpty())
            people.emplace_back (name, mail);
    }

    return people;
}

void InviteListComponent::appendFromText (const juce::String& pasted)
{
    // Filled from the first empty row on, so pasting into the trailing
    // empty row of a list of five adds people six, seven...
    for (const auto& [name, mail] : parsePeople (pasted))
    {
        Row* target = nullptr;
        for (auto* row : rows)
            if (row->isEmpty())
            {
                target = row;
                break;
            }

        if (target == nullptr)
        {
            target = rows.add (new Row (*this));
            content.addAndMakeVisible (target);
        }

        target->name.setText (name, false);
        target->mail.setText (mail, false);
        target->refreshMailOutline();
        rowChanged (target);
    }

    ensureTrailingEmptyRow();
    setStrings (text);
    layoutRows();

    if (onChanged)
        onChanged();
}

void InviteListComponent::ensureTrailingEmptyRow()
{
    if (rows.isEmpty() || ! rows.getLast()->isEmpty())
    {
        auto* row = rows.add (new Row (*this));
        content.addAndMakeVisible (row);
        row->name.setTextToShowWhenEmpty (text.namePlaceholder, AbcTrainTheme::current().textDim);
        row->mail.setTextToShowWhenEmpty (text.mailPlaceholder, AbcTrainTheme::current().textDim);
        layoutRows();
    }
}

void InviteListComponent::rowChanged (Row* row)
{
    // A code as soon as the row has somebody in it; the same code for as
    // long as the row lives, so a printed card stays right after an edit.
    if (! row->isEmpty() && row->code.isEmpty())
    {
        juce::StringArray taken;
        for (auto* r : rows)
            if (r->code.isNotEmpty())
                taken.add (r->code);

        row->code = LocalRoom::makeCode (random, taken);
    }

    ensureTrailingEmptyRow();
    row->removeButton.setVisible (! row->isEmpty());
    row->repaint();

    if (onChanged)
        onChanged();
}

void InviteListComponent::removeRow (Row* row)
{
    if (row == rows.getLast() && row->isEmpty())
        return;

    content.removeChildComponent (row);
    rows.removeObject (row);
    ensureTrailingEmptyRow();
    layoutRows();

    if (onChanged)
        onChanged();
}

void InviteListComponent::focusNext (Row* row, bool fromMail)
{
    if (! fromMail)
    {
        row->mail.grabKeyboardFocus();
        return;
    }

    row->refreshMailOutline();
    ensureTrailingEmptyRow();
    const auto i = rows.indexOf (row);

    if (auto* next = rows[i + 1])
    {
        next->name.grabKeyboardFocus();
        viewport.setViewPosition (0, juce::jmax (0, next->getBottom() - viewport.getHeight()));
    }
}

void InviteListComponent::layoutRows()
{
    // The empty row at the bottom has nothing to remove.
    for (auto* row : rows)
        row->removeButton.setVisible (! row->isEmpty());

    const auto w = juce::jmax (100, viewport.getWidth() - (rows.size() * rowHeight > viewport.getHeight() ? 10 : 0));
    content.setSize (w, rows.size() * rowHeight);

    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * rowHeight, w, rowHeight);
}

void InviteListComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    auto header = getLocalBounds().removeFromTop (headerHeight);
    header.removeFromRight (36);
    auto codeBox = header.removeFromRight (80);
    const auto half = (header.getWidth() - 8) * 45 / 100;

    g.setColour (theme.textDim);
    g.setFont (LnF::labelFont());
    LnF::fitText (g, text.name, header.removeFromLeft (half).withTrimmedLeft (2), juce::Justification::centredLeft, true);
    header.removeFromLeft (8);
    LnF::fitText (g, text.mail, header.withTrimmedLeft (2), juce::Justification::centredLeft, true);
    LnF::fitText (g, text.code, codeBox, juce::Justification::centred, true);
}

void InviteListComponent::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
    layoutRows();
}
