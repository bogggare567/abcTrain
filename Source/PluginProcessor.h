#pragma once

#include "shared/audio/ClipPreview.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include "GameManager.h"
#include "ProgressManager.h"
#include "shared/analysis/Vectorscope.h"
#include "shared/analysis/SpectrumAnalyzer.h"
#include "shared/analysis/WaveformDisplay.h"
#include "shared/analysis/AWeightedMeter.h"
#include "shared/audio/PinkNoiseGenerator.h"
#include <array>
#include <atomic>
#include <memory>

class EarTrainerProcessor : public juce::AudioProcessor
{
public:
    EarTrainerProcessor();
    ~EarTrainerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // A literal rather than JucePlugin_Name, matching all three Learner
    // processors: the JucePlugin_* macros only exist inside a
    // juce_add_plugin target, and this file is also compiled into the
    // EditorSnapshots console app. Same reason recorded in
    // docs/diagrams/ci-pipeline.md, bug 1.
    const juce::String getName() const override { return "ABC Ear Trainer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // The trainer keeps its own progress in a PropertiesFile; what the
    // app's wrapper saves here is only the Studio's three knob states, so a
    // session's EQ is still there next time the app opens.
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    GameManager& getGameManager() noexcept { return gameManager; }
    ProgressManager& getProgressManager() noexcept { return progressManager; }

    // Scope feeds for the editor's hint panel. Raw atomics, null-checked
    // on the audio thread, because the editor can be closed while the
    // processor keeps running - the same registration pattern
    // LearnerEQ/Comp/Verb already use for their own displays.
    // Silence on the menu. The trainer generates its own signal, so
    // without this it keeps playing over the home screen where there's
    // nothing to listen for. Defaults to on, and the editor restores it
    // on teardown, so a plugin with no UI open still makes sound.
    void setSignalEnabled (bool shouldBeEnabled) noexcept { signalEnabled.store (shouldBeEnabled); }
    bool isSignalEnabled() const noexcept { return signalEnabled.load(); }


    void setVectorscope (Vectorscope* scope) noexcept { vectorscope.store (scope); }
    void setSpectrumAnalyzer (SpectrumAnalyzerComponent* analyzer) noexcept { spectrum.store (analyzer); }

    // Level against time - the third hint view. Compression is invisible
    // on the other two: a spectrum shows where the energy is, not whether
    // the loud part got held back. See Game::getHintView.
    void setWaveformDisplay (WaveformDisplay* display) noexcept { waveform.store (display); }

    // Output level, in dB, applied to the very last thing that leaves this
    // processor.
    //
    // Where it is applied is the whole design. Every exercise levels its
    // treated signal against its own untreated one offline, so that "which
    // is louder" cannot answer "which is compressed" - and a volume
    // control applied inside a game, or to one side of the A/B, would put
    // that tell straight back. This sits after all of it, moves both sides
    // by the same number, and so cannot change any answer.
    static constexpr float minOutputGainDb = -40.0f;   // below this, silent
    static constexpr float maxOutputGainDb = 6.0f;

    void setOutputGainDb (float db) noexcept
    {
        outputGainDb.store (juce::jlimit (minOutputGainDb, maxOutputGainDb, db));
    }

    float getOutputGainDb() const noexcept { return outputGainDb.load(); }

    // ---- hearing (ADR 036) ----
    //
    // What left the processor, A-weighted, since the editor last asked.
    AWeightedMeter::Reading drainOutputLevel() noexcept { return outputMeter.drain(); }

    // Pink noise at exactly -20 dBFS RMS in each channel, replacing
    // everything else while on - the one sound the player measures with a
    // meter to tie dBFS to dB SPL. Plays even on menus: it is only ever
    // switched on from the calibration row in Settings.
    void setCalibrationNoise (bool shouldPlay) noexcept { calibrationNoise.store (shouldPlay); }
    bool isPlayingCalibrationNoise() const noexcept { return calibrationNoise.load(); }

    // The calibration noise as AWeightedMeter reads it - what a reading of
    // "calibrationDb" corresponds to.
    double getCalibrationReferenceMeanSquare() const noexcept { return calibrationReference; }

    static constexpr float calibrationLevelDbFs = -20.0f;

    // ---- Studio (ADR 041) ----
    //
    // Learner EQ, Comp and Verb, run inside the app. While one is active it
    // replaces the trainer's signal entirely: its input is the audio
    // interface's input and its own practice source, exactly as when it
    // runs as a plugin, and only the output level and the hearing meter
    // are applied after it. -1 is none.
    static constexpr int numStudioEffects = 3;
    void setStudioEffect (int index) noexcept { studioEffect.store (juce::jlimit (-1, numStudioEffects - 1, index)); }
    int getStudioEffect() const noexcept { return studioEffect.load(); }
    juce::AudioProcessor& getStudioProcessor (int index) { return *studio[(size_t) juce::jlimit (0, numStudioEffects - 1, index)]; }

    // The Training Sounds page's "click to hear". Sounds only while the
    // trainer's own signal is off (menus), so it can never mix into a round.
    ClipPreview& getClipPreview() noexcept { return clipPreview; }

private:
    ClipPreview clipPreview;

    void applyOutputGain (juce::AudioBuffer<float>&) noexcept;

    std::array<std::unique_ptr<juce::AudioProcessor>, numStudioEffects> studio;
    std::atomic<int> studioEffect { -1 };

    // Declaration order matters: gameManager must be constructed before
    // progressManager, since ProgressManager's constructor registers
    // itself as a listener on every game.
    GameManager gameManager;
    ProgressManager progressManager { gameManager };

    // Starts *off*, and only the training screen turns it on.
    //
    // It used to default to true, which meant the processor generated its
    // test signal from the moment it was constructed - before any editor
    // existed to tell it otherwise. On the standalone app that is an
    // audible half-second of noise on launch, straight into a menu that is
    // supposed to be silent. Reported by the user; the wrong default was
    // invisible in every test, because nothing tests "what does it sound
    // like before you have done anything".
    std::atomic<bool> signalEnabled { false };

    std::atomic<float> outputGainDb { 0.0f };

    // Ramped per sample, not applied per block. Nothing else in this
    // codebase smooths a gain, which is why the A/B button clicks - and a
    // volume slider is dragged continuously, so an unsmoothed one would
    // be the loudest zipper in the product.
    juce::LinearSmoothedValue<float> outputGain { 1.0f };

    AWeightedMeter outputMeter;
    std::atomic<bool> calibrationNoise { false };
    PinkNoiseGenerator calibrationGenerator { 0x5eed };
    float calibrationGain = 1.0f;          // scales the generator to -20 dBFS RMS
    double calibrationReference = 0.0;
    void measureCalibrationNoise (double sampleRate);

    std::atomic<Vectorscope*> vectorscope { nullptr };
    std::atomic<SpectrumAnalyzerComponent*> spectrum { nullptr };
    std::atomic<WaveformDisplay*> waveform { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerProcessor)
};
