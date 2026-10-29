#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/learning/LessonFile.h"
#include <map>
#include <optional>

/**
    A course lesson played on somebody else's plugin (ADR 068).

    A Studio lesson knows its plugin's parameter ids and units. A lesson for
    "your own plugin" knows neither: it says `@knob threshold -20` and has
    to work on bx_glue, Pro-C and kHs Compressor alike. Two things make
    that possible:

    - the knob is found by the glossary key (KnobGlossary): the parameter
      whose name means "threshold";
    - the value is found by what the plugin itself *shows*: the knob's
      travel is searched for the position at which its own text reads
      -20 dB (or 0.02 s for 20 ms, 1.2 kHz for 1200 Hz, "4.0:1" for 4).
      Plugins disagree on taper and range; they agree on what a decibel is.

    A plugin that cannot reach the value (a threshold that stops at -40)
    gets the nearest position, and the step says so in `problems`.

    Pure: no GUI, no audio, nothing kept. The page (OwnPluginsPanel) owns
    the glide and puts the player's setting back.
*/
namespace OwnLesson
{
    /** The number a plugin's text means, in the unit lessons write for
        `key` (KnobGlossary::unitOf). Empty when the text has no number
        ("Off", "Auto"). `label` is the parameter's unit label, used when
        the text itself has none. */
    std::optional<double> shownValue (const juce::String& text, const juce::String& label, const juce::String& key);

    struct Placement
    {
        float position = 0.0f;   // normalised, 0..1
        bool exact = true;       // false: the nearest the plugin can do
    };

    /** Where the knob has to go for the plugin to show `target`. Empty if
        the plugin shows no number anywhere on the knob's travel. */
    std::optional<Placement> placeAt (const juce::AudioProcessorParameter&, const juce::String& key, double target);

    /** A lesson's knobs on this plugin: glossary key -> parameter index. */
    using KnobMap = std::map<juce::String, int>;

    /** True if the plugin has every knob the lesson turns. */
    bool fits (const LessonFile::Lesson&, const KnobMap&);

    /** Where the knobs stand after step `upTo` (inclusive), counted from
        the player's own setting `mine` (parameter index -> normalised
        value when the lesson began). A lesson step says what the state is
        after it, so going back is recomputing, not undoing. */
    struct State
    {
        std::map<int, float> positions;   // only the knobs the lesson has touched
        bool dry = false;                 // @bypass on
        juce::StringArray problems;       // "Threshold: -60 dB is out of reach, nearest -40 dB"
    };

    State stateAfter (const LessonFile::Lesson&, int upTo, const KnobMap&,
                      const juce::Array<juce::AudioProcessorParameter*>&, const std::map<int, float>& mine);

    /** The positions min / low / high / max stand for. low and high are
        the same 15 % and 85 % as the Less / More buttons on the knob card. */
    inline constexpr float lowPosition = 0.15f, highPosition = 0.85f;
}
