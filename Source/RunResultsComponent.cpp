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
    g.drawText (value, area.removeFromTop (34), juce::Justification::centredLeft, false);
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
            g.drawText (newBestText, pill.toNearestInt(), juce::Justification::centred, false);
        }

        g.setColour (theme.textBright);
        g.setFont (AbcTrainLookAndFeel::headingFont().withHeight (22.0f));
        g.drawText (summary.exerciseName + "  ·  " + summary.modeName, top,
                    juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText (titleText, inner.removeFromTop (18), juce::Justification::centredLeft, true);
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
            g.drawFittedText (text, column.withTrimmedTop (48), juce::Justification::topLeft, 1, 0.85f);
        };

        auto column = row.removeFromLeft (columnWidth);
        paintStat (g, column, detail.rounds, juce::String (juce::roundToInt ((float) summary.score * counted)),
                   summary.isNewBest ? theme.positive : theme.textBright);
        note (column, bestCaption + " " + juce::String (juce::jmax (summary.previousBest, summary.score)));

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
    // One cell per answer: green inside the band, red outside, and filled
    // from the bottom by how close it was. The run as it happened, where
    // a percentage says only how it averaged out.
    {
        AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (detail.roundByRound),
                                               inner.removeFromTop (16).toFloat(),
                                               AbcTrainLookAndFeel::microFont(), theme.textDim, 1.4f);

        auto strip = inner.removeFromTop (46);
        const auto n = juce::jmax (1, (int) summary.marks.size());
        const auto gap = 4;
        const auto cellWidth = juce::jmin (64, (strip.getWidth() - gap * (n - 1)) / n);

        for (int i = 0; i < (int) summary.marks.size(); ++i)
        {
            const auto& m = summary.marks[(size_t) i];
            auto cell = juce::Rectangle<int> (strip.getX() + i * (cellWidth + gap), strip.getY(), cellWidth, strip.getHeight()).toFloat();
            const auto colour = m.correct ? theme.positive : theme.negative;
            const auto shown = (float) i < counted * (float) n;

            g.setColour (colour.withAlpha (0.10f));
            g.fillRect (cell);

            if (shown)
            {
                const auto height = cell.getHeight() * (m.correct ? juce::jmax (0.12f, m.quality) : 1.0f);
                g.setColour (colour.withAlpha (m.correct ? 0.55f : 0.30f));
                g.fillRect (cell.withTop (cell.getBottom() - height));
            }

            g.setColour (colour.withAlpha (0.7f));
            g.drawRect (cell, 1.0f);

            g.setColour (theme.textBright);
            g.setFont (AbcTrainLookAndFeel::microFont());
            g.drawText (juce::String (i + 1), cell.toNearestInt().withTrimmedTop (4).withHeight (12),
                        juce::Justification::centred, false);
        }
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::large);

    // --- by range, and what to do about it ---------------------------------
    //
    // Counts, not a histogram: "Mids 2 / 3" says exactly what happened,
    // where a bar a few pixels tall made the reader estimate it.
    {
        auto block = inner.removeFromTop (16 + 7 * 20);
        auto left = block.removeFromLeft (juce::jmin (330, block.getWidth() / 2));
        block.removeFromLeft (AbcTrainTheme::Spacing::large);

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

        for (int b = 0; b < juce::jmin (7, (int) summary.buckets.size()); ++b)
        {
            const auto& bucket = summary.buckets[(size_t) b];
            auto line = left.removeFromTop (20);

            g.setColour (b == worst ? theme.textBright : theme.text);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            g.drawText (bucket.label, line.removeFromLeft (96), juce::Justification::centredLeft, true);

            auto count = line.removeFromRight (56);
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (12.0f));
            g.drawText (juce::String (bucket.attempts - bucket.misses) + " / " + juce::String (bucket.attempts),
                        count, juce::Justification::centredRight, false);

            // Hits then misses as one bar, the share of each - the same
            // two colours as the round cells above.
            auto bar = line.reduced (6, 5).toFloat();
            g.setColour (theme.displayBackground);
            g.fillRect (bar);

            if (bucket.attempts > 0)
            {
                const auto hitShare = (float) (bucket.attempts - bucket.misses) / (float) bucket.attempts;
                g.setColour (theme.positive.withAlpha (0.75f));
                g.fillRect (bar.withWidth (bar.getWidth() * hitShare * counted));
                g.setColour (theme.negative.withAlpha (0.85f));
                g.fillRect (bar.withLeft (bar.getX() + bar.getWidth() * hitShare).withWidth (
                    bar.getWidth() * (1.0f - hitShare) * counted));
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
        g.drawFittedText (text, block.withTrimmedTop (16), juce::Justification::topLeft, 5, 1.0f);
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
