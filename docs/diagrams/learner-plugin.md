# Learner-series teaching plugins

Two plugins in this pattern so far: `LearnerEQ` and `LearnerComp`. Both
process the host's real audio and are genuinely usable, host-automatable
effects (unlike EarTrainer's games). Both follow the same shape: their own
`juce_add_plugin` target, `AudioProcessorValueTreeState` for parameters, a
visualization component fed from the audio thread, and a short contextual
guide string per control. **They don't share code with each other yet**
(see the note at the bottom) — `SpectrumAnalyserComponent` and
`WaveformDisplay` are separate, parallel implementations of the same
FIFO-fill/timer-repaint pattern.

## LearnerEQ

```mermaid
flowchart LR
    subgraph Processor["LearnerEQ Processor"]
        Proc["PluginProcessor"]
        APVTS[("APVTS: 4 bands x freq/gain/Q")]
        Filters["4x ProcessorDuplicator&lt;IIR::Filter&gt;"]
        Proc --> APVTS
        Proc --> Filters
    end

    subgraph Editor["LearnerEQ Editor"]
        Edit["PluginEditor"]
        Sliders["4 columns of freq/gain/Q rotary sliders"]
        Spectrum["SpectrumAnalyserComponent"]
        Guide["guideLabel (contextual tooltip)"]
        Edit --> Sliders
        Edit --> Spectrum
        Edit --> Guide
    end

    Coeffs["EQCoefficients\n(band index -> filter type + coefficients)"]
    FreqGuide["FrequencyGuide\n(log-freq to x-position mapping + descriptions)"]

    Sliders -- "SliderAttachment" --> APVTS
    Filters -- "recomputed from APVTS\nonce per block" --> Coeffs
    Proc -- "pushNextSampleIntoFifo\n(audio thread)" --> Spectrum
    Spectrum -- "response curve" --> Coeffs
    Spectrum -- "x-axis mapping" --> FreqGuide
    Guide -- "describe(freq)" --> FreqGuide
```

**Why `EQCoefficients` and `FrequencyGuide` are separate headers, shared by
both processor and editor:** the processor uses `EQCoefficients::make` for
real-time filtering; the editor uses the exact same function purely to
draw the response curve. If they used different logic to decide "what does
band N sound like," the displayed curve could silently disagree with the
actual audio — sharing one source of truth rules that out. Same reasoning
for `FrequencyGuide`: the live spectrum, the response curve, and the
highlighted-band overlay all convert frequency to x-position through the
same function, so they're guaranteed to line up on screen.

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
        Knobs["7 rotary knobs + bypass toggle"]
        Waveform["WaveformDisplay"]
        Presets["4 preset buttons"]
        Guide2["guideLabel (contextual tooltip)"]
        Edit --> Knobs
        Edit --> Waveform
        Edit --> Presets
        Edit --> Guide2
    end

    ParamGuide["CompressorGuide\n(tooltip text + preset table)"]

    Knobs -- "SliderAttachment" --> APVTS
    Proc -- "computeGain(detection)\nstereo-linked, audio thread" --> Engine
    Proc -- "pushSample(in, out, gainReductionDb)\naudio thread" --> Waveform
    Presets -- "processor.applyPreset(i)" --> ParamGuide
    Guide2 -- "describe(paramId)" --> ParamGuide
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

## Shared-code gap

`SpectrumAnalyserComponent` (LearnerEQ) and `WaveformDisplay` (LearnerComp)
are both "accumulate from the audio thread into a fixed-size buffer, flush
on a UI timer, repaint" — structurally the same pattern, implemented twice.
Nothing has been extracted into a shared component yet. Worth doing once a
third Learner plugin needs the same shape, rather than guessing the right
abstraction from two data points.
