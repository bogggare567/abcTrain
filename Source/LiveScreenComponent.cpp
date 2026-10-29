#include "LiveScreenComponent.h"
#include "shared/ui/CommunityLink.h"
#include <array>
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

    sideChoice.onChange = [this] (int value)
    {
        compactSide = value;
        refreshVisibility();
        resized();
        repaint();
    };
    addChildComponent (sideChoice);

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
    codeEditor.setInputRestrictions (28, "0123456789 .:/httpHTTP");
    codeEditor.setJustification (juce::Justification::centred);
    codeEditor.setFont (LnF::monoFont().withHeight (24.0f));
    codeEditor.setTextToShowWhenEmpty ("192.168.0.14:8930", AbcTrainTheme::current().textDim);
    addChildComponent (codeEditor);

    joinButton.onClick = [this]
    {
        // An address from a presenter's screen ("192.168.0.14:8930"): a local
        // room, opened in the browser - the same page the phones get.
        const auto typed = codeEditor.getText().removeCharacters (" ");

        if (typed.containsChar ('.') || typed.containsChar (':'))
        {
            auto address = typed.startsWith ("http") ? typed : "http://" + typed;
            if (! address.fromFirstOccurrenceOf ("//", false, false).containsChar (':'))
                address << ":8930";

            juce::URL (address).launchInDefaultBrowser();
            return;
        }

        if (requireServer())
            setNote (text.onlineNotYet);
    };
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
       #if JUCE_IOS || JUCE_ANDROID
        return;   // see refreshVisibility: a phone does not open a room
       #endif
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

            openLocalRoom();
            return;
        }

        if (! requireServer())
            return;

        setNote (text.onlineNotYet);
    };
    LnF::makePrimary (openRoomButton, true);
    addChildComponent (openRoomButton);

    projectorButton.onClick = [this]
    {
        if (projector != nullptr)
        {
            projector.reset();
            refreshHostButtons();
            return;
        }

        openProjector();
    };

    // One button, the next step of the round: Start -> Show the answer ->
    // Next -> ... -> Once more.
    startButton.onClick = [this]
    {
        if (seminar == nullptr)
            return;

        switch (seminar->getStage())
        {
            case SeminarHost::Stage::lobby:     seminar->start(); break;
            case SeminarHost::Stage::listening: seminar->reveal(); break;
            case SeminarHost::Stage::revealed:  seminar->next(); break;
            case SeminarHost::Stage::finished:  seminar->again(); break;
            case SeminarHost::Stage::closed:    break;
        }

        refreshHostButtons();
        refreshVisibility();
        resized();
        repaint();
    };
    LnF::makePrimary (startButton, true);

    playAButton.onClick = [this] { if (seminar != nullptr) seminar->setSound (true, false); refreshHostButtons(); };
    playBButton.onClick = [this] { if (seminar != nullptr) seminar->setSound (true, true); refreshHostButtons(); };
    stopButton.onClick = [this] { if (seminar != nullptr) seminar->setSound (false, seminar->isProcessed()); refreshHostButtons(); };

    closeRoomButton.onClick = [this] { closeLocalRoom(); };

    for (auto* b : { &projectorButton, &startButton, &closeRoomButton, &playAButton, &playBButton, &stopButton })
        addChildComponent (b);

    // ---- the host screen and the archive (Sem-Host, Sem-Archive) ----
    anonChoice.setValue (0);
    anonChoice.onChange = [this] (int v) { if (seminar != nullptr) seminar->setAnonymous (v == 1); repaint(); };
    soundChoice.setValue (1);
    soundChoice.onChange = [this] (int v) { if (seminar != nullptr) seminar->setSoundPerRound (v == 1); repaint(); };
    archiveFilter.setValue (1);
    archiveFilter.onChange = [this] (int) { repaint(); };

    archiveButton.onClick = [this]
    {
        archiveOpen = true;
        archiveSelected = 0;
        refreshArchive();
        refreshVisibility();
        resized();
        repaint();
    };
    archiveBackButton.onClick = [this]
    {
        archiveOpen = false;
        refreshVisibility();
        resized();
        repaint();
    };
    exportButton.onClick = [this]
    {
        if (archiveSelected < 0 || archiveSelected >= (int) archive.size())
            return;

        const auto session = archive[(size_t) archiveSelected];
        exportChooser = std::make_unique<juce::FileChooser> (text.archiveExport,
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (session.when.formatted ("abcTrain seminar %Y-%m-%d.csv")), "*.csv");
        exportChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                    [session] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file != juce::File())
                file.replaceWithText (SeminarArchive::toCsv (session));
        });
    };
    repeatButton.onClick = [this]
    {
        if (seminar == nullptr || archiveSelected < 0 || archiveSelected >= (int) archive.size())
            return;

        std::vector<int> games;
        for (const auto& r : archive[(size_t) archiveSelected].rounds)
            if (r.game >= 0 && (r.right < r.answered || ! r.missed.isEmpty()))
                games.push_back (r.game);

        if (games.empty())
        {
            setNote (text.archiveNoMistakes);
            return;
        }

        seminar->setNextPlan (games);
        archiveOpen = false;
        setNote (text.archiveRepeatSet.replace ("{{n}}", juce::String ((int) games.size())));
        refreshVisibility();
        resized();
        repaint();
    };
    LnF::makePrimary (repeatButton, true);

    for (auto* c : { (juce::Component*) &anonChoice, (juce::Component*) &soundChoice, (juce::Component*) &archiveFilter,
                     (juce::Component*) &archiveButton, (juce::Component*) &archiveBackButton,
                     (juce::Component*) &exportButton, (juce::Component*) &repeatButton })
        addChildComponent (c);

    // ---- battle ----
    battleFamily.setValue (0);
    battleFamily.setUppercase (false);
    addChildComponent (battleFamily);
    searchButton.onClick = [this]
    {
        if (readyForYou)
        {
            if (onAcceptMatch != nullptr) onAcceptMatch (true);
            return;
        }

        if (searchingOnline)
        {
            if (onCancelSearch != nullptr) onCancelSearch();
            return;
        }

        if (account == nullptr || ! account->isSignedIn())
        {
            openSignIn();
            return;
        }

        if (requireServer() && onFindOpponent != nullptr)
            onFindOpponent (juce::jlimit (0, 3, battleFamily.getValue()), ratedChoice.getValue() == 1);
    };
    // While a room waits for "Accept", this is "Decline". (Challenges by
    // code need a second queue on the server; not yet - ADR 048.)
    challengeButton.onClick = [this] { if (readyForYou && onAcceptMatch != nullptr) onAcceptMatch (false); };
    battleSignInButton.onClick = [this] { openSignIn(); };

    for (auto* b : { &searchButton, &challengeButton, &battleSignInButton })
        addChildComponent (b);
    addChildComponent (searchRadar);

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

    // ---- battle on a computer (Battle-PC board) ----
    ratedChoice.setValue (0);
    ratedChoice.setUppercase (true);
    ratedChoice.onChange = [this] (int) { repaint(); };
    addChildComponent (ratedChoice);
    addChildComponent (familyTiles);

    for (int i = 0; i < BotListener::numBots; ++i)
    {
        auto* b = ladderButtons.add (new juce::TextButton());
        b->onClick = [this, i]
        {
            const auto bot = BattleRecords::ladder()[(size_t) i];
            botChoice.setValue ((int) bot);

            // A locked bot says what opens it, rather than being a dead
            // button (ClickMap: nothing on a page is there to do nothing).
            if (records != nullptr && ! records->isOpen (bot))
            {
                setNote (text.botNames[(int) bot] + ": " + text.ladderLocked.replace ("{{n}}", juce::String (BattleRecords::priceOf (i))));
                return;
            }

            if (onStartBotBattle != nullptr)
                onStartBotBattle (bot, juce::jlimit (0, 3, battleFamily.getValue()));
        };
        addChildComponent (b);
    }

    botDayButton.onClick = [this]
    {
        if (records == nullptr || onStartBotBattle == nullptr)
            return;
        const auto d = records->daily (juce::Time::getCurrentTime());
        battleFamily.setValue (d.family);
        botChoice.setValue ((int) d.bot);
        onStartBotBattle (d.bot, d.family);
    };
    LnF::makePrimary (botDayButton, true);
    addChildComponent (botDayButton);

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

    // People to battle are found by hand, in the group's "Battles" topic -
    // no matchmaking through Telegram, just the way there.
    communityButton.setComponentID ("live.community");
    communityButton.onClick = [] { CommunityLink::open(); };
    addChildComponent (communityButton);
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

        // Print the cards (local), or - online - mail them, which needs
        // the server that is not running yet.
        if (whereChoice.getValue() != 1)
        {
            setNote (text.onlineNotYet);
            return;
        }

        const auto people = inviteList.getInvitees();

        if (people.empty())
        {
            setNote (text.needPeople);
            return;
        }

        lan = LiveLink::scanLan();
        CodeSheet::open (text.roomTitle, roomAddress(), people, text.sheet);
    };

    // Paste: whatever is on the clipboard, as rows.
    overlayTertiary.onClick = [this]
    {
        inviteList.appendFromText (juce::SystemClipboard::getTextFromClipboard());
    };
    LnF::makePrimary (overlayPrimary, true);

    inviteList.onChanged = [this] { overlayLayer.repaint(); };

    for (auto* c : { (juce::Component*) &overlayPrimary, (juce::Component*) &overlaySecondary,
                     (juce::Component*) &overlayTertiary, (juce::Component*) &inviteList })
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
    sideChoice.setOptions ({ 0, 1 }, { text.humansTitle, text.botTitle });
    sideChoice.setValue (compactSide);
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
    searchButton.setButtonText (searchingOnline ? text.cancelSearch : text.search);
    challengeButton.setButtonText (text.decline);
    battleSignInButton.setButtonText (text.signIn);
    botStartButton.setButtonText (text.botStart);
    ratedChoice.setOptions ({ 0, 1 }, { text.battleRated, text.battleFriendly });
    anonChoice.setOptions ({ 0, 1 }, { text.semNames, text.semAnon });
    soundChoice.setOptions ({ 0, 1 }, { text.semOneSound, text.semSoundPerRound });
    archiveFilter.setOptions ({ 0, 1 }, { text.archiveAll, text.archiveErrors });
    archiveButton.setButtonText (text.archiveButton);
    archiveBackButton.setButtonText (juce::String::fromUTF8 ("\xe2\x80\xb9  ") + text.archiveBack);
    exportButton.setButtonText (text.archiveExport);
    repeatButton.setButtonText (text.archiveRepeat);
    stopButton.setButtonText (text.stopSound);
    botDayButton.setButtonText (text.acceptDay);
    refreshLadder();
    {
        std::vector<int> ids;
        for (int i = 0; i < BotListener::numBots; ++i)
            ids.push_back (i);
        botChoice.setOptions (ids, text.botNames);
    }
    openSiteButton.setButtonText (text.openOnSite);
    communityButton.setButtonText (text.communityFind);
    inviteList.setStrings ({ text.nameCol, text.mailCol, text.codeCol, text.namePlaceholder, text.mailPlaceholder, text.cancel });
    playAButton.setButtonText (text.playA);
    playBButton.setButtonText (text.playB);
    stopButton.setButtonText (text.stopSound);

    if (seminar != nullptr)
        seminar->getRoom().setPageStrings (text.phone);

    if (projector != nullptr)
        projector->getView().setStrings (text.projectorText);

    refreshHostButtons();

    const auto& theme = AbcTrainTheme::current();

    for (auto* editor : { &codeEditor })
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
    if (tab == Tab::battle && isShowing() && onBattleTabShown != nullptr)
        onBattleTabShown();
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
    whoChoice.setValue (0);

    // Through the real button, not around it: a shortcut here once hid
    // the host controls staying on top of the open room.
    if (openRoomButton.onClick != nullptr)
        openRoomButton.onClick();

    repaint();
}

void LiveScreenComponent::openInvitesForSnapshot()
{
    showTab (Tab::seminar);
    whereChoice.setValue (1);
    whoChoice.setValue (1);
    inviteList.setInvitees ({});
    inviteList.appendFromText (juce::String::fromUTF8 (
        "\xd0\x98\xd0\xb2\xd0\xb0\xd0\xbd \xd0\x9f\xd0\xb5\xd1\x82\xd1\x80\xd0\xbe\xd0\xb2\tivan.p@school.ru\n"
        "\xd0\x9c\xd0\xb0\xd1\x80\xd0\xb8\xd1\x8f \xd0\xa1\xd0\xbe\xd0\xba\xd0\xbe\xd0\xbb\xd0\xbe\xd0\xb2\xd0\xb0\tm.sokolova@school\n"
        "\xd0\x90\xd1\x80\xd1\x82\xd1\x91\xd0\xbc \xd0\x9a\xd0\xb8\xd0\xbc; a.kim@school.ru"));
    overlay = Overlay::invites;
    layoutOverlay();
    refreshVisibility();
}

void LiveScreenComponent::runRoundForSnapshot (bool revealed)
{
    openRoomForSnapshot (true);

    if (seminar == nullptr || ! roomOpen)
        return;

    // Three phones' worth of people, answering through the real protocol.
    auto& room = seminar->getRoom();
    const auto code = room.getRoomCode();
    juce::StringArray tokens;

    for (const auto* who : { "Ivan", "Maria", "Artyom" })
    {
        LocalRoom::Request r;
        r.method = "POST";
        r.path = "/api/join";
        r.body = "{\"name\":\"" + juce::String (who) + "\",\"room\":\"" + code + "\"}";
        const auto response = room.handleRequest (r);
        tokens.add (juce::JSON::parse (response.body.toString()).getProperty ("token", {}).toString());
    }

    seminar->start();

    for (int i = 0; i < tokens.size(); ++i)
    {
        LocalRoom::Request r;
        r.method = "POST";
        r.path = "/api/vote";
        r.body = "{\"token\":\"" + tokens[i] + "\",\"choice\":" + juce::String (i) + ",\"value\":" + juce::String (0.3 + 0.1 * i) + "}";
        room.handleRequest (r);
    }

    if (revealed)
        seminar->reveal();

    refreshHostButtons();
    refreshVisibility();
    resized();
    repaint();
}

juce::String LiveScreenComponent::roomAddress() const
{
    if (whereChoice.getValue() == 1)
    {
        const auto port = seminar != nullptr && seminar->getRoom().isOpen() ? seminar->getRoom().getPort() : 8930;
        return lan.any() ? lan.address.toString() + ":" + juce::String (port) : juce::String ("-");
    }

    return "soundkorb.ru/live/" + roomCode.removeCharacters (" ");
}

juce::String LiveScreenComponent::roomJoinUrl() const
{
    if (whereChoice.getValue() != 1 || ! lan.any())
        return {};

    const auto listOnly = whoChoice.getValue() == 1;
    return "http://" + roomAddress() + "/" + (listOnly || seminar == nullptr ? juce::String()
                                                                              : "?r=" + seminar->getRoom().getRoomCode());
}

bool LiveScreenComponent::running() const
{
    return seminar != nullptr && roomOpen
        && seminar->getStage() != SeminarHost::Stage::lobby && seminar->getStage() != SeminarHost::Stage::closed;
}

void LiveScreenComponent::setSeminarHost (SeminarHost* host)
{
    seminar = host;

    if (seminar != nullptr)
    {
        seminar->getRoom().setPageStrings (text.phone);
        seminar->getRoom().onChanged = [safe = juce::Component::SafePointer<LiveScreenComponent> (this)]
        {
            if (safe != nullptr)
                safe->repaint();
        };
    }
}

void LiveScreenComponent::openLocalRoom()
{
    if (seminar == nullptr)
        return;

    const auto listOnly = whoChoice.getValue() == 1;
    const auto people = inviteList.getInvitees();

    if (listOnly && people.empty())
    {
        setNote (text.needPeople);
        return;
    }

    int families = 0;
    for (int i = 0; i < familyToggles.size(); ++i)
        if (familyToggles[i]->getToggleState())
            families |= 1 << i;

    seminar->configure (text.roomTitle, families, roundsChoice.getValue(), listOnly, people);
    seminar->getRoom().setPageStrings (text.phone);

    juce::String why;

    if (! seminar->openRoom (why))
    {
        setNote (text.roomFailed.replace ("{{why}}", why));
        return;
    }

    roomCode = seminar->getRoom().getRoomCode();
    roomOpen = true;
    note.clear();
    refreshHostButtons();
    refreshVisibility();   // the host controls give way to the room - without this they stayed on top of it
    resized();
    repaint();
}

void LiveScreenComponent::closeLocalRoom()
{
    projector.reset();

    if (seminar != nullptr)
        seminar->closeRoom();

    roomOpen = false;
    refreshHostButtons();
    refreshVisibility();
    resized();
    repaint();
}

void LiveScreenComponent::openProjector()
{
    if (seminar == nullptr || ! roomOpen)
        return;

    auto address = [safe = juce::Component::SafePointer<LiveScreenComponent> (this)]
    {
        return safe != nullptr ? safe->roomAddress() : juce::String();
    };

    projector = std::make_unique<ProjectorWindow> (*seminar, address, text.projectorText);

    // Closed from its own title bar: let go of it after the click is over.
    projector->onClosed = [safe = juce::Component::SafePointer<LiveScreenComponent> (this)]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe != nullptr)
            {
                safe->projector.reset();
                safe->refreshHostButtons();
            }
        });
    };

    projector->getView().onChanged = [safe = juce::Component::SafePointer<LiveScreenComponent> (this)]
    {
        if (safe != nullptr)
        {
            safe->refreshHostButtons();
            safe->refreshVisibility();
            safe->resized();
            safe->repaint();
        }
    };

    projector->placeOnBestDisplay();
    refreshHostButtons();
}

void LiveScreenComponent::refreshHostButtons()
{
    projectorButton.setButtonText (projector != nullptr ? text.closeProjector : text.projector);

    if (seminar == nullptr)
    {
        startButton.setButtonText (text.start);
        return;
    }

    switch (seminar->getStage())
    {
        case SeminarHost::Stage::closed:
        case SeminarHost::Stage::lobby:     startButton.setButtonText (text.start); break;
        case SeminarHost::Stage::listening: startButton.setButtonText (text.showAnswer); break;
        case SeminarHost::Stage::revealed:  startButton.setButtonText (seminar->getRound() >= seminar->getTotalRounds() ? text.results : text.nextRound); break;
        case SeminarHost::Stage::finished:  startButton.setButtonText (text.again); break;
    }

    LnF::makePrimary (playAButton, seminar->isPlaying() && ! seminar->isProcessed());
    LnF::makePrimary (playBButton, seminar->isPlaying() && seminar->isProcessed());
    repaint();
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
    const auto onSeminar = tab == Tab::seminar;
    const auto battle = tab == Tab::battle;
    const auto rating = tab == Tab::rating;
    // In a seminar a phone only takes part: it does not offer to open a room
    // (a server on the venue's network, a projector window). Joining by
    // address still works.
   #if JUCE_IOS || JUCE_ANDROID
    constexpr auto canHost = false;
   #else
    constexpr auto canHost = true;
   #endif
    // A phone-width window has no second card to put them in (ADR 060).
    const auto desk = ! compact();
    const auto inArchive = onSeminar && desk && archiveOpen;
    const auto hosting = canHost && onSeminar && ! roomOpen && desk && ! archiveOpen;

    // A room open on a computer takes the whole page (the host screen), and
    // so does the archive: the join card goes for that time.
    const auto joinShown = onSeminar && ! inArchive && ! (roomOpen && desk);
    codeEditor.setVisible (joinShown);
    joinButton.setVisible (joinShown);

    anonChoice.setVisible (onSeminar && desk && ! inArchive && (hosting || roomOpen));
    soundChoice.setVisible (onSeminar && desk && ! inArchive && (hosting || roomOpen));
    archiveButton.setVisible (hosting);

    for (auto* c : { (juce::Component*) &archiveFilter, (juce::Component*) &archiveBackButton,
                     (juce::Component*) &exportButton, (juce::Component*) &repeatButton })
        c->setVisible (inArchive);

    for (auto* c : { (juce::Component*) &whereChoice, (juce::Component*) &whoChoice,
                     (juce::Component*) &roundsChoice, (juce::Component*) &openRoomButton })
        c->setVisible (hosting);

    for (auto* toggle : familyToggles)
        toggle->setVisible (hosting);

    invitesButton.setVisible (hosting && whoChoice.getValue() == 1);

    for (auto* b : { &projectorButton, &startButton, &closeRoomButton })
        b->setVisible (onSeminar && roomOpen && ! inArchive);

    for (auto* b : { &playAButton, &playBButton, &stopButton })
        b->setVisible (onSeminar && running() && this->seminar->getStage() != SeminarHost::Stage::finished);

    // On a phone only one of the battle's two cards is up (ADR 060).
    const auto peopleShown = battle && (! compact() || compactSide == 0);
    const auto botShown = battle && (! compact() || compactSide == 1);

    for (auto* c : { (juce::Component*) &searchButton, (juce::Component*) &battleSignInButton,
                     (juce::Component*) &communityButton })
        c->setVisible (peopleShown);

    // A computer gets the Battle-PC page: tiles, the ladder, the bot of
    // the day. The phone keeps its two cards and their switch.

    for (auto* c : { (juce::Component*) &battleFamily, (juce::Component*) &botChoice,
                     (juce::Component*) &botStartButton })
        c->setVisible (botShown && ! desk);

    ratedChoice.setVisible (battle && desk);
    familyTiles.setVisible (battle && desk);
    botDayButton.setVisible (battle && desk && records != nullptr);

    for (auto* b : ladderButtons)
        b->setVisible (battle && desk);

    sideChoice.setVisible (battle && compact());
    challengeButton.setVisible (peopleShown && readyForYou);
    searchRadar.setVisible (peopleShown && radarWanted && ! radarArea.isEmpty());

    for (auto* c : { (juce::Component*) &ratingFamily, (juce::Component*) &scopeChoice,
                     (juce::Component*) &periodChoice, (juce::Component*) &openSiteButton })
        c->setVisible (rating);

    const auto overlayOpen = overlay != Overlay::none;
    overlayPrimary.setVisible (overlayOpen);
    overlaySecondary.setVisible (overlayOpen);
    overlayTertiary.setVisible (overlay == Overlay::invites);
    inviteList.setVisible (overlay == Overlay::invites);

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
        overlayTertiary.setButtonText (text.pasteList);
    }


    refreshAccountButton();
}

// ---- layout -----------------------------------------------------------

void LiveScreenComponent::resized()
{
    using namespace AbcTrainTheme;
    auto area = getLocalBounds().reduced (compact() ? Spacing::medium : Spacing::large, Spacing::medium);

    auto top = area.removeFromTop (tabsRowHeight);

    if (compact())
    {
        // The three tabs take the row; signing in lives on the battle card
        // and in Settings, where a phone has room for it.
        tabs.setBounds (top);
        accountButton.setBounds ({});
    }
    else
    {
        tabs.setBounds (top.removeFromLeft (juce::jmax (300, tabs.getPreferredWidth())));
        accountButton.setBounds (top.removeFromRight (160));
    }

    // Visibility depends on the width (one card or two), so a rotation or a
    // Split View resize has to re-decide it, not just move things.
    refreshVisibility();

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

    if (! compact() && archiveOpen)
    {
        layoutArchive (area);
        return;
    }

    if (! compact() && roomOpen)
    {
        layoutHost (area);
        return;
    }

    if (compact())
    {
        // A phone joins a seminar; it does not host one (ADR 060).
        cardA = area.removeFromTop (juce::jmin (area.getHeight(), cardTitleHeight + 70 + 22 + 48 + Spacing::medium
                                                                    + controlHeight + 4 + Spacing::large * 2));
        cardB = {};
    }
    else
    {
        cardA = area.removeFromLeft (area.getWidth() * 38 / 100);
        area.removeFromLeft (Spacing::large);
        cardB = area;
    }

    {
        auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + 70);
        a.removeFromTop (22);   // "Room code" caption
        codeEditor.setBounds (a.removeFromTop (48));
        a.removeFromTop (Spacing::medium);
        joinButton.setBounds (a.removeFromTop (controlHeight + 4));
    }

    if (cardB.isEmpty())
        return;

    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);

    if (roomOpen)
    {
        // Two rows on one grid of three equal columns, so the sound row and
        // the room row line up edge to edge whatever the language: the
        // clamped widths before left each row ragged in its own way.
        const auto gap = Spacing::medium;
        const auto rowH = controlHeight + 4;
        const auto cells = [gap] (juce::Rectangle<int> row)
        {
            const auto w = (row.getWidth() - 2 * gap) / 3;
            std::array<juce::Rectangle<int>, 3> c;
            c[0] = row.removeFromLeft (w);
            row.removeFromLeft (gap);
            c[1] = row.removeFromLeft (w);
            row.removeFromLeft (gap);
            c[2] = row;   // takes the rounding remainder: both rows end on the card edge
            return c;
        };

        auto grid = b.removeFromBottom (2 * rowH + gap);
        const auto sound = cells (grid.removeFromTop (rowH));
        grid.removeFromTop (gap);
        const auto room = cells (grid);

        playAButton.setBounds (sound[0]);
        playBButton.setBounds (sound[1]);
        stopButton.setBounds (sound[2]);
        closeRoomButton.setBounds (room[0]);
        projectorButton.setBounds (room[1]);
        startButton.setBounds (room[2]);

        invitesButton.setBounds ({});
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
    anonChoice.setBounds (row (controlHeight).withWidth (juce::jmin (b.getWidth() - labelColumn, juce::jmax (260, anonChoice.getPreferredWidth()))));
    soundChoice.setBounds (row (controlHeight).withWidth (juce::jmin (b.getWidth() - labelColumn, juce::jmax (300, soundChoice.getPreferredWidth()))));

    auto buttons = b.removeFromBottom (controlHeight + 4);
    openRoomButton.setBounds (buttons.removeFromRight (200));
    buttons.removeFromRight (Spacing::small);
    invitesButton.setBounds (buttons.removeFromRight (200));
    archiveButton.setBounds (buttons.removeFromLeft (juce::jmin (220, buttons.getWidth())));
}

void LiveScreenComponent::layoutBattle (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;

    if (! compact())
    {
        layoutBattleDesk (area);
        return;
    }

    if (compact())
    {
        sideChoice.setBounds (area.removeFromTop (controlHeight));
        area.removeFromTop (Spacing::medium);
        cardA = compactSide == 0 ? area : juce::Rectangle<int>();
        cardB = compactSide == 1 ? area : juce::Rectangle<int>();
    }
    else
    {
        sideChoice.setBounds ({});
        cardA = area.removeFromLeft (area.getWidth() * 38 / 100);
        area.removeFromLeft (Spacing::large);
        cardB = area;
    }

    if (sideBySide())
    {
        // Bot: the choice and its profile on the left, the exercises and
        // the start on the right.
        auto left = cardHalf (cardB, false).withTrimmedTop (26);
        botChoice.setBounds (left.removeFromTop (controlHeight));
        auto right = cardHalf (cardB, true).withTrimmedTop (22);
        battleFamily.setBounds (right.removeFromTop (controlHeight));
        botStartButton.setBounds (right.removeFromBottom (controlHeight + 4));

        // People: Decibelo on the left, finding an opponent on the right.
        auto people = cardHalf (cardA, true).withTrimmedTop (30 + Spacing::small);
        auto row = people.removeFromTop (controlHeight + 4);
        challengeButton.setBounds (readyForYou ? row.removeFromRight (row.getWidth() / 3) : juce::Rectangle<int>());
        if (readyForYou)
            row.removeFromRight (Spacing::small);
        searchButton.setBounds (row);

        auto bottom = people.removeFromBottom (controlHeight + 4);
        radarArea = people.withTrimmedTop (Spacing::small).withTrimmedBottom (Spacing::small);
        if (radarArea.getHeight() < 120)
            radarArea = {};
        searchRadar.setBounds (radarArea);
        searchRadar.setVisible (radarWanted && ! radarArea.isEmpty());
        battleSignInButton.setBounds (bottom.removeFromLeft (juce::jmin (160, bottom.getWidth() / 2)));
        bottom.removeFromLeft (Spacing::small);
        communityButton.setBounds (bottom);
        return;
    }

    // Right: a bot - the battle that works today, offline.
    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + 26);
    botChoice.setBounds (b.removeFromTop (controlHeight).withWidth (juce::jmin (b.getWidth(), juce::jmax (480, botChoice.getPreferredWidth()))));
    b.removeFromTop (Spacing::medium + 150);   // the bot's profile, painted
    b.removeFromTop (22);                      // "Exercises"
    battleFamily.setBounds (b.removeFromTop (controlHeight).withWidth (juce::jmin (b.getWidth(), juce::jmax (420, battleFamily.getPreferredWidth()))));
    botStartButton.setBounds (cardB.reduced (Spacing::large).removeFromBottom (controlHeight + 4).removeFromRight (260));

    // Left: people - Decibelo, and the buttons that wait for the round server.
    auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + 4 * 40 + Spacing::medium + 44 + Spacing::large + 30);
    {
        auto row = a.removeFromTop (controlHeight + 4);
        if (readyForYou)
        {
            challengeButton.setBounds (row.removeFromRight (row.getWidth() / 3));
            row.removeFromRight (Spacing::small);
        }
        else
        {
            challengeButton.setBounds ({});
        }
        searchButton.setBounds (row);
    }
    radarArea = a.withTrimmedTop (Spacing::medium).withTrimmedBottom (controlHeight + 4 + Spacing::medium);
    if (radarArea.getHeight() < 120)
        radarArea = {};
    searchRadar.setBounds (radarArea);
    searchRadar.setVisible (radarWanted && ! radarArea.isEmpty() && ! cardA.isEmpty());
    {
        auto bottom = cardA.reduced (Spacing::large).removeFromBottom (controlHeight + 4);
        battleSignInButton.setBounds (bottom.removeFromLeft (160));
        bottom.removeFromLeft (Spacing::small);
        communityButton.setBounds (bottom.removeFromRight (juce::jmin (240, bottom.getWidth())));
    }
}

juce::Rectangle<int> LiveScreenComponent::cardHalf (juce::Rectangle<int> card, bool right) const
{
    auto inner = card.reduced (AbcTrainTheme::Spacing::large).withTrimmedTop (cardTitleHeight);
    const auto half = (inner.getWidth() - AbcTrainTheme::Spacing::large) / 2;
    return right ? inner.removeFromRight (half) : inner.removeFromLeft (half);
}

void LiveScreenComponent::layoutRating (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    if (compact())
    {
        // Two rows: which family, then which slice of it. The site link
        // goes: on a phone the site is the same screen, bigger.
        ratingFamily.setBounds (area.removeFromTop (controlHeight));
        area.removeFromTop (Spacing::small);
        auto row = area.removeFromTop (controlHeight);
        scopeChoice.setBounds (row.removeFromLeft ((row.getWidth() - Spacing::small) / 2));
        row.removeFromLeft (Spacing::small);
        periodChoice.setBounds (row);
        openSiteButton.setBounds ({});
        area.removeFromTop (Spacing::medium);
        tableArea = area;
        cardA = cardB = {};
        return;
    }

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
    const auto height = overlay == Overlay::invites ? juce::jmin (620, getHeight() - 40) : 440;
    overlayBox = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 40, overlay == Overlay::invites ? 820 : 760),
                                                         juce::jmin (getHeight() - 20, height));

    auto box = overlayBox.reduced (Spacing::large + 8);
    auto buttons = box.removeFromBottom (controlHeight + 4);
    overlayPrimary.setBounds (buttons.removeFromRight (180));
    buttons.removeFromRight (Spacing::small);
    overlaySecondary.setBounds (buttons.removeFromRight (juce::jmin (240, buttons.getWidth() / 2)));
    overlayTertiary.setBounds (buttons.removeFromLeft (juce::jmin (180, buttons.getWidth())));

    if (overlay == Overlay::invites)
    {
        box.removeFromBottom (Spacing::medium + 22);    // the offline hint
        inviteList.setBounds (box.withTrimmedTop (36 + 26 + Spacing::small));
    }
}

// ---- painting ---------------------------------------------------------

void LiveScreenComponent::paintCard (juce::Graphics& g, juce::Rectangle<int> card, const juce::String& title) const
{
    if (card.isEmpty())
        return;   // the card a phone is not showing (ADR 060)

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

    // Account state beside the button - on a desktop. A phone's tab row is
    // the three tabs and nothing else (ADR 060); who is signed in is said
    // on the battle card and in Settings.
    if (! compact())
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

    if (! compact())
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

    if (! compact() && archiveOpen)
    {
        paintArchive (g);
        return;
    }

    if (! compact() && roomOpen)
    {
        paintHost (g);
        return;
    }

    paintCard (g, cardA, text.joinTitle);
    {
        auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitLines (g, text.joinLocal, a.removeFromTop (66), juce::Justification::topLeft, 3, 0.9f);
        a.removeFromTop (4);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.joinCode, a.removeFromTop (20), juce::Justification::centredLeft, true);
    }

    // A phone does not host (the card is not laid out there): without
    // this guard an open room painted its QR into an empty rectangle at
    // the top-left corner of the screen.
    if (roomOpen && ! cardB.isEmpty())
    {
        const auto local = whereChoice.getValue() == 1;
        paintCard (g, cardB, local ? text.roomOpenLocal : text.roomOpenOnline);

        auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
        b.removeFromBottom (2 * (controlHeight + 4) + Spacing::medium * 2);   // the buttons

        if (running())
        {
            paintRunning (g, b);
            return;
        }

        const auto side = juce::jmin (200, b.getHeight() - 60, b.getWidth() / 3);
        auto qrBox = b.removeFromLeft (side).removeFromTop (side);
        paintQrInto (g, roomQr, roomQrUrl, roomJoinUrl(), qrBox);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.scanToJoin, juce::Rectangle<int> (qrBox.getX(), qrBox.getBottom() + 4, qrBox.getWidth(), 18),
                      juce::Justification::centred, true);

        b.removeFromLeft (Spacing::large);
        auto info = b;
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, text.typeAddress, info.removeFromTop (24), juce::Justification::centredLeft, true);
        g.setColour (theme.textBright);
        g.setFont (LnF::monoFont().withHeight (24.0f));
        LnF::fitText (g, roomAddress(), info.removeFromTop (38), juce::Justification::centredLeft, true);

        if (! local || whoChoice.getValue() == 0)
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, text.roomCodeLabel, info.removeFromTop (20), juce::Justification::centredLeft, true);
            g.setFont (LnF::monoFont().withHeight (28.0f));
            g.setColour (theme.accent);
            LnF::fitText (g, roomCode, info.removeFromTop (36), juce::Justification::centredLeft, true);
        }

        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, local ? text.sameWifi : text.onlineHint, info.removeFromTop (36), juce::Justification::topLeft, 2, 0.9f);

        const auto snap = seminar != nullptr ? seminar->getRoom().snapshot() : LocalRoom::Snapshot();
        const auto joined = (int) snap.players.size();
        const auto total = snap.listOnly ? snap.invited : 0;
        info.removeFromTop (Spacing::small);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, text.joined + "  " + juce::String (joined) + (total > 0 ? " / " + juce::String (total) : juce::String()),
                      info.removeFromTop (24), juce::Justification::centredLeft, true);

        // The names as they come in, one line.
        juce::StringArray names;
        for (const auto& p : snap.players)
            names.add (p.name);

        g.setColour (names.isEmpty() ? theme.textDim : theme.textBright);
        g.setFont (LnF::labelFont());
        LnF::fitLines (g, names.isEmpty() ? text.nobodyYet : names.joinIntoString (", "), info.removeFromTop (38),
                       juce::Justification::topLeft, 2, 0.9f);

        if (local)
        {
            // What to do when a phone cannot open the address - the one
            // question every local seminar gets, answered where it is asked.
            info.removeFromTop (Spacing::small);
            g.setColour (lan.linkLocal ? theme.accentWarm : theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitLines (g, lan.linkLocal ? text.lanLinkLocal + "\n" + text.lanTrouble : text.lanTrouble,
                           info.removeFromTop (juce::jmax (0, juce::jmin (info.getHeight(), 72))),
                           juce::Justification::topLeft, 4, 0.9f);
        }

        return;
    }

    if (cardB.isEmpty())
        return;   // a phone: no hosting card (ADR 060)

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
    b.removeFromTop (Spacing::medium);
    label (b.removeFromTop (controlHeight), text.semPeopleRow);
    b.removeFromTop (Spacing::medium);
    label (b.removeFromTop (controlHeight), text.semSoundRow);

}

void LiveScreenComponent::paintBattle (juce::Graphics& g)
{
    using namespace AbcTrainTheme;

    if (! compact())
    {
        paintBattleDesk (g);
        return;
    }

    const auto& theme = current();
    const Family families[] { Family::frequency, Family::dynamics, Family::space, Family::character };
    const juce::String fams[] { text.freq, text.dyn, text.space, text.character };

    paintCard (g, cardA, text.decibelo);

    if (sideBySide() && ! cardA.isEmpty())
    {
        auto a = cardHalf (cardA, false);
        const auto rowHeight = juce::jmin (40, (a.getHeight() - 40) / 4);

        for (int i = 0; i < 4; ++i)
        {
            auto row = a.removeFromTop (rowHeight);
            g.setColour (accentFor (families[i]));
            g.fillRect (row.removeFromLeft (8).withSizeKeepingCentre (8, 8));
            row.removeFromLeft (12);
            g.setColour (theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, fams[i], row.removeFromLeft (row.getWidth() - 60), juce::Justification::centredLeft, true);
            g.setColour (theme.textDim);
            g.setFont (LnF::monoFont().withHeight (18.0f));
            LnF::fitText (g, myRatings[(size_t) i] > 0 ? juce::String (myRatings[(size_t) i]) : juce::String::fromUTF8 ("\xe2\x80\x94"),
                          row, juce::Justification::centredRight, true);
            g.setColour (theme.divider);
            g.fillRect (a.getX(), row.getBottom(), a.getWidth(), 1);
        }

        a.removeFromTop (Spacing::small);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.decibeloHint, a, juce::Justification::topLeft, 2, 0.9f);

        auto p = cardHalf (cardA, true);
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (18.0f));
        LnF::fitText (g, text.humansTitle, p.removeFromTop (30), juce::Justification::centredLeft, true);
        p.removeFromTop (Spacing::small + controlHeight + 4 + Spacing::small);
        p.removeFromBottom (controlHeight + 4 + Spacing::small);

        const auto signedIn = account != nullptr && account->isSignedIn();
        g.setColour (onlineLine.isNotEmpty() && searchingOnline ? theme.accent : theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, onlineLine.isNotEmpty() ? onlineLine : (signedIn ? text.battleOnline : text.battleNeedsAccount),
                       p, juce::Justification::topLeft, 4, 0.85f);
    }
    else
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
            LnF::fitText (g, myRatings[(size_t) i] > 0 ? juce::String (myRatings[(size_t) i]) : juce::String::fromUTF8 ("\xe2\x80\x94"),
                          row, juce::Justification::centredRight, true);
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

        // Under the button: the search as it goes, or what a battle is.
        a.removeFromTop ((controlHeight + 4) + Spacing::medium);
        const auto signedIn = account != nullptr && account->isSignedIn();

        if (onlineLine.isNotEmpty())
        {
            g.setColour (searchingOnline ? theme.accent : theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitLines (g, onlineLine, a.removeFromTop (48), juce::Justification::topLeft, 2, 0.9f);
            a.removeFromTop (Spacing::small);
        }

        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        auto bottom = a.withTrimmedBottom (controlHeight + 4 + Spacing::small);
        auto hint = bottom.removeFromBottom (36);
        LnF::fitLines (g, signedIn ? text.battleOnline : text.battleNeedsAccount,
                       bottom.withTrimmedBottom (Spacing::small), juce::Justification::topLeft, 6, 0.85f);
        g.setColour (theme.text);
        LnF::fitLines (g, text.communityHint, hint, juce::Justification::bottomLeft, 2, 0.85f);
    }

    paintCard (g, cardB, text.botTitle);
    if (! cardB.isEmpty())
    {
        const auto split = sideBySide();
        auto b = split ? cardHalf (cardB, false) : cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.botHint, b.removeFromTop (22), juce::Justification::topLeft, true);
        b.removeFromTop (4 + controlHeight + Spacing::medium);

        // The selected bot: what it is good at, as four bars - one per
        // family, from its psychometric thresholds - and how it answers.
        const auto bot = (BotListener::Bot) juce::jlimit (0, BotListener::numBots - 1, botChoice.getValue());
        const auto& profile = BotListener::profileOf (bot);
        auto profileBox = b.removeFromTop (split ? juce::jmin (150, b.getHeight()) : 150);

        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, text.botSpecialty[(int) bot], profileBox.removeFromTop (26), juce::Justification::centredLeft, true);

        const std::vector<int> gamesOf[] { { 0, 8 }, { 1, 7 }, { 2, 3, 4, 6 }, { 5 } };

        for (int f = 0; f < 4; ++f)
        {
            auto row = profileBox.removeFromTop (split ? 20 : 22);
            float sum = 0.0f;
            for (auto gi : gamesOf[f])
                sum += profile.threshold[(size_t) gi];
            const auto strength = juce::jlimit (0.05f, 1.0f, (sum / (float) gamesOf[f].size() - 3.5f) / 5.0f);

            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, fams[f], row.removeFromLeft (split ? 100 : 150), juce::Justification::centredLeft, true);
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

        if (split)
            b = cardHalf (cardB, true);

        g.setColour (theme.text);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.botFamily, b.removeFromTop (22), juce::Justification::centredLeft, true);

        auto foot = split ? b.withTrimmedTop (controlHeight + Spacing::small).withTrimmedBottom (controlHeight + 4 + Spacing::small)
                          : cardB.reduced (Spacing::large).removeFromBottom (controlHeight + 4);

        if (! split)
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
    // On a phone the country column goes: five tracked headings in 360
    // points ran into each other ("COUNTRYDECIBELOWON-LOST").
    const auto narrow = area.getWidth() < 480;
    const float wide[] { 0.08f, 0.40f, 0.14f, 0.18f, 0.20f };
    const float slim[] { 0.10f, 0.42f, 0.0f, 0.26f, 0.22f };
    const auto* widths = narrow ? slim : wide;
    auto header = area.removeFromTop (30);
    auto x = (float) header.getX();

    for (int i = 0; i < 5; ++i)
    {
        const auto w = widths[i] * (float) header.getWidth();

        if (w <= 0.0f)
            continue;

        const auto cell = juce::Rectangle<float> (x, (float) header.getY(), w - 6.0f, (float) header.getHeight());
        const auto caps = LnF::toCaps (cols[i]);

        if (LnF::trackedTextWidth (caps, LnF::microFont(), 1.4f) <= cell.getWidth())
        {
            LnF::drawTrackedText (g, caps, cell, LnF::microFont(), theme.textDim, 1.4f);
        }
        else
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::microFont());
            LnF::fitText (g, caps, cell.toNearestInt(), juce::Justification::centredLeft, true);
        }

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

            if (w <= 0.0f)
                continue;
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

        // The same page as a QR code: confirm on the phone you are
        // already signed in on, instead of typing a mail on this computer.
        {
            const auto* ls = account != nullptr ? &account->getLink() : nullptr;
            const auto code = ls != nullptr ? ls->code : signInCode;
            const auto url = ls != nullptr && ls->url.startsWith ("https://soundkorb.ru/") ? ls->url
                           : code.isNotEmpty() ? "https://soundkorb.ru/link?code=" + juce::URL::addEscapeChars (code, true)
                                               : juce::String();
            auto side = box.removeFromRight (150);
            box.removeFromRight (Spacing::large);
            auto qrBox = side.removeFromTop (150);
            paintQrInto (g, signInQr, signInQrUrl, url, qrBox);
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitLines (g, text.scanToSignIn, side.removeFromTop (50), juce::Justification::centredTop, 3, 0.85f);
        }

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
    auto titleRow = box.removeFromTop (36);
    LnF::fitText (g, text.invitesTitle, titleRow.removeFromLeft (titleRow.getWidth() / 2), juce::Justification::centredLeft, true);
    g.setColour (theme.textDim);
    g.setFont (LnF::bodyFont());
    LnF::fitText (g, text.listCount.replace ("{{n}}", juce::String (inviteList.getNumPeople())), titleRow,
                  juce::Justification::centredRight, true);
    g.setFont (LnF::captionFont());
    LnF::fitText (g, text.listPasteHint, box.removeFromTop (26), juce::Justification::centredLeft, true);

    if (whereChoice.getValue() == 1)
    {
        auto hint = overlayBox.reduced (Spacing::large + 8).removeFromBottom (controlHeight + 4 + Spacing::medium + 22).removeFromTop (22);
        g.setColour (theme.accentWarm);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.localNoMail, hint, juce::Justification::centredLeft, true);
    }
}

void LiveScreenComponent::paintQrInto (juce::Graphics& g, std::unique_ptr<QrCode>& qr, juce::String& cachedUrl,
                                       const juce::String& url, juce::Rectangle<int> area) const
{
    const auto& theme = AbcTrainTheme::current();

    if (url.isEmpty())
    {
        g.setColour (theme.displayBackground);
        g.fillRect (area);
        g.setColour (theme.outline);
        g.drawRect (area, 1);
        return;
    }

    if (qr == nullptr || cachedUrl != url)
    {
        qr = std::make_unique<QrCode> (url);
        cachedUrl = url;
    }

    // Always dark on white, whatever the theme: an inverted code is the
    // one phones still refuse.
    qr->paint (g, area.toFloat());
}

void LiveScreenComponent::paintRunning (juce::Graphics& g, juce::Rectangle<int> b)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();
    const auto snap = seminar->getRoom().snapshot();
    const auto& q = snap.question;

    // A small code for latecomers, top right.
    {
        auto corner = b.removeFromRight (120).removeFromTop (120);
        paintQrInto (g, roomQr, roomQrUrl, roomJoinUrl(), corner);
        b.removeFromRight (Spacing::large);
    }

    const auto fill = [] (juce::String t, int n, int m)
    {
        return t.replace ("{{n}}", juce::String (n)).replace ("{{m}}", juce::String (m));
    };

    if (seminar->getStage() == SeminarHost::Stage::finished)
    {
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (24.0f));
        LnF::fitText (g, text.results, b.removeFromTop (34), juce::Justification::centredLeft, true);

        int place = 0, previous = -1;
        for (size_t i = 0; i < snap.players.size() && b.getHeight() >= 24; ++i)
        {
            const auto& p = snap.players[i];
            if (p.score != previous)
                place = (int) i + 1;
            previous = p.score;

            auto row = b.removeFromTop (24);
            g.setColour (place == 1 ? theme.accent : theme.textDim);
            g.setFont (LnF::monoFont());
            LnF::fitText (g, juce::String (place), row.removeFromLeft (30), juce::Justification::centredLeft, true);
            g.setColour (theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, p.name, row.removeFromLeft (row.getWidth() - 50), juce::Justification::centredLeft, true);
            g.setFont (LnF::monoFont());
            LnF::fitText (g, juce::String (p.score), row, juce::Justification::centredRight, true);
        }

        return;
    }

    g.setColour (theme.textDim);
    g.setFont (LnF::labelFont());
    LnF::fitText (g, fill (text.hostRound, q.round, q.totalRounds) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + q.exercise,
                  b.removeFromTop (22), juce::Justification::centredLeft, true);

    g.setColour (theme.textBright);
    g.setFont (LnF::headingFont().withHeight (20.0f));
    LnF::fitLines (g, q.prompt, b.removeFromTop (52), juce::Justification::topLeft, 2, 0.85f);
    b.removeFromTop (Spacing::small);

    const auto answered = fill (text.answeredOf, snap.answered, (int) snap.players.size());
    g.setColour (theme.text);
    g.setFont (LnF::bodyFont());
    LnF::fitText (g, answered, b.removeFromTop (24), juce::Justification::centredLeft, true);

    auto bar = b.removeFromTop (8).withWidth (juce::jmin (b.getWidth(), 360)).toFloat();
    g.setColour (theme.displayBackground);
    g.fillRect (bar);
    g.setColour (theme.accent);
    g.fillRect (bar.withWidth (bar.getWidth() * (float) snap.answered / (float) juce::jmax (1, (int) snap.players.size())));
    b.removeFromTop (Spacing::medium);

    if (seminar->getStage() == SeminarHost::Stage::revealed)
    {
        g.setColour (theme.positive);
        g.setFont (LnF::headingFont().withHeight (22.0f));
        LnF::fitText (g, text.theAnswer.replace ("{{answer}}", snap.answer.label), b.removeFromTop (30), juce::Justification::centredLeft, true);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, fill (text.rightOf, snap.right, snap.answered), b.removeFromTop (24), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.hostHint, b.removeFromTop (36), juce::Justification::topLeft, 2, 0.9f);
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

void LiveScreenComponent::setOnlineStatus (bool searching, const juce::String& line)
{
    searchingOnline = searching;
    onlineLine = line;
    searchButton.setButtonText (readyForYou ? text.accept : searching ? text.cancelSearch : text.search);
    repaint();
}

void LiveScreenComponent::setReadyCheck (bool waitingForYou)
{
    if (readyForYou == waitingForYou)
        return;

    readyForYou = waitingForYou;
    searchButton.setButtonText (readyForYou ? text.accept : searchingOnline ? text.cancelSearch : text.search);
    challengeButton.setButtonText (text.decline);
    refreshVisibility();
    resized();
    repaint();
}

void LiveScreenComponent::setSearchProgress (int waitingMs, int inQueue)
{
    radarWanted = true;
    searchRadar.setOwnRating (myRatings[(size_t) juce::jlimit (0, 3, battleFamily.getValue())]);
    searchRadar.showSearching (waitingMs, inQueue);
    searchRadar.setVisible (tab == Tab::battle && ! radarArea.isEmpty());
}

void LiveScreenComponent::setReadyProgress (int deadlineInMs, int members, int accepted)
{
    radarWanted = true;
    searchRadar.showReady (deadlineInMs, members, accepted);
    searchRadar.setVisible (tab == Tab::battle && ! radarArea.isEmpty());
}

void LiveScreenComponent::clearSearchProgress()
{
    if (! radarWanted)
        return;

    radarWanted = false;
    searchRadar.stop();
}

void LiveScreenComponent::setMyRatings (const std::array<int, 4>& ratings)
{
    myRatings = ratings;
    familyTiles.repaint();
    repaint();
}

void LiveScreenComponent::setBattleRecords (BattleRecords* r)
{
    records = r;
    refreshLadder();
    refreshVisibility();
    repaint();
}

void LiveScreenComponent::refreshLadder()
{
    const auto order = BattleRecords::ladder();

    for (int i = 0; i < ladderButtons.size() && i < (int) order.size(); ++i)
    {
        const auto bot = order[(size_t) i];
        const auto open = records != nullptr ? records->isOpen (bot) : BattleRecords::priceOf (i) == 0;
        const auto stars = records != nullptr ? records->stars (bot) : 0;
        auto* b = ladderButtons[i];
        b->setAlpha (open ? 1.0f : 0.45f);
        b->setButtonText (! open ? juce::String::fromUTF8 ("\xe2\x80\x94") : stars >= 3 ? text.rematch : text.fight);
    }

    repaint();
}

// ---- battle on a computer (Battle-PC board, 2026-10-24) ------------------

juce::Rectangle<int> LiveScreenComponent::FamilyTiles::tile (int i) const
{
    const auto gap = 8;
    const auto w = (getWidth() - gap) / 2, h = (getHeight() - gap) / 2;
    return { (i % 2) * (w + gap), (i / 2) * (h + gap), w, h };
}

void LiveScreenComponent::FamilyTiles::paint (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();
    const Family families[] { Family::frequency, Family::dynamics, Family::space, Family::character };
    const juce::String names[] { owner.text.freq, owner.text.dyn, owner.text.space, owner.text.character };
    const auto chosen = owner.battleFamily.getValue();

    for (int i = 0; i < 4; ++i)
    {
        auto r = tile (i);
        const auto on = i == chosen;
        g.setColour (on ? theme.accent.withAlpha (0.16f) : theme.widgetBackground.withAlpha (0.5f));
        g.fillRect (r);
        g.setColour (on ? theme.accent : theme.outline);
        g.drawRect (r, on ? 2 : 1);
        g.setColour (accentFor (families[i]));
        g.fillRect (r.removeFromLeft (4).reduced (0, 1));

        auto inner = r.reduced (12, 6);
        auto top = inner.removeFromTop (inner.getHeight() / 2);
        g.setColour (on ? theme.textBright : theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, names[i], top.withTrimmedRight (20), juce::Justification::bottomLeft, true);

        if (on)
        {
            g.setColour (theme.accent);
            LnF::fitText (g, juce::String::fromUTF8 ("\xe2\x9c\x93"), top.removeFromRight (20), juce::Justification::centredRight, false);
        }

        const auto rating = owner.myRatings[(size_t) i];
        g.setColour (theme.textDim);
        g.setFont (LnF::monoFont().withHeight (16.0f));
        LnF::fitText (g, rating > 0 ? juce::String (rating) : juce::String::fromUTF8 ("\xe2\x80\x94"),
                      inner, juce::Justification::centredLeft, false);
    }
}

void LiveScreenComponent::FamilyTiles::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < 4; ++i)
        if (tile (i).contains (e.getPosition()))
        {
            owner.battleFamily.setValue (i);
            repaint();
            owner.repaint();
            return;
        }
}

juce::Rectangle<int> LiveScreenComponent::ladderRowBounds (int index) const
{
    constexpr int header = 26;
    const auto rowH = juce::jmin (40, (ladderArea.getHeight() - header) / BotListener::numBots);
    return { ladderArea.getX(), ladderArea.getY() + header + index * rowH, ladderArea.getWidth(), rowH };
}

void LiveScreenComponent::layoutBattleDesk (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    sideChoice.setBounds ({});

    cardA = area.removeFromLeft (area.getWidth() * 36 / 100);
    area.removeFromLeft (Spacing::large);
    cardB = area;

    // People.
    auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
    ratedChoice.setBounds (a.removeFromTop (controlHeight).withWidth (juce::jmin (a.getWidth(), juce::jmax (320, ratedChoice.getPreferredWidth()))));
    a.removeFromTop (22 + Spacing::medium);          // what the mode does
    a.removeFromTop (26);                            // "Exercises in the battle"
    familyTiles.setBounds (a.removeFromTop (juce::jlimit (96, 132, a.getHeight() / 4)));
    a.removeFromTop (Spacing::large);
    {
        auto row = a.removeFromTop (controlHeight + 6);
        if (readyForYou)
        {
            challengeButton.setBounds (row.removeFromRight (row.getWidth() / 3));
            row.removeFromRight (Spacing::small);
        }
        else
        {
            challengeButton.setBounds ({});
        }
        searchButton.setBounds (row);
    }
    a.removeFromTop (Spacing::medium);
    auto bottom = a.removeFromBottom (controlHeight + 4);
    battleSignInButton.setBounds (bottom.removeFromLeft (juce::jmin (160, bottom.getWidth() / 2)));
    bottom.removeFromLeft (Spacing::small);
    communityButton.setBounds (bottom.removeFromRight (juce::jmin (240, bottom.getWidth())));
    a.removeFromBottom (Spacing::medium + 36);       // the community hint
    peopleInfo = a;
    radarArea = a.getHeight() >= 140 ? a : juce::Rectangle<int>();
    searchRadar.setBounds (radarArea);
    searchRadar.setVisible (radarWanted && ! radarArea.isEmpty());

    // Bots.
    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);
    b.removeFromTop (20 + Spacing::small);            // the subtitle
    dayBox = records != nullptr ? b.removeFromTop (60) : juce::Rectangle<int>();
    botDayButton.setBounds (dayBox.isEmpty() ? juce::Rectangle<int>()
                                             : dayBox.reduced (10).removeFromRight (150));
    if (! dayBox.isEmpty())
        b.removeFromTop (Spacing::large);

    b.removeFromBottom (20);                          // the disclaimer
    b.removeFromBottom (Spacing::small);
    const auto ladderHeight = juce::jmin (26 + BotListener::numBots * 40, juce::jmax (26 + BotListener::numBots * 30, b.getHeight() * 60 / 100));
    ladderArea = b.removeFromTop (ladderHeight);
    b.removeFromTop (Spacing::small + 24);            // the selected bot's line
    b.removeFromTop (Spacing::medium);
    recentArea = b;

    for (int i = 0; i < ladderButtons.size(); ++i)
        ladderButtons[i]->setBounds (ladderRowBounds (i).removeFromRight (120).reduced (0, 5));
}

void LiveScreenComponent::paintBattleDesk (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    const juce::String fams[] { text.freq, text.dyn, text.space, text.character };

    // ---- people ----
    paintCard (g, cardA, text.humansTitle);
    {
        auto a = cardA.reduced (Spacing::large).withTrimmedTop (cardTitleHeight + controlHeight + 4);
        g.setColour (ratedChoice.getValue() == 0 ? theme.textDim : theme.accentWarm);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, ratedChoice.getValue() == 0 ? text.ratedHint : text.friendlyHint, a.removeFromTop (22), juce::Justification::centredLeft, true);
        a.removeFromTop (Spacing::medium);
        g.setColour (theme.textDim);
        g.setFont (LnF::microFont());
        LnF::fitText (g, text.familiesCaption.toUpperCase(), a.removeFromTop (20), juce::Justification::topLeft, true);

        // Under the button: the search as it goes, or what a battle is.
        if (! (radarWanted && ! radarArea.isEmpty()))
        {
            auto info = peopleInfo;
            const auto signedIn = account != nullptr && account->isSignedIn();

            if (onlineLine.isNotEmpty())
            {
                g.setColour (searchingOnline ? theme.accent : theme.text);
                g.setFont (LnF::bodyFont());
                LnF::fitLines (g, onlineLine, info.removeFromTop (48), juce::Justification::topLeft, 2, 0.9f);
                info.removeFromTop (Spacing::small);
            }

            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitLines (g, signedIn ? text.battleOnline : text.battleNeedsAccount, info, juce::Justification::topLeft, 5, 0.85f);
        }

        auto hint = juce::Rectangle<int> (peopleInfo.getX(), peopleInfo.getBottom() + Spacing::small, peopleInfo.getWidth(), 36);
        g.setColour (theme.text);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.communityHint, hint, juce::Justification::bottomLeft, 2, 0.85f);
    }

    // ---- bots ----
    paintCard (g, cardB, text.botsTitle);
    if (cardB.isEmpty())
        return;

    auto b = cardB.reduced (Spacing::large).withTrimmedTop (cardTitleHeight);

    // The stars collected, at the title's right.
    if (records != nullptr)
    {
        g.setColour (theme.accentWarm);
        g.setFont (LnF::monoFont().withHeight (16.0f));
        auto corner = cardB.reduced (Spacing::large).removeFromTop (24);
        const auto label = juce::String (records->totalStars()) + "/" + juce::String (3 * BotListener::numBots);
        const auto w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label)) + 4;
        LnF::fitText (g, label, corner.removeFromRight (w), juce::Justification::centredRight, false);
        auto box = corner.removeFromRight (20).withSizeKeepingCentre (14, 14).toFloat();
        juce::Path star;
        star.addStar (box.getCentre(), 5, 3.0f, 7.0f);
        g.fillPath (star);
    }

    g.setColour (theme.textDim);
    g.setFont (LnF::captionFont());
    LnF::fitText (g, text.botsSub, b.removeFromTop (20), juce::Justification::centredLeft, true);

    // The bot of the day: a box of its own, the one call to action here.
    if (records != nullptr && ! dayBox.isEmpty())
    {
        const auto d = records->daily (juce::Time::getCurrentTime());
        g.setColour (theme.accent.withAlpha (0.10f));
        g.fillRect (dayBox);
        g.setColour (theme.accent.withAlpha (0.6f));
        g.drawRect (dayBox, 1);

        auto inner = dayBox.reduced (14, 8);
        inner.removeFromRight (160);
        auto top = inner.removeFromTop (inner.getHeight() / 2);
        g.setColour (theme.accent);
        g.setFont (LnF::microFont());
        LnF::fitText (g, text.botOfDay.toUpperCase() + dot + (d.wonToday ? text.botDayWon : text.botDayLeft.replace ("{{n}}", juce::String (d.hoursLeft))),
                      top, juce::Justification::bottomLeft, true);
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (19.0f));
        LnF::fitText (g, text.botNames[(int) d.bot] + dot + fams[d.family] + dot + "Decibelo " + juce::String (BotListener::profileOf (d.bot).rating),
                      inner, juce::Justification::centredLeft, true);
    }

    // The ladder: name, Decibelo, score, stars, what the next star asks for.
    {
        auto head = ladderArea.withHeight (26);
        auto cols = [&] (juce::Rectangle<int> r)
        {
            r.removeFromRight (130);   // the button
            std::array<juce::Rectangle<int>, 5> c;
            const auto w = r.getWidth();
            c[0] = r.removeFromLeft (w * 26 / 100);
            c[1] = r.removeFromLeft (w * 14 / 100);
            c[2] = r.removeFromLeft (w * 12 / 100);
            c[3] = r.removeFromLeft (w * 16 / 100);
            c[4] = r;
            return c;
        };

        const auto hc = cols (head);
        g.setColour (theme.textDim);
        g.setFont (LnF::microFont());
        const juce::String heads[] { text.colBot, "DECIBELO", text.colScore, text.colStars, text.colNext };
        for (int i = 0; i < 5; ++i)
            LnF::fitText (g, heads[i].toUpperCase(), hc[(size_t) i], juce::Justification::centredLeft, true);
        g.setColour (theme.divider);
        g.fillRect (head.getX(), head.getBottom() - 1, head.getWidth(), 1);

        const auto order = BattleRecords::ladder();
        const auto total = records != nullptr ? records->totalStars() : 0;

        for (int i = 0; i < BotListener::numBots; ++i)
        {
            const auto bot = order[(size_t) i];
            const auto row = ladderRowBounds (i);
            const auto rec = records != nullptr ? records->recordOf (bot) : BattleRecords::BotRecord {};
            const auto stars = BattleRecords::starsOf (rec);
            const auto open = records != nullptr ? records->isOpen (bot) : BattleRecords::priceOf (i) == 0;
            const auto c = cols (row);

            if ((int) bot == botChoice.getValue())
            {
                g.setColour (theme.accent.withAlpha (0.10f));
                g.fillRect (row);
            }

            g.setColour (open ? theme.textBright : theme.textDim);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, text.botNames[(int) bot], c[0].reduced (6, 0), juce::Justification::centredLeft, true);

            g.setFont (LnF::monoFont().withHeight (14.0f));
            g.setColour (theme.text);
            LnF::fitText (g, juce::String (BotListener::profileOf (bot).rating), c[1], juce::Justification::centredLeft, false);
            g.setColour (theme.textDim);
            LnF::fitText (g, rec.wins + rec.losses > 0 ? juce::String (rec.wins) + juce::String::fromUTF8 ("\xe2\x80\x93") + juce::String (rec.losses) : juce::String(),
                          c[2], juce::Justification::centredLeft, false);

            // Stars: filled in the warm colour, the rest outlined - drawn,
            // since a typed star falls back to another font at another size.
            {
                auto sr = c[3];
                for (int k = 0; k < 3; ++k)
                {
                    auto box = sr.removeFromLeft (22).withSizeKeepingCentre (14, 14).toFloat();
                    juce::Path p;
                    p.addStar (box.getCentre(), 5, 3.0f, 7.0f);
                    if (k < stars)
                    {
                        g.setColour (theme.accentWarm);
                        g.fillPath (p);
                    }
                    else
                    {
                        g.setColour (theme.outline);
                        g.strokePath (p, juce::PathStrokeType (1.2f));
                    }
                }
            }

            const auto next = ! open ? text.ladderLocked.replace ("{{n}}", juce::String (BattleRecords::priceOf (i)))
                            : stars >= 3 ? text.ladderPassed
                            : rec.wins >= 3 ? text.ladderFlawless
                            : rec.wins > 0 ? text.ladderWins.replace ("{{n}}", juce::String (rec.wins))
                                           : text.ladderOpen;
            g.setColour (! open ? theme.textDim : stars >= 3 ? theme.positive : theme.text);
            g.setFont (LnF::captionFont());
            LnF::fitText (g, next, c[4].withTrimmedRight (8), juce::Justification::centredLeft, true);

            g.setColour (theme.divider);
            g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);
        }
        juce::ignoreUnused (total);

        // The selected bot's character, one line under the table.
        const auto chosen = (BotListener::Bot) juce::jlimit (0, BotListener::numBots - 1, botChoice.getValue());
        const auto& profile = BotListener::profileOf (chosen);
        const auto speed = profile.reactionMs <= 1300 ? text.botSpeedFast
                         : profile.reactionMs >= 2200 ? text.botSpeedSlow : juce::String();
        auto line = juce::Rectangle<int> (ladderArea.getX(), ladderArea.getBottom() + Spacing::small, ladderArea.getWidth(), 24);
        g.setColour (theme.text);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.botNames[(int) chosen] + ": " + text.botSpecialty[(int) chosen]
                           + (speed.isNotEmpty() ? juce::String::fromUTF8 ("  \xc2\xb7  ") + speed : juce::String()),
                      line, juce::Justification::centredLeft, true);
    }

    // The last battles.
    {
        auto r = recentArea;
        g.setColour (theme.textDim);
        g.setFont (LnF::microFont());
        LnF::fitText (g, text.recentTitle.toUpperCase(), r.removeFromTop (20), juce::Justification::centredLeft, true);

        const auto list = records != nullptr ? records->recent() : std::vector<BattleRecords::Recent> {};

        if (list.empty())
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitLines (g, text.recentEmpty, r.removeFromTop (40), juce::Justification::topLeft, 2, 0.9f);
        }

        const auto now = juce::Time::getCurrentTime();
        const auto dayOf = [] (juce::Time t) { return (t.toMilliseconds() + (juce::int64) t.getUTCOffsetSeconds() * 1000) / (24LL * 3600 * 1000); };

        for (const auto& e : list)
        {
            if (r.getHeight() < 26)
                break;

            auto row = r.removeFromTop (28);
            const auto when = juce::Time (e.when);
            const auto days = dayOf (now) - dayOf (when);
            const auto whenText = days == 0 ? text.today : days == 1 ? text.yesterday : when.formatted ("%d.%m");

            juce::String who = e.who;
            if (e.isBot)
                for (int k = 0; k < BotListener::numBots; ++k)
                    if (e.who == BotListener::idOf ((BotListener::Bot) k))
                        who = text.botNames[k] + " " + text.botSuffix;

            const auto result = e.place == 1 ? text.resWon : e.place == 0 ? text.resLost : e.place < 0 ? text.resDraw
                                                                                          : text.resPlace.replace ("{{n}}", juce::String (e.place));

            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, whenText, row.removeFromLeft (90), juce::Justification::centredLeft, true);
            auto deltaBox = row.removeFromRight (60);
            auto resultBox = row.removeFromRight (120);
            g.setColour (theme.text);
            LnF::fitText (g, who, row.withTrimmedRight (8), juce::Justification::centredLeft, true);
            g.setColour (e.place == 1 ? theme.positive : e.place == 0 ? theme.accentWarm : theme.text);
            LnF::fitText (g, result, resultBox, juce::Justification::centredLeft, true);

            if (e.rated && e.ratingDelta != 0)
            {
                g.setColour (e.ratingDelta > 0 ? theme.positive : theme.accentWarm);
                g.setFont (LnF::monoFont().withHeight (13.0f));
                LnF::fitText (g, (e.ratingDelta > 0 ? "+" : juce::String::fromUTF8 ("\xe2\x88\x92")) + juce::String (std::abs (e.ratingDelta)),
                              deltaBox, juce::Justification::centredRight, false);
            }

            g.setColour (theme.divider);
            g.fillRect (row.getX() - 90, row.getBottom() - 1, recentArea.getWidth(), 1);
        }
    }

    // The disclaimer along the foot.
    g.setColour (theme.textDim);
    g.setFont (LnF::captionFont());
    LnF::fitText (g, text.botDisclaimer, cardB.reduced (Spacing::large).removeFromBottom (20), juce::Justification::centredLeft, true);
}

void LiveScreenComponent::mouseUp (const juce::MouseEvent& e)
{
    // A seminar in the archive's list: its rounds on the right.
    if (tab == Tab::seminar && archiveOpen && ! compact())
    {
        auto l = archiveList.reduced (14).withTrimmedTop (34);
        for (int i = 0; i < (int) archive.size(); ++i)
            if (l.removeFromTop (52).contains (e.getPosition()))
            {
                archiveSelected = i;
                repaint();
                return;
            }
        return;
    }

    // A row of the ladder: that bot's character in the line under it.
    if (tab != Tab::battle || compact())
        return;

    for (int i = 0; i < BotListener::numBots; ++i)
        if (ladderRowBounds (i).withTrimmedRight (130).contains (e.getPosition()))
        {
            botChoice.setValue ((int) BattleRecords::ladder()[(size_t) i]);
            repaint();
            return;
        }
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

// ---- the host screen (Sem-Host board, 2026-10-24) ----------------------------

void LiveScreenComponent::layoutHost (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    cardA = cardB = {};

    hostHeader = area.removeFromTop (44);
    area.removeFromTop (Spacing::medium);
    hostFooter = area.removeFromBottom (56);
    area.removeFromBottom (Spacing::medium);
    hostPeople = area.removeFromLeft (juce::jlimit (220, 300, area.getWidth() / 5));
    area.removeFromLeft (Spacing::medium);
    hostRounds = area.removeFromRight (juce::jlimit (220, 300, area.getWidth() / 4));
    area.removeFromRight (Spacing::medium);
    hostCentre = area;

    {
        auto h = hostHeader;
        closeRoomButton.setBounds (h.removeFromRight (180).withSizeKeepingCentre (180, controlHeight + 4));
        h.removeFromRight (Spacing::small);
        projectorButton.setBounds (h.removeFromRight (220).withSizeKeepingCentre (220, controlHeight + 4));
    }

    anonChoice.setBounds (hostPeople.reduced (14).withTrimmedTop (24).removeFromTop (controlHeight));
    soundChoice.setBounds (hostRounds.reduced (14).withTrimmedTop (24).removeFromTop (controlHeight));

    auto c = hostCentre.reduced (18);
    c.removeFromTop (20 + 40 + 44 + Spacing::medium);   // round, exercise, prompt
    auto buttons = c.removeFromTop (60);
    const auto third = (buttons.getWidth() - 2 * Spacing::small) / 3;
    playAButton.setBounds (buttons.removeFromLeft (third));
    buttons.removeFromLeft (Spacing::small);
    playBButton.setBounds (buttons.removeFromLeft (third));
    buttons.removeFromLeft (Spacing::small);
    stopButton.setBounds (buttons);
    c.removeFromTop (Spacing::large);
    hostVotes = c;

    startButton.setBounds (hostFooter.reduced (10).removeFromRight (260));
    invitesButton.setBounds ({});
}

void LiveScreenComponent::paintVotes (juce::Graphics& g, juce::Rectangle<int> box, const LocalRoom::Snapshot& snap) const
{
    using namespace AbcTrainTheme;
    const auto& theme = current();
    const auto& q = snap.question;
    const auto revealed = snap.phase == LocalRoom::Phase::revealed;

    if (box.getHeight() < 60)
        return;

    if (q.continuous)
    {
        auto axis = box.removeFromBottom (22);
        auto plot = box.withTrimmedBottom (2);
        const auto xOf = [&plot] (float v) { return (float) plot.getX() + juce::jlimit (0.0f, 1.0f, v) * (float) plot.getWidth(); };

        // The answer's zone and line, once shown.
        if (revealed)
        {
            const auto left = xOf (snap.answer.value - q.tolerance), right = xOf (snap.answer.value + q.tolerance);
            g.setColour (theme.positive.withAlpha (0.12f));
            g.fillRect (juce::Rectangle<float> (left, (float) plot.getY(), right - left, (float) plot.getHeight()));
            g.setColour (theme.positive);
            g.fillRect (juce::Rectangle<float> (xOf (snap.answer.value) - 1.0f, (float) plot.getY(), 2.0f, (float) plot.getHeight()));
        }

        g.setColour (theme.outline);
        g.drawHorizontalLine (plot.getBottom(), (float) plot.getX(), (float) plot.getRight());

        // Each vote a square, stacked where votes land close together.
        constexpr int buckets = 40;
        std::array<int, buckets> stack {};
        const auto size = 14.0f;

        for (auto v : snap.votes)
        {
            const auto k = juce::jlimit (0, buckets - 1, (int) (v * buckets));
            const auto y = (float) plot.getBottom() - 4.0f - size - (float) stack[(size_t) k]++ * (size + 3.0f);
            if (y < (float) plot.getY())
                continue;
            const auto inside = std::abs (v - snap.answer.value) <= q.tolerance;
            g.setColour (! revealed ? theme.accent : inside ? theme.positive : theme.accentWarm);
            g.fillRect (juce::Rectangle<float> (size, size).withPosition (xOf (v) - size * 0.5f, y));
        }

        g.setColour (theme.textDim);
        g.setFont (LnF::monoFont().withHeight (11.0f));
        for (const auto& m : q.marks)
            LnF::fitText (g, m.label, juce::Rectangle<float> (xOf (m.position) - 30.0f, (float) axis.getY() + 4.0f, 60.0f, 14.0f).toNearestInt(),
                          juce::Justification::centred, false);
        return;
    }

    // Choices: a bar per option, as long as its share of the votes.
    const auto n = juce::jmax (1, q.choices.size());
    std::vector<int> counts ((size_t) n, 0);
    for (auto v : snap.votes)
        counts[(size_t) juce::jlimit (0, n - 1, juce::roundToInt (v))]++;

    const auto rowH = juce::jmin (44, box.getHeight() / n);
    for (int i = 0; i < n; ++i)
    {
        auto row = box.removeFromTop (rowH).reduced (0, 4);
        const auto right = revealed && i == snap.answer.choice;
        g.setColour (right ? theme.positive : theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, q.choices[i], row.removeFromLeft (juce::jmin (220, row.getWidth() / 3)), juce::Justification::centredLeft, true);
        auto count = row.removeFromRight (40);
        auto track = row.reduced (8, juce::jmax (0, (row.getHeight() - 14) / 2)).toFloat();
        g.setColour (theme.displayBackground);
        g.fillRect (track);
        g.setColour (right ? theme.positive : revealed ? theme.accentWarm : theme.accent);
        g.fillRect (track.withWidth (track.getWidth() * (float) counts[(size_t) i] / (float) juce::jmax (1, (int) snap.votes.size())));
        g.setColour (theme.textDim);
        g.setFont (LnF::monoFont().withHeight (13.0f));
        LnF::fitText (g, juce::String (counts[(size_t) i]), count, juce::Justification::centredRight, false);
    }
}

void LiveScreenComponent::paintHost (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();

    if (seminar == nullptr)
        return;

    const auto snap = seminar->getRoom().snapshot();
    const auto& q = snap.question;
    const auto stage = seminar->getStage();
    const auto dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    const auto fill = [] (juce::String t, int n, int m) { return t.replace ("{{n}}", juce::String (n)).replace ("{{m}}", juce::String (m)); };

    const auto panel = [&] (juce::Rectangle<int> r)
    {
        g.setColour (theme.panelBackground);
        g.fillRect (r);
        g.setColour (theme.outline);
        g.drawRect (r, 1);
    };
    const auto caption = [&] (juce::Rectangle<int> r, const juce::String& t, juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour (theme.textDim);
        g.setFont (LnF::microFont());
        LnF::fitText (g, t.toUpperCase(), r, j, true);
    };

    // ---- header: the room, its code and address ----
    {
        auto h = hostHeader;
        h.removeFromRight (180 + Spacing::small + 220 + Spacing::large);
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (22.0f));
        const auto titleW = juce::jmin (h.getWidth() / 2, juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), snap.title)) + 8);
        LnF::fitText (g, snap.title, h.removeFromLeft (titleW), juce::Justification::centredLeft, true);
        h.removeFromLeft (Spacing::large);

        g.setColour (theme.accent);
        g.setFont (LnF::monoFont().withHeight (20.0f));
        auto code = h.removeFromLeft (110);
        LnF::fitText (g, snap.roomCode, code, juce::Justification::centredLeft, true);
        g.setColour (theme.textDim);
        g.setFont (LnF::monoFont().withHeight (13.0f));
        LnF::fitText (g, roomAddress(), h, juce::Justification::centredLeft, true);
    }

    // ---- people ----
    panel (hostPeople);
    {
        auto p = hostPeople.reduced (14);
        auto top = p.removeFromTop (20);
        caption (top, fill (text.semInRoom, (int) snap.players.size(), 0));
        caption (top, fill (text.semAnswered, snap.answered, 0), juce::Justification::centredRight);
        p.removeFromTop (4 + controlHeight + 4);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.semAnonHint, p.removeFromTop (30), juce::Justification::topLeft, 2, 0.85f);
        p.removeFromTop (4);

        auto players = snap.players;
        std::sort (players.begin(), players.end(), [] (const LocalRoom::Player& a, const LocalRoom::Player& b) { return a.seat < b.seat; });

        if (players.empty())
        {
            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, text.nobodyYet, p.removeFromTop (30), juce::Justification::centredLeft, true);
        }

        for (const auto& pl : players)
        {
            if (p.getHeight() < 30)
                break;
            auto row = p.removeFromTop (34);
            g.setColour (theme.textDim);
            g.setFont (LnF::monoFont().withHeight (11.0f));
            LnF::fitText (g, juce::String (pl.seat), row.removeFromLeft (22), juce::Justification::centredLeft, false);

            const auto revealed = stage == SeminarHost::Stage::revealed;
            g.setColour (! pl.voted ? theme.outline : revealed ? (pl.lastRight ? theme.positive : theme.accentWarm) : theme.accent);
            g.fillRect (row.removeFromLeft (10).withSizeKeepingCentre (10, 10));
            row.removeFromLeft (8);

            g.setColour (pl.present ? theme.text : theme.textDim);
            g.setFont (LnF::bodyFont());
            auto score = row.removeFromRight (36);
            LnF::fitText (g, pl.name, row, juce::Justification::centredLeft, true);
            g.setColour (theme.textDim);
            g.setFont (LnF::monoFont().withHeight (12.0f));
            LnF::fitText (g, juce::String (pl.score), score, juce::Justification::centredRight, false);
            g.setColour (theme.divider);
            g.fillRect (row.getX() - 40, row.getBottom() - 1, hostPeople.getWidth() - 28, 1);
        }
    }

    // ---- the round ----
    panel (hostCentre);
    {
        auto c = hostCentre.reduced (18);

        if (stage == SeminarHost::Stage::lobby || stage == SeminarHost::Stage::closed)
        {
            // Before the start: the way in, large.
            const auto side = juce::jmin (260, c.getHeight() - 80, c.getWidth() / 2);
            auto qrBox = c.removeFromLeft (side).withSizeKeepingCentre (side, side);
            paintQrInto (g, roomQr, roomQrUrl, roomJoinUrl(), qrBox);
            c.removeFromLeft (Spacing::large * 2);
            auto info = c.withSizeKeepingCentre (c.getWidth(), juce::jmin (c.getHeight(), 240));
            g.setColour (theme.textDim);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, text.typeAddress, info.removeFromTop (26), juce::Justification::centredLeft, true);
            g.setColour (theme.textBright);
            g.setFont (LnF::monoFont().withHeight (28.0f));
            LnF::fitText (g, roomAddress(), info.removeFromTop (42), juce::Justification::centredLeft, true);
            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, text.roomCodeLabel, info.removeFromTop (22), juce::Justification::centredLeft, true);
            g.setColour (theme.accent);
            g.setFont (LnF::monoFont().withHeight (44.0f));
            LnF::fitText (g, snap.roomCode, info.removeFromTop (56), juce::Justification::centredLeft, true);
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitLines (g, text.sameWifi, info.removeFromTop (40), juce::Justification::topLeft, 2, 0.9f);
        }
        else if (stage == SeminarHost::Stage::finished)
        {
            g.setColour (theme.textBright);
            g.setFont (LnF::headingFont().withHeight (26.0f));
            LnF::fitText (g, text.results, c.removeFromTop (40), juce::Justification::centredLeft, true);
            g.setColour (theme.positive);
            g.setFont (LnF::captionFont());
            LnF::fitText (g, text.archiveSaved, c.removeFromTop (22), juce::Justification::centredLeft, true);
            c.removeFromTop (Spacing::medium);

            int place = 0, previous = -1;
            for (size_t i = 0; i < snap.players.size() && c.getHeight() >= 28; ++i)
            {
                const auto& pl = snap.players[i];
                if (pl.score != previous)
                    place = (int) i + 1;
                previous = pl.score;
                auto row = c.removeFromTop (28);
                g.setColour (place == 1 ? theme.accent : theme.textDim);
                g.setFont (LnF::monoFont());
                LnF::fitText (g, juce::String (place), row.removeFromLeft (34), juce::Justification::centredLeft, true);
                g.setColour (theme.text);
                g.setFont (LnF::bodyFont());
                LnF::fitText (g, pl.name, row.removeFromLeft (row.getWidth() - 60), juce::Justification::centredLeft, true);
                g.setFont (LnF::monoFont());
                LnF::fitText (g, juce::String (pl.score), row, juce::Justification::centredRight, true);
            }
        }
        else
        {
            caption (c.removeFromTop (20), fill (text.hostRound, q.round, q.totalRounds));
            g.setColour (theme.textBright);
            g.setFont (LnF::headingFont().withHeight (28.0f));
            LnF::fitText (g, q.exercise + (stage == SeminarHost::Stage::revealed ? dot + snap.answer.label : juce::String()),
                          c.removeFromTop (40), juce::Justification::centredLeft, true);
            g.setColour (theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitLines (g, q.prompt, c.removeFromTop (44), juce::Justification::topLeft, 2, 0.85f);

            auto v = hostVotes;
            auto head = v.removeFromTop (20);
            caption (head, text.semHallAnswers);
            caption (head, fill (text.answeredOf, snap.answered, (int) snap.players.size())
                               + (stage == SeminarHost::Stage::revealed ? dot + fill (text.semRightOf, snap.right, snap.answered) : juce::String()),
                     juce::Justification::centredRight);
            v.removeFromTop (Spacing::small);
            paintVotes (g, v, snap);
        }
    }

    // ---- the rounds ----
    panel (hostRounds);
    {
        auto r = hostRounds.reduced (14);
        caption (r.removeFromTop (20), text.semRounds);
        r.removeFromTop (4 + controlHeight + Spacing::small);

        const auto& plan = seminar->getPlan();
        const auto& records = seminar->getRecords();
        const auto current = seminar->getRound();
        const auto rowH = juce::jlimit (30, 46, r.getHeight() / juce::jmax (1, (int) plan.size()));

        for (int i = 0; i < (int) plan.size() && r.getHeight() >= rowH; ++i)
        {
            auto row = r.removeFromTop (rowH);
            const auto n = i + 1;
            const auto done = i < (int) records.size();
            const auto now = n == current && ! done && stage != SeminarHost::Stage::finished;

            if (now)
            {
                g.setColour (theme.accent.withAlpha (0.16f));
                g.fillRect (row.expanded (6, 0));
            }

            g.setColour (theme.textDim);
            g.setFont (LnF::monoFont().withHeight (11.0f));
            LnF::fitText (g, juce::String (n), row.removeFromLeft (22), juce::Justification::centredLeft, false);

            auto score = row.removeFromRight (44);
            if (done)
            {
                const auto& rec = records[(size_t) i];
                g.setColour (rec.right * 2 >= juce::jmax (1, rec.answered) ? theme.positive : theme.accentWarm);
                g.setFont (LnF::monoFont().withHeight (12.0f));
                LnF::fitText (g, juce::String (rec.right) + "/" + juce::String (rec.answered), score, juce::Justification::centredRight, false);
            }
            else if (now)
            {
                g.setColour (theme.accent);
                g.setFont (LnF::labelFont());
                LnF::fitText (g, text.semNow, score, juce::Justification::centredRight, false);
            }

            const auto name = gameNameOf != nullptr ? gameNameOf (plan[(size_t) i]) : juce::String (plan[(size_t) i]);
            const auto sound = done ? records[(size_t) i].sound : juce::String();
            auto top = sound.isNotEmpty() ? row.removeFromTop (row.getHeight() / 2 + 2) : row;
            g.setColour (n > current ? theme.textDim : theme.text);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, name, top, sound.isNotEmpty() ? juce::Justification::bottomLeft : juce::Justification::centredLeft, true);

            if (sound.isNotEmpty())
            {
                g.setColour (theme.textDim);
                g.setFont (LnF::captionFont());
                LnF::fitText (g, juce::String::fromUTF8 ("\xe2\x99\xaa ") + sound, row, juce::Justification::topLeft, true);
            }
        }
    }

    // ---- footer: the keys ----
    g.setColour (theme.outline);
    g.drawRect (hostFooter, 1);
    {
        auto f = hostFooter.reduced (14, 0);
        f.removeFromRight (260 + 14);
        const std::pair<const char*, juce::String> keys[] { { "sem.next", text.semKeyNext }, { "sem.reveal", text.semKeyReveal },
                                                             { "sem.play", text.semKeyPlay }, { "sem.projector", text.semKeyProjector } };
        for (const auto& [id, what] : keys)
        {
            const auto key = hotkeyLabel != nullptr ? hotkeyLabel (id) : juce::String();
            if (key.isEmpty())
                continue;

            g.setFont (LnF::monoFont().withHeight (12.0f));
            const auto kw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), key)) + 12;
            auto cap = f.removeFromLeft (kw).withSizeKeepingCentre (kw, 22);
            g.setColour (theme.outline);
            g.drawRect (cap, 1);
            g.setColour (theme.textBright);
            LnF::fitText (g, key, cap, juce::Justification::centred, false);
            f.removeFromLeft (6);
            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            const auto tw = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), what)) + 4;
            LnF::fitText (g, what, f.removeFromLeft (juce::jmin (tw, f.getWidth())), juce::Justification::centredLeft, true);
            f.removeFromLeft (18);
        }

        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, text.semKeysHint, f, juce::Justification::centredRight, true);
    }
}

bool LiveScreenComponent::runSeminarHotkey (const juce::String& id)
{
    if (seminar == nullptr || ! roomOpen || tab != Tab::seminar)
        return false;

    const auto stage = seminar->getStage();

    if (id == "sem.next")
    {
        startButton.triggerClick();
        return true;
    }

    if (id == "sem.reveal")
    {
        if (stage == SeminarHost::Stage::listening)
            startButton.triggerClick();
        return true;
    }

    if (id == "sem.play")
    {
        if (running())
        {
            seminar->setSound (! seminar->isPlaying(), seminar->isProcessed());
            refreshHostButtons();
        }
        return true;
    }

    if (id == "sem.projector")
    {
        projectorButton.triggerClick();
        return true;
    }

    return false;
}

// ---- the archive (Sem-Archive board) ------------------------------------------

void LiveScreenComponent::refreshArchive()
{
    if (! archiveFrozen)
        archive = SeminarArchive::list();

    archiveSelected = juce::jlimit (0, juce::jmax (0, (int) archive.size() - 1), archiveSelected);
    repaint();
}

void LiveScreenComponent::openArchiveForSnapshot (const std::vector<SeminarArchive::Session>& sessions)
{
    archive = sessions;
    archiveFrozen = true;
    archiveOpen = true;
    archiveSelected = 0;
    showTab (Tab::seminar);
}

std::vector<int> LiveScreenComponent::archiveRowsShown() const
{
    std::vector<int> rows;
    if (archiveSelected < 0 || archiveSelected >= (int) archive.size())
        return rows;

    const auto& s = archive[(size_t) archiveSelected];
    for (int i = 0; i < (int) s.rounds.size(); ++i)
    {
        const auto& r = s.rounds[(size_t) i];
        if (archiveFilter.getValue() == 0 || r.right < r.answered || ! r.missed.isEmpty())
            rows.push_back (i);
    }
    return rows;
}

void LiveScreenComponent::layoutArchive (juce::Rectangle<int> area)
{
    using namespace AbcTrainTheme;
    cardA = cardB = {};
    archiveList = area.removeFromLeft (juce::jlimit (240, 320, area.getWidth() / 4));
    area.removeFromLeft (Spacing::large);
    archiveTable = area;

    archiveBackButton.setBounds (archiveList.reduced (14).removeFromBottom (controlHeight + 4));

    auto t = archiveTable.reduced (18);
    t.removeFromTop (40 + Spacing::small);
    auto row = t.removeFromTop (controlHeight);
    archiveFilter.setBounds (row.removeFromLeft (juce::jmin (row.getWidth() / 2, juce::jmax (320, archiveFilter.getPreferredWidth()))));
    repeatButton.setBounds (row.removeFromRight (juce::jmin (320, row.getWidth() / 2)));
    row.removeFromRight (Spacing::small);
    exportButton.setBounds (row.removeFromRight (juce::jmin (180, row.getWidth())));
}

void LiveScreenComponent::paintArchive (juce::Graphics& g)
{
    using namespace AbcTrainTheme;
    const auto& theme = current();

    for (auto r : { archiveList, archiveTable })
    {
        g.setColour (theme.panelBackground);
        g.fillRect (r);
        g.setColour (theme.outline);
        g.drawRect (r, 1);
    }

    // ---- the list ----
    {
        auto l = archiveList.reduced (14);
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (22.0f));
        LnF::fitText (g, text.archiveTitle, l.removeFromTop (34), juce::Justification::centredLeft, true);
        l.removeFromBottom (controlHeight + 4 + Spacing::medium);
        auto foot = l.removeFromBottom (56);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, text.archiveStored, foot, juce::Justification::bottomLeft, 4, 0.85f);

        for (int i = 0; i < (int) archive.size() && l.getHeight() >= 48; ++i)
        {
            const auto& s = archive[(size_t) i];
            auto row = l.removeFromTop (52);
            if (i == archiveSelected)
            {
                g.setColour (theme.accent.withAlpha (0.16f));
                g.fillRect (row);
            }
            auto inner = row.reduced (8, 6);
            g.setColour (theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, s.title, inner.removeFromTop (inner.getHeight() / 2), juce::Justification::centredLeft, true);
            const auto pct = s.answeredTotal() > 0 ? juce::roundToInt (100.0f * (float) s.rightTotal() / (float) s.answeredTotal()) : 0;
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitText (g, s.when.formatted ("%d.%m.%Y") + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (s.people)
                               + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (pct) + "%",
                          inner, juce::Justification::centredLeft, true);
            g.setColour (theme.divider);
            g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);
        }
    }

    // ---- the table ----
    auto t = archiveTable.reduced (18);

    if (archive.empty())
    {
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitLines (g, text.archiveEmpty, t.withSizeKeepingCentre (juce::jmin (t.getWidth(), 520), 80), juce::Justification::centred, 3, 0.9f);
        return;
    }

    const auto& s = archive[(size_t) juce::jlimit (0, (int) archive.size() - 1, archiveSelected)];
    {
        auto head = t.removeFromTop (40);
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (26.0f));
        const auto title = s.title + juce::String::fromUTF8 (" \xc2\xb7 ") + s.when.formatted ("%d.%m");
        const auto w = juce::jmin (head.getWidth() * 3 / 5, juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title)) + 8);
        LnF::fitText (g, title, head.removeFromLeft (w), juce::Justification::centredLeft, true);
        head.removeFromLeft (Spacing::large);
        const auto pct = s.answeredTotal() > 0 ? juce::roundToInt (100.0f * (float) s.rightTotal() / (float) s.answeredTotal()) : 0;
        g.setColour (theme.textDim);
        g.setFont (LnF::labelFont());
        LnF::fitText (g, text.archiveSummary.replace ("{{people}}", juce::String (s.people))
                                            .replace ("{{rounds}}", juce::String ((int) s.rounds.size()))
                                            .replace ("{{pct}}", juce::String (pct)),
                      head, juce::Justification::centredLeft, true);
    }
    t.removeFromTop (Spacing::small + controlHeight + Spacing::medium);

    // Columns: # | round · answer | sound | group | missed.
    const auto cols = [] (juce::Rectangle<int> r)
    {
        std::array<juce::Rectangle<int>, 5> c;
        const auto w = r.getWidth();
        c[0] = r.removeFromLeft (34);
        c[1] = r.removeFromLeft (w * 28 / 100);
        c[2] = r.removeFromLeft (w * 18 / 100);
        c[3] = r.removeFromLeft (w * 18 / 100);
        c[4] = r;
        return c;
    };

    {
        auto head = t.removeFromTop (28);
        const auto c = cols (head);
        g.setColour (theme.textDim);
        g.setFont (LnF::microFont());
        const juce::String heads[] { "#", text.colRoundAnswer, text.colSound, text.colGroup, text.colMissed };
        for (int i = 0; i < 5; ++i)
            LnF::fitText (g, heads[i].toUpperCase(), c[(size_t) i], juce::Justification::centredLeft, true);
        g.setColour (theme.outline);
        g.fillRect (head.getX(), head.getBottom() - 1, head.getWidth(), 1);
    }

    const auto rows = archiveRowsShown();

    if (rows.empty())
    {
        g.setColour (theme.positive);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, text.archiveNoMistakes, t.removeFromTop (48), juce::Justification::centredLeft, true);
    }

    for (auto i : rows)
    {
        if (t.getHeight() < 50)
            break;

        const auto& r = s.rounds[(size_t) i];
        auto row = t.removeFromTop (56);
        const auto c = cols (row);

        g.setColour (theme.textDim);
        g.setFont (LnF::monoFont().withHeight (12.0f));
        LnF::fitText (g, juce::String (i + 1), c[0], juce::Justification::centredLeft, false);

        {
            auto cell = c[1].reduced (0, 8);
            g.setColour (theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitText (g, r.exercise, cell.removeFromTop (cell.getHeight() / 2), juce::Justification::centredLeft, true);
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitText (g, r.answer, cell, juce::Justification::centredLeft, true);
        }

        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, r.sound.isNotEmpty() ? juce::String::fromUTF8 ("\xe2\x99\xaa ") + r.sound : juce::String(), c[2].withTrimmedRight (8),
                      juce::Justification::centredLeft, true);

        {
            auto cell = c[3].withTrimmedRight (12);
            auto label = cell.removeFromRight (44);
            auto track = cell.withSizeKeepingCentre (cell.getWidth(), 8).toFloat();
            const auto share = r.answered > 0 ? (float) r.right / (float) r.answered : 0.0f;
            g.setColour (theme.displayBackground);
            g.fillRect (track);
            g.setColour (share < 0.6f ? theme.accentWarm : theme.accent);
            g.fillRect (track.withWidth (track.getWidth() * share));
            g.setColour (theme.text);
            g.setFont (LnF::monoFont().withHeight (12.0f));
            LnF::fitText (g, juce::String (r.right) + "/" + juce::String (r.answered), label, juce::Justification::centredRight, false);
        }

        {
            // Who missed it: a chip each, as many as fit on two lines.
            auto cell = c[4].reduced (0, 6);
            g.setFont (LnF::captionFont());
            int x = cell.getX(), y = cell.getY();
            const auto chipH = 20;
            for (const auto& name : r.missed)
            {
                const auto w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name)) + 14;
                if (x + w > cell.getRight())
                {
                    x = cell.getX();
                    y += chipH + 4;
                    if (y + chipH > cell.getBottom())
                        break;
                }
                juce::Rectangle<int> chip (x, y, juce::jmin (w, cell.getWidth()), chipH);
                g.setColour (theme.accentWarm.withAlpha (0.6f));
                g.drawRect (chip, 1);
                g.setColour (theme.accentWarm);
                LnF::fitText (g, name, chip.reduced (6, 0), juce::Justification::centred, true);
                x += w + 4;
            }
        }

        g.setColour (theme.divider);
        g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);
    }
}
