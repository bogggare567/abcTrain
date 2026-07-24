#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ReverbEngine.h"
#include <atomic>

class WaveformDisplay;

// A real reverb that processes the host's audio, like LearnerEQ/
// LearnerComp but for space instead of frequency/dynamics. See
// ReverbEngine.h for the Room/Hall/Plate (juce::dsp::Reverb) vs. Spring
// (custom allpass cascade) split.
class LearnerVerbProcessor : public juce::AudioProcessor
{
public:
    LearnerVerbProcessor();
    ~LearnerVerbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Learner Verb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    void setWaveformDisplay (WaveformDisplay* display) noexcept { waveformDisplay = display; }

    // Sets every parameter to one of ReverbGuide::presets by index.
    // Exposed here (not just as an editor button handler) so it's
    // testable without constructing an editor/GUI.
    void applyPreset (int presetIndex);

    static constexpr const char* typeParamId = "type";
    static constexpr const char* decayParamId = "decay";
    static constexpr const char* preDelayParamId = "preDelay";
    static constexpr const char* sizeParamId = "size";
    static constexpr const char* dampingParamId = "damping";
    static constexpr const char* dryWetParamId = "dryWet";
    static constexpr const char* widthParamId = "width";

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateEngineParameters();

    ReverbEngine engine;
    juce::AudioBuffer<float> wetBuffer;
    std::atomic<WaveformDisplay*> waveformDisplay { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerVerbProcessor)
};
