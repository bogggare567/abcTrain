#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SpectrumAnalyser.h"
#include "../../shared/WaveformDisplay.h"

LearnerEQProcessor::LearnerEQProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LearnerEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const std::array<float, numBands> defaultFreqs { 100.0f, 800.0f, 3000.0f, 8000.0f };

    juce::NormalisableRange<float> freqRange (20.0f, 20000.0f, 1.0f, 0.3f);
    juce::NormalisableRange<float> gainRange (-18.0f, 18.0f, 0.1f);
    juce::NormalisableRange<float> qRange (0.1f, 10.0f, 0.01f, 0.4f);

    for (int band = 0; band < numBands; ++band)
    {
        const juce::String bandName (EQCoefficients::nameForBand (band));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (freqParamId (band), 1), bandName + " Freq",
            freqRange, defaultFreqs[(size_t) band]));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (gainParamId (band), 1), bandName + " Gain",
            gainRange, 0.0f));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (qParamId (band), 1), bandName + " Q",
            qRange, 0.7f));
    }

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID (bypassParamId, 1), "Bypass", false));

    return { params.begin(), params.end() };
}

bool LearnerEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void LearnerEQProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    for (auto& filter : filters)
        filter.prepare (spec);

    dryBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);

    updateFilters();
}

void LearnerEQProcessor::updateFilters()
{
    for (int band = 0; band < numBands; ++band)
    {
        const auto freq = apvts.getRawParameterValue (freqParamId (band))->load();
        const auto gain = apvts.getRawParameterValue (gainParamId (band))->load();
        const auto q = apvts.getRawParameterValue (qParamId (band))->load();

        *filters[(size_t) band].state = *EQCoefficients::make (band, sampleRate, freq, gain, q);
    }
}

void LearnerEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    // Captured before filtering so the waveform display can show the
    // untreated input alongside whatever bypass leaves in `buffer`.
    dryBuffer.makeCopyOf (buffer, true);

    const bool bypassed = apvts.getRawParameterValue (bypassParamId)->load() > 0.5f;

    if (! bypassed)
    {
        updateFilters();

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        for (auto& filter : filters)
            filter.process (context);
    }

    if (auto* display = waveformDisplay.load())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto dry = numChannels > 0 ? dryBuffer.getSample (0, i) : 0.0f;
            const auto wet = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;
            display->pushSample (dry, wet);
        }
    }

    if (auto* analyser = spectrumAnalyser.load())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += buffer.getSample (ch, i);
            if (numChannels > 0)
                mono /= (float) numChannels;

            analyser->pushNextSampleIntoFifo (mono);
        }
    }
}

juce::AudioProcessorEditor* LearnerEQProcessor::createEditor()
{
    return new LearnerEQEditor (*this);
}

void LearnerEQProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void LearnerEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
