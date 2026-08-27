#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../shared/PracticeAudioSource.h"
#include <juce_dsp/juce_dsp.h>
#include "EQCoefficients.h"
#include <array>
#include <atomic>

class SpectrumAnalyserComponent;
class WaveformDisplay;

// A real graphical EQ that processes whatever audio the host feeds it
// (unlike the EarTrainer games, which ignore host input and generate their
// own test signal).
//
// **Eight free bands, any type, added and removed on the curve.** It was
// four fixed bands - a low shelf, two bells and a high shelf - with twelve
// rotary knobs under the display. That shape teaches the wrong thing: an
// EQ move starts as "something is wrong around *there*", and a fixed slot
// list makes you translate that into which of four knobs to reach for
// before you have even decided what the move is. Here you point at the
// place and the band appears.
//
// Eight rather than literally unlimited: every band is a set of APVTS
// parameters that has to exist up front to stay host-automatable and to
// save with the session, so the count is a fixed ceiling with an on/off
// per band. Eight is past the point where a *teaching* EQ stops being a
// lesson and starts being a job.
//
// Parameters are exposed via AudioProcessorValueTreeState so they are
// host-automatable like any real EQ plugin.
class LearnerEQProcessor : public juce::AudioProcessor
{
public:
    // The ceiling, not the count. getNumActiveBands() is what is on.
    static constexpr int maxBands = 8;

    LearnerEQProcessor();
    ~LearnerEQProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ABC Learner EQ"; }
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
    static juce::String qParamId (int band)    { return "band" + juce::String (band) + "Q"; }
    static juce::String typeParamId (int band) { return "band" + juce::String (band) + "Type"; }
    static juce::String onParamId (int band)   { return "band" + juce::String (band) + "On"; }

    // ---- band bookkeeping, for the editor ----
    bool isBandOn (int band) const noexcept;
    EQCoefficients::BandType getBandType (int band) const noexcept;

    // Turns on the first band that is off, at this frequency/gain, and
    // returns its index (-1 if all eight are in use). Lives here rather
    // than in the editor so the "add a band" gesture is one call and the
    // defaults for a new band are defined in one place.
    int addBand (float freqHz, float gainDb, EQCoefficients::BandType type);
    void removeBand (int band);
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
                                               juce::dsp::IIR::Coefficients<float>>, maxBands> filters;

    // Which bands processBlock should actually run. Refreshed on the audio
    // thread in updateFilters, read a few lines later - a plain array, not
    // atomics, because both happen on the same thread inside one block.
    std::array<bool, maxBands> bandActive {};

    // Raw-parameter pointers, resolved once. getRawParameterValue looks up
    // a std::map keyed by string, and every call site above builds that
    // string fresh - "band3Freq" is a juce::String with no small-string
    // optimisation, so two or three heap allocations each. updateFilters
    // ran forty of those per block, i.e. tens of thousands of mallocs per
    // second on the audio thread, where malloc takes a lock. The pointers
    // are stable for the processor's lifetime, so looking them up once is
    // both faster and the only version that is real-time safe.
    struct BandParams
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
    };

    std::array<BandParams, maxBands> bandParams {};
    std::atomic<float>* bypassParam = nullptr;
    void cacheParameterPointers();
    double sampleRate = 44100.0;
    juce::AudioBuffer<float> dryBuffer;

    std::atomic<SpectrumAnalyserComponent*> spectrumAnalyser { nullptr };
    std::atomic<WaveformDisplay*> waveformDisplay { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LearnerEQProcessor)
};
