#include "RunResultsComponent.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include <array>

namespace
{
    constexpr int tickHz = 60;
    constexpr double appearMs = 260.0;
    constexpr double countMs = 620.0;
}

RunResultsComponent::RunResultsComponent()
{
    setOpaque (true);

    againButton.onClick = [this] { if (onPlayAgain != nullptr) onPlayAgain(); };
    addAndMakeVisible (againButton);

    homeButton.onClick = [this] { if (onGoHome != nullptr) onGoHome(); };
    addAndMakeVisible (homeButton);

    shareButton.setComponentID ("results.share");
    shareButton.onClick = [this]
    {
        if (summary.shareText.isEmpty())
            return;
        juce::SystemClipboard::copyTextToClipboard (summary.shareText);
        shared = true;
        refreshShare();
    };
    addChildComponent (shareButton);

    communityButton.setComponentID ("results.community");
    communityButton.onClick = [this] { if (onOpenCommunity != nullptr) onOpenCommunity(); };
    addChildComponent (communityButton);

    startTimerHz (tickHz);
}

RunResultsComponent::~RunResultsComponent()
{
    stopTimer();
}

void RunResultsComponent::setStrings (juce::String title, juce::String again, juce::String home,
                                       juce::String score, juce::String accuracy,
                                       juce::String streak, juce::String best,
                                       juce::String newBest, juce::String whereYouStand)
{
    titleText = std::move (title);
    againText = std::move (again);
    homeText = std::move (home);
    scoreCaption = std::move (score);
    accuracyCaption = std::move (accuracy);
    streakCaption = std::move (streak);
    bestCaption = std::move (best);
    newBestText = std::move (newBest);
    whereYouStandText = std::move (whereYouStand);

    againButton.setButtonText (againText);
    homeButton.setButtonText (homeText);

    repaint();
}

void RunResultsComponent::setShareStrings (juce::String share, juce::String copied, juce::String community)
{
    shareLabel = std::move (share);
    copiedLabel = std::move (copied);
    communityLabel = std::move (community);
    refreshShare();
}

void RunResultsComponent::refreshShare()
{
    const auto offer = summary.shareText.isNotEmpty();
    shareButton.setVisible (offer);
    shareButton.setButtonText (shared ? copiedLabel : shareLabel);
    communityButton.setVisible (offer && shared);
    communityButton.setButtonText (communityLabel);

    for (auto* b : modeButtons)
        b->setVisible (! offer);

    resized();
}

void RunResultsComponent::show (Summary newSummary)
{
    summary = std::move (newSummary);
    shared = false;
    refreshShare();

    appearAmount = 0.0f;
    countAmount = 0.0f;

    setVisible (true);
    toFront (false);
    resized();
    repaint();
}

void RunResultsComponent::completeAnimation()
{
    appearAmount = 1.0f;
    countAmount = 1.0f;
    repaint();
}

void RunResultsComponent::timerCallback()
{
    if (! isVisible())
        return;

    auto changed = false;

    if (appearAmount < 1.0f)
    {
        appearAmount = juce::jmin (1.0f, appearAmount + (float) (1000.0 / (double) tickHz / appearMs));
        changed = true;
    }
    else if (countAmount < 1.0f)
    {
        // Numbers only start climbing once the card has arrived, so the
        // two motions read as one sequence rather than as a scramble.
        countAmount = juce::jmin (1.0f, countAmount + (float) (1000.0 / (double) tickHz / countMs));
        changed = true;
    }

    if (changed)
        repaint();
}

void RunResultsComponent::setDetailStrings (DetailStrings strings)
{
    detail = std::move (strings);
    repaint();
}

juce::Rectangle<int> RunResultsComponent::cardBounds() const
{
    // Height follows the content, width a share of the window with a
    // floor - a dialogue that keeps its old width in a bigger window reads
    // as something that failed to notice the window.
    using namespace AbcTrainTheme;
    constexpr int contentHeight = 26 + 18 + Spacing::large       // heading
                                    + 64 + Spacing::large           // four numbers
                                    + 16 + 46 + Spacing::large      // round by round
                                    + 16 + 7 * 20 + Spacing::large  // ranges and the sentence
                                    + 38;                           // buttons

    if (compact())
    {
        constexpr int compactHeight = contentHeight
                                        + 64 + Spacing::small             // the second row of numbers
                                        + 16 + 5 * 20                      // the sentence under the plot
                                        + 38 + Spacing::small;             // "Play again" on its own row
        return getLocalBounds().reduced (Spacing::small, 0)
                   .withSizeKeepingCentre (getWidth() - Spacing::small * 2,
                                           juce::jmin (getHeight() - Spacing::small * 2,
                                                       compactHeight + Spacing::medium * 2));
    }

    // Short: no by-range block, but the plot is 62 tall, not the 46 above,
    // and its round numbers need their line.
    const auto height = contentHeight - (shortCard() ? 16 + 7 * 20 + Spacing::large - 16 - 12 : 0);

    return juce::Rectangle<int> (juce::jlimit (480, getWidth() - 60,
                                                juce::roundToInt ((float) getWidth() * 0.72f)),
                                  juce::jmin (getHeight() - Spacing::medium * 2, height + Spacing::large * 2))
               .withCentre (getLocalBounds().getCentre());
}

void RunResultsComponent::paintStat (juce::Graphics& g, juce::Rectangle<int> area,
                                      const juce::String& caption, const juce::String& value,
                                      juce::Colour valueColour)
{
    // Left-aligned, caption over value over a note: four of these in a row
    // read as a line of facts, where centred ones read as a scoreboard.
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (caption),
                                           area.removeFromTop (13).toFloat(),
                                           AbcTrainLookAndFeel::microFont(), theme.textDim, 1.4f);

    g.setColour (valueColour);
    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (28.0f));
    AbcTrainLookAndFeel::fitText (g, value, area.removeFromTop (34), juce::Justification::centredLeft, false);
}

namespace
{
    juce::String fillIn (juce::String text, std::initializer_list<std::pair<const char*, juce::String>> fields)
    {
        for (const auto& [key, value] : fields)
            text = text.replace (juce::String ("{{") + key + "}}", value);

        return text;
    }
}

void RunResultsComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    const auto eased = AbcTrainTheme::Ease::out (appearAmount);
    const auto card = cardBounds().toFloat().translated (0.0f, (1.0f - eased) * 14.0f);

    juce::DropShadow (theme.shadow.withAlpha (0.55f * theme.shadowStrength * eased), 26, { 0, 8 })
        .drawForRectangle (g, card.toNearestInt());

    g.setColour (theme.panelBackground);
    g.setOpacity (eased);
    g.fillRect (card);
    g.setOpacity (1.0f);
    g.setColour (theme.outline);
    g.drawRect (card, 1.0f);

    auto inner = card.reduced ((float) (compact() ? AbcTrainTheme::Spacing::medium
                                                  : AbcTrainTheme::Spacing::large)).toNearestInt();
    const auto counted = AbcTrainTheme::Ease::out (countAmount);

    // --- heading: which exercise, which mode, how it ended ---------------
    {
        auto top = inner.removeFromTop (26);

        if (summary.isNewBest)
        {
            const auto width = juce::roundToInt (AbcTrainLookAndFeel::trackedTextWidth (
                                   newBestText, AbcTrainLookAndFeel::headingFont(), 0.0f)) + 28;
            auto pill = top.removeFromRight (width).withSizeKeepingCentre (width, 24).toFloat();
            g.setColour (theme.positive.withAlpha (0.14f));
            g.fillRect (pill);
            g.setColour (theme.positive.withAlpha (0.6f));
            g.drawRect (pill, 1.0f);
            g.setColour (theme.positive);
            g.setFont (AbcTrainLookAndFeel::headingFont());
            AbcTrainLookAndFeel::fitText (g, newBestText, pill.toNearestInt(), juce::Justification::centred, false);
        }
        else if (summary.badge.isNotEmpty())
        {
            g.setFont (AbcTrainLookAndFeel::headingFont());
            const auto width = juce::jmin (top.getWidth() / 2,
                                           juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), summary.badge)) + 28);
            auto pill = top.removeFromRight (width).withSizeKeepingCentre (width, 24).toFloat();
            g.setColour (theme.accentWarm.withAlpha (0.14f));
            g.fillRect (pill);
            g.setColour (theme.accentWarm.withAlpha (0.7f));
            g.drawRect (pill, 1.0f);
            // A leading star is drawn, not typed: the font has no star of
            // its own and the fallback one came out twice the size (Mac).
            auto label = summary.badge;
            auto inside = pill.toNearestInt().reduced (8, 0);
            g.setColour (theme.accentWarm);
            const auto star = juce::String::fromUTF8 ("\xe2\x98\x85");
            if (label.startsWith (star))
            {
                label = label.substring (star.length()).trimStart();
                auto box = inside.removeFromLeft (16).withSizeKeepingCentre (12, 12).toFloat();
                juce::Path p;
                p.addStar (box.getCentre(), 5, box.getWidth() * 0.22f, box.getWidth() * 0.5f, -juce::MathConstants<float>::halfPi * 0.0f);
                g.fillPath (p);
                inside.removeFromLeft (4);
            }
            AbcTrainLookAndFeel::fitText (g, label, inside, juce::Justification::centred, true);
        }

        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::headingFont().withHeight (22.0f));
        AbcTrainLookAndFeel::fitText (g, summary.exerciseName + "  ·  " + summary.modeName, top,
                    juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        AbcTrainLookAndFeel::fitText (g, titleText, inner.removeFromTop (18), juce::Justification::centredLeft, true);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::large);

    // --- four numbers, one line ------------------------------------------
    //
    // What the run was (rounds), how often you were inside the band, how
    // far inside, and what that did to the staircase. Each with a note
    // that says what the number is measured against, because a bare "92%"
    // leaves the player to guess what 92 percent of.
    {
        // Four cells: one row of four, or (phone) two rows of two.
        std::array<juce::Rectangle<int>, 4> cells;
        {
            if (compact())
            {
                auto first = inner.removeFromTop (64);
                inner.removeFromTop (AbcTrainTheme::Spacing::small);
                auto second = inner.removeFromTop (64);
                const auto half = first.getWidth() / 2;
                cells = { first.removeFromLeft (half), first, second.removeFromLeft (half), second };
            }
            else
            {
                auto row = inner.removeFromTop (64);
                const auto quarter = row.getWidth() / 4;
                cells = { row.removeFromLeft (quarter), row.removeFromLeft (quarter), row.removeFromLeft (quarter), row };
            }
        }

        const auto total = (int) summary.marks.size();
        int inBand = 0;
        std::vector<float> qualities;

        for (const auto& m : summary.marks)
        {
            if (m.correct)
            {
                ++inBand;
                qualities.push_back (m.quality);
            }
        }

        std::sort (qualities.begin(), qualities.end());
        const auto median = qualities.empty() ? 0.0f : qualities[qualities.size() / 2];

        auto note = [&] (juce::Rectangle<int> column, const juce::String& text)
        {
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            AbcTrainLookAndFeel::fitLines (g, text, column.withTrimmedTop (48), juce::Justification::topLeft, 1, 0.85f);
        };

        auto column = cells[0];
        if (summary.pointsText.isNotEmpty())
        {
            // Points first - precision counts - with the count and the
            // record (still kept in right answers) underneath.
            paintStat (g, column, detail.points, counted >= 0.999f ? summary.pointsText
                                                                    : juce::String (juce::roundToInt ((float) summary.score * counted)),
                       summary.isNewBest ? theme.positive : theme.textBright);
            note (column, summary.pointsNote.isNotEmpty()
                            ? summary.pointsNote
                            : fillIn (detail.pointsNote, { { "n", juce::String (summary.score) },
                                                           { "best", juce::String (juce::jmax (summary.previousBest, summary.score)) } }));
        }
        else
        {
            paintStat (g, column, detail.rounds, juce::String (juce::roundToInt ((float) summary.score * counted)),
                       summary.isNewBest ? theme.positive : theme.textBright);
            note (column, bestCaption + " " + juce::String (juce::jmax (summary.previousBest, summary.score)));
        }

        column = cells[1];
        paintStat (g, column, accuracyCaption,
                   juce::String (juce::roundToInt (summary.runAccuracy * 100.0f * counted)) + "%", theme.text);
        note (column, fillIn (detail.ofInBand, { { "n", juce::String (inBand) }, { "m", juce::String (total) } }));

        column = cells[2];
        paintStat (g, column, detail.precision, juce::String (juce::roundToInt (median * 100.0f * counted)) + "%",
                   theme.text);
        note (column, detail.precisionNote);

        column = cells[3];
        const auto moved = summary.levelBefore != summary.levelAfter;
        paintStat (g, column, detail.threshold, moved ? summary.levelAfter : summary.levelBefore,
                   summary.levelDelta > 0 ? theme.positive : theme.text);
        note (column, summary.levelDelta > 0 ? detail.narrower
                    : summary.levelDelta < 0 ? detail.wider : detail.unchanged);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::large);

    // --- round by round ----------------------------------------------------
    //
    // A line, not a row of bars (the owner's call: graphs, not columns).
    // Each point is one answer: how close it was, 0 to 1, green inside the
    // band and red outside, so the run's shape - a wobble at round five -
    // reads as a dip in one line. In a battle the two HP lines take the
    // same place: yours in the accent, the opponent's warm, falling as
    // the damage lands - who was ahead when, not only who won.
    {
        const auto battle = ! summary.hpYou.empty() && summary.hpYou.size() == summary.hpThem.size();
        auto heading = inner.removeFromTop (16);

        // Who is which line - two colours with no key was a guess.
        if (battle)
        {
            g.setFont (AbcTrainLookAndFeel::labelFont());
            for (auto [label, colour] : { std::pair { summary.themLabel, theme.accentWarm }, std::pair { summary.youLabel, theme.accent } })
            {
                if (label.isEmpty())
                    continue;
                const auto w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label)) + 4;
                auto key = heading.removeFromRight (w + 14);
                g.setColour (theme.textDim);
                AbcTrainLookAndFeel::fitText (g, label, key.removeFromRight (w), juce::Justification::centredLeft, false);
                g.setColour (colour);
                g.fillEllipse (key.removeFromRight (14).toFloat().withSizeKeepingCentre (7.0f, 7.0f));
                heading.removeFromRight (AbcTrainTheme::Spacing::medium);
            }
        }

        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (detail.roundByRound),
                                               heading.toFloat(),
                                               AbcTrainLookAndFeel::microFont(), theme.textDim, 1.4f);

        // A battle has no by-range block under it, so its HP lines take
        // the room that block would have had instead of leaving it empty.
        auto anyBucketTried = false;
        for (const auto& bucket : summary.buckets)
            anyBucketTried = anyBucketTried || bucket.attempts > 0;
        const auto spare = inner.getHeight() - 38 - 2 * AbcTrainTheme::Spacing::large - 12;
        const auto plotHeight = anyBucketTried ? 62 : juce::jlimit (62, 220, spare);

        auto plotRow = inner.removeFromTop (plotHeight);

        // A battle on a computer: the damage round by round beside the HP
        // lines (Battle-Result board) - what each round cost you and what
        // it cost them, as two bars from the middle.
        juce::Rectangle<int> damageBox;
        if (battle && ! compact() && plotRow.getWidth() > 640)
        {
            damageBox = plotRow.removeFromRight (juce::jmin (360, plotRow.getWidth() * 36 / 100));
            plotRow.removeFromRight (AbcTrainTheme::Spacing::large);
        }

        auto plot = plotRow.toFloat().withTrimmedLeft (26.0f).reduced (0.0f, 5.0f);
        const auto n = battle ? (int) summary.hpYou.size() + 1 : (int) summary.marks.size();

        // Grid: three faint lines and their labels.
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.0f));
        for (int k = 0; k <= 2; ++k)
        {
            const auto y = plot.getBottom() - plot.getHeight() * (float) k * 0.5f;
            g.setColour (theme.divider);
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
            g.setColour (theme.textDim);
            AbcTrainLookAndFeel::fitText (g, battle ? juce::String (k * 50) : juce::String (k * 50) + "%",
                                          juce::Rectangle<float> (plot.getX() - 28.0f, y - 7.0f, 24.0f, 14.0f).toNearestInt(),
                                          juce::Justification::centredRight, false);
        }

        const auto xAt = [&] (int i) { return n <= 1 ? plot.getCentreX() : plot.getX() + plot.getWidth() * (float) i / (float) (n - 1); };
        const auto shownUpTo = counted * (float) (n - 1);

        const auto line = [&] (const std::vector<float>& values01, juce::Colour colour)
        {
            juce::Path p;
            for (int i = 0; i < (int) values01.size() && (float) i <= shownUpTo + 0.001f; ++i)
            {
                const juce::Point<float> pt { xAt (i), plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, values01[(size_t) i]) };
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            g.setColour (colour);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        if (battle)
        {
            std::vector<float> you { 1.0f }, them { 1.0f };
            for (size_t i = 0; i < summary.hpYou.size(); ++i)
            {
                you.push_back (summary.hpYou[i] / 100.0f);
                them.push_back (summary.hpThem[i] / 100.0f);
            }
            line (them, theme.accentWarm);
            line (you, theme.accent);

            for (int i = 0; i < n && (float) i <= shownUpTo + 0.001f; ++i)
                for (auto [values, colour] : { std::pair { &you, theme.accent }, std::pair { &them, theme.accentWarm } })
                {
                    g.setColour (colour);
                    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ xAt (i), plot.getBottom() - plot.getHeight() * (*values)[(size_t) i] }));
                }

            if (! damageBox.isEmpty())
            {
                auto box = damageBox;
                auto head = box.removeFromTop (16);
                const auto numW = 22;
                const auto half = (box.getWidth() - numW) / 2;

                head.removeFromLeft (numW);
                AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (detail.damageTaken),
                                                       head.removeFromLeft (half).withTrimmedRight (6).toFloat(),
                                                       AbcTrainLookAndFeel::microFont(), theme.textDim, 1.2f);
                AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (detail.damageDealt),
                                                       head.withTrimmedLeft (6).toFloat(),
                                                       AbcTrainLookAndFeel::microFont(), theme.textDim, 1.2f);

                const auto rounds = (int) summary.hpYou.size();
                const auto rowH = juce::jmin (22.0f, (float) box.getHeight() / (float) juce::jmax (1, rounds));
                auto prevYou = 100.0f, prevThem = 100.0f;

                for (int i = 0; i < rounds; ++i)
                {
                    auto row = box.removeFromTop (juce::roundToInt (rowH));
                    const auto taken = juce::jmax (0.0f, prevYou - summary.hpYou[(size_t) i]);
                    const auto dealt = juce::jmax (0.0f, prevThem - summary.hpThem[(size_t) i]);
                    prevYou = summary.hpYou[(size_t) i];
                    prevThem = summary.hpThem[(size_t) i];

                    if ((float) i > shownUpTo + 0.001f)
                        continue;

                    g.setColour (theme.textDim);
                    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.0f));
                    AbcTrainLookAndFeel::fitText (g, juce::String (i + 1), row.removeFromLeft (numW), juce::Justification::centredLeft, false);

                    auto left = row.removeFromLeft (half).reduced (3, 0);
                    auto right = row.reduced (3, 0);
                    const auto barH = juce::jmin (10.0f, rowH - 6.0f);
                    constexpr float maxDamage = 20.0f;   // SessionManager::damageFor's ceiling

                    const auto labelW = 30;
                    auto leftTrack = left.withTrimmedLeft (labelW).toFloat();
                    auto rightTrack = right.withTrimmedRight (labelW).toFloat();

                    if (taken >= 0.5f)
                    {
                        const auto w = leftTrack.getWidth() * juce::jmin (1.0f, taken / maxDamage);
                        g.setColour (theme.accentWarm);
                        g.fillRect (juce::Rectangle<float> (leftTrack.getRight() - w, leftTrack.getCentreY() - barH * 0.5f, w, barH));
                        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (11.0f));
                        AbcTrainLookAndFeel::fitText (g, juce::String::fromUTF8 ("\xe2\x88\x92") + juce::String (juce::roundToInt (taken)),
                                                      left.removeFromLeft (labelW), juce::Justification::centredLeft, false);
                    }

                    if (dealt >= 0.5f)
                    {
                        const auto w = rightTrack.getWidth() * juce::jmin (1.0f, dealt / maxDamage);
                        g.setColour (theme.positive);
                        g.fillRect (juce::Rectangle<float> (rightTrack.getX(), rightTrack.getCentreY() - barH * 0.5f, w, barH));
                        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (11.0f));
                        AbcTrainLookAndFeel::fitText (g, "+" + juce::String (juce::roundToInt (dealt)),
                                                      right.removeFromRight (labelW), juce::Justification::centredRight, false);
                    }

                    g.setColour (theme.divider);
                    g.drawVerticalLine (left.getRight() + 3, (float) row.getY() + 2.0f, (float) row.getBottom() - 2.0f);
                }
            }
        }
        else if (n > 0)
        {
            // One column per round (visual review, ADR 063): a hit stands as
            // tall as it was close to the centre of the band, a miss is a
            // short block in the warning colour. Read at a glance, where a
            // line through dots had to be followed point by point.
            const auto slot = plot.getWidth() / (float) n;
            const auto barWidth = juce::jlimit (3.0f, 28.0f, slot - juce::jmax (2.0f, slot * 0.18f));

            for (int i = 0; i < n && (float) i <= shownUpTo + 0.001f; ++i)
            {
                const auto& m = summary.marks[(size_t) i];
                const auto height = plot.getHeight() * (m.correct ? juce::jmax (0.18f, m.quality) : 0.14f);
                const auto cx = plot.getX() + slot * ((float) i + 0.5f);
                g.setColour (m.correct ? theme.positive : theme.negative);
                g.fillRect (juce::Rectangle<float> (barWidth, height).withPosition (cx - barWidth * 0.5f, plot.getBottom() - height));
            }
        }

        // Round numbers under the line, every one up to twelve, then sparser.
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.0f));
        const auto first = battle ? 1 : 0;
        const auto every = n > 12 ? (n + 11) / 12 : 1;
        for (int i = first; i < n; i += every)
        {
            const auto cx = battle ? xAt (i) : plot.getX() + plot.getWidth() / (float) n * ((float) i + 0.5f);
            AbcTrainLookAndFeel::fitText (g, juce::String (battle ? i : i + 1),
                                          juce::Rectangle<float> (cx - 12.0f, plot.getBottom() + 3.0f, 24.0f, 12.0f).toNearestInt(),
                                          juce::Justification::centred, false);
        }
        inner.removeFromTop (12);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::large);

    // --- by range, and what to do about it ---------------------------------
    //
    // Counts, not a histogram: "Mids 2 / 3" says exactly what happened,
    // where a bar a few pixels tall made the reader estimate it.
    // A phone on its side has no height for it: the plot and the four
    // numbers say most of it, and the buttons must stay reachable.
    // A battle: the HP lines and the damage took this room; the sentence
    // under them ran into the buttons (the author's screenshot, 2026-10-24).
    const auto battleCard = ! summary.hpYou.empty() && summary.hpYou.size() == summary.hpThem.size();

    if (! shortCard() && ! (battleCard && ! compact()))
    {
        // On a phone the sentence goes under the plot; its 16-point top
        // margin below is the gap between them.
        auto block = inner.removeFromTop (16 + 6 * 20 + (compact() ? 16 + 5 * 20 : 0));
        auto left = compact() ? block.removeFromTop (16 + 6 * 20)
                              : block.removeFromLeft (juce::jmin (330, block.getWidth() / 2));

        if (! compact())
            block.removeFromLeft (AbcTrainTheme::Spacing::large);

        auto anyTried = false;
        for (const auto& bucket : summary.buckets)
            anyTried = anyTried || bucket.attempts > 0;

        if (anyTried)
            AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (detail.byRange),
                                               left.removeFromTop (16).toFloat(),
                                               AbcTrainLookAndFeel::microFont(), theme.textDim, 1.4f);

        auto worst = -1;
        auto worstRate = 0.0f;

        for (int b = 0; b < (int) summary.buckets.size(); ++b)
            if (summary.buckets[(size_t) b].attempts >= 3 && summary.buckets[(size_t) b].missRate() > worstRate)
            {
                worstRate = summary.buckets[(size_t) b].missRate();
                worst = b;
            }

        // A line over the ranges, in their order (low to high, left to
        // right): the share of hits in each. A range never tried has no
        // point - a gap in the line, not a fake zero. Nothing tried at all:
        // no empty axes, the sentence beside says so.
        if (anyTried)
        {
            // A row per range: its name, a bar as long as the share of
            // hits, and the count. The weakest range is the one in the
            // warning colour (visual review: the line over seven points cut
            // its own labels on a phone - "High-mi... Presence").
            const auto count = juce::jmin (7, (int) summary.buckets.size());
            auto rowsArea = left.toFloat();
            const auto rowHeight = juce::jmin (20.0f, rowsArea.getHeight() / (float) juce::jmax (1, count));
            const auto nameWidth = juce::jmin (110.0f, rowsArea.getWidth() * 0.34f);

            for (int b = 0; b < count; ++b)
            {
                const auto& bucket = summary.buckets[(size_t) b];
                auto row = rowsArea.removeFromTop (rowHeight);
                auto name = row.removeFromLeft (nameWidth);
                auto number = row.removeFromRight (44.0f);
                const auto track = row.reduced (6.0f, 0.0f).withSizeKeepingCentre (row.getWidth() - 12.0f, 7.0f);

                g.setColour (b == worst ? theme.textBright : theme.text);
                g.setFont (AbcTrainLookAndFeel::captionFont());
                AbcTrainLookAndFeel::fitText (g, bucket.label, name.toNearestInt(), juce::Justification::centredLeft, true);

                g.setColour (theme.divider);
                g.fillRect (track);

                if (bucket.attempts > 0)
                {
                    const auto hit = 1.0f - bucket.missRate();
                    g.setColour (b == worst ? theme.negative : theme.accent);
                    g.fillRect (track.withWidth (track.getWidth() * hit * counted));
                    g.setColour (theme.textDim);
                    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (11.0f));
                    AbcTrainLookAndFeel::fitText (g, juce::String (bucket.attempts - bucket.misses) + "/" + juce::String (bucket.attempts),
                                                  number.toNearestInt(), juce::Justification::centredRight, false);
                }
            }
        }

        // The sentence: the last miss named in units, then where misses
        // pile up - a place to go and listen, not a grade.
        juce::String text;

        for (auto it = summary.marks.rbegin(); it != summary.marks.rend(); ++it)
            if (! it->correct && it->target.isNotEmpty() && it->answer.isNotEmpty())
            {
                text = fillIn (detail.lastMiss, { { "answer", it->answer }, { "target", it->target } });
                break;
            }

        if (summary.missVerdict.isNotEmpty())
            text = (text.isNotEmpty() ? text + " " : juce::String()) + summary.missVerdict;

        g.setColour (theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        AbcTrainLookAndFeel::fitLines (g, text, block.withTrimmedTop (16), juce::Justification::topLeft, 5, 1.0f);
    }
}

void RunResultsComponent::resized()
{
    using namespace AbcTrainTheme;

    auto footer = cardBounds().reduced (compact() ? Spacing::medium : Spacing::large).removeFromBottom (38);

    // "Play again" is the one filled button, rightmost; Home beside it.
    // On a phone it is the whole bottom row - the thumb's row - and the
    // rest share the row above it.
    AbcTrainLookAndFeel::makePrimary (againButton, true);

    if (compact())
    {
        againButton.setBounds (footer);
        footer = footer.translated (0, -(38 + Spacing::small));
        homeButton.setBounds (footer.removeFromRight (juce::jmin (110, footer.getWidth() / 3)));
        footer.removeFromRight (Spacing::small);
    }
    else
    {
        againButton.setBounds (footer.removeFromRight (150));
        footer.removeFromRight (Spacing::small);
        homeButton.setBounds (footer.removeFromRight (110));
    }

    // The other modes sit on the left of the same row, so "again" and
    // "differently" are the same distance from the eye - a run ending is
    // the moment somebody is most willing to change how they play.
    //
    // Width comes from what is left rather than being written down as 128:
    // two mode buttons at 128 plus a gap need 272px against the ~204 this
    // row actually has once "Play again" and "Home" have taken their side,
    // so they used to run underneath them.
    // A battle: share instead of the other modes - "again" and "show
    // someone" are what a battle ending asks for.
    if (summary.shareText.isNotEmpty())
    {
        shareButton.setBounds (footer.removeFromLeft (juce::jmin (220, footer.getWidth() / 2)));
        footer.removeFromLeft (Spacing::small);
        communityButton.setBounds (footer.removeFromLeft (juce::jmin (200, footer.getWidth() - Spacing::small)));
    }
    else if (! modeButtons.isEmpty())
    {
        const auto gaps = Spacing::small * (modeButtons.size() - 1);
        const auto width = juce::jlimit (72, 128,
                                          (footer.getWidth() - Spacing::medium - gaps)
                                              / modeButtons.size());

        for (auto* button : modeButtons)
        {
            button->setBounds (footer.removeFromLeft (width).withHeight (38));
            footer.removeFromLeft (Spacing::small);
        }
    }
}

void RunResultsComponent::setModeOffer (juce::String caption, juce::StringArray modeNames,
                                         int currentMode)
{
    modeCaption = std::move (caption);
    modeButtons.clear();

    for (int i = 0; i < modeNames.size(); ++i)
    {
        // Only the ways you did *not* just play. Offering the mode that
        // just ended, next to a button that already replays it, is two
        // controls for one action.
        if (i == currentMode)
            continue;

        auto* button = modeButtons.add (new juce::TextButton (modeNames[i]));
        button->onClick = [this, i] { if (onModeChosen != nullptr) onModeChosen (i); };
        addChildComponent (*button);
        button->setVisible (summary.shareText.isEmpty());
    }

    resized();
    repaint();
}
