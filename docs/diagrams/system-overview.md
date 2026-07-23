# System overview

What's actually built (`EarTrainer`, `LearnerEQ`, `shared/`, `EarTrainerTests`)
plus where the next teaching plugins/games would plug in. Dashed boxes are
**not built yet** — see [../roadmap.md](../roadmap.md) for status.

```mermaid
flowchart TB
    subgraph EarTrainer["EarTrainer plugin (VST3 / AU / Standalone)"]
        ETProc["PluginProcessor"]
        ETEdit["PluginEditor (generic)"]
        GM["GameManager"]
        GameIface["Game interface"]
        EQGame["EQGame"]
        CompGame["CompressionGame"]
        Pink["PinkNoiseGenerator"]

        ETProc --> GM
        ETEdit --> GM
        GM --> GameIface
        GameIface --> EQGame
        GameIface --> CompGame
        EQGame --> Pink
        CompGame --> Pink
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

    subgraph Tests["EarTrainerTests (console app)"]
        Runner["TestRunner (juce::UnitTestRunner)"]
        TestUtils["shared/TestUtils.h"]
    end

    ReverbGame["ReverbGame"]
    LearnerComp["LearnerComp plugin"]
    LearnerVerb["LearnerVerb plugin"]

    Runner -. "compiles & runs directly, no host/GUI" .-> EQGame
    Runner -. "compiles & runs directly" .-> CompGame
    Runner -. "compiles & runs directly" .-> GM
    Runner -. "compiles & runs directly" .-> LEProc
    Runner --> TestUtils

    GameIface -.-> ReverbGame
    LEProc -.-> LearnerComp
    LEProc -.-> LearnerVerb

    classDef planned stroke-dasharray:4 3,opacity:0.55;
    class ReverbGame,LearnerComp,LearnerVerb planned;
```

**Key structural fact:** `EarTrainerTests` links the game/processor `.cpp`
files directly rather than depending on the plugin targets. It deliberately
excludes `Source/PluginProcessor.cpp` (EarTrainer's) because both it and
`LearnerEQ/Source/PluginProcessor.cpp` define `createPluginFilter()` — linking
both into one binary would collide.
