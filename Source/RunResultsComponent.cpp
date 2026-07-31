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
    return juce::Rectangle<int> (juce::jmin (getWidth() - 40, 520),
                                  juce::jmin (getHeight() - 40, 400))
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

    // --- the four numbers -------------------------------------------------
    const auto counted = AbcTrainTheme::Ease::out (countAmount);

    {
        auto row = inner.removeFromTop (46);
        const auto columnWidth = row.getWidth() / 4;

        paintStat (g, row.removeFromLeft (columnWidth), scoreCaption,
                    juce::String (juce::roundToInt ((float) summary.score * counted)),
                    summary.isNewBest ? theme.positive : theme.textBright);

        paintStat (g, row.removeFromLeft (columnWidth), accuracyCaption,
                    juce::String (juce::roundToInt (summary.runAccuracy * 100.0f * counted)) + "%",
                    theme.textBright);

        paintStat (g, row.removeFromLeft (columnWidth), streakCaption,
                    juce::String (juce::roundToInt ((float) summary.bestStreakThisRun * counted)),
                    theme.textBright);

        paintStat (g, row, bestCaption,
                    juce::String (juce::jmax (summary.previousBest, summary.score)),
                    theme.textDim);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::small);

    // A personal best is an event, so it gets a line of its own rather
    // than being left for the player to work out by comparing two numbers.
    if (summary.isNewBest)
    {
        g.setColour (theme.positive);
        g.setFont (AbcTrainLookAndFeel::headingFont());
        g.drawText (newBestText, inner.removeFromTop (20), juce::Justification::centred, false);
    }
    else
    {
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.drawText (accuracyCaption + ": " + juce::String (juce::roundToInt (summary.lifetimeAccuracy * 100.0f))
                        + "%  (" + juce::String (summary.rounds) + ")",
                     inner.removeFromTop (20), juce::Justification::centred, false);
    }

    inner.removeFromTop (AbcTrainTheme::Spacing::medium);

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
    for (auto* button : modeButtons)
    {
        button->setBounds (footer.removeFromLeft (128).withHeight (34));
        footer.removeFromLeft (Spacing::small);
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
