#include "RunResultsComponent.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

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

void RunResultsComponent::show (Summary newSummary)
{
    summary = std::move (newSummary);

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

    return juce::Rectangle<int> (juce::jlimit (480, getWidth() - 60,
                                                juce::roundToInt ((float) getWidth() * 0.72f)),
                                  juce::jmin (getHeight() - 40, contentHeight + Spacing::large * 2))
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

    auto inner = card.reduced ((float) AbcTrainTheme::Spacing::large).toNearestInt();
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
        auto row = inner.removeFromTop (64);
        const auto columnWidth = row.getWidth() / 4;

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

        auto column = row.removeFromLeft (columnWidth);
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

        column = row.removeFromLeft (columnWidth);
        paintStat (g, column, accuracyCaption,
                   juce::String (juce::roundToInt (summary.runAccuracy * 100.0f * counted)) + "%", theme.text);
        note (column, fillIn (detail.ofInBand, { { "n", juce::String (inBand) }, { "m", juce::String (total) } }));

        column = row.removeFromLeft (columnWidth);
        paintStat (g, column, detail.precision, juce::String (juce::roundToInt (median * 100.0f * counted)) + "%",
                   theme.text);
        note (column, detail.precisionNote);

        column = row;
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

        auto plot = inner.removeFromTop (plotHeight).toFloat().withTrimmedLeft (26.0f).reduced (0.0f, 5.0f);
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
        }
        else if (n > 0)
        {
            std::vector<float> values;
            for (const auto& m : summary.marks)
                values.push_back (m.correct ? juce::jmax (0.08f, m.quality) : 0.0f);

            line (values, theme.textDim);

            for (int i = 0; i < n && (float) i <= shownUpTo + 0.001f; ++i)
            {
                const auto& m = summary.marks[(size_t) i];
                g.setColour (m.correct ? theme.positive : theme.negative);
                g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ xAt (i), plot.getBottom() - plot.getHeight() * values[(size_t) i] }));
            }
        }

        // Round numbers under the line, every one up to twelve, then sparser.
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.0f));
        const auto first = battle ? 1 : 0;
        const auto every = n > 12 ? (n + 11) / 12 : 1;
        for (int i = first; i < n; i += every)
            AbcTrainLookAndFeel::fitText (g, juce::String (battle ? i : i + 1),
                                          juce::Rectangle<float> (xAt (i) - 12.0f, plot.getBottom() + 3.0f, 24.0f, 12.0f).toNearestInt(),
                                          juce::Justification::centred, false);
        inner.removeFromTop (12);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::large);

    // --- by range, and what to do about it ---------------------------------
    //
    // Counts, not a histogram: "Mids 2 / 3" says exactly what happened,
    // where a bar a few pixels tall made the reader estimate it.
    {
        auto block = inner.removeFromTop (16 + 6 * 20);
        auto left = block.removeFromLeft (juce::jmin (330, block.getWidth() / 2));
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
            const auto count = juce::jmin (7, (int) summary.buckets.size());
            auto plot = left.withTrimmedBottom (18).toFloat().reduced (8.0f, 8.0f);
            const auto xAt = [&] (int i) { return count <= 1 ? plot.getCentreX() : plot.getX() + plot.getWidth() * (float) i / (float) (count - 1); };

            for (int k = 0; k <= 2; ++k)
            {
                g.setColour (theme.divider);
                g.drawHorizontalLine (juce::roundToInt (plot.getBottom() - plot.getHeight() * (float) k * 0.5f), plot.getX(), plot.getRight());
            }

            juce::Path p;
            bool open = false;
            for (int b = 0; b < count; ++b)
            {
                const auto& bucket = summary.buckets[(size_t) b];
                if (bucket.attempts == 0) { open = false; continue; }
                const auto hit = 1.0f - bucket.missRate();
                const juce::Point<float> pt { xAt (b), plot.getBottom() - plot.getHeight() * hit * counted };
                if (! open) p.startNewSubPath (pt); else p.lineTo (pt);
                open = true;
            }
            g.setColour (theme.textDim);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            for (int b = 0; b < count; ++b)
            {
                const auto& bucket = summary.buckets[(size_t) b];
                const auto x = xAt (b);

                if (bucket.attempts > 0)
                {
                    const auto hit = 1.0f - bucket.missRate();
                    const auto y = plot.getBottom() - plot.getHeight() * hit * counted;
                    g.setColour (b == worst ? theme.negative : theme.positive);
                    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ x, y }));
                    g.setColour (theme.textDim);
                    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (10.0f));
                    AbcTrainLookAndFeel::fitText (g, juce::String (bucket.attempts - bucket.misses) + "/" + juce::String (bucket.attempts),
                                                  juce::Rectangle<float> (x - 20.0f, y - 18.0f, 40.0f, 12.0f).toNearestInt(),
                                                  juce::Justification::centred, false);
                }

                g.setColour (b == worst ? theme.textBright : theme.textDim);
                g.setFont (AbcTrainLookAndFeel::microFont());
                const auto labelWidth = count <= 1 ? 80.0f : juce::jmax (40.0f, plot.getWidth() / (float) (count - 1));
                AbcTrainLookAndFeel::fitText (g, bucket.label,
                                              juce::Rectangle<float> (x - labelWidth * 0.5f, left.toFloat().getBottom() - 16.0f, labelWidth, 14.0f).toNearestInt(),
                                              b == 0 ? juce::Justification::centredLeft : b == count - 1 ? juce::Justification::centredRight
                                                                                                         : juce::Justification::centred, true);
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

    auto footer = cardBounds().reduced (Spacing::large).removeFromBottom (38);

    // "Play again" is the one filled button, rightmost; Home beside it.
    AbcTrainLookAndFeel::makePrimary (againButton, true);
    againButton.setBounds (footer.removeFromRight (150));
    footer.removeFromRight (Spacing::small);
    homeButton.setBounds (footer.removeFromRight (110));

    // The other modes sit on the left of the same row, so "again" and
    // "differently" are the same distance from the eye - a run ending is
    // the moment somebody is most willing to change how they play.
    //
    // Width comes from what is left rather than being written down as 128:
    // two mode buttons at 128 plus a gap need 272px against the ~204 this
    // row actually has once "Play again" and "Home" have taken their side,
    // so they used to run underneath them.
    if (! modeButtons.isEmpty())
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
        addAndMakeVisible (*button);
    }

    resized();
    repaint();
}
