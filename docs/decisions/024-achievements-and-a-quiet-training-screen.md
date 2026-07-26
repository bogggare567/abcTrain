# 024 — Achievements instead of a bar, and a training screen that holds one thing

Status: accepted.

Three changes that turn out to be the same change: the training screen
should contain the thing you are answering with, and nothing else.

## The bar is gone

`LevelProgressBar` sat under every round, reporting distance to the next
level. Two problems with that as the main feedback of a training app:

- **It answers a question about the app, not about hearing.** "62% of the
  way to level 6" says nothing about *what* got better. It is the same
  shape whichever exercise you played and whatever you learned.
- **It is on screen whether or not anything happened.** Permanent
  furniture is the opposite of feedback.

It has moved to the home screen — the screen you plan from — along with
the level selector, the streak and the daily challenge.

## Achievements are claims about your own history

`Source/Achievements.{h,cpp}`: a list of named things, each a rule over
numbers `ProgressManager` already keeps, evaluated purely.

The design constraint that shaped the list: **every achievement is a real
claim about what the player did.** No trophies for opening the app, and
nothing comparing the player to anyone else — there is no server, and a
fabricated percentile would be a lie (this came up directly, and was
refused then too; see `docs/roadmap.md`).

Two consequences worth recording, because both were tempting to get
wrong:

- **Accuracy achievements need a floor of rounds.** Three answers, three
  correct, is 100% accuracy. Without `accuracyMinimumRounds`, the
  hardest-*sounding* achievements would be the first ones anyone got, and
  the whole list would be worthless within five minutes. Progress toward
  them reports whichever gate is further behind, so someone three rounds
  in sees "3 rounds of 20", not "90% of the way there".
- **There is no "reach level N" achievement**, even though
  `Kind::levelReached` exists for one. Level is player-selectable from a
  dropdown (`setLevelManually`, added so difficulty isn't only an
  automatic side effect of points), and ADR 002's decision that there is
  exactly *one* notion of level means the earned and chosen paths are
  indistinguishable by design. An achievement for reaching a level would
  therefore be earned by opening a menu. `AchievementsTest` asserts this
  directly, so nobody adds one back by accident.

Ids are strings, not indices, because they are the persistence key and
they must survive the list being reordered. Earned ids are never removed:
an achievement records something that happened, so it must not un-earn
itself when a later bad run drags an average back down.

Loading backfills silently — a player who already had 500 correct answers
before achievements existed does not have to earn "your first hundred"
again, and does not get a burst of toasts on first launch, because
`onAchievementEarned` isn't wired yet at load time.

## What appears during a round

`Source/AchievementToast.h` — a card that slides down from the top edge
when one is earned, holds ~2.6s, and leaves. It is the only thing that
appears on the training screen without being asked for.

`setInterceptsMouseClicks (false, false)` is not incidental: a card
drifting across the top of the answer area must never swallow a click
meant for the scale underneath. It is also added last, so it paints over
everything — the same z-order rule ADR 015 and 017 each had to learn
once.

Showing one while another is still up retargets rather than queueing. The
newest is the one worth reading, and a queued card would arrive long
after the moment that earned it.

## The hint no longer reserves a strip

The vectorscope and spectrum used to be laid out on every round —
dimmed, captioned "not bought yet" — specifically so buying one could
never resize the window under the player.

That trade was wrong, and it was called out as such: a strip of grey
placeholder present every round is worse than a window that changes size
on the rare occasions you actually buy a hint. The panel now does not
exist until it is bought, and the window grows by exactly its height when
it is. `getLogicalHeight()` is the single place that decides, so the
window size and the layout can never disagree.

It is also no longer called "scopes". "Scope" is jargon, and jargon is
the wrong register for a thing you are buying because you are stuck. It
is now **"Show the sound" / "Показать звук"**, with the panel headed
"What the sound looks like", in all twelve languages.

## What this did not do

The home screen shows an `n / total` count, not a grid of achievement
cards with their descriptions and progress bars. `Achievements::
progressTowards` exists and is tested precisely so that screen can be
built without touching the rules — but it is a screen, and it is not this
change.
