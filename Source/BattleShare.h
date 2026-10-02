#pragma once

#include <juce_core/juce_core.h>

// A finished battle as text somebody can paste into a chat (ADR 053).
// Knows nothing about the battle engine or Telegram: the editor fills a
// Result from what the session already has, the words come localised, and
// the UI decides whether the text goes to the clipboard or anywhere else.
// Nothing here invents a number the battle did not produce.
namespace BattleShare
{
    enum class Outcome { won, lost, draw };

    struct Result
    {
        juce::String exercise;        // "Guess the Band"
        juce::String you;             // the nick, or "Я" when not signed in
        juce::String them;            // the bot's name or the opponent's nick
        juce::String themBot;         // the bot's id ("cat") - empty for a person
        Outcome outcome = Outcome::draw;
        int hpYou = 0, hpThem = 0;
        int scoreYou = 0, scoreThem = 0;   // rounds each side landed
        juce::String rounds;          // "10 раундов", already pluralised
        juce::StringArray notes;      // "Decibelo +16 → 1516", "Место 2 из 5"
    };

    struct Words
    {
        juce::String title { "abcTrain Battle" };
        juce::String won { "Win" }, lost { "Loss" }, draw { "Draw" };
        juce::String vs { "vs" };
    };

    // The emoji for a bot's animal (a person gets none - the nick says it).
    juce::String animalOf (const juce::String& botId);

    juce::String format (const Result&, const Words&);
}
