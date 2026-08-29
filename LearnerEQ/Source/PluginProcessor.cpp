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
    cacheParameterPointers();
}

void LearnerEQProcessor::cacheParameterPointers()
{
    for (int band = 0; band < maxBands; ++band)
    {
        auto& p = bandParams[(size_t) band];
        p.on   = apvts.getRawParameterValue (onParamId (band));
        p.type = apvts.getRawParameterValue (typeParamId (band));
        p.freq = apvts.getRawParameterValue (freqParamId (band));
        p.gain = apvts.getRawParameterValue (gainParamId (band));
        p.q    = apvts.getRawParameterValue (qParamId (band));
    }

    bypassParam = apvts.getRawParameterValue (bypassParamId);
}

juce::AudioProcessorValueTreeState::ParameterLayout LearnerEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::NormalisableRange<float> freqRange (20.0f, 20000.0f, 1.0f, 0.3f);
    juce::NormalisableRange<float> gainRange (-18.0f, 18.0f, 0.1f);
    juce::NormalisableRange<float> qRange (0.1f, 18.0f, 0.01f, 0.4f);

    juce::StringArray typeNames;
    for (int t = 0; t < EQCoefficients::numTypes; ++t)
        typeNames.add (EQCoefficients::nameForType (EQCoefficients::typeFromIndex (t)));

    // Every band exists as parameters whether it is switched on or not:
    // APVTS is fixed at construction, and a host needs the automation
    // lanes and the saved state to be there regardless of how many bands
    // happen to be in use right now.
    //
    // One band is on by default - a flat bell at 1 kHz. An EQ that opens
    // completely empty gives a first-time user nothing to grab, and the
    // whole interaction here is grabbing something.
    for (int band = 0; band < maxBands; ++band)
    {
        const auto bandName = "Band " + juce::String (band + 1);

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (onParamId (band), 1), bandName + " On", band == 0));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (typeParamId (band), 1), bandName + " Type",
            typeNames, (int) EQCoefficients::BandType::bell));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (freqParamId (band), 1), bandName + " Freq",
            freqRange, 1000.0f));

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
    // The library only knows the real rate here, same as GameManager.
    practiceLibrary.prepare (newSampleRate);
    practiceSource.prepare (newSampleRate);

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
    for (int band = 0; band < maxBands; ++band)
    {
        const auto& p = bandParams[(size_t) band];
        const auto on = p.on->load() > 0.5f;
        bandActive[(size_t) band] = on;

        if (! on)
            continue;   // a band that is off costs nothing to skip

        const auto type = EQCoefficients::typeFromIndex ((int) p.type->load());
        const auto freq = p.freq->load();
        const auto gain = p.gain->load();
        const auto q = p.q->load();

        *filters[(size_t) band].state = *EQCoefficients::make (type, sampleRate, freq, gain, q);
    }
}

bool LearnerEQProcessor::isBandOn (int band) const noexcept
{
    if (band < 0 || band >= maxBands)
        return false;

    return apvts.getRawParameterValue (onParamId (band))->load() > 0.5f;
}

EQCoefficients::BandType LearnerEQProcessor::getBandType (int band) const noexcept
{
    if (band < 0 || band >= maxBands)
        return EQCoefficients::BandType::bell;

    return EQCoefficients::typeFromIndex (
        (int) apvts.getRawParameterValue (typeParamId (band))->load());
}

int LearnerEQProcessor::addBand (float freqHz, float gainDb, EQCoefficients::BandType type)
{
    for (int band = 0; band < maxBands; ++band)
    {
        if (isBandOn (band))
            continue;

        const auto set = [this] (const juce::String& id, float value)
        {
            if (auto* parameter = apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        // Order matters: the shape is set before the band is switched on,
        // so it can never be audible for a block at the previous band's
        // settings.
        set (typeParamId (band), (float) (int) type);
        set (freqParamId (band), freqHz);
        set (gainParamId (band), EQCoefficients::usesGain (type) ? gainDb : 0.0f);
        set (qParamId (band), type == EQCoefficients::BandType::notch ? 6.0f : 0.7f);
        set (onParamId (band), 1.0f);

        return band;
    }

    return -1;
}

void LearnerEQProcessor::removeBand (int band)
{
    if (band < 0 || band >= maxBands)
        return;

    if (auto* parameter = apvts.getParameter (onParamId (band)))
        parameter->setValueNotifyingHost (0.0f);
}

void LearnerEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Practice audio replaces the host's input before anything else
    // touches it, so every meter, curve and knob downstream behaves
    // exactly as it would on a real track. Off unless someone asked for
    // it; see shared/PracticeAudioSource.h.
    practiceSource.fillBlock (buffer);


    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    // Captured before filtering so the waveform display can show the
    // untreated input alongside whatever bypass leaves in `buffer`.
    dryBuffer.makeCopyOf (buffer, true);

    const bool bypassed = bypassParam->load() > 0.5f;

    if (! bypassed)
    {
        updateFilters();

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);

        for (int band = 0; band < maxBands; ++band)
            if (bandActive[(size_t) band])
                filters[(size_t) band].process (context);
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
