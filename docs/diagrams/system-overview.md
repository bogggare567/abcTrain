# System overview

What's actually built (`EarTrainer`, `LearnerEQ`, `LearnerComp`, `shared/`,
`EarTrainerTests`) plus where the next teaching plugins/games would plug
in. Dashed boxes are **not built yet** — see
[../roadmap.md](../roadmap.md) for status.

```mermaid
flowchart TB
    subgraph EarTrainer["EarTrainer plugin (VST3 / AU / Standalone)"]
        ETProc["PluginProcessor"]
        ETEdit["PluginEditor (generic)"]
        GM["GameManager"]
        PM["ProgressManager"]
        GameIface["Game interface"]
        EQGame["EQGame"]
        CompGame["CompressionGame"]
        RevGame["ReverbGame"]
        Pink["PinkNoiseGenerator"]

        ETProc --> GM
        ETProc --> PM
        ETEdit --> GM
        ETEdit --> PM
        PM --> GM
        GM --> GameIface
        GameIface --> EQGame
        GameIface --> CompGame
        GameIface --> RevGame
        EQGame --> Pink
        CompGame --> Pink
        RevGame --> Pink
    end

    subgraph LearnerEQ["LearnerEQ plugin (VST3 / AU / Standalone)"]
        LEProc["PluginProcessor"]
        LEEdit["PluginEditor"]
        Spectrum["SpectrumAnalyserComponent"]
        Coeffs["EQCoefficients"]
        FreqGuide["FrequencyGuide"]
        APVTS[("AudioProcessorValueTreeState")]

        LEProc --> APVTS
        LEProc --> Coeffs
        LEEdit --> APVTS
        LEEdit --> Spectrum
        LEEdit --> FreqGuide
        Spectrum --> Coeffs
        Spectrum --> FreqGuide
    end

    subgraph LearnerComp["LearnerComp plugin (VST3 / AU / Standalone)"]
        LCProc["PluginProcessor"]
        LCEdit["PluginEditor"]
        Engine["CompressorEngine"]
        Waveform["WaveformDisplay"]
        Guide["CompressorGuide"]

        LCProc --> Engine
        LCEdit --> Waveform
        LCEdit --> Guide
    end

    subgraph Tests["EarTrainerTests (console app)"]
        Runner["TestRunner (juce::UnitTestRunner)"]
        TestUtils["shared/TestUtils.h"]
    end

    LearnerVerb["LearnerVerb plugin"]

    Runner -. "compiles & runs directly, no host/GUI" .-> EQGame
    Runner -. "compiles & runs directly" .-> CompGame
    Runner -. "compiles & runs directly" .-> RevGame
    Runner -. "compiles & runs directly" .-> GM
    Runner -. "compiles & runs directly" .-> LEProc
    Runner -. "compiles & runs directly" .-> LCProc
    Runner --> TestUtils

    LCProc -.-> LearnerVerb

    classDef planned stroke-dasharray:4 3,opacity:0.55;
    class LearnerVerb planned;
```

**Key structural fact:** `EarTrainerTests` links the game/processor `.cpp`
files directly rather than depending on the plugin targets. It excludes
`Source/PluginProcessor.cpp` (EarTrainer's) entirely, and excludes every
plugin's `PluginEntry.cpp` (the file containing just `createPluginFilter()`)
— LearnerEQ's and LearnerComp's `PluginProcessor.cpp` are both linked in
(each `*Test.cpp` needs the real processor), but their `createPluginFilter()`
factory functions live in separate files specifically so linking both
processors into one test binary doesn't collide two definitions of that
function.
