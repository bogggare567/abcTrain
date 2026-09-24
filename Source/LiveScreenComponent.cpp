#include "LiveScreenComponent.h"
#include "shared/updates/Version.h"
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

    accountButton.onClick = [this]
    {
        // Signed in, the same button signs out: this computer forgets its
        // key and tells the server if it can.
        if (account != nullptr && account->isSignedIn())
            account->signOut();
        else
            openSignIn();
    };
    addAndMakeVisible (accountButton);

    // ---- seminar: join ----
    codeEditor.setInputRestrictions (7, "0123456789 ");
    codeEditor.setJustification (juce::Justification::centred);
    codeEditor.setFont (LnF::monoFont().withHeight (24.0f));
    codeEditor.setTextToShowWhenEmpty ("482 913", AbcTrainTheme::current().textDim);
    addChildComponent (codeEditor);

    joinButton.onClick = [this] { if (requireServer()) setNote (text.notYet); };
    addChildComponent (joinButton);

    // ---- seminar: host ----
    whereChoice.onChange = [this] (int)
    {
        if (whereChoice.getValue() == 1)
            lan = LiveLink::scanLan();   // local: only this machine's own addresses, no network traffic
        else
            checkConnection();

        refreshVisibility();
        resized();
        repaint();
    };
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
        // A local room needs an address phones can reach; an online one
        // needs the server. Say which is missing instead of opening a room
        // nobody can enter.
        if (whereChoice.getValue() == 1)
        {
            lan = LiveLink::scanLan();

            if (! lan.any())
            {
                setNote (text.lanNone);
                resized();
                return;
            }
        }
        else if (! requireServer())
        {
            return;
        }

        juce::Random r;
        roomCode = randomRoomCode (r);
        roomOpen = true;
        refreshVisibility();   // the host controls give way to the room - without this they stayed on top of it
        resized();
        repaint();
    };
    LnF::makePrimary (openRoomButton, true);
    addChildComponent (openRoomButton);

    projectorButton.onClick = [this] { setNote (text.notYet); };
    startButton.onClick = [this] { if (whereChoice.getValue() == 1 || requireServer()) setNote (text.notYet); };
    LnF::makePrimary (startButton, true);
    closeRoomButton.onClick = [this]
    {
        roomOpen = false;
        refreshVisibility();
        resized();
        repaint();
    };

    for (auto* b : { &projectorButton, &startButton, &closeRoomButton })
        addChildComponent (b);

    // ---- battle ----
    battleFamily.setValue (0);
    battleFamily.setUppercase (false);
    addChildComponent (battleFamily);
    searchButton.onClick = [this] { if (requireServer()) setNote (text.battlesNext); };
    challengeButton.onClick = [this] { if (requireServer()) setNote (text.battlesNext); };
    battleSignInButton.onClick = [this] { openSignIn(); };

    for (auto* b : { &searchButton, &challengeButton, &battleSignInButton })
        addChildComponent (b);

    // ---- battle: a bot ----
    botChoice.setValue (0);
    botChoice.setUppercase (false);
    botChoice.onChange = [this] (int) { repaint(); };
    addChildComponent (botChoice);

    botStartButton.onClick = [this]
    {
        if (onStartBotBattle != nullptr)
            onStartBotBattle ((BotListener::Bot) juce::jlimit (0, BotListener::numBots - 1, botChoice.getValue()),
                              juce::jlimit (0, 3, battleFamily.getValue()));
    };
    LnF::makePrimary (botStartButton, true);
    addChildComponent (botStartButton);

    // ---- rating ----
    ratingFamily.setValue (0);
    scopeChoice.setValue (0);
    periodChoice.setValue (0);

    for (auto* c : { &ratingFamily, &scopeChoice, &periodChoice })
    {
        c->onChange = [this] (int) { loadRating(); };
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
            // A browser opened by a click is the player's own request.
            if (account == nullptr)
            {
                juce::URL ("https://soundkorb.ru/link").withParameter ("code", signInCode).launchInDefaultBrowser();
                return;
            }

            const auto stage = account->getLink().stage;
            const auto again = stage == LiveAccount::LinkStage::expired || stage == LiveAccount::LinkStage::failed
                            || stage == LiveAccount::LinkStage::idle;

            if (again)
                account->cancelSignIn();

            account->startSignIn (! again);
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
            if (account != nullptr)
                account->cancelSignIn();

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

    checkAgainButton.onClick = [this]
    {
        lan = LiveLink::scanLan();   // a local room's problem is this machine's own network
        checkConnection (true);
        resized();
        repaint();
    };
    addChildComponent (checkAgainButton);

    lan = LiveLink::scanLan();

    setStrings ({});
    showTab (Tab::seminar);
}


void LiveScreenComponent::setStrings (Strings newStrings)
{
    text = std::move (newStrings);

    tabs.setOptions ({ 0, 1, 2 }, { text.seminar, text.battle, text.rating });
    tabs.setValue ((int) tab);
    accountButton.setButtonText (text.signIn);
    checkAgainButton.setButtonText (text.checkAgain);
    refreshAccountButton();
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
    botStartButton.setButtonText (text.botStart);
    {
        std::vector<int> ids;
        for (int i = 0; i < BotListener::numBots; ++i)
            ids.push_back (i);
        botChoice.setOptions (ids, text.botNames);
    }
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

    if (needsServer() && isShowing())
        checkConnection();

    if (tab == Tab::rating && isShowing())
        loadRating();
    refreshVisibility();
    resized();
    repaint();
}

void LiveScreenComponent::openSignIn()
{
    juce::Random r;
    signInCode = randomSignInCode (r);

    if (account != nullptr)
    {
        if (account->isSignedIn())
            return;

        if (LiveLink::networkAllowed.load())
            account->startSignIn (false);
        else
            account->showLinkForSnapshot (signInCode);
    }

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

    // Through the real button, not around it: a shortcut here once hid
    // the host controls staying on top of the open room.
    if (openRoomButton.onClick != nullptr)
        openRoomButton.onClick();

    roomCode = "482 913";
    repaint();
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
        return lan.any() ? lan.address.toString() + ":8930" : juce::String ("-");

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
                     (juce::Component*) &challengeButton, (juce::Component*) &battleSignInButton,
                     (juce::Component*) &botChoice, (juce::Component*) &botStartButton })
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
        const auto stage = account != nullptr ? account->getLink().stage : LiveAccount::LinkStage::waiting;
        const auto again = stage == LiveAccount::LinkStage::expired || stage == LiveAccount::LinkStage::failed;
        overlayPrimary.setButtonText (again ? text.newCode : text.openSite);
        overlaySecondary.setButtonText (text.cancel);
    }
    else if (overlay == Overlay::invites)
    {
        overlayPrimary.setButtonText (text.done);
        overlaySecondary.setButtonText (whereChoice.getValue() == 1 ? text.printCodes : text.mailCodes);
        overlayTertiary.setButtonText (text.makeCodes);
    }


    refreshAccountButton();
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

    // The banner takes the top of the page when something is in the way of
    // what it shows - above the cards, not over them.
    bannerBox = {};
    const auto problem = currentProblem();
    checkAgainButton.setVisible (problem.title.isNotEmpty() && overlay == Overlay::none);

    if (problem.title.isNotEmpty())
    {
        const auto lines = juce::StringArray::fromLines (problem.hints).size();
        bannerBox = area.removeFromTop (52 + 19 * juce::jmin (4, lines));
        area.removeFromTop (Spacing::medium);
        checkAgainButton.setBounds (bannerBox.reduced (Spacing::medium).removeFromTop (controlHeight).removeFromRight (170));
    }

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

    // Right: a bot - the battle that works today, offline.
    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + 26);
    botChoice.setBounds (b.removeFromTop (controlHeight).withWidth (juce::jmin (b.getWidth(), juce::jmax (480, botChoice.getPreferredWidth()))));
    b.removeFromTop (Spacing::medium + 150);   // the bot's profile, painted
    b.removeFromTop (22);                      // "Exercises"
    battleFamily.setBounds (b.removeFromTop (controlHeight).withWidth (juce::jmin (b.getWidth(), juce::jmax (420, battleFamily.getPreferredWidth()))));
    botStartButton.setBounds (cardB.reduced (Spacing::large).removeFromBottom (controlHeight + 4).removeFromRight (260));

    // Left: people - Decibelo, and the buttons that wait for the round server.
    auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + 4 * 40 + Spacing::medium + 44 + Spacing::large + 30);
    searchButton.setBounds (a.removeFromTop (controlHeight + 4));
    a.removeFromTop (Spacing::small);
    challengeButton.setBounds (a.removeFromTop (controlHeight + 4));
    battleSignInButton.setBounds (cardA.reduced (Spacing::large).removeFromBottom (controlHeight + 4).removeFromLeft (160));
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

        juce::String who = text.notSignedIn;
        if (account != nullptr && account->isSignedIn())
            who = account->getNick().isNotEmpty() ? text.signedInAs.replace ("{{nick}}", account->getNick())
                                                  : text.signedInNoNick;

        LnF::fitText (g, who, top.removeFromRight (220), juce::Justification::centredRight, true);
    }

    paintLinkStatus (g, tabs.getBounds().withX (tabs.getRight() + AbcTrainTheme::Spacing::large)
                            .withRight (accountButton.getX() - 230));
    paintBanner (g);

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

            // What to do when a phone cannot open the address - the one
            // question every local seminar gets, answered where it is asked.
            rest.removeFromTop (Spacing::small);
            g.setColour (lan.linkLocal ? theme.accentWarm : theme.textDim);
            LnF::fitLines (g, lan.linkLocal ? text.lanLinkLocal + "\n" + text.lanTrouble : text.lanTrouble,
                           rest.removeFromTop (juce::jmin (rest.getHeight() - controlHeight - 40, 90)),
                           juce::Justification::topLeft, 5, 0.9f);
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
    const Family families[] { Family::frequency, Family::dynamics, Family::space, Family::character };
    const juce::String fams[] { text.freq, text.dyn, text.space, text.character };

    paintCard (g, cardA, text.decibelo);
    {
        auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);

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
        LnF::fitLines (g, text.decibeloHint, a.removeFromTop (44), juce::Justification::topLeft, 2, 0.9f);

        a.removeFromTop (Spacing::large);
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (18.0f));
        LnF::fitText (g, text.humansTitle, a.removeFromTop (30), juce::Justification::centredLeft, true);

        // Under the two buttons: why they only explain themselves for now.
        a.removeFromTop (2 * (controlHeight + 4) + Spacing::small + Spacing::medium);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        const auto signedIn = account != nullptr && account->isSignedIn();
        LnF::fitLines (g, signedIn ? text.battlesNext : text.battlesNext + "\n" + text.battleNeedsAccount,
                       a.withTrimmedBottom (controlHeight + 4 + Spacing::medium), juce::Justification::topLeft, 6, 0.85f);
    }

    paintCard (g, cardB, text.botTitle);
    {
        auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.botHint, b.removeFromTop (22), juce::Justification::topLeft, true);
        b.removeFromTop (4 + controlHeight + Spacing::medium);

        // The selected bot: what it is good at, as four bars - one per
        // family, from its psychometric thresholds - and how it answers.
        const auto bot = (BotListener::Bot) juce::jlimit (0, BotListener::numBots - 1, botChoice.getValue());
        const auto& profile = BotListener::profileOf (bot);
        auto profileBox = b.removeFromTop (150);

        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, text.botSpecialty[(int) bot], profileBox.removeFromTop (26), juce::Justification::centredLeft, true);

        const std::vector<int> gamesOf[] { { 0, 8 }, { 1, 7 }, { 2, 3, 4, 6 }, { 5 } };

        for (int f = 0; f < 4; ++f)
        {
            auto row = profileBox.removeFromTop (22);
            float sum = 0.0f;
            for (auto gi : gamesOf[f])
                sum += profile.threshold[(size_t) gi];
            const auto strength = juce::jlimit (0.05f, 1.0f, (sum / (float) gamesOf[f].size() - 3.5f) / 5.0f);

            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, fams[f], row.removeFromLeft (150), juce::Justification::centredLeft, true);
            auto bar = row.removeFromLeft (juce::jmin (320, row.getWidth() - 20)).withSizeKeepingCentre (juce::jmin (320, row.getWidth() - 20), 8).toFloat();
            g.setColour (theme.displayBackground);
            g.fillRect (bar);
            g.setColour (accentFor (families[f]));
            g.fillRect (bar.withWidth (bar.getWidth() * strength));
        }

        profileBox.removeFromTop (6);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        // Only the extremes get a word: the Bat and the Cat are quick, the
        // Owl and the Elephant take their time; the rest say nothing.
        const auto speed = profile.reactionMs <= 1300 ? text.botSpeedFast
                         : profile.reactionMs >= 2200 ? text.botSpeedSlow : juce::String();
        LnF::fitText (g, "Decibelo " + juce::String (profile.rating)
                           + (speed.isNotEmpty() ? juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + speed : juce::String()),
                      profileBox.removeFromTop (20), juce::Justification::centredLeft, true);

        g.setColour (theme.text);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.botFamily, b.removeFromTop (22), juce::Justification::centredLeft, true);

        auto foot = cardB.reduced (Spacing::large).removeFromBottom (controlHeight + 4);
        foot.removeFromRight (260 + Spacing::large);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.botDisclaimer, foot, juce::Justification::centredLeft, 2, 0.85f);
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

    if (ratingState != RatingState::loaded || ratingRows.isEmpty())
    {
        const auto message = ratingState == RatingState::loading ? text.ratingLoading
                           : ratingState == RatingState::failed  ? text.ratingFailed
                                                                 : text.ratingEmpty;
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, message, area.withSizeKeepingCentre (area.getWidth(), 40), juce::Justification::centred, true);
        return;
    }

    // The rows the server returned, the player's own highlighted. Deviation
    // beside the number and "?" for provisional, as on the site.
    const auto me = account != nullptr ? account->getNick() : juce::String();
    constexpr int rowHeight = 30;

    for (const auto& r : ratingRows)
    {
        if (area.getHeight() < rowHeight)
            break;

        auto row = area.removeFromTop (rowHeight);

        if (me.isNotEmpty() && r.nick.equalsIgnoreCase (me))
        {
            g.setColour (theme.accent.withAlpha (0.15f));
            g.fillRect (row);
        }

        const juce::String cells[] {
            juce::String (r.place),
            r.nick + (r.provisional ? " ?" : ""),
            r.country.isNotEmpty() ? r.country : juce::String ("-"),
            juce::String (r.rating) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb1")) + juce::String (r.deviation),
            juce::String (r.wins) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + juce::String (r.losses)
        };

        auto cx = (float) row.getX();
        for (int i = 0; i < 5; ++i)
        {
            const auto w = widths[i] * (float) row.getWidth();
            g.setColour (i == 1 ? theme.textBright : theme.text);
            g.setFont (i == 1 ? LnF::bodyFont() : LnF::monoFont().withHeight (14.0f));
            LnF::fitText (g, cells[i], juce::Rectangle<float> (cx, (float) row.getY(), w - 8.0f, (float) row.getHeight()).toNearestInt(),
                          juce::Justification::centredLeft, true);
            cx += w;
        }

        g.setColour (theme.divider.withAlpha (0.5f));
        g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);
    }
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
        const auto* linkState = account != nullptr ? &account->getLink() : nullptr;
        const auto shownCode = linkState == nullptr ? signInCode
                             : linkState->code.isNotEmpty() ? linkState->code
                                                            : juce::String (juce::CharPointer_UTF8 ("\xc2\xb7 \xc2\xb7 \xc2\xb7"));
        g.setColour (theme.textBright);
        g.setFont (LnF::monoFont().withHeight (40.0f));
        LnF::fitText (g, shownCode, codeBox, juce::Justification::centred, true);

        codeRow.removeFromLeft (Spacing::large);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitLines (g, text.signInSteps, codeRow, juce::Justification::centredLeft, 3, 0.85f);

        box.removeFromTop (Spacing::large);
        const auto endsAt = linkState != nullptr ? linkState->expiresAtMs : signInStartedMs + 600000.0;
        const auto left = juce::jmax (0, (int) ((endsAt - juce::Time::getMillisecondCounterHiRes()) / 1000.0));
        const auto time = juce::String (left / 60) + ":" + juce::String (left % 60).paddedLeft ('0', 2);

        juce::String status = text.waiting.replace ("{{time}}", time);
        auto statusColour = theme.accentWarm;

        if (linkState != nullptr)
        {
            using Stage = LiveAccount::LinkStage;
            if (linkState->stage == Stage::starting || linkState->stage == Stage::idle) { status = text.linkStarting; statusColour = theme.textDim; }
            if (linkState->stage == Stage::failed)   { status = text.linkFailed;  statusColour = theme.negative; }
            if (linkState->stage == Stage::expired)  { status = text.linkExpired; statusColour = theme.negative; }
        }

        g.setColour (statusColour);
        g.setFont (LnF::labelFont());
        LnF::fitLines (g, status, box.removeFromTop (40), juce::Justification::topLeft, 2, 0.9f);

        box.removeFromTop (Spacing::small);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.noPasswords, box.removeFromTop (36), juce::Justification::topLeft, 2, 0.9f);
        LnF::fitLines (g, text.serverInRussia, box.removeFromTop (36), juce::Justification::topLeft, 2, 0.9f);
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

// ---- the connection ---------------------------------------------------------

class LiveScreenComponent::Checker : public juce::Thread
{
public:
    explicit Checker (LiveScreenComponent& ownerToUse) : juce::Thread ("abcTrain live check"), owner (ownerToUse) {}
    ~Checker() override { stopThread (7000); }

    void run() override
    {
        const auto result = LiveLink::check (CurrentVersion::string);
        juce::Component::SafePointer<LiveScreenComponent> safe (&owner);

        juce::MessageManager::callAsync ([safe, result]
        {
            if (safe == nullptr)
                return;

            safe->link = result.state;
            safe->linkHttpStatus = result.httpStatus;
            safe->lan = result.lan;
            safe->resized();
            safe->repaint();
        });
    }

private:
    LiveScreenComponent& owner;
};

void LiveScreenComponent::checkConnection (bool evenIfRecent)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (! LiveLink::networkAllowed.load())
        return;

    if (checker != nullptr && checker->isThreadRunning())
        return;

    if (! evenIfRecent && now - lastCheckMs < 60000.0 && link != LiveLink::State::unknown)
        return;

    lastCheckMs = now;
    link = LiveLink::State::checking;
    checker = std::make_unique<Checker> (*this);
    checker->startThread();
    resized();
    repaint();
}

void LiveScreenComponent::visibilityChanged()
{
    // Opening Live is the player using Live - the one moment the app may
    // look for the server (the offline rule).
    if (isShowing())
        checkConnection();
}

void LiveScreenComponent::setLinkForSnapshot (LiveLink::State state, bool lanPresent)
{
    link = state;
    lan = {};

    if (lanPresent)
        lan.address = juce::IPAddress ("192.168.1.24");

    resized();
    repaint();
}

bool LiveScreenComponent::needsServer() const
{
    if (tab != Tab::seminar)
        return true;

    // A seminar page needs the server only for an online room; joining by
    // code is online too, but a whole-page banner over a local setup would
    // be noise - that button says so itself when pressed.
    return whereChoice.getValue() == 0;
}

LiveScreenComponent::Problem LiveScreenComponent::currentProblem() const
{
    using S = LiveLink::State;

    if (tab == Tab::seminar && whereChoice.getValue() == 1)
    {
        if (! lan.any())
            return { text.lanNone, text.lanNoneHint };

        return {};
    }

    if (! needsServer())
        return {};

    switch (link)
    {
        case S::noNetwork:   return { text.linkNoNetwork, text.hintNoNetwork };
        case S::noInternet:  return { text.linkNoInternet, text.hintNoInternet };
        case S::serverDown:  return { text.linkServerDown, text.hintServerDown };
        case S::serverError: return { text.linkServerError.replace ("{{code}}", juce::String (linkHttpStatus)), text.hintServerDown };
        case S::appTooOld:   return { text.linkAppTooOld, text.hintAppTooOld };
        case S::unknown: case S::checking: case S::online: break;
    }

    return {};
}

bool LiveScreenComponent::requireServer()
{
    if (link == LiveLink::State::online)
        return true;

    if (link == LiveLink::State::checking)
    {
        setNote (text.linkChecking);
        return false;
    }

    checkConnection (true);

    auto problem = currentProblem();
    setNote (problem.title.isNotEmpty() ? problem.title : text.linkChecking);
    return false;
}

void LiveScreenComponent::paintLinkStatus (juce::Graphics& g, juce::Rectangle<int> area)
{
    using S = LiveLink::State;

    if (link == S::unknown || area.getWidth() < 60)
        return;

    const auto& theme = AbcTrainTheme::current();
    juce::String label;
    juce::Colour colour;

    switch (link)
    {
        case S::checking:    label = text.linkChecking;   colour = theme.textDim; break;
        case S::online:      label = text.linkOnline;     colour = theme.positive; break;
        case S::noNetwork:   label = text.linkNoNetwork;  colour = theme.negative; break;
        case S::noInternet:  label = text.linkNoInternet; colour = theme.negative; break;
        case S::serverDown:  label = text.linkServerDown; colour = theme.accentWarm; break;
        case S::serverError: label = text.linkServerError.replace ("{{code}}", juce::String (linkHttpStatus)); colour = theme.accentWarm; break;
        case S::appTooOld:   label = text.linkAppTooOld;  colour = theme.accentWarm; break;
        case S::unknown:     break;
    }

    auto dot = area.removeFromLeft (14).withSizeKeepingCentre (8, 8).toFloat();
    g.setColour (colour);
    g.fillEllipse (dot);
    area.removeFromLeft (4);
    g.setColour (theme.textDim);
    g.setFont (LnF::labelFont());
    LnF::fitText (g, label, area, juce::Justification::centredLeft, true);
}

void LiveScreenComponent::paintBanner (juce::Graphics& g)
{
    if (bannerBox.isEmpty())
        return;

    const auto& theme = AbcTrainTheme::current();
    const auto problem = currentProblem();

    g.setColour (theme.panelBackground);
    g.fillRect (bannerBox);
    g.setColour (theme.accentWarm);
    g.fillRect (bannerBox.withWidth (3));
    g.setColour (theme.outline);
    g.drawRect (bannerBox, 1);

    auto body = bannerBox.reduced (AbcTrainTheme::Spacing::medium).withTrimmedLeft (8);
    body.removeFromRight (170 + AbcTrainTheme::Spacing::medium);   // "Check again"

    g.setColour (theme.textBright);
    g.setFont (LnF::headingFont().withHeight (18.0f));
    LnF::fitText (g, problem.title, body.removeFromTop (26), juce::Justification::centredLeft, true);
    body.removeFromTop (4);

    g.setFont (LnF::captionFont());
    g.setColour (theme.text);

    const auto lines = juce::StringArray::fromLines (problem.hints);
    for (int i = 0; i < juce::jmin (4, lines.size()); ++i)
        LnF::fitText (g, juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2  ")) + lines[i],
                      body.removeFromTop (19), juce::Justification::centredLeft, true);
}

// Out of line and after Checker: a unique_ptr to it needs the whole type.
LiveScreenComponent::~LiveScreenComponent()
{
    if (account != nullptr)
        account->removeChangeListener (this);

    checker.reset();
}

// ---- the account ----------------------------------------------------------------

void LiveScreenComponent::setAccount (LiveAccount* newAccount)
{
    if (account != nullptr)
        account->removeChangeListener (this);

    account = newAccount;

    if (account != nullptr)
        account->addChangeListener (this);

    refreshAccountButton();
    repaint();
}

void LiveScreenComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    accountChanged();
}

void LiveScreenComponent::refreshAccountButton()
{
    const auto signedIn = account != nullptr && account->isSignedIn();
    accountButton.setButtonText (signedIn ? text.signOut : text.signIn);
    battleSignInButton.setVisible (tab == Tab::battle && ! signedIn);
}

void LiveScreenComponent::accountChanged()
{
    if (account == nullptr)
        return;

    // Linked: the overlay has done its job.
    if (overlay == Overlay::signIn && account->getLink().stage == LiveAccount::LinkStage::approved)
    {
        overlay = Overlay::none;
        stopTimer();
        account->cancelSignIn();   // back to idle; the account itself stays signed in
        setNote (text.signedInDone.replace ("{{nick}}", account->getNick().isNotEmpty() ? account->getNick() : juce::String ("-")));
    }

    refreshVisibility();
    refreshAccountButton();
    layoutOverlay();
    repaint();
    overlayLayer.repaint();
}

void LiveScreenComponent::loadRating()
{
    if (account == nullptr || ! LiveLink::networkAllowed.load())
        return;

    static const char* families[] { "freq", "dyn", "space", "char" };
    const auto family = families[juce::jlimit (0, 3, ratingFamily.getValue())];

    // "Country" is the player's own if the account has one, otherwise Russia
    // - where most players are for now.
    const auto country = scopeChoice.getValue() == 1
                           ? (account->getCountry().isNotEmpty() ? account->getCountry() : juce::String ("RU"))
                           : juce::String();

    ratingState = RatingState::loading;
    repaint();

    juce::Component::SafePointer<LiveScreenComponent> safe (this);
    account->fetchRating (family, country, [safe] (bool ok, juce::Array<LiveAccount::RatingRow> rows)
    {
        if (safe == nullptr)
            return;

        safe->ratingRows = std::move (rows);
        safe->ratingState = ok ? RatingState::loaded : RatingState::failed;
        safe->repaint();
    });
}
