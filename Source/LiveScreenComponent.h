#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/SegmentedChoice.h"
#include "LiveLink.h"
#include "LiveAccount.h"
#include "BotListener.h"
#include "SeminarHost.h"
#include "ProjectorWindow.h"
#include "InviteListComponent.h"
#include "CodeSheet.h"
#include <functional>
#include <vector>

// The Live tab: seminars, 1-on-1 battles, the Decibelo rating - as a
// complete set of screens and buttons, with no network behind them yet
// (the author, 2026-09-24: "готовая заготовка всех нужных кнопок, потом
// будем допиливать"). Design: the canvas «abcTrain Live и рейтинги» and
// docs/design/education-and-live.md.
//
// What works today, locally:
//   - every tab, choice and overlay opens and closes;
//   - a local room shows this computer's real address on the network;
//   - "list only" turns a pasted list of names/e-mails into codes;
//   - "Sign in" shows a code and opens soundkorb.ru/link in the browser.
// What says plainly that it is not there yet: anything that would need the
// Live server - joining a room, finding an opponent, the rating itself.
//
// The local seminar works (2026-09-25): the room is served by this
// computer (LocalRoom, SeminarHost), phones join by a real QR code, the
// projector window shows the round. Online rooms still wait for the round
// server and say so. The offline rule holds: a socket opens only when the
// presenter opens a local room, and only on the local network.
class LiveScreenComponent : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    struct Strings
    {
        juce::String seminar { "Seminar" }, battle { "Battle" }, rating { "Rating" };
        juce::String notSignedIn { "Not signed in" }, signIn { "Sign in" }, signOut { "Sign out" };

        juce::String joinTitle { "At a seminar" };
        juce::String joinHint { "The code is on the presenter's screen. Sound goes to your own output - studio headphones beat a phone." };
        juce::String joinCode { "Room code" }, join { "Join" };

        juce::String hostTitle { "Run a session" };
        juce::String where { "Where" }, online { "Over the internet" }, local { "Locally, no internet" };
        juce::String localHint { "This computer serves the room on the venue's Wi-Fi. Nothing leaves the room." };
        juce::String onlineHint { "The room lives on soundkorb.ru; anyone with a network can join." };
        juce::String who { "Who can join" }, anyone { "Anyone" }, listOnly { "List only" };
        juce::String families { "Exercises" }, rounds { "Rounds" };
        juce::String freq { "Frequency" }, dyn { "Dynamics" }, space { "Space" }, character { "Character" };
        juce::String invites { "List and codes" }, openRoom { "Open the room" };

        juce::String roomOpenLocal { "Room open - local network" }, roomOpenOnline { "Room open - soundkorb.ru" };
        juce::String typeAddress { "Or type in the phone's browser:" };
        juce::String sameWifi { "Phones must be on the same Wi-Fi. Their mobile data can be off." };
        juce::String joined { "Joined" }, projector { "Projector screen" }, start { "Start" }, closeRoom { "Close the room" };
        juce::String resultsLocal { "Results stay on this computer; to the site only if you upload them." };

        juce::String invitesTitle { "List and codes" };
        juce::String invitesHint { "One person per line: name, e-mail. Each gets a code." };
        juce::String makeCodes { "Make codes" }, printCodes { "Print codes" }, mailCodes { "Send codes by e-mail" }, done { "Done" };
        juce::String localNoMail { "Offline there is no mail: print the codes or show them on screen." };

        juce::String decibelo { "Your Decibelo" };
        juce::String decibeloHint { "Separately per family: you can hear frequencies better than compression." };
        juce::String findTitle { "Find an opponent" };
        juce::String battleRules { "7 rounds, the same for both - an opponent within 100 of your rating." };
        juce::String search { "Find an opponent" }, challenge { "Challenge by code" };
        juce::String battleNeedsAccount { "Battles need an account: the rating has to be yours and impossible to inflate. Seminars and training do not." };
        juce::String fairPlay { "The server runs each round: you get the sound, the answer stays with it." };

        juce::String world { "World" }, country { "Country" }, season { "Season" }, allTime { "All time" };
        juce::String colRank { "#" }, colNick { "Nick" }, colCountry { "Country" }, colRating { "Decibelo" }, colRecord { "Won-lost" };
        juce::String ratingEmpty { "The rating appears when the battle server is running." };
        juce::String openOnSite { "Open on soundkorb.ru" };

        juce::String signInTitle { "Sign in on soundkorb.ru" };
        juce::String signInSteps { "1. Open the site - the code fills itself in.\n2. Sign in with your e-mail: a 6-digit code arrives.\n3. Press Connect, then come back here." };
        juce::String waiting { "Waiting for confirmation - the code works for {{time}}" };
        juce::String openSite { "Open the site" }, cancel { "Cancel" };
        juce::String noPasswords { "No passwords in the app: you sign in on the site, the app gets a key you can revoke." };

        juce::String notYet { "Not connected yet: this is the layout, the Live server comes next." };

        // Connection (LiveLink). Titles are short; hints are one per line.
        juce::String linkChecking { "Checking the connection..." };
        juce::String linkOnline { "soundkorb.ru is reachable" };
        juce::String linkNoNetwork { "No network connection" };
        juce::String linkNoInternet { "Network, but no internet" };
        juce::String linkServerDown { "The server is not answering" };
        juce::String linkServerError { "The server answered with an error ({{code}})" };
        juce::String linkAppTooOld { "This version of abcTrain is too old for Live" };
        juce::String checkAgain { "Check again" };
        juce::String hintNoNetwork { "Turn on Wi-Fi or plug in a cable.\nA local seminar needs no internet - only a Wi-Fi shared with the phones." };
        juce::String hintNoInternet { "Open any website: if it does not load, the problem is the network, not abcTrain.\nPublic Wi-Fi (cafe, university) may want a sign-in page first.\nA VPN or proxy can block it - try without.\nTo run a seminar right now, choose Local." };
        juce::String hintServerDown { "The internet works but soundkorb.ru does not answer - probably maintenance; try again in a few minutes.\nIf soundkorb.ru does not open in a browser either, the fault is on our side.\nA work or school network may block it - try another network or mobile data.\nTo run a seminar right now, choose Local." };
        juce::String hintAppTooOld { "Update abcTrain: Settings - About - Check for updates.\nTraining and local seminars keep working in this version." };
        juce::String lanNone { "This computer is not on a local network" };
        juce::String lanNoneHint { "Connect to the venue's Wi-Fi.\nNo Wi-Fi? Share a hotspot from this laptop (macOS: Settings - General - Sharing - Internet Sharing; Windows: Settings - Mobile hotspot) and connect the phones to it." };
        juce::String lanLinkLocal { "The address starts with 169.254: the network has no router, phones will probably not reach it. Use a router or a hotspot." };
        // Account (ADR 045).
        juce::String signedInAs { "Signed in: {{nick}}" };
        juce::String signedInNoNick { "Signed in" };
        juce::String linkStarting { "Asking soundkorb.ru for a code..." };
        juce::String linkFailed { "Could not get a code from soundkorb.ru. Check the connection and try again." };
        juce::String linkExpired { "The code has expired - get a new one." };
        juce::String newCode { "New code" };
        juce::String signedInDone { "This computer is now linked to {{nick}}. Your progress will sync." };
        juce::String serverInRussia { "The server is in Russia: from some countries it can be slow or blocked - a VPN usually helps." };
        juce::String ratingLoading { "Loading the rating..." };
        juce::String ratingFailed { "Could not load the rating. Check the connection." };
        juce::String battlesNext { "Battles open in the next update: the round server is not running yet. The rating already works." };
        // Battles with a virtual listener (ADR 046).
        juce::String botTitle { "Against a bot" };
        juce::String botHint { "Works offline. Seven rounds, the same round for both; the bot answers from its hearing profile at your level." };
        juce::StringArray botNames { "Hound", "Cat", "Viper", "Owl", "Bat", "Elephant" };
        juce::StringArray botSpecialty { "", "", "", "", "", "" };
        juce::String botStart { "Start the battle" };
        juce::String botFamily { "Exercises" };
        juce::String botSpeedFast { "answers fast" }, botSpeedSlow { "thinks long, rarely slips" };
        juce::String botDisclaimer { "Characters inspired by differences in animal hearing - game profiles, not biology." };
        juce::String humansTitle { "Against people" };
        juce::String lanTrouble { "Phones cannot open the address? Guest Wi-Fi often keeps devices apart - use a normal network or a hotspot. Allow abcTrain incoming connections in the firewall. Turn off mobile data on the phone." };

        // The local seminar, working (2026-09-25).
        juce::String roomTitle { "abcTrain seminar" };
        juce::String scanToJoin { "Scan with the phone's camera" };
        juce::String roomCodeLabel { "Room code" };
        juce::String playA { "A - clean" }, playB { "B - processed" }, stopSound { "Stop" };
        juce::String showAnswer { "Show the answer" }, nextRound { "Next" }, again { "Once more" }, results { "Results" };
        juce::String hostRound { "Round {{n}} of {{m}}" };
        juce::String answeredOf { "Answered: {{n}} of {{m}}" };
        juce::String rightOf { "Right: {{n}} of {{m}}" };
        juce::String theAnswer { "The answer: {{answer}}" };
        juce::String hostHint { "The sound plays here, into the hall. Phones only answer. Keys in the projector window: Space, A, B, S." };
        juce::String roomFailed { "Could not open the room on this computer: {{why}}" };
        juce::String needPeople { "Add at least one person to the list, or let anyone in." };
        juce::String pasteList { "Paste a list" };
        juce::String listCount { "{{n}} [[person|people]] on the list" };
        juce::String listPasteHint { "Rows from a spreadsheet or a mail paste straight in: one person per line." };
        juce::String nameCol { "Name" }, mailCol { "E-mail" }, codeCol { "Code" };
        juce::String namePlaceholder { "Ivan Petrov" }, mailPlaceholder { "ivan@school.ru" };
        juce::String scanToSignIn { "Or scan it with your phone - the same page opens there." };
        juce::String joinLocal { "At a local seminar: type the address from the presenter's screen here, or scan the QR code with a phone." };
        juce::String openAddress { "Open" };
        juce::String onlineNotYet { "Online rooms open with the round server. A local room works today, without the internet." };
        juce::String closeProjector { "Close the projector screen" };
        juce::String nobodyYet { "Nobody in the room yet" };

        ProjectorView::Strings projectorText;
        CodeSheet::Strings sheet;
        juce::var phone;    // the phone page's texts, by key
    };

    LiveScreenComponent();
    ~LiveScreenComponent() override;

    void setStrings (Strings);
    const Strings& getStrings() const noexcept { return text; }

    enum class Tab { seminar, battle, rating };
    void showTab (Tab);

    // For Settings -> Live and account.
    void openSignIn();

    // The account this page signs in and out (the processor owns it).
    void setAccount (LiveAccount*);

    // The local seminar this page opens and runs (the editor owns it).
    void setSeminarHost (SeminarHost*);

    // "Start the battle" against a bot: which bot, and which family
    // (0 frequency, 1 dynamics, 2 space, 3 character). The editor picks
    // the exercise and runs it in the duel mode.
    std::function<void (BotListener::Bot, int family)> onStartBotBattle;

    // For tools/EditorSnapshots.
    void openRoomForSnapshot (bool local);
    void openInvitesForSnapshot();
    void runRoundForSnapshot (bool revealed);

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    // Checks the way to the server now (a worker thread). Called when the
    // page is shown - at most once a minute - and by "Check again".
    void checkConnection (bool evenIfRecent = false);

    // For tools/EditorSnapshots: a fixed connection state instead of
    // whatever the rendering machine's network happens to be.
    void setLinkForSnapshot (LiveLink::State state, bool lanPresent);

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void accountChanged();
    void loadRating();
    void refreshAccountButton();

    LiveAccount* account = nullptr;
    juce::Array<LiveAccount::RatingRow> ratingRows;
    enum class RatingState { idle, loading, loaded, failed } ratingState = RatingState::idle;
    void layoutSeminar (juce::Rectangle<int>);
    void layoutBattle (juce::Rectangle<int>);
    void layoutRating (juce::Rectangle<int>);
    void layoutOverlay();
    void refreshVisibility();
    void setNote (const juce::String&);
    juce::String roomAddress() const;      // "192.168.0.14:8930"
    juce::String roomJoinUrl() const;      // what the QR code opens
    void openLocalRoom();
    void closeLocalRoom();
    void openProjector();
    void refreshHostButtons();
    bool running() const;

    void paintCard (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;
    void paintSeminar (juce::Graphics&);
    void paintBattle (juce::Graphics&);
    void paintRating (juce::Graphics&);
    void paintOverlay (juce::Graphics&);

    Strings text;
    Tab tab = Tab::seminar;

    SegmentedChoice tabs;
    juce::TextButton accountButton;

    // Seminar: join
    juce::TextEditor codeEditor;
    juce::TextButton joinButton;

    // Seminar: host
    SegmentedChoice whereChoice, whoChoice, roundsChoice;
    juce::OwnedArray<juce::TextButton> familyToggles;
    juce::TextButton invitesButton, openRoomButton;

    // Seminar: open room
    bool roomOpen = false;
    juce::String roomCode;
    juce::TextButton projectorButton, startButton, closeRoomButton;
    juce::TextButton playAButton, playBButton, stopButton;
    SeminarHost* seminar = nullptr;
    std::unique_ptr<ProjectorWindow> projector;
    mutable std::unique_ptr<QrCode> roomQr, signInQr;
    mutable juce::String roomQrUrl, signInQrUrl;
    void paintQrInto (juce::Graphics&, std::unique_ptr<QrCode>&, juce::String& cachedUrl,
                      const juce::String& url, juce::Rectangle<int>) const;
    void paintRunning (juce::Graphics&, juce::Rectangle<int>);

    // Battle
    SegmentedChoice battleFamily, botChoice;
    juce::TextButton searchButton, challengeButton, battleSignInButton, botStartButton;

    // Rating
    SegmentedChoice ratingFamily, scopeChoice, periodChoice;
    juce::TextButton openSiteButton;

    // Overlays: sign-in, and the list of invitees. A layer of its own on
    // top of everything, so the page's controls cannot paint through it.
    struct OverlayLayer : public juce::Component
    {
        explicit OverlayLayer (LiveScreenComponent& o) : owner (o) {}
        void paint (juce::Graphics& g) override { owner.paintOverlay (g); }
        LiveScreenComponent& owner;
    };

    OverlayLayer overlayLayer { *this };
    enum class Overlay { none, signIn, invites };
    Overlay overlay = Overlay::none;
    juce::String signInCode;
    double signInStartedMs = 0.0;
    juce::TextButton overlayPrimary, overlaySecondary, overlayTertiary;
    InviteListComponent inviteList;

    juce::String note;
    juce::Rectangle<int> cardA, cardB, overlayBox, tableArea, bannerBox;

    // ---- the connection ----
    LiveLink::State link = LiveLink::State::unknown;
    int linkHttpStatus = 0;
    LiveLink::LanInfo lan;
    double lastCheckMs = -1.0e9;
    juce::TextButton checkAgainButton;
    class Checker;
    std::unique_ptr<Checker> checker;

    // What stands in the way of what this page is showing, if anything:
    // no server for an online view, no local network for a local room.
    struct Problem { juce::String title, hints; };
    Problem currentProblem() const;
    bool needsServer() const;
    bool requireServer();    // false, with the reason in the note line, when it is not there
    void paintLinkStatus (juce::Graphics&, juce::Rectangle<int>);
    void paintBanner (juce::Graphics&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveScreenComponent)
};
