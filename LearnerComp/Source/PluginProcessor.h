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

    const juce::String getName() const override { return "ABC Learner Comp"; }
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
    // While a training module's check is auditioning its hidden reference,
    // one parameter is processed at a value the *knob does not show*.
    //
    // The obvious implementation - write the target into the parameter -
    // is the one that cannot work: the knob is bound to that parameter, so
    // it would move to the answer and give it away. So the parameter keeps
    // holding whatever the player dialled, and the override is applied on
    // the way into the DSP instead.
    //
    // Matched by pointer rather than by ID string, because the comparison
    // happens per block on the audio thread and comparing juce::Strings
    // there is work with no reason to exist.
    void setCheckOverride (const juce::String& parameterID, float value);
    void clearCheckOverride();

    ReferenceAudioLibrary& getPracticeLibrary() noexcept { return practiceLibrary; }
    PracticeAudioSource& getPracticeSource() noexcept { return practiceSource; }
    juce::PropertiesFile& getSharedProperties() noexcept { return sharedProperties; }

private:

    // See setCheckOverride. Null target means "no override", which is the
    // state for all but a few seconds of this plugin's life.
    std::atomic<std::atomic<float>*> checkOverrideTarget { nullptr };
    std::atomic<float> checkOverrideValue { 0.0f };

    // Audio thread. One atomic pointer compare per parameter read.
    //
    // juce::StringRef, not const juce::String& - every call site passes a
    // constexpr const char*, and binding that to a String reference builds
    // a heap-allocated temporary per read, eight or nine times per block.
    // StringRef wraps the literal without owning it, which is what
    // getRawParameterValue takes anyway.
    float valueOf (juce::StringRef parameterID) const noexcept
    {
        auto* raw = apvts.getRawParameterValue (parameterID);

        if (raw == nullptr)
            return 0.0f;

        return raw == checkOverrideTarget.load() ? checkOverrideValue.load() : raw->load();
    }

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
