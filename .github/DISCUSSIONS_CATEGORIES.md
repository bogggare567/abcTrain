# Discussions category descriptions

GitHub's GraphQL API has no `createDiscussionCategory` or
`updateDiscussionCategory` mutation, so these cannot be set from a script —
they have to be typed into **Settings → Discussions → Categories** by hand.
Kept here so they are versioned with everything else rather than living
only in a browser form.

A category description is not decoration: it is the last thing somebody
reads before deciding whether their post belongs here or somewhere else.
Each one below says what goes in it **and what does not**, because a
category that only says what it wants gets everything.

---

### 📣 Announcements
*Format: Announcement (maintainers post, anyone replies)*

> Releases and what changed in them. Every post here is a version: what is
> new, what broke, what you need to do. Nothing else is posted here, so
> watching this category is a way to hear about updates without hearing
> about anything else. Discussion of a release belongs in its replies.

---

### 💬 Q&A
*Format: Q&A (answers can be marked as accepted)*

> "How do I…" and "why does it…". Anything with a correct answer.
>
> Say which plugin, which host, and which OS — the same question has
> different answers in Ableton and Reaper. If something is *broken* rather
> than confusing — a crash, a plugin the host will not load, an installer
> that fails — open an Issue instead, because that needs tracking to a fix
> and a discussion thread cannot be closed by a commit.

---

### 💡 Ideas
*Format: Open-ended discussion*

> What the product should do that it does not.
>
> **Start with the problem, not the solution.** "I cannot tell which
> exercise I am worst at" is something we can solve several ways;
> "add a radar chart" is one solution to a problem nobody has stated, and
> if the chart turns out to be wrong the thread has nowhere left to go.
>
> Ideas that are not built are not rejected — see
> [docs/roadmap.md](../blob/main/docs/roadmap.md) for what is deliberately
> deferred and why.

---

### 👂 Ear training
*Format: Open-ended discussion*

> The subject, not the software. What you are struggling to hear, what
> finally made a frequency range click, what you practise away from the
> plugin, how you set up your listening.
>
> This is the category that is not about abcTrain. It exists because
> everything the product knows about is one small part of learning to
> hear, and the rest of it is worth talking about too.

---

### 🎧 Show and tell
*Format: Open-ended discussion*

> Something you made, or something you got better at.
>
> A mix you are happy with, a before-and-after, a screenshot of a level you
> ground out. No leaderboard exists and none is planned — there is no
> server and no way to verify a number, so any ranking would be decoration
> pretending to be a fact. This is the place where a number means something
> because you attached it to the work.

---

### 🛠 Development
*Format: Open-ended discussion*

> Building it, changing it, porting it. Build failures on your platform,
> JUCE questions, the architecture decisions in
> [docs/decisions/](../tree/main/docs/decisions).
>
> If you are about to write code, this is the right place to check the
> approach first — several things in this repository were deliberately
> *not* built, and the reasoning is written down. A pull request that
> reverses a recorded decision is not unwelcome, but it should argue with
> the ADR rather than around it.

---

## Also worth doing once

- **Pin the welcome discussion** (Discussions → the first post → ⋯ → Pin).
- Set the Announcements category to **Announcement** format so only
  maintainers can start threads there; everything else stays open.
