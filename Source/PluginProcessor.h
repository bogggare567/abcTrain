#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GameManager.h"
#include "ProgressManager.h"
#include "../shared/Vectorscope.h"
#include "../shared/SpectrumAnalyzer.h"
#include <atomic>

class EarTrainerProcessor : public juce::AudioProcessor
{
public:
    EarTrainerProcessor();
    ~EarTrainerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

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

    void setVectorscope (Vectorscope* scope) noexcept { vectorscope.store (scope); }
    void setSpectrumAnalyzer (SpectrumAnalyzerComponent* analyzer) noexcept { spectrum.store (analyzer); }

private:
    // Declaration order matters: gameManager must be constructed before
    // progressManager, since ProgressManager's constructor registers
    // itself as a listener on every game.
    GameManager gameManager;
    ProgressManager progressManager { gameManager };

    std::atomic<bool> signalEnabled { true };
    std::atomic<Vectorscope*> vectorscope { nullptr };
    std::atomic<SpectrumAnalyzerComponent*> spectrum { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EarTrainerProcessor)
};
