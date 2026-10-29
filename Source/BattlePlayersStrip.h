#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include <vector>

/**
    Everyone in a battle, one card each, above the exercise (the battle
    screens of 2026-10-24).

    The round itself is the trainer's own screen - same scale, same A/B,
    same frame (the author: "бери просто оттуда"). What a battle adds is
    who else is in it, so that is all this strip shows: name, HP as a bar
    and a number, and whether they have answered this round.

    Two shapes, chosen by how many are playing, because three people and
    six people are different screens: up to three get tall cards with a
    big HP number, four to six get two rows of compact cards. A player
    who left stays in place, dimmed and struck through - a card that
    vanished mid-battle would shift everyone after it.

    The strip takes the instruction's place in the exercise section, so
    the answer section below keeps exactly the frame it has outside a
    battle.
*/
class BattlePlayersStrip : public juce::Component,
                           private juce::Timer
{
public:
    struct Player
    {
        juce::String name;
        float hp = 100.0f;
        bool you = false;
        bool left = false;       // forfeited / disconnected
        bool answered = false;   // this round
    };

    void setPlayers (std::vector<Player> next)
    {
        // A drop in HP flashes the card and floats the damage up.
        for (size_t i = 0; i < next.size() && i < players.size(); ++i)
            if (next[i].name == players[i].name && next[i].hp < players[i].hp - 0.5f)
            {
                if (flashes.size() < next.size())
                    flashes.resize (next.size(), {});
                flashes[i] = { players[i].hp - next[i].hp, 1.0f };
                startTimerHz (30);
            }

        players = std::move (next);
        flashes.resize (players.size(), {});
        repaint();
    }

    const std::vector<Player>& getPlayers() const noexcept { return players; }

    // Two rows from four players on: the caller sizes the strip with this.
    static int preferredHeight (int playerCount, bool phone)
    {
        if (playerCount <= 3)
            return phone ? 60 : 56;
        return phone ? 2 * 30 + 4 : 2 * 26 + 4;
    }

    void paint (juce::Graphics& g) override
    {
        if (players.empty())
            return;

        const auto& theme = AbcTrainTheme::current();
        const auto n = (int) players.size();
        const auto rows = n <= 3 ? 1 : 2;
        const auto perRow = rows == 1 ? juce::jmax (n, 2) : 3;
        constexpr float gap = 6.0f;

        auto area = getLocalBounds().toFloat();
        const auto cellW = (area.getWidth() - gap * (float) (perRow - 1)) / (float) perRow;
        const auto cellH = (area.getHeight() - gap * (float) (rows - 1)) / (float) rows;
        const auto big = rows == 1 && cellH >= 40.0f;
        const auto oneLine = cellH < 34.0f;   // compact cards: name | bar | number on one line

        for (int i = 0; i < n; ++i)
        {
            const auto& p = players[(size_t) i];
            const auto r = juce::Rectangle<float> (area.getX() + (float) (i % perRow) * (cellW + gap),
                                                   area.getY() + (float) (i / perRow) * (cellH + gap),
                                                   cellW, cellH);
            const auto flash = i < (int) flashes.size() ? flashes[(size_t) i] : Flash {};

            g.setColour (theme.panelBackground.interpolatedWith (theme.negative, 0.35f * flash.amount));
            g.fillRect (r);
            g.setColour (p.you ? theme.accent : theme.outline);
            g.drawRect (r, p.you ? 1.5f : 1.0f);

            const auto alpha = p.left ? 0.45f : 1.0f;
            auto inner = r.reduced (8.0f, big ? 6.0f : 3.0f);

            // Top line: name, then the round state at the right. On a
            // one-line card the "top" is the left 38% and the bar follows.
            auto top = oneLine ? inner.removeFromLeft (inner.getWidth() * 0.38f)
                               : inner.removeFromTop (big ? 18.0f : inner.getHeight() * 0.55f);
            const auto state = p.left ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95"))     // ✕
                             : p.answered ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93"))  // ✓
                                          : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")); // …
            g.setFont (AbcTrainLookAndFeel::labelFont());
            g.setColour ((p.left ? theme.negative : p.answered ? theme.positive : theme.textDim).withMultipliedAlpha (alpha));
            AbcTrainLookAndFeel::fitText (g, state, top.removeFromRight (16.0f), juce::Justification::centredRight, false);
            g.setColour ((p.you ? theme.textBright : theme.text).withMultipliedAlpha (alpha));
            AbcTrainLookAndFeel::fitText (g, p.name, top, juce::Justification::centredLeft, true);
            if (p.left)
            {
                g.setColour (theme.textDim.withMultipliedAlpha (alpha));
                g.drawHorizontalLine ((int) top.getCentreY(), top.getX(),
                                      top.getX() + juce::jmin (top.getWidth(), (float) p.name.length() * 7.5f));
            }

            // Bottom: the HP bar, and the number - big beside the bar for
            // three players, small after it for six.
            auto bottom = oneLine ? inner.withTrimmedLeft (6.0f) : inner;
            const auto numberW = big ? 46.0f : 26.0f;
            auto numberBox = bottom.removeFromRight (numberW);
            const auto bar = bottom.withSizeKeepingCentre (bottom.getWidth() - 6.0f, big ? 7.0f : 5.0f)
                                   .withX (bottom.getX());
            g.setColour (theme.displayBackground);
            g.fillRect (bar);
            const auto frac = juce::jlimit (0.0f, 1.0f, p.hp / 100.0f);
            const auto barColour = p.left ? theme.textDim
                                 : p.hp > 60.0f ? theme.positive
                                 : p.hp > 30.0f ? theme.accentWarm
                                                : theme.negative;
            g.setColour (barColour.withMultipliedAlpha (alpha));
            g.fillRect (bar.withWidth (bar.getWidth() * frac));

            g.setColour ((p.you ? theme.textBright : theme.text).withMultipliedAlpha (alpha));
            g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (AbcTrainLookAndFeel::monoFontHeight
                                                                 * AbcTrainLookAndFeel::getTextScale() * (big ? 1.5f : 1.0f)));
            AbcTrainLookAndFeel::fitText (g, juce::String (juce::roundToInt (p.hp)), numberBox,
                                          juce::Justification::centredRight, false);

            // The damage, rising out of the card as it fades.
            if (flash.amount > 0.01f)
            {
                g.setColour (theme.negative.withAlpha (flash.amount));
                g.setFont (AbcTrainLookAndFeel::monoFont().withHeight (AbcTrainLookAndFeel::monoFontHeight
                                                                     * AbcTrainLookAndFeel::getTextScale() * 1.2f));
                const auto rise = (1.0f - flash.amount) * 10.0f;
                AbcTrainLookAndFeel::fitText (g, "-" + juce::String (juce::roundToInt (flash.damage)),
                                              r.withTrimmedRight (numberW + 6.0f).withTrimmedTop (2.0f - rise).withHeight (16.0f),
                                              juce::Justification::centredRight, false);
            }
        }
    }

private:
    struct Flash { float damage = 0.0f, amount = 0.0f; };

    void timerCallback() override
    {
        auto any = false;
        for (auto& f : flashes)
        {
            f.amount = juce::jmax (0.0f, f.amount - 0.035f);
            any = any || f.amount > 0.0f;
        }
        if (! any)
            stopTimer();
        repaint();
    }

    std::vector<Player> players;
    std::vector<Flash> flashes;
};
