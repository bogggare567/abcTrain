#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/SegmentedChoice.h"
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
// Nothing here opens a socket. The offline rule holds: the app connects
// only when the player uses Live, and today Live does not connect at all.
class LiveScreenComponent : public juce::Component,
                            private juce::Timer
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
        juce::String signInSteps { "1. Open the site - the code fills itself in.\n2. Sign in with your e-mail (a code arrives) or Telegram.\n3. Come back here." };
        juce::String waiting { "Waiting for confirmation - the code works for {{time}}" };
        juce::String openSite { "Open the site" }, cancel { "Cancel" };
        juce::String noPasswords { "No passwords in the app: you sign in on the site, the app gets a key you can revoke." };

        juce::String notYet { "Not connected yet: this is the layout, the Live server comes next." };
    };

    LiveScreenComponent();
    ~LiveScreenComponent() override;

    void setStrings (Strings);

    enum class Tab { seminar, battle, rating };
    void showTab (Tab);

    // For Settings -> Live and account.
    void openSignIn();

    // For tools/EditorSnapshots.
    void openRoomForSnapshot (bool local);
    void openInvitesForSnapshot();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutSeminar (juce::Rectangle<int>);
    void layoutBattle (juce::Rectangle<int>);
    void layoutRating (juce::Rectangle<int>);
    void layoutOverlay();
    void refreshVisibility();
    void setNote (const juce::String&);
    void makeCodes();
    juce::String roomAddress() const;

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

    // Battle
    SegmentedChoice battleFamily;
    juce::TextButton searchButton, challengeButton, battleSignInButton;

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
    juce::TextEditor inviteEditor;
    struct Invitee { juce::String name, mail, code; };
    std::vector<Invitee> invitees;

    juce::String note;
    juce::Rectangle<int> cardA, cardB, overlayBox, tableArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveScreenComponent)
};
