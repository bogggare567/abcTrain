#include "LiveScreenComponent.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

namespace
{
    constexpr int tabsRowHeight = 36;
    constexpr int controlHeight = 32;
    constexpr int cardTitleHeight = 44;
    constexpr int labelColumn = 150;

    using LnF = AbcTrainLookAndFeel;

    juce::String randomRoomCode (juce::Random& r)
    {
        return juce::String (r.nextInt ({ 100, 999 })) + " " + juce::String (r.nextInt ({ 100, 999 }));
    }

    // Letters that cannot be misread for digits or each other.
    juce::String randomSignInCode (juce::Random& r)
    {
        const juce::String alphabet ("ACDEFHJKMNPQRTUVWXY34679");
        juce::String code;

        for (int i = 0; i < 6; ++i)
        {
            if (i == 3)
                code << "-";
            code << alphabet[r.nextInt (alphabet.length())];
        }

        return code;
    }
}

LiveScreenComponent::LiveScreenComponent()
{
    setOpaque (true);

    tabs.onChange = [this] (int value) { showTab ((Tab) value); };
    addAndMakeVisible (tabs);

    accountButton.onClick = [this] { openSignIn(); };
    addAndMakeVisible (accountButton);

    // ---- seminar: join ----
    codeEditor.setInputRestrictions (7, "0123456789 ");
    codeEditor.setJustification (juce::Justification::centred);
    codeEditor.setFont (LnF::monoFont().withHeight (24.0f));
    codeEditor.setTextToShowWhenEmpty ("482 913", AbcTrainTheme::current().textDim);
    addChildComponent (codeEditor);

    joinButton.onClick = [this] { setNote (text.notYet); };
    addChildComponent (joinButton);

    // ---- seminar: host ----
    whereChoice.onChange = [this] (int) { refreshVisibility(); repaint(); };
    whoChoice.onChange = [this] (int) { refreshVisibility(); repaint(); };
    whereChoice.setValue (1);
    whoChoice.setValue (0);
    roundsChoice.setOptions ({ 5, 10, 15 }, { "5", "10", "15" });
    roundsChoice.setValue (10);

    for (auto* c : { &whereChoice, &whoChoice, &roundsChoice })
    {
        c->setUppercase (false);
        addChildComponent (*c);
    }

    for (int i = 0; i < 4; ++i)
    {
        auto* toggle = familyToggles.add (new juce::TextButton());
        toggle->setClickingTogglesState (true);
        toggle->setToggleState (i == 0, juce::dontSendNotification);
        toggle->onStateChange = [toggle] { LnF::makePrimary (*toggle, toggle->getToggleState()); };
        LnF::makePrimary (*toggle, toggle->getToggleState());
        addChildComponent (toggle);
    }

    invitesButton.onClick = [this]
    {
        overlay = Overlay::invites;
        layoutOverlay();
        refreshVisibility();
        repaint();
    };
    addChildComponent (invitesButton);

    openRoomButton.onClick = [this]
    {
        juce::Random r;
        roomCode = randomRoomCode (r);
        roomOpen = true;
        resized();
        repaint();
    };
    LnF::makePrimary (openRoomButton, true);
    addChildComponent (openRoomButton);

    projectorButton.onClick = [this] { setNote (text.notYet); };
    startButton.onClick = [this] { setNote (text.notYet); };
    LnF::makePrimary (startButton, true);
    closeRoomButton.onClick = [this]
    {
        roomOpen = false;
        resized();
        repaint();
    };

    for (auto* b : { &projectorButton, &startButton, &closeRoomButton })
        addChildComponent (b);

    // ---- battle ----
    battleFamily.setValue (0);
    battleFamily.setUppercase (false);
    addChildComponent (battleFamily);
    searchButton.onClick = [this] { setNote (text.notYet); };
    challengeButton.onClick = [this] { setNote (text.notYet); };
    battleSignInButton.onClick = [this] { openSignIn(); };
    LnF::makePrimary (searchButton, true);

    for (auto* b : { &searchButton, &challengeButton, &battleSignInButton })
        addChildComponent (b);

    // ---- rating ----
    ratingFamily.setValue (0);
    scopeChoice.setValue (0);
    periodChoice.setValue (0);

    for (auto* c : { &ratingFamily, &scopeChoice, &periodChoice })
    {
        c->setUppercase (false);
        addChildComponent (*c);
    }

    openSiteButton.onClick = [] { juce::URL ("https://soundkorb.ru/abctrain/rating").launchInDefaultBrowser(); };
    addChildComponent (openSiteButton);

    // ---- overlays ----
    overlayPrimary.onClick = [this]
    {
        if (overlay == Overlay::signIn)
        {
            // A browser opened by a click is the player's own request; the
            // app itself sends nothing.
            juce::URL ("https://soundkorb.ru/link").withParameter ("code", signInCode).launchInDefaultBrowser();
            return;
        }

        overlay = Overlay::none;
        stopTimer();
        refreshVisibility();
        repaint();
    };

    overlaySecondary.onClick = [this]
    {
        if (overlay == Overlay::signIn)
        {
            overlay = Overlay::none;
            stopTimer();
            refreshVisibility();
            repaint();
            return;
        }

        setNote (text.notYet);   // print / mail the codes
    };

    overlayTertiary.onClick = [this] { makeCodes(); };
    LnF::makePrimary (overlayPrimary, true);

    inviteEditor.setMultiLine (true, false);
    inviteEditor.setReturnKeyStartsNewLine (true);
    inviteEditor.setFont (LnF::bodyFont());

    for (auto* c : { (juce::Component*) &overlayPrimary, (juce::Component*) &overlaySecondary,
                     (juce::Component*) &overlayTertiary, (juce::Component*) &inviteEditor })
        overlayLayer.addChildComponent (c);

    addChildComponent (overlayLayer);

    setStrings ({});
    showTab (Tab::seminar);
}

LiveScreenComponent::~LiveScreenComponent() = default;

void LiveScreenComponent::setStrings (Strings newStrings)
{
    text = std::move (newStrings);

    tabs.setOptions ({ 0, 1, 2 }, { text.seminar, text.battle, text.rating });
    tabs.setValue ((int) tab);
    accountButton.setButtonText (text.signIn);
    joinButton.setButtonText (text.join);
    whereChoice.setOptions ({ 0, 1 }, { text.online, text.local });
    whoChoice.setOptions ({ 0, 1 }, { text.anyone, text.listOnly });

    const juce::String fams[] { text.freq, text.dyn, text.space, text.character };

    for (int i = 0; i < familyToggles.size(); ++i)
        familyToggles[i]->setButtonText (fams[i]);

    battleFamily.setOptions ({ 0, 1, 2, 3 }, { text.freq, text.dyn, text.space, text.character });
    ratingFamily.setOptions ({ 0, 1, 2, 3 }, { text.freq, text.dyn, text.space, text.character });
    scopeChoice.setOptions ({ 0, 1 }, { text.world, text.country });
    periodChoice.setOptions ({ 0, 1 }, { text.season, text.allTime });

    invitesButton.setButtonText (text.invites);
    openRoomButton.setButtonText (text.openRoom);
    projectorButton.setButtonText (text.projector);
    startButton.setButtonText (text.start);
    closeRoomButton.setButtonText (text.closeRoom);
    searchButton.setButtonText (text.search);
    challengeButton.setButtonText (text.challenge);
    battleSignInButton.setButtonText (text.signIn);
    openSiteButton.setButtonText (text.openOnSite);
    inviteEditor.setTextToShowWhenEmpty (juce::String::fromUTF8 ("\xd0\x98\xd0\xb2\xd0\xb0\xd0\xbd \xd0\x9f\xd0\xb5\xd1\x82\xd1\x80\xd0\xbe\xd0\xb2, ivan@school.ru"),
                                         AbcTrainTheme::current().textDim);

    const auto& theme = AbcTrainTheme::current();

    for (auto* editor : { &codeEditor, &inviteEditor })
    {
        editor->setColour (juce::TextEditor::backgroundColourId, theme.displayBackground);
        editor->setColour (juce::TextEditor::textColourId, theme.textBright);
        editor->setColour (juce::TextEditor::outlineColourId, theme.outline);
        editor->setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
        editor->applyColourToAllText (theme.textBright);
    }

    layoutOverlay();
    resized();
    repaint();
}

void LiveScreenComponent::showTab (Tab newTab)
{
    tab = newTab;
    tabs.setValue ((int) tab);
    note.clear();
    refreshVisibility();
    resized();
    repaint();
}

void LiveScreenComponent::openSignIn()
{
    juce::Random r;
    signInCode = randomSignInCode (r);
    signInStartedMs = juce::Time::getMillisecondCounterHiRes();
    overlay = Overlay::signIn;
    startTimer (1000);
    layoutOverlay();
    refreshVisibility();
    repaint();
}

void LiveScreenComponent::openRoomForSnapshot (bool local)
{
    showTab (Tab::seminar);
    whereChoice.setValue (local ? 1 : 0);
    whoChoice.setValue (1);
    roomCode = "482 913";
    roomOpen = true;
    refreshVisibility();
    resized();
}

void LiveScreenComponent::openInvitesForSnapshot()
{
    showTab (Tab::seminar);
    whereChoice.setValue (1);
    whoChoice.setValue (1);
    inviteEditor.setText (juce::String::fromUTF8 (
        "\xd0\x98\xd0\xb2\xd0\xb0\xd0\xbd \xd0\x9f\xd0\xb5\xd1\x82\xd1\x80\xd0\xbe\xd0\xb2, ivan.p@school.ru\n"
        "\xd0\x9c\xd0\xb0\xd1\x80\xd0\xb8\xd1\x8f \xd0\xa1\xd0\xbe\xd0\xba\xd0\xbe\xd0\xbb\xd0\xbe\xd0\xb2\xd0\xb0, m.sokolova@school.ru\n"
        "\xd0\x90\xd1\x80\xd1\x82\xd1\x91\xd0\xbc \xd0\x9a\xd0\xb8\xd0\xbc, a.kim@school.ru"));
    overlay = Overlay::invites;
    makeCodes();
    layoutOverlay();
    refreshVisibility();
}

void LiveScreenComponent::makeCodes()
{
    invitees.clear();
    juce::Random r (0x5eed);   // stable within a session is enough for a skeleton

    for (auto line : juce::StringArray::fromLines (inviteEditor.getText()))
    {
        line = line.trim();

        if (line.isEmpty())
            continue;

        Invitee person;
        person.name = line.upToFirstOccurrenceOf (",", false, false).trim();
        person.mail = line.fromFirstOccurrenceOf (",", false, false).trim();
        person.code = juce::String (r.nextInt ({ 1000, 9999 }));
        invitees.push_back (person);
    }

    repaint();
}

juce::String LiveScreenComponent::roomAddress() const
{
    if (whereChoice.getValue() == 1)
        return juce::IPAddress::getLocalAddress().toString() + ":8930";

    return "soundkorb.ru/live/" + roomCode.removeCharacters (" ");
}

void LiveScreenComponent::setNote (const juce::String& newNote)
{
    note = newNote;
    repaint();
}

void LiveScreenComponent::timerCallback()
{
    if (overlay != Overlay::signIn)
    {
        stopTimer();
        return;
    }

    overlayLayer.repaint();
}

void LiveScreenComponent::refreshVisibility()
{
    const auto seminar = tab == Tab::seminar;
    const auto battle = tab == Tab::battle;
    const auto rating = tab == Tab::rating;
    const auto hosting = seminar && ! roomOpen;

    codeEditor.setVisible (seminar);
    joinButton.setVisible (seminar);

    for (auto* c : { (juce::Component*) &whereChoice, (juce::Component*) &whoChoice,
                     (juce::Component*) &roundsChoice, (juce::Component*) &openRoomButton })
        c->setVisible (hosting);

    for (auto* toggle : familyToggles)
        toggle->setVisible (hosting);

    invitesButton.setVisible (seminar && whoChoice.getValue() == 1);

    for (auto* b : { &projectorButton, &startButton, &closeRoomButton })
        b->setVisible (seminar && roomOpen);

    for (auto* c : { (juce::Component*) &battleFamily, (juce::Component*) &searchButton,
                     (juce::Component*) &challengeButton, (juce::Component*) &battleSignInButton })
        c->setVisible (battle);

    for (auto* c : { (juce::Component*) &ratingFamily, (juce::Component*) &scopeChoice,
                     (juce::Component*) &periodChoice, (juce::Component*) &openSiteButton })
        c->setVisible (rating);

    const auto overlayOpen = overlay != Overlay::none;
    overlayPrimary.setVisible (overlayOpen);
    overlaySecondary.setVisible (overlayOpen);
    overlayTertiary.setVisible (overlay == Overlay::invites);
    inviteEditor.setVisible (overlay == Overlay::invites);

    // The layer covers the page and takes its clicks while it is open.
    overlayLayer.setVisible (overlayOpen);
    overlayLayer.toFront (overlayOpen);

    if (overlay == Overlay::signIn)
    {
        overlayPrimary.setButtonText (text.openSite);
        overlaySecondary.setButtonText (text.cancel);
    }
    else if (overlay == Overlay::invites)
    {
        overlayPrimary.setButtonText (text.done);
        overlaySecondary.setButtonText (whereChoice.getValue() == 1 ? text.printCodes : text.mailCodes);
        overlayTertiary.setButtonText (text.makeCodes);
    }

}

// ---- layout -----------------------------------------------------------

void LiveScreenComponent::resized()
{
    using namespace AbcTrainTheme;
    auto area = getLocalBounds().reduced (Spacing::large, Spacing::medium);

    auto top = area.removeFromTop (tabsRowHeight);
    tabs.setBounds (top.removeFromLeft (juce::jmax (300, tabs.getPreferredWidth())));
    accountButton.setBounds (top.removeFromRight (160));

    area.removeFromTop (Spacing::large);
    area.removeFromBottom (28);   // the note line

    if (tab == Tab::seminar)       layoutSeminar (area);
    else if (tab == Tab::battle)   layoutBattle (area);
    else                           layoutRating (area);

    layoutOverlay();
}

void LiveScreenComponent::layoutSeminar (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    cardA = area.removeFromLeft (area.getWidth() * 38 / 100);
    area.removeFromLeft (Spacing::large);
    cardB = area;

    {
        auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + 70);
        a.removeFromTop (22);   // "Room code" caption
        codeEditor.setBounds (a.removeFromTop (48));
        a.removeFromTop (Spacing::medium);
        joinButton.setBounds (a.removeFromTop (controlHeight + 4));
    }

    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);

    if (roomOpen)
    {
        auto buttons = b.removeFromBottom (controlHeight + 4);
        startButton.setBounds (buttons.removeFromRight (140));
        buttons.removeFromRight (Spacing::small);
        projectorButton.setBounds (buttons.removeFromRight (juce::jmin (220, buttons.getWidth() / 2)));
        closeRoomButton.setBounds (buttons.removeFromLeft (juce::jmin (200, buttons.getWidth())));
        invitesButton.setBounds (b.removeFromBottom (controlHeight + 4 + Spacing::medium).withTrimmedBottom (Spacing::medium)
                                   .withWidth (200));
        return;
    }

    const auto row = [&b] (int height)
    {
        auto r = b.removeFromTop (height);
        b.removeFromTop (AbcTrainTheme::Spacing::medium);
        return r.withTrimmedLeft (labelColumn);
    };

    whereChoice.setBounds (row (controlHeight + 30).removeFromTop (controlHeight)
                               .withWidth (juce::jmin (b.getWidth() - labelColumn, juce::jmax (320, whereChoice.getPreferredWidth()))));
    whoChoice.setBounds (row (controlHeight).withWidth (juce::jmin (b.getWidth() - labelColumn, juce::jmax (240, whoChoice.getPreferredWidth()))));

    {
        auto r = row (controlHeight);
        const auto w = juce::jmax (80, (r.getWidth() - 3 * 6) / 4);

        for (auto* toggle : familyToggles)
        {
            toggle->setBounds (r.removeFromLeft (w));
            r.removeFromLeft (6);
        }
    }

    roundsChoice.setBounds (row (controlHeight).withWidth (180));

    auto buttons = b.removeFromBottom (controlHeight + 4);
    openRoomButton.setBounds (buttons.removeFromRight (200));
    buttons.removeFromRight (Spacing::small);
    invitesButton.setBounds (buttons.removeFromRight (200));
}

void LiveScreenComponent::layoutBattle (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    cardA = area.removeFromLeft (area.getWidth() * 38 / 100);
    area.removeFromLeft (Spacing::large);
    cardB = area;

    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
    battleFamily.setBounds (b.removeFromTop (controlHeight).withWidth (juce::jmin (b.getWidth(), juce::jmax (420, battleFamily.getPreferredWidth()))));
    b.removeFromTop (Spacing::medium + 30);   // the rules line
    auto buttons = b.removeFromTop (controlHeight + 4);
    searchButton.setBounds (buttons.removeFromLeft (220));
    buttons.removeFromLeft (Spacing::small);
    challengeButton.setBounds (buttons.removeFromLeft (220));

    auto bottom = b.removeFromBottom (controlHeight + 4);
    battleSignInButton.setBounds (bottom.removeFromLeft (160));
}

void LiveScreenComponent::layoutRating (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    auto filters = area.removeFromTop (controlHeight);
    ratingFamily.setBounds (filters.removeFromLeft (juce::jmax (420, ratingFamily.getPreferredWidth())));
    filters.removeFromLeft (Spacing::medium);
    openSiteButton.setBounds (filters.removeFromRight (220));
    filters.removeFromRight (Spacing::medium);
    periodChoice.setBounds (filters.removeFromRight (juce::jmax (200, periodChoice.getPreferredWidth())));
    filters.removeFromRight (Spacing::small);
    scopeChoice.setBounds (filters.removeFromRight (juce::jmax (180, scopeChoice.getPreferredWidth())));

    area.removeFromTop (Spacing::large);
    tableArea = area;
    cardA = cardB = {};
}

void LiveScreenComponent::layoutOverlay()
{
    using namespace AbcTrainTheme;
    overlayLayer.setBounds (getLocalBounds());
    const auto height = overlay == Overlay::invites ? 520 : 400;
    overlayBox = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 40, 700), juce::jmin (getHeight() - 20, height));

    auto box = overlayBox.reduced (Spacing::large + 8);
    auto buttons = box.removeFromBottom (controlHeight + 4);
    overlayPrimary.setBounds (buttons.removeFromRight (180));
    buttons.removeFromRight (Spacing::small);
    overlaySecondary.setBounds (buttons.removeFromRight (juce::jmin (240, buttons.getWidth() / 2)));
    overlayTertiary.setBounds (buttons.removeFromLeft (juce::jmin (180, buttons.getWidth())));

    if (overlay == Overlay::invites)
        inviteEditor.setBounds (box.withTrimmedTop (cardTitleHeight + 26).removeFromTop (96));
}

// ---- painting ---------------------------------------------------------

void LiveScreenComponent::paintCard (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title) const
{
    const auto& theme = AbcTrainTheme::current();
    g.setColour (theme.panelBackground);
    g.fillRect (card);
    g.setColour (theme.outline);
    g.drawRect (card, 1);

    g.setColour (theme.textBright);
    g.setFont (LnF::headingFont().withHeight (22.0f));
    LnF::fitText (g, title, card.reduced (AbcTrainTheme::Spacing::large).removeFromTop (cardTitleHeight - 10),
                  juce::Justification::topLeft, true);
}

void LiveScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    g.fillAll (theme.windowBackground);

    // Account state beside the button.
    {
        auto top = getLocalBounds().reduced (AbcTrainTheme::Spacing::large, AbcTrainTheme::Spacing::medium)
                       .removeFromTop (tabsRowHeight);
        top.removeFromRight (160 + AbcTrainTheme::Spacing::medium);
        g.setColour (theme.textDim);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.notSignedIn, top.removeFromRight (220), juce::Justification::centredRight, true);
    }

    if (tab == Tab::seminar)       paintSeminar (g);
    else if (tab == Tab::battle)   paintBattle (g);
    else                           paintRating (g);

    if (note.isNotEmpty())
    {
        auto line = getLocalBounds().reduced (AbcTrainTheme::Spacing::large, AbcTrainTheme::Spacing::medium).removeFromBottom (22);
        g.setColour (theme.accentWarm);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, note, line, juce::Justification::centredLeft, true);
    }

}

void LiveScreenComponent::paintSeminar (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();

    paintCard (g, cardA, text.joinTitle);
    {
        auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitLines (g, text.joinHint, a.removeFromTop (66), juce::Justification::topLeft, 3, 0.9f);
        a.removeFromTop (4);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.joinCode, a.removeFromTop (20), juce::Justification::centredLeft, true);
    }

    if (roomOpen)
    {
        const auto local = whereChoice.getValue() == 1;
        paintCard (g, cardB, local ? text.roomOpenLocal : text.roomOpenOnline);

        auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
        auto qrBox = b.removeFromLeft (170).removeFromTop (170).toFloat();

        // Where the QR will be drawn once there is a URL to put in it.
        g.setColour (theme.displayBackground);
        g.fillRect (qrBox);
        g.setColour (theme.outline);
        g.drawRect (qrBox, 1.0f);
        LnF::drawTrackedText (g, "QR", qrBox, LnF::headingFont(), theme.textDim, 2.0f, juce::Justification::centred);

        b.removeFromLeft (Spacing::large);
        auto info = b.removeFromTop (170);
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, text.typeAddress, info.removeFromTop (24), juce::Justification::centredLeft, true);
        g.setColour (theme.textBright);
        g.setFont (LnF::monoFont().withHeight (24.0f));
        LnF::fitText (g, roomAddress(), info.removeFromTop (40), juce::Justification::centredLeft, true);

        if (! local)
        {
            g.setFont (LnF::monoFont().withHeight (18.0f));
            g.setColour (theme.text);
            LnF::fitText (g, roomCode, info.removeFromTop (28), juce::Justification::centredLeft, true);
        }

        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, local ? text.sameWifi : text.onlineHint, info.removeFromTop (44), juce::Justification::topLeft, 2, 0.9f);

        auto rest = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + 186);
        const auto total = whoChoice.getValue() == 1 && ! invitees.empty() ? (int) invitees.size() : 0;
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, text.joined + "  0" + (total > 0 ? " / " + juce::String (total) : juce::String()),
                      rest.removeFromTop (24), juce::Justification::centredLeft, true);
        auto bar = rest.removeFromTop (10).toFloat();
        g.setColour (theme.displayBackground);
        g.fillRect (bar);
        g.setColour (theme.outline);
        g.drawRect (bar, 1.0f);

        if (local)
        {
            rest.removeFromTop (Spacing::medium);
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitLines (g, text.resultsLocal, rest.removeFromTop (36), juce::Justification::topLeft, 2, 0.9f);
        }

        return;
    }

    paintCard (g, cardB, text.hostTitle);
    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
    const auto label = [&g, &theme] (juce::Rectangle<int> r, const juce::String& s)
    {
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, s, r.withWidth (labelColumn - 12).withHeight (controlHeight), juce::Justification::centredLeft, true);
    };

    auto r = b.removeFromTop (controlHeight + 30);
    label (r, text.where);
    g.setColour (theme.textDim);
    g.setFont (LnF::captionFont());
    LnF::fitText (g, whereChoice.getValue() == 1 ? text.localHint : text.onlineHint,
                  r.withTrimmedLeft (labelColumn).withTrimmedTop (controlHeight + 4), juce::Justification::topLeft, true);
    b.removeFromTop (Spacing::medium);

    label (b.removeFromTop (controlHeight), text.who);
    b.removeFromTop (Spacing::medium);
    label (b.removeFromTop (controlHeight), text.families);
    b.removeFromTop (Spacing::medium);
    label (b.removeFromTop (controlHeight), text.rounds);
}

void LiveScreenComponent::paintBattle (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();

    paintCard (g, cardA, text.decibelo);
    {
        auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
        const juce::String fams[] { text.freq, text.dyn, text.space, text.character };
        const Family families[] { Family::frequency, Family::dynamics, Family::space, Family::character };

        for (int i = 0; i < 4; ++i)
        {
            auto row = a.removeFromTop (40);
            g.setColour (accentFor (families[i]));
            g.fillRect (row.removeFromLeft (8).withSizeKeepingCentre (8, 8));
            row.removeFromLeft (12);
            g.setColour (theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, fams[i], row.removeFromLeft (row.getWidth() - 80), juce::Justification::centredLeft, true);
            g.setColour (theme.textDim);
            g.setFont (LnF::monoFont().withHeight (18.0f));
            LnF::fitText (g, juce::String::fromUTF8 ("\xe2\x80\x94"), row, juce::Justification::centredRight, true);
            g.setColour (theme.divider);
            g.fillRect (a.getX(), row.getBottom(), a.getWidth(), 1);
        }

        a.removeFromTop (Spacing::medium);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.decibeloHint, a.removeFromTop (40), juce::Justification::topLeft, 2, 0.9f);
    }

    paintCard (g, cardB, text.findTitle);
    {
        auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + controlHeight + 6);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.battleRules, b.removeFromTop (24), juce::Justification::centredLeft, true);

        auto bottom = cardB.reduced (Spacing::large);
        bottom = bottom.removeFromBottom (controlHeight + 4 + Spacing::medium + 60);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitLines (g, text.battleNeedsAccount, bottom.removeFromTop (44), juce::Justification::topLeft, 2, 0.85f);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.fairPlay, bottom.removeFromTop (20), juce::Justification::centredLeft, true);
    }
}

void LiveScreenComponent::paintRating (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    auto area = tableArea;

    const juce::String cols[] { text.colRank, text.colNick, text.colCountry, text.colRating, text.colRecord };
    const float widths[] { 0.08f, 0.40f, 0.14f, 0.18f, 0.20f };
    auto header = area.removeFromTop (30);
    auto x = (float) header.getX();

    for (int i = 0; i < 5; ++i)
    {
        const auto w = widths[i] * (float) header.getWidth();
        LnF::drawTrackedText (g, LnF::toCaps (cols[i]), juce::Rectangle<float> (x, (float) header.getY(), w, (float) header.getHeight()),
                              LnF::microFont(), theme.textDim, 1.4f);
        x += w;
    }

    g.setColour (theme.divider);
    g.fillRect (area.getX(), area.getY(), area.getWidth(), 1);

    g.setColour (theme.textDim);
    g.setFont (LnF::bodyFont());
    LnF::fitText (g, text.ratingEmpty, area.withSizeKeepingCentre (area.getWidth(), 40), juce::Justification::centred, true);
}

void LiveScreenComponent::paintOverlay (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (getLocalBounds());

    g.setColour (theme.panelBackground);
    g.fillRect (overlayBox);
    g.setColour (theme.outline);
    g.drawRect (overlayBox, 1);

    auto box = overlayBox.reduced (Spacing::large + 8);
    box.removeFromBottom (controlHeight + 4 + Spacing::medium);

    if (overlay == Overlay::signIn)
    {
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (24.0f));
        LnF::fitText (g, text.signInTitle, box.removeFromTop (36), juce::Justification::centredLeft, true);
        box.removeFromTop (Spacing::medium);

        auto codeRow = box.removeFromTop (76);
        auto codeBox = codeRow.removeFromLeft (260);
        g.setColour (theme.displayBackground);
        g.fillRect (codeBox);
        g.setColour (theme.outline);
        g.drawRect (codeBox, 1);
        g.setColour (theme.textBright);
        g.setFont (LnF::monoFont().withHeight (40.0f));
        LnF::fitText (g, signInCode, codeBox, juce::Justification::centred, true);

        codeRow.removeFromLeft (Spacing::large);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitLines (g, text.signInSteps, codeRow, juce::Justification::centredLeft, 3, 0.85f);

        box.removeFromTop (Spacing::large);
        const auto left = juce::jmax (0, 600 - (int) ((juce::Time::getMillisecondCounterHiRes() - signInStartedMs) / 1000.0));
        const auto time = juce::String (left / 60) + ":" + juce::String (left % 60).paddedLeft ('0', 2);
        g.setColour (theme.accentWarm);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.waiting.replace ("{{time}}", time), box.removeFromTop (22), juce::Justification::centredLeft, true);

        box.removeFromTop (Spacing::medium);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.noPasswords, box.removeFromTop (40), juce::Justification::topLeft, 2, 0.9f);
        return;
    }

    // Invites.
    g.setColour (theme.textBright);
    g.setFont (LnF::headingFont().withHeight (24.0f));
    LnF::fitText (g, text.invitesTitle, box.removeFromTop (36), juce::Justification::centredLeft, true);
    g.setColour (theme.textDim);
    g.setFont (LnF::captionFont());
    LnF::fitText (g, text.invitesHint, box.removeFromTop (26), juce::Justification::centredLeft, true);
    box.removeFromTop (8 + 96 + Spacing::medium);   // the editor

    for (const auto& person : invitees)
    {
        if (box.getHeight() < 26 + 24)
            break;

        auto row = box.removeFromTop (26);
        g.setColour (theme.textBright);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, person.name, row.removeFromLeft (row.getWidth() * 4 / 10), juce::Justification::centredLeft, true);
        g.setColour (theme.textDim);
        LnF::fitText (g, person.mail, row.removeFromLeft (row.getWidth() * 7 / 10), juce::Justification::centredLeft, true);
        g.setColour (theme.textBright);
        g.setFont (LnF::monoFont().withHeight (18.0f));
        LnF::fitText (g, person.code, row, juce::Justification::centredRight, true);
    }

    if (whereChoice.getValue() == 1)
    {
        auto hint = overlayBox.reduced (Spacing::large + 8).removeFromBottom (controlHeight + 4 + Spacing::medium + 22).removeFromTop (22);
        g.setColour (theme.accentWarm);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.localNoMail, hint, juce::Justification::centredLeft, true);
    }
}
