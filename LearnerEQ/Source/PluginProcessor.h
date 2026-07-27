#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../shared/PracticeAudioSource.h"
#include <juce_dsp/juce_dsp.h>
#include "EQCoefficients.h"
#include <array>
#include <atomic>

class SpectrumAnalyserComponent;
class WaveformDisplay;

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

    const juce::String getName() const override { return "Learner EQ"; }
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
    void setWaveformDisplay (WaveformDisplay* display) noexcept { waveformDisplay = display; }

    static juce::String freqParamId (int band) { return "band" + juce::String (band) + "Freq"; }
    static juce::String gainParamId (int band) { return "band" + juce::String (band) + "Gain"; }
    static juce::String qParamId (int band) { return "band" + juce::String (band) + "Q"; }
    static constexpr const char* bypassParamId = "bypass";

    // Practice audio: the shared reference library, played through this
    // plugin so it is not silent outside a DAW. Off by default - see
    // shared/PracticeAudioSource.h.
    ReferenceAudioLibrary& getPracticeLibrary() noexcept { return practiceLibrary; }
    PracticeAudioSource& getPracticeSource() noexcept { return practiceSource; }
    juce::PropertiesFile& getSharedProperties() noexcept { return sharedProperties; }

private:

    // Product-wide preferences (the same "abcTrain" file the theme,
    // language and reference library already share), not APVTS state:
    // which clip you practise on is a property of the person, not of the
    // host session, and it should not travel in a saved project.
    juce::PropertiesFile sharedProperties { ReferenceAudioLibrary::makeDefaultOptions() };
    ReferenceAudioLibrary practiceLibrary { sharedProperties };
    PracticeAudioSource practiceSource { practiceLibrary };
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateFilters();

    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>, numBands> filters;
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<SpectrumAnalyserComponent*> spectrumAnalyser { nullptr };
    std::atomic<WaveformDisplay*> waveformDisplay { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEQProcessor)
};
