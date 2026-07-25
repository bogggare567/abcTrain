# System overview

What's actually built (`EarTrainer`, `LearnerEQ`, `LearnerComp`,
`LearnerVerb`, `shared/`, `EarTrainerTests`) plus where the next teaching
plugins/games would plug in. Dashed boxes are **not built yet** — see
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
        PanGame["PanGame"]
        DelayGame["DelayGame"]
        DistGame["DistortionGame"]
        WidthGame["StereoWidthGame"]
        DBGame["DBGame"]
        FreqRangeGame["FrequencyRangeGame"]
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
        GameIface --> PanGame
        GameIface --> DelayGame
        GameIface --> DistGame
        GameIface --> WidthGame
        GameIface --> DBGame
        GameIface --> FreqRangeGame
        EQGame --> Pink
        CompGame --> Pink
        RevGame --> Pink
        PanGame --> Pink
        DelayGame --> Pink
        DistGame --> Pink
        WidthGame --> Pink
        DBGame --> Pink
        FreqRangeGame --> Pink
    end

    subgraph LearnerEQ["LearnerEQ plugin (VST3 / AU / Standalone)"]
        LEProc["PluginProcessor"]
        LEEdit["PluginEditor"]
        Spectrum["SpectrumAnalyserComponent\n(extends shared SpectrumAnalyzer)"]
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
        Guide["CompressorGuide"]

        LCProc --> Engine
        LCEdit --> Guide
    end

    subgraph LearnerVerb["LearnerVerb plugin (VST3 / AU / Standalone)"]
        LVProc["PluginProcessor"]
        LVEdit["PluginEditor"]
        RevEngine["ReverbEngine"]
        RevGuide["ReverbGuide"]

        LVProc --> RevEngine
        LVEdit --> RevGuide
    end

    subgraph Shared["shared/"]
        Waveform["WaveformDisplay"]
        SpectrumBase["SpectrumAnalyzerComponent\n(FFT/FIFO base)"]
        MicroLesson["MicroLesson\n(pure state machine)"]
        LessonController["LessonController\n(APVTS + UI)"]
        UpdateChecker["UpdateChecker\n(pure logic + async GitHub check,\nstable/beta channel-aware)"]
        LookAndFeel["AbcTrainLookAndFeel\n(shared dark theme)"]
        LocalisationManager["LocalisationManager\n(12 languages, core UI strings)"]
        VersionInfo["VersionChannel / Version.h\n(git describe-derived)"]
        TestUtils["TestUtils.h"]
        LessonController --> MicroLesson
    end

    subgraph Tests["EarTrainerTests (console app)"]
        Runner["TestRunner (juce::UnitTestRunner)"]
    end

    LearnerSat["LearnerSat plugin"]

    LEEdit --> Waveform
    LCEdit --> Waveform
    LVEdit --> Waveform
    Spectrum --> SpectrumBase
    LCEdit --> SpectrumBase
    LVEdit --> SpectrumBase
    LEEdit --> LessonController
    LCEdit --> LessonController
    LVEdit --> LessonController
    ETEdit --> UpdateChecker
    LEEdit --> UpdateChecker
    LCEdit --> UpdateChecker
    LVEdit --> UpdateChecker
    ETEdit --> LookAndFeel
    LEEdit --> LookAndFeel
    LCEdit --> LookAndFeel
    LVEdit --> LookAndFeel
    ETEdit --> LocalisationManager
    ETEdit --> VersionInfo
    LEEdit --> VersionInfo
    LCEdit --> VersionInfo
    LVEdit --> VersionInfo

    Runner -. "compiles & runs directly, no host/GUI" .-> EQGame
    Runner -. "compiles & runs directly" .-> CompGame
    Runner -. "compiles & runs directly" .-> RevGame
    Runner -. "compiles & runs directly" .-> PanGame
    Runner -. "compiles & runs directly" .-> DelayGame
    Runner -. "compiles & runs directly" .-> DistGame
    Runner -. "compiles & runs directly" .-> WidthGame
    Runner -. "compiles & runs directly" .-> DBGame
    Runner -. "compiles & runs directly" .-> FreqRangeGame
    Runner -. "compiles & runs directly" .-> GM
    Runner -. "compiles & runs directly" .-> LEProc
    Runner -. "compiles & runs directly" .-> LCProc
    Runner -. "compiles & runs directly" .-> LVProc
    Runner -. "compiles & runs directly" .-> MicroLesson
    Runner -. "compiles & runs directly\n(pure functions only)" .-> UpdateChecker
    Runner -. "compiles & runs directly" .-> LocalisationManager
    Runner -. "compiles & runs directly\n(pure function only)" .-> VersionInfo
    Runner --> TestUtils

    LVProc -.-> LearnerSat

    classDef planned stroke-dasharray:4 3,opacity:0.55;
    class LearnerSat planned;
```

**Key structural fact:** `EarTrainerTests` links the game/processor `.cpp`
files directly rather than depending on the plugin targets. It excludes
`Source/PluginProcessor.cpp` (EarTrainer's) entirely, and excludes every
plugin's `PluginEntry.cpp` (the file containing just `createPluginFilter()`)
— LearnerEQ's, LearnerComp's, and LearnerVerb's `PluginProcessor.cpp` are
all linked in (each `*Test.cpp` needs the real processor), but their
`createPluginFilter()` factory functions live in separate files
specifically so linking three processors into one test binary doesn't
collide three definitions of that function.
