#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "BotListener.h"
#include <array>
#include <vector>

/**
    What the battles leave behind on this computer (Battle-PC board,
    2026-10-24): the bot ladder with its stars, the bot of the day, and the
    last few battles.

    The ladder is the six bots by their Decibelo, weakest first. Stars per
    bot: the first win, three wins, and a win without a single round lost
    (a flawless win, any time). A bot opens when the stars on the whole
    ladder reach its price - the first two are open from the start - so
    the strong ones are earned, not just clicked.

    The bot of the day is the same for everyone on a given day (the day
    number picks a bot and a family): something to come back for, nothing
    to pay with.

    Kept in the user's settings file next to the rest of the progress,
    never sent anywhere (offline-first).
*/
class BattleRecords
{
public:
    explicit BattleRecords (juce::PropertiesFile& file) : properties (file) {}

    struct BotRecord { int wins = 0, losses = 0; bool flawless = false; };

    BotRecord recordOf (BotListener::Bot) const;
    static int starsOf (const BotRecord&) noexcept;
    int stars (BotListener::Bot b) const { return starsOf (recordOf (b)); }
    int totalStars() const;

    /** The ladder: bots by rating, weakest first. */
    static std::array<BotListener::Bot, BotListener::numBots> ladder();
    static int priceOf (int ladderIndex) noexcept;   // stars needed to open
    bool isOpen (BotListener::Bot) const;

    /** After a battle with a bot. Returns how many stars it added. */
    int recordBotBattle (BotListener::Bot, bool won, bool flawless, int family, juce::Time when);

    // ---- the bot of the day ----
    struct Daily { BotListener::Bot bot; int family; int hoursLeft; bool wonToday; };
    Daily daily (juce::Time now) const;

    // ---- the last battles ----
    struct Recent
    {
        juce::int64 when = 0;     // ms since epoch
        juce::String who;         // a bot's id or the opponents' nicks
        bool isBot = false;
        int place = 0;            // 1 won, 2+ place in a room; 0 lost a duel, -1 a draw
        int ratingDelta = 0;      // online, rated; 0 otherwise
        bool rated = false;
    };
    std::vector<Recent> recent() const;
    void addRecent (const Recent&);
    static constexpr int recentKept = 8;

private:
    static juce::String key (BotListener::Bot b, const char* what);
    static int dayNumber (juce::Time t);
    juce::PropertiesFile& properties;
};
