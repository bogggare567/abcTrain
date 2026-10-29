#include "BattleRecords.h"
#include <algorithm>

juce::String BattleRecords::key (BotListener::Bot b, const char* what)
{
    return juce::String ("battles.bot.") + BotListener::idOf (b) + "." + what;
}

BattleRecords::BotRecord BattleRecords::recordOf (BotListener::Bot b) const
{
    BotRecord r;
    r.wins = properties.getIntValue (key (b, "wins"), 0);
    r.losses = properties.getIntValue (key (b, "losses"), 0);
    r.flawless = properties.getBoolValue (key (b, "flawless"), false);
    return r;
}

int BattleRecords::starsOf (const BotRecord& r) noexcept
{
    return (r.wins >= 1 ? 1 : 0) + (r.wins >= 3 ? 1 : 0) + (r.flawless ? 1 : 0);
}

int BattleRecords::totalStars() const
{
    int n = 0;
    for (int i = 0; i < BotListener::numBots; ++i)
        n += stars ((BotListener::Bot) i);
    return n;
}

std::array<BotListener::Bot, BotListener::numBots> BattleRecords::ladder()
{
    std::array<BotListener::Bot, BotListener::numBots> order {};
    for (int i = 0; i < BotListener::numBots; ++i)
        order[(size_t) i] = (BotListener::Bot) i;

    std::stable_sort (order.begin(), order.end(), [] (BotListener::Bot a, BotListener::Bot b)
    {
        return BotListener::profileOf (a).rating < BotListener::profileOf (b).rating;
    });
    return order;
}

int BattleRecords::priceOf (int ladderIndex) noexcept
{
    constexpr int prices[] { 0, 0, 1, 3, 6, 9 };
    return prices[juce::jlimit (0, 5, ladderIndex)];
}

bool BattleRecords::isOpen (BotListener::Bot b) const
{
    const auto order = ladder();
    const auto index = (int) (std::find (order.begin(), order.end(), b) - order.begin());

    // A bot once beaten stays open even if the prices change later.
    return totalStars() >= priceOf (index) || recordOf (b).wins > 0;
}

int BattleRecords::recordBotBattle (BotListener::Bot b, bool won, bool flawless, int family, juce::Time when)
{
    const auto before = stars (b);

    if (won)
    {
        properties.setValue (key (b, "wins"), recordOf (b).wins + 1);
        if (flawless)
            properties.setValue (key (b, "flawless"), true);

        const auto d = daily (when);
        if (d.bot == b && d.family == family)
            properties.setValue ("battles.dailyWon", dayNumber (when));
    }
    else
    {
        properties.setValue (key (b, "losses"), recordOf (b).losses + 1);
    }

    Recent r;
    r.when = when.toMilliseconds();
    r.who = BotListener::idOf (b);
    r.isBot = true;
    r.place = won ? 1 : 0;
    addRecent (r);   // saves

    return stars (b) - before;
}

int BattleRecords::dayNumber (juce::Time t)
{
    // Local days: the bot of the day changes at the player's midnight.
    const auto local = t.toMilliseconds() + (juce::int64) t.getUTCOffsetSeconds() * 1000;
    return (int) (local / (24LL * 3600 * 1000));
}

BattleRecords::Daily BattleRecords::daily (juce::Time now) const
{
    const auto day = dayNumber (now);
    const auto order = ladder();

    Daily d;
    // Not the two easiest: a bot of the day should be worth the day.
    d.bot = order[(size_t) (2 + (day * 5) % 4)];
    d.family = (day * 3 + 1) % 4;

    const auto localMs = now.toMilliseconds() + (juce::int64) now.getUTCOffsetSeconds() * 1000;
    const auto msIntoDay = localMs % (24LL * 3600 * 1000);
    d.hoursLeft = juce::jmax (1, (int) ((24LL * 3600 * 1000 - msIntoDay + 3599999) / 3600000));
    d.wonToday = properties.getIntValue ("battles.dailyWon", -1) == day;
    return d;
}

std::vector<BattleRecords::Recent> BattleRecords::recent() const
{
    std::vector<Recent> list;
    const auto parsed = juce::JSON::parse (properties.getValue ("battles.recent"));

    if (auto* array = parsed.getArray())
        for (const auto& v : *array)
        {
            Recent r;
            r.when = (juce::int64) v.getProperty ("when", 0);
            r.who = v.getProperty ("who", "").toString();
            r.isBot = (bool) v.getProperty ("bot", false);
            r.place = (int) v.getProperty ("place", 0);
            r.ratingDelta = (int) v.getProperty ("delta", 0);
            r.rated = (bool) v.getProperty ("rated", false);
            list.push_back (r);
        }

    return list;
}

void BattleRecords::addRecent (const Recent& r)
{
    auto list = recent();
    list.insert (list.begin(), r);
    if ((int) list.size() > recentKept)
        list.resize ((size_t) recentKept);

    juce::Array<juce::var> array;
    for (const auto& e : list)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("when", e.when);
        o->setProperty ("who", e.who);
        o->setProperty ("bot", e.isBot);
        o->setProperty ("place", e.place);
        o->setProperty ("delta", e.ratingDelta);
        o->setProperty ("rated", e.rated);
        array.add (juce::var (o));
    }

    properties.setValue ("battles.recent", juce::JSON::toString (juce::var (array), true));
    properties.saveIfNeeded();
}
