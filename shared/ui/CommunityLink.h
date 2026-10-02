#pragma once

#include <juce_core/juce_core.h>

// Where the people around abcTrain live: the Telegram group. One address,
// opened in the browser like every other link in the app - no bot, no
// login, no request of our own (ADR 053). Changing the community means
// changing this line.
namespace CommunityLink
{
    inline constexpr const char* url = "https://t.me/vstabcchat";
    inline constexpr const char* shortUrl = "t.me/vstabcchat";

    inline void open() { juce::URL (url).launchInDefaultBrowser(); }
}
