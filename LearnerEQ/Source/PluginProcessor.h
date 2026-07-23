#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "EQCoefficients.h"
#include <array>
#include <atomic>

class SpectrumAnalyserComponent;

// A real 4-band EQ that processes whatever audio the host feeds it (unlike
// the EarTrainer games, which ignore host input and generate their own
// test signal). Band 0 is a low shelf, band 3 a high shelf, bands 1-2 are
// bell filters. Parameters are exposed via AudioProcessorValueTreeState so
// they're host-automatable like any real EQ plugin.
class LearnerEQProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 4;

    LearnerEQProcessor();
    ~LearnerEQProcessor() override = default;

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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    void setSpectrumAnalyser (SpectrumAnalyserComponent* analyser) noexcept { spectrumAnalyser = analyser; }

    static juce::String freqParamId (int band) { return "band" + juce::String (band) + "Freq"; }
    static juce::String gainParamId (int band) { return "band" + juce::String (band) + "Gain"; }
    static juce::String qParamId (int band) { return "band" + juce::String (band) + "Q"; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateFilters();

    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>, numBands> filters;
    double sampleRate = 44100.0;

    std::atomic<SpectrumAnalyserComponent*> spectrumAnalyser { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEQProcessor)
};
