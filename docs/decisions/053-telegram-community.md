# 053 — The Telegram community: a link and a clipboard, not an integration

Date: 2026-10-02. Status: accepted (Bogdan: «Telegram — социальный слой
abcTrain, а не часть серверной архитектуры игры»).

## Problem

People who play abcTrain have nowhere to find each other: a battle against
a person needs a second person searching at the same minute, and a result
worth showing has no way out of the app. The community exists —
https://t.me/vstabcchat, with a «⚔️ Батлы» topic — but the app never
mentions it.

## Decision

- **One address**, `shared/ui/CommunityLink.h`, opened with
  `juce::URL::launchInDefaultBrowser` like every other link in the app.
- **Where it appears** — secondary, never a banner:
  - Settings → About: a «Community» button under «Сообщить или предложить».
  - Live → Battle, the people card: «Нет соперника? Найди его в Community,
    тема «Батлы»» and a «Найти соперника» button. Finding an opponent there
    is manual; the app does no matchmaking through Telegram.
  - The results card after any battle (bot or person): «Поделиться
    результатом».
- **Share** (`Source/BattleShare`): the editor fills a `BattleShare::Result`
  from what the session already has — nicks, the outcome by HP, HP left,
  rounds landed, rounds played, the server's Decibelo/place line — and
  `BattleShare::format` writes the text. The button copies it to the
  clipboard and turns into «Скопировано — вставь в чат»; «Открыть Community»
  appears beside it. Nothing is sent by the app. The battle engine does not
  know any of this exists; another way of sharing is another consumer of
  the same text.

## Not done, on purpose

Bot API, Telegram login, posting results automatically, any request to
Telegram from the app — the offline-first rule stands (ADR 042). A player's
nick and Decibelo in the chat are the player's own doing: the shared text
carries the nick, so the chat can tell who plays at what rating.
