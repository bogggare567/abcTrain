#include "RunResultsComponent.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"

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

juce::Rectangle<int> RunResultsComponent::cardBounds() const
{
    // Height follows the content. It was a flat 400, which left about a
    // hundred and thirty pixels of nothing between the skills row and the
    // buttons - a card mostly made of gap, which is what a results screen
    // must never be: this is the one moment the player is looking *at*
    // rather than through.
    using namespace AbcTrainTheme;
    constexpr int contentHeight = 20 + 26 + 18 + Spacing::large
                                    + 14 + 64 + 26 + Spacing::medium
                                    + 44 + Spacing::large
                                    + 22 + Spacing::small + 56
                                    + Spacing::large + 34 + 20;

    return juce::Rectangle<int> (juce::jmin (getWidth() - 40, 520),
                                  juce::jmin (getHeight() - 40, contentHeight))
               .withCentre (getLocalBounds().getCentre());
}

void RunResultsComponent::paintStat (juce::Graphics& g, juce::Rectangle<int> area,
                                      const juce::String& caption, const juce::String& value,
                                      juce::Colour valueColour)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::drawTrackedText (g, caption.toUpperCase(),
                                           area.removeFromTop (13).toFloat(),
                                           AbcTrainLookAndFeel::captionFont(),
                                           theme.textDim, 1.4f,
                                           juce::Justification::centred);

    g.setColour (valueColour);
    g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (26.0f));
    g.drawText (value, area.removeFromTop (30), juce::Justification::centred, false);
}

void RunResultsComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    const auto eased = AbcTrainTheme::Ease::out (appearAmount);
    const auto card = cardBounds().toFloat().translated (0.0f, (1.0f - eased) * 14.0f);

    juce::Path shape;
    shape.addRoundedRectangle (card, AbcTrainTheme::Radius::panel);

    juce::DropShadow (theme.shadow.withAlpha (0.55f * theme.shadowStrength * eased), 26, { 0, 8 })
        .drawForPath (g, shape);

    g.setColour (theme.panelBackground);
    g.setOpacity (eased);
    g.fillPath (shape);
    g.setOpacity (1.0f);

    g.setColour (theme.outline);
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    auto inner = card.reduced ((float) AbcTrainTheme::Spacing::large).toNearestInt();

    // --- heading: which exercise, which mode -----------------------------
    AbcTrainLookAndFeel::drawTrackedText (g, titleText, inner.removeFromTop (26).toFloat(),
                                           AbcTrainLookAndFeel::headingFont(),
                                           theme.textBright, 1.2f);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    g.drawText (summary.exerciseName + "  ·  " + summary.modeName,
                 inner.removeFromTop (18), juce::Justification::centredLeft, true);

    inner.removeFromTop (AbcTrainTheme::Spacing::large);

    const auto counted = AbcTrainTheme::Ease::out (countAmount);

    // --- the number the run was about ------------------------------------
    //
    // Four numbers at one size is a table, and a table is what you read
    // when you are looking something up - not what you want at the end of
    // ninety seconds of concentrating. The score is the thing that just
    // happened; accuracy, streak and personal best are how to read it.
    // So one of them is large and three are small, which is the whole
    // difference between a result and a receipt.
    {
        AbcTrainLookAndFeel::drawTrackedText (g, scoreCaption.toUpperCase(),
                                               inner.removeFromTop (14).toFloat(),
                                               AbcTrainLookAndFeel::captionFont(),
                                               theme.textDim.withAlpha (0.75f), 1.4f,
                                               juce::Justification::centred);

        auto heroRow = inner.removeFromTop (64);

        g.setColour (summary.isNewBest ? theme.positive : theme.textBright);
        g.setFont (AbcTrainLookAndFeel::displayFont().withHeight (
            58.0f * AbcTrainLookAndFeel::getTextScale()));
        g.drawText (juce::String (juce::roundToInt ((float) summary.score * counted)),
                     heroRow, juce::Justification::centred, false);
    }

    // A personal best is the one thing here worth a moment. It gets a
    // pill rather than a line of green text, because a sentence in the
    // middle of a column of numbers reads as another number.
    if (summary.isNewBest)
    {
        auto badgeRow = inner.removeFromTop (26);
        const auto textWidth = AbcTrainLookAndFeel::trackedTextWidth (
            newBestText, AbcTrainLookAndFeel::headingFont(), 0.0f);
        auto pill = badgeRow.withSizeKeepingCentre (juce::roundToInt (textWidth) + 34, 24).toFloat();

        g.setColour (theme.positive.withAlpha (0.16f));
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (theme.positive.withAlpha (0.55f));
        g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.0f);

        g.setColour (theme.positive);
        g.setFont (AbcTrainLookAndFeel::headingFont());
        g.drawText (newBestText, pill.toNearestInt(), juce::Justification::centred, false);
    }
    else
    {
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.drawText (accuracyCaption + ": " + juce::String (juce::roundToInt (summary.lifetimeAccuracy * 100.0f))
                        + "%  (" + juce::String (summary.rounds) + ")",
                     inner.removeFromTop (26), juce::Justification::centred, false);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::medium);

    // --- the three that put it in context ---------------------------------
    {
        auto row = inner.removeFromTop (44);
        const auto columnWidth = row.getWidth() / 3;

        paintStat (g, row.removeFromLeft (columnWidth), accuracyCaption,
                    juce::String (juce::roundToInt (summary.runAccuracy * 100.0f * counted)) + "%",
                    theme.text);

        paintStat (g, row.removeFromLeft (columnWidth), streakCaption,
                    juce::String (juce::roundToInt ((float) summary.bestStreakThisRun * counted)),
                    theme.text);

        paintStat (g, row, bestCaption,
                    juce::String (juce::jmax (summary.previousBest, summary.score)),
                    theme.textDim);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::large);

    // --- where the four skills stand -------------------------------------
    AbcTrainLookAndFeel::paintSectionHeading (g, inner.removeFromTop (22).toFloat(), whereYouStandText);
    inner.removeFromTop (AbcTrainTheme::Spacing::small);

    {
        auto row = inner.removeFromTop (56);
        const auto columnWidth = row.getWidth() / juce::jmax (1, (int) summary.skills.size());

        for (const auto& skill : summary.skills)
        {
            auto column = row.removeFromLeft (columnWidth).reduced (AbcTrainTheme::Spacing::tight, 0);

            AppIcons::drawBadged (g, skill.icon,
                                   column.removeFromTop (26).withSizeKeepingCentre (26, 26).toFloat(),
                                   skill.isCurrent ? theme.accent : theme.text,
                                   skill.isCurrent ? 1.0f : 0.6f);

            column.removeFromTop (2);

            g.setColour (skill.isCurrent ? theme.textBright : theme.textDim);
            g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (11.0f));
            g.drawText ("L" + juce::String (skill.level), column.removeFromTop (14),
                         juce::Justification::centred, false);

            auto track = column.removeFromTop (4).reduced (4, 0).toFloat();
            g.setColour (theme.displayBackground);
            g.fillRoundedRectangle (track, 2.0f);

            if (skill.levelProgress > 0.001f)
            {
                g.setColour ((skill.isCurrent ? theme.accent : theme.textDim).withAlpha (0.8f));
                g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * skill.levelProgress)),
                                         2.0f);
            }
        }
    }
}

void RunResultsComponent::resized()
{
    using namespace AbcTrainTheme;

    auto footer = cardBounds().reduced (Spacing::large).removeFromBottom (34);

    homeButton.setBounds (footer.removeFromRight (120));
    footer.removeFromRight (Spacing::small);
    againButton.setBounds (footer.removeFromRight (140));

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
            button->setBounds (footer.removeFromLeft (width).withHeight (34));
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
