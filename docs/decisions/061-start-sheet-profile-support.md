# 061. Mode at the door, a profile page, and support that explains itself

Date: 2026-10-19. Status: accepted (Bogdan, from the phone mock-ups: «выбор
режима перед тренировкой, общая статистика и поддержка материальная» -
for the desktop as well).

## Context

The phone mock-ups (claude.ai artifact "abcTrain Mobile") had three things
the desktop app lacked: a start sheet that asks for the mode, a profile with
the player's statistics, and a support page that says what the money is for.

## Decision

- **Start sheet, every time, last mode chosen** (Bogdan: «каждый вход, с
  прошлым выбранным»). Opening an exercise shows what it is, where the
  staircase stands, the three modes with their records (locked ones dimmed,
  saying what opens them) and the training sound; Start is one tap when
  nothing changes. The three mode pills leave the training screen: one
  button there names the mode and opens the sheet. The pills stay as the
  carriers of the state, at zero size, so every path that set a mode keeps
  working.
- **Profile**: totals (rounds, streak, time in trainings), Decibelo per
  family, its history, today's practice and the strongest and weakest
  exercise (20 rounds or more). The server keeps only the current rating, so
  the history is kept on this computer - one point a day, a year at most.
- **Navigation** (Bogdan: «только на телефоне»): the desktop bar gains a
  seventh tab, Profile. A phone's bar holds four - Trainings, Studio, Live,
  Profile; Achievements and Settings open from Profile, the training sound
  from the start sheet.
- **Support**: a page under Profile, also where the desktop footer link now
  goes. It says the app is free and stays free, and what support pays for.
  Money only on the desktop (DonationAlerts in the browser); an iOS build
  shows only the free ways to help until in-app purchases exist (App Store
  rule 3.1.1). The free ways: a GitHub star, the site's link copied, the
  feedback issue.

## Consequences

- Picking an exercise is two taps instead of one; the mock-up's argument is
  that the second tap is the moment to decide.
- No amounts or cost figures on the support page until there are real ones.
