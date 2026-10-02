#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/learning/LessonFile.h"
#include <vector>

// Turns a lesson step's commands into knob moves on the Studio plugin the
// lesson is about (ADR 052).
//
// **The knobs travel.** A float parameter glides from where it is to where
// the step wants it over glideMs, eased, through the host-notifying path,
// so the plugin's own knob is seen turning - "the threshold goes down to
// -24" is watched, not read. A choice or a switch (band on, filter type,
// Plate, Send) has nothing between its values and changes at the start.
//
// Message thread only. Parameters are set through the APVTS like a person
// turning them, so the audio thread sees the ordinary smoothed change.
class LessonRunner : private juce::Timer
{
public:
    static constexpr int glideMs = 700;

    // (parameter id, value in the knob's own units: Hz, dB, ms, %, index)
    using Targets = std::vector<std::pair<juce::String, float>>;

    // What a step sets, for `plugin` ("eq" / "comp" / "verb"). Commands
    // that are not knobs (@material, @highlight) are left to the caller.
    static Targets targetsFor (const LessonFile::Step&, const juce::String& plugin);

    // Starts gliding `apvts` to the step's targets; a running glide is
    // finished at once first, so steps clicked quickly never pile up.
    void apply (juce::AudioProcessorValueTreeState&, const Targets&);

    // Snaps any running glide to its end.
    void finish();

    bool isGliding() const noexcept { return ! glides.empty(); }

    ~LessonRunner() override;

private:
    struct Glide
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float from = 0.0f, to = 0.0f;    // normalised
    };

    void timerCallback() override;
    void endGestures();

    std::vector<Glide> glides;
    double startMs = 0.0;
};
