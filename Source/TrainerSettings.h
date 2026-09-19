#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <vector>

// Every setting the trainer has that changes how it trains or how it
// looks after your ears - one list, with its default, its range and
// whether a beginner sees it.
//
// **Two modes (ADR 036).** *Beginner* is the standard trainer: every rule
// at its default, few switches. *Pro* opens everything that makes sense to
// tune. The point of the split is not to hide features but to keep the
// first weeks about listening: a rule you can change is a rule you start
// negotiating with, and someone who does not yet know what "three in a
// row" is for cannot choose a better number.
//
// Switching to Beginner does not erase anything. Pro values stay in the
// settings file and come back when Pro does; in Beginner, `get()` simply
// answers with the default. That is the whole mechanism, and it is why
// every caller goes through `get()` rather than reading the file.
//
// The ones marked `beginner = true` are shown in both modes: they are
// about the person (their ears, their eyes), not about the game.
class TrainerSettings
{
public:
    enum class Mode { beginner, pro };

    enum class Id
    {
        // Training
        stepRule,             // right answers in a row for a step harder: 2, 3, 4
        answerPause,          // 0 short, 1 normal, 2 long
        survivalLives,        // 1, 3, 5
        blitzSeconds,         // 60, 90, 120, 180
        blitzPenalty,         // 0, 5, 10 seconds
        hints,                // 0 off, 1 on
        allModesOpen,         // 0 earned by a streak, 1 open from the start

        // Hearing (ADR 036: WHO / ITU-T H.870)
        hearingOn,            // 0 off, 1 on - the master switch
        breakReminderMinutes, // 0 off, 30, 45, 60, 90
        fatigueHint,          // 0 off, 1 on
        weeklyLimit,          // 0 = 80 dB(A) for 40 h, 1 = 75 dB(A) for 40 h
        calibrationDb,        // dB(A) measured for pink noise at -20 dBFS; 0 = not calibrated

        numIds
    };

    struct Spec
    {
        const char* key;
        int defaultValue;
        std::vector<int> choices;   // the values the UI offers, in order
        bool beginner;              // shown (and honoured) in Beginner mode too
    };

    static const Spec& spec (Id id)
    {
        static const std::vector<Spec> specs {
            { "train.stepRule",        3,  { 2, 3, 4 },               false },
            { "train.answerPause",     1,  { 0, 1, 2 },               false },
            { "train.survivalLives",   3,  { 1, 3, 5 },               false },
            { "train.blitzSeconds",    90, { 60, 90, 120, 180 },      false },
            { "train.blitzPenalty",    5,  { 0, 5, 10 },              false },
            { "train.hints",           1,  { 0, 1 },                  false },
            { "train.allModesOpen",    0,  { 0, 1 },                  false },

            { "hearing.on",            1,  { 0, 1 },                  true  },
            { "hearing.breakMinutes",  60, { 0, 30, 45, 60, 90 },     false },
            { "hearing.fatigueHint",   1,  { 0, 1 },                  false },
            { "hearing.weeklyLimit",   0,  { 0, 1 },                  false },
            // Not a choice list: any whole number 60..110 is valid. The
            // list is the range the UI steps through.
            { "hearing.calibrationDb", 0,  {},                        true  },
        };

        return specs[(size_t) id];
    }

    static constexpr const char* modeKey = "settingsMode";

    explicit TrainerSettings (juce::PropertiesFile& fileToUse) : file (fileToUse) {}

    Mode getMode() const
    {
        return file.getValue (modeKey, "beginner") == "pro" ? Mode::pro : Mode::beginner;
    }

    void setMode (Mode newMode)
    {
        file.setValue (modeKey, newMode == Mode::pro ? "pro" : "beginner");
        file.saveIfNeeded();
    }

    // What the app should use right now.
    int get (Id id) const
    {
        const auto& s = spec (id);

        if (! s.beginner && getMode() == Mode::beginner)
            return s.defaultValue;

        return stored (id);
    }

    // What the file holds, whatever the mode - for the Pro page, so that
    // it shows your choices even while Beginner is overriding them.
    int stored (Id id) const
    {
        const auto& s = spec (id);
        const auto value = file.getIntValue (s.key, s.defaultValue);

        if (id == Id::calibrationDb)
            return (value == 0 || (value >= 60 && value <= 110)) ? value : 0;

        // A value the list does not offer (an old file, a hand edit) is
        // treated as absent rather than trusted.
        for (auto c : s.choices)
            if (c == value)
                return value;

        return s.defaultValue;
    }

    void set (Id id, int value)
    {
        file.setValue (spec (id).key, value);
        file.saveIfNeeded();
    }

    bool isVisible (Id id) const
    {
        return spec (id).beginner || getMode() == Mode::pro;
    }

    bool isDefault (Id id) const { return stored (id) == spec (id).defaultValue; }

    // Every value back to its default, for "reset" - the escape hatch any
    // page of switches needs. Except the calibration: that is a
    // measurement of the room, not a preference, and losing it to a
    // "reset" nobody associated with it would silently stop the dose.
    void resetAll()
    {
        for (int i = 0; i < (int) Id::numIds; ++i)
            if ((Id) i != Id::calibrationDb)
                file.removeValue (spec ((Id) i).key);

        file.saveIfNeeded();
    }

private:
    juce::PropertiesFile& file;
};
