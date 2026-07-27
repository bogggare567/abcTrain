#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "TrainingModule.h"

// The sounds a training module teaches over, synthesized from nothing.
//
// **Why generated rather than sampled.** A fixed set of files is a fixed
// set of answers: play the same kick twenty times and a player stops
// hearing a kick and starts recognising a recording. This project already
// learned that lesson once, when EQGame's eight fixed octave centres
// turned out to be memorisable as positions rather than as frequencies.
// A generator has no such ceiling - a kick here is a starting pitch, a
// pitch decay, an amplitude decay, a click amount and a drive, each drawn
// from the span that still reads as "kick", so every round is a different
// kick and none of them can be learned as a picture. It also weighs
// nothing on disk and needs nobody's permission.
//
// **What it honestly covers.** Families: kick against snare against hat
// against tom against clap; sine against saw against square against noise.
// That is a real skill and it is the one a mixing ear needs first. It does
// not cover an 808 against a Linn against a kit recorded in a room - that
// is the character of a *recording*, and no oscillator produces it. For
// acoustic instruments the honest route is the player's own imported
// library, not a synthesizer pretending.
//
// **Half the value of a module is which bed it uses.** Attack is inaudible
// on a sustained pad, pre-delay disappears inside a busy loop, damping
// needs something bright enough to have a top end to lose. Choosing the
// material the knob is audible on *is* the teaching.
//
// Message thread only: this allocates and fills buffers. Nothing here is
// real-time safe, and nothing here needs to be - a bed is rendered once
// when a module opens and then looped.
namespace LessonAudioBed
{
    // Renders `bed` as a seamless stereo loop at `sampleRate`. The
    // variation seed picks one instance out of the family: same seed, same
    // sound, which is what lets a check be repeated and a test be written.
    juce::AudioBuffer<float> render (TrainingModule::Bed bed, double sampleRate,
                                     int variationSeed);

    // How long each bed's loop is, in seconds. Beds meant for hearing a
    // tail (singleHit, brightHit) leave real silence after the hit, because
    // the silence is where the tail lives.
    double lengthSeconds (TrainingModule::Bed) noexcept;
}
