#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../shared/PracticeAudioSource.h"
#include "CompressorEngine.h"
#include <atomic>

class WaveformDisplay;
class SpectrumAnalyzerComponent;

// A real compressor that processes the host's audio, like LearnerEQ but
// for dynamics instead of frequency. Parameters are host-automatable via
// AudioProcessorValueTreeState. Detection is stereo-linked (the loudest
// channel at each sample drives gain reduction, applied equally to all
// channels) rather than per-channel, matching how most real compressors
// avoid pumping the stereo image.
class LearnerCompProcessor : public juce::AudioProcessor
{
public:
    LearnerCompProcessor();
    ~LearnerCompProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Learner Comp"; }
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

    void setWaveformDisplay (WaveformDisplay* display) noexcept { waveformDisplay = display; }
    void setSpectrumAnalyzer (SpectrumAnalyzerComponent* analyzer) noexcept { spectrumAnalyzer = analyzer; }

    // Sets every parameter to one of CompressorGuide::presets by index.
    // Exposed here (not just as an editor button handler) so it can be
    // tested directly without constructing an editor/GUI.
    void applyPreset (int presetIndex);

    static constexpr const char* thresholdParamId = "threshold";
    static constexpr const char* ratioParamId = "ratio";
    static constexpr const char* attackParamId = "attack";
    static constexpr const char* releaseParamId = "release";
    static constexpr const char* kneeParamId = "knee";
    static constexpr const char* makeupParamId = "makeup";
    static constexpr const char* dryWetParamId = "dryWet";
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
    void updateEngineParameters();

    CompressorEngine engine;
    std::atomic<WaveformDisplay*> waveformDisplay { nullptr };
    std::atomic<SpectrumAnalyzerComponent*> spectrumAnalyzer { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerCompProcessor)
};
