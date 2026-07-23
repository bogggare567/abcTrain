# Learner-series teaching plugins

`LearnerEQ` is the first (and so far only) plugin in this pattern —
unlike EarTrainer's games, it processes the host's real audio and is a
genuinely usable, host-automatable effect. Any future teaching plugin
(`LearnerComp`, `LearnerVerb`, ...) would follow the same shape: its own
`juce_add_plugin` target, `AudioProcessorValueTreeState` for parameters, a
visualization component, and a short contextual guide string per control.

```mermaid
flowchart LR
    subgraph Processor
        Proc["PluginProcessor"]
        APVTS[("APVTS: 4 bands x freq/gain/Q")]
        Filters["4x ProcessorDuplicator&lt;IIR::Filter&gt;"]
        Proc --> APVTS
        Proc --> Filters
    end

    subgraph Editor
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

    HintsComponent["HintsComponent (planned,\nshared across Learner plugins)"]
    KnowledgeBrowser["Knowledge base browser (planned)"]

    classDef planned stroke-dasharray:4 3,opacity:0.55;
    class HintsComponent,KnowledgeBrowser planned;
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
