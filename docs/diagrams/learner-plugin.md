# Learner-series teaching plugins

Three plugins in this pattern so far: `LearnerEQ`, `LearnerComp`, and
`LearnerVerb`. All process the host's real audio and are genuinely
usable, host-automatable effects (unlike EarTrainer's games). All follow
the same shape: their own `juce_add_plugin` target, `AudioProcessorValueTreeState`
for parameters, a visualization component fed from the audio thread, and
a short contextual guide string per control.

## LearnerEQ

```mermaid
flowchart LR
    subgraph Processor["LearnerEQ Processor"]
        Proc["PluginProcessor"]
        APVTS[("APVTS: 4 bands x freq/gain/Q\n+ bypass")]
        Filters["4x ProcessorDuplicator&lt;IIR::Filter&gt;"]
        Proc --> APVTS
        Proc --> Filters
    end

    subgraph Editor["LearnerEQ Editor"]
        Edit["PluginEditor"]
        Sliders["4 columns of freq/gain/Q rotary sliders"]
        Spectrum["SpectrumAnalyserComponent\n(extends shared/SpectrumAnalyzer)"]
        Waveform["shared/WaveformDisplay"]
        Guide["guideLabel (contextual tooltip)"]
        BypassBtn["Bypass toggle"]
        LessonBtn["Lesson button"]
        Lesson["shared/LessonController\n(Vocal EQ Basics, VocalEqLesson.h)"]
        Edit --> Sliders
        Edit --> Spectrum
        Edit --> Waveform
        Edit --> Guide
        Edit --> BypassBtn
        Edit --> LessonBtn
        LessonBtn -- "showAndStart()" --> Lesson
    end

    Coeffs["EQCoefficients\n(band index -> filter type + coefficients)"]
    FreqGuide["FrequencyGuide\n(log-freq to x-position mapping + descriptions)"]

    Sliders -- "SliderAttachment" --> APVTS
    BypassBtn -- "ButtonAttachment" --> APVTS
    Filters -- "recomputed from APVTS\nonce per block, skipped when bypassed" --> Coeffs
    Proc -- "pushNextSampleIntoFifo(output)\naudio thread" --> Spectrum
    Proc -- "pushSample(dry, wet)\naudio thread" --> Waveform
    Spectrum -- "response curve, paintOverlay()" --> Coeffs
    Spectrum -- "x-axis mapping" --> FreqGuide
    Guide -- "describe(freq)" --> FreqGuide
    Lesson -- "setValueNotifyingHost\nper step" --> APVTS
```

**Why `EQCoefficients` and `FrequencyGuide` are separate headers, shared by
both processor and editor:** the processor uses `EQCoefficients::make` for
real-time filtering; the editor uses the exact same function purely to
draw the response curve. If they used different logic to decide "what does
band N sound like," the displayed curve could silently disagree with the
actual audio — sharing one source of truth rules that out. Same reasoning
for `FrequencyGuide`: the response curve and the highlighted-band overlay
both convert frequency to x-position through the same function, so they're
guaranteed to line up on screen with each other and with the spectrum drawn
underneath.

`SpectrumAnalyserComponent` now extends `shared/SpectrumAnalyzerComponent`
(see [decisions/006](../decisions/006-unified-visualization.md)) — the
FFT/FIFO/timer machinery is shared with LearnerComp and LearnerVerb, and
this subclass only adds the response-curve + highlighted-band overlay via
`paintOverlay()`. Its spectrum is fed the *output* (post-filter) signal,
unchanged from before — the point is seeing what the EQ curve did.
`WaveformDisplay` is new here too, showing input vs. output with no
highlight tint (LearnerEQ has no per-sample "how hard is this working"
concept the way gain reduction gives LearnerComp one).

## LearnerComp

```mermaid
flowchart LR
    subgraph Processor["LearnerComp Processor"]
        Proc["PluginProcessor"]
        APVTS[("APVTS: threshold/ratio/attack/\nrelease/knee/makeup/dryWet/bypass")]
        Engine["CompressorEngine\n(custom soft-knee gain computer,\nsee ADR 003)"]
        Proc --> APVTS
        Proc --> Engine
    end

    subgraph Editor["LearnerComp Editor"]
        Edit["PluginEditor"]
        Knobs["7 rotary knobs"]
        Spectrum2["SpectrumAnalyzerComponent\n(shared/SpectrumAnalyzer, no overlay)"]
        Waveform["shared/WaveformDisplay"]
        Presets["4 preset buttons"]
        Guide2["guideLabel (contextual tooltip)"]
        BypassBtn2["Bypass toggle"]
        LessonBtn2["Lesson button"]
        Lesson2["shared/LessonController\n(Vocal Compression, VocalCompressionLesson.h)"]
        Edit --> Knobs
        Edit --> Spectrum2
        Edit --> Waveform
        Edit --> Presets
        Edit --> Guide2
        Edit --> BypassBtn2
        Edit --> LessonBtn2
        LessonBtn2 -- "showAndStart()" --> Lesson2
    end

    ParamGuide["CompressorGuide\n(tooltip text + preset table)"]

    Knobs -- "SliderAttachment" --> APVTS
    BypassBtn2 -- "ButtonAttachment" --> APVTS
    Proc -- "computeGain(detection)\nstereo-linked, audio thread" --> Engine
    Proc -- "pushSample(in, out, gainReductionDb)\naudio thread" --> Waveform
    Proc -- "pushNextSampleIntoFifo(mono input)\naudio thread" --> Spectrum2
    Presets -- "processor.applyPreset(i)" --> ParamGuide
    Guide2 -- "describe(paramId)" --> ParamGuide
    Lesson2 -- "setValueNotifyingHost\nper step" --> APVTS
```

**Why detection and gain application are separate steps in
`CompressorEngine`:** `computeGain(detectionSample)` takes one value (the
loudest of the input channels at that sample) and returns a gain factor;
`PluginProcessor` applies that *same* gain to every channel. This is
stereo-linked detection — it's what keeps compression from pumping the
stereo image the way independent per-channel compression can.

**Why `applyPreset` lives on the processor, not just as an editor button
handler:** it's directly unit-testable that way (`LearnerCompTest.cpp`
calls it without needing to construct an editor/GUI at all) — see
[testing-strategy.md](../testing-strategy.md) for why constructing a
`Component` in the console test binary was avoided rather than relied on.
Same reasoning applies to `LearnerVerbProcessor::applyPreset` below.

## LearnerVerb

```mermaid
flowchart LR
    subgraph Processor["LearnerVerb Processor"]
        Proc["PluginProcessor"]
        APVTS[("APVTS: type/decay/preDelay/\nsize/damping/dryWet/width/bypass")]
        Engine["ReverbEngine\n(Room/Hall/Plate via dsp::Reverb,\nSpring via allpass cascade, see ADR 004)"]
        Proc --> APVTS
        Proc --> Engine
    end

    subgraph Editor["LearnerVerb Editor"]
        Edit["PluginEditor"]
        TypeBox["Type ComboBox + 6 rotary knobs"]
        Spectrum3["SpectrumAnalyzerComponent\n(shared/SpectrumAnalyzer, no overlay)"]
        Waveform2["shared/WaveformDisplay"]
        Presets2["4 preset buttons"]
        Guide3["guideLabel (contextual tooltip)"]
        BypassBtn3["Bypass toggle"]
        LessonBtn3["Lesson button"]
        Lesson3["shared/LessonController\n(Space for Vocals, VocalSpaceLesson.h)"]
        Edit --> TypeBox
        Edit --> Spectrum3
        Edit --> Waveform2
        Edit --> Presets2
        Edit --> Guide3
        Edit --> BypassBtn3
        Edit --> LessonBtn3
        LessonBtn3 -- "showAndStart()" --> Lesson3
    end

    ParamGuide2["ReverbGuide\n(tooltip text + preset table)"]

    TypeBox -- "ComboBoxAttachment /\nSliderAttachment" --> APVTS
    BypassBtn3 -- "ButtonAttachment" --> APVTS
    Proc -- "engine.process()\non a wet-only copy of the block,\nalways runs even when bypassed" --> Engine
    Proc -- "pushSample(dry, blended)\naudio thread" --> Waveform2
    Proc -- "pushNextSampleIntoFifo(mono input)\naudio thread" --> Spectrum3
    Presets2 -- "processor.applyPreset(i)" --> ParamGuide2
    Guide3 -- "describe(paramId)" --> ParamGuide2
    Lesson3 -- "setValueNotifyingHost\nper step" --> APVTS
```

**Why bypass forces the wet mix to 0% instead of writing `dryWet = 0`:**
un-bypassing needs to restore whatever `Dry/Wet` the user had dialled in,
not silently zero it out. `ReverbEngine` keeps running every block
regardless of bypass so its internal tail state stays warm — no
cold-start click when bypass is toggled off mid-tail (see
[decisions/006](../decisions/006-unified-visualization.md)).

**Why `PluginProcessor` makes a wet-only copy of the block instead of
processing in place:** `ReverbEngine::process` always renders 100% wet,
same division of responsibility as `CompressorEngine` — the processor
blends dry/wet itself afterward, sample by sample, so `ReverbEngine` never
needs to know about the `Dry/Wet` parameter at all.

**Why `WaveformDisplay`'s highlight channel goes unused here:**
LearnerComp tints the output trace red by gain reduction; LearnerVerb has
no equivalent per-sample "how hard is this working" value, so it just
passes the default (no tint). The shared component tolerates a consumer
that doesn't need the highlight feature.

## Shared code

`shared/WaveformDisplay.{h,cpp}` — the scrolling peak-based dual-waveform
component — is now used by all three Learner plugins. It started as
LearnerComp-only; once LearnerVerb needed the identical FIFO-accumulate/
30 Hz-timer/scrolling-columns shape, it was extracted rather than copied a
second time (see [decisions/004](../decisions/004-learnerverb-scope.md)),
and LearnerEQ picked it up later once all three plugins' visualizations
were unified (see [decisions/006](../decisions/006-unified-visualization.md)).

`shared/SpectrumAnalyzer.{h,cpp}` — the FIFO-accumulate/FFT/30 Hz-timer
live spectrum, extracted from what used to be LearnerEQ's standalone
`SpectrumAnalyserComponent` once LearnerComp and LearnerVerb both wanted a
plain live spectrum too (see [decisions/006](../decisions/006-unified-visualization.md)).
`SpectrumAnalyzerComponent` (the shared base) knows nothing about EQ bands
or highlighting; LearnerEQ's `SpectrumAnalyserComponent` subclasses it and
adds the response-curve + highlighted-band overlay via `paintOverlay()`.
Note the spelling: the shared base and the two new processor-side
`setSpectrumAnalyzer` methods use "Analyzer" (as specified for this task),
while LearnerEQ's own already-shipped `SpectrumAnalyserComponent`/
`setSpectrumAnalyser` keep their established British spelling rather than
being renamed for cosmetic consistency — both appear side by side in
`LearnerEQ/Source/SpectrumAnalyser.h` intentionally.

`shared/MicroLesson.h` + `shared/LessonController.{h,cpp}` — the guided-
lesson machinery used by all three editors above, one lesson each. See
[decisions/005](../decisions/005-microlesson-architecture.md).
`MicroLesson` is pure step-sequence data/state with no APVTS or UI
dependency; `LessonController` is the `Component` that owns one, applies
each step's target parameters via `setValueNotifyingHost` (same idiom as
`applyPreset` above), and is added as a full-size hidden child toggled by
each editor's "Lesson" button. The lesson *content* itself
(`VocalEqLesson.h`, `VocalCompressionLesson.h`, `VocalSpaceLesson.h`)
stays per-plugin, not in `shared/`, since it references that plugin's own
parameter IDs — only the machinery above is shared.
