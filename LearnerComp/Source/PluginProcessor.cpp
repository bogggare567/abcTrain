#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/WaveformDisplay.h"
#include "../../shared/SpectrumAnalyzer.h"
#include "ParameterGuide.h"

LearnerCompProcessor::LearnerCompProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LearnerCompProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (thresholdParamId, 1), "Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 1.0f), -12.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (ratioParamId, 1), "Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.5f), 4.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (attackParamId, 1), "Attack",
        juce::NormalisableRange<float> (0.1f, 100.0f, 0.01f, 0.3f), 10.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (releaseParamId, 1), "Release",
        juce::NormalisableRange<float> (10.0f, 1000.0f, 0.1f, 0.3f), 100.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (kneeParamId, 1), "Knee",
        juce::NormalisableRange<float> (0.0f, 24.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (makeupParamId, 1), "Makeup Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.5f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (dryWetParamId, 1), "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID (bypassParamId, 1), "Bypass", false));

    return { params.begin(), params.end() };
}

bool LearnerCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void LearnerCompProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.reset();
    updateEngineParameters();
}

void LearnerCompProcessor::updateEngineParameters()
{
    engine.setParameters (
        apvts.getRawParameterValue (thresholdParamId)->load(),
        apvts.getRawParameterValue (ratioParamId)->load(),
        apvts.getRawParameterValue (attackParamId)->load(),
        apvts.getRawParameterValue (releaseParamId)->load(),
        apvts.getRawParameterValue (kneeParamId)->load(),
        apvts.getRawParameterValue (makeupParamId)->load());
}

void LearnerCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    auto* display = waveformDisplay.load();
    auto* analyzer = spectrumAnalyzer.load();

    const bool bypassed = apvts.getRawParameterValue (bypassParamId)->load() > 0.5f;

    if (bypassed)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto sample = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;

            if (display != nullptr)
                display->pushSample (sample, sample, 0.0f);

            if (analyzer != nullptr)
            {
                float mono = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    mono += buffer.getSample (ch, i);
                if (numChannels > 0)
                    mono /= (float) numChannels;
                analyzer->pushNextSampleIntoFifo (mono);
            }
        }
        return;
    }

    updateEngineParameters();
    const auto dryWetFraction = juce::jlimit (0.0f, 1.0f,
        apvts.getRawParameterValue (dryWetParamId)->load() / 100.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        float detection = 0.0f;
        float monoInput = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto inputSample = buffer.getSample (ch, i);
            detection = juce::jmax (detection, std::abs (inputSample));
            monoInput += inputSample;
        }
        if (numChannels > 0)
            monoInput /= (float) numChannels;

        const auto gain = engine.computeGain (detection);
        const auto inputForDisplay = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto dry = buffer.getSample (ch, i);
            const auto wet = dry * gain;
            buffer.setSample (ch, i, dryWetFraction * wet + (1.0f - dryWetFraction) * dry);
        }

        if (display != nullptr)
        {
            const auto outputForDisplay = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;
            display->pushSample (inputForDisplay, outputForDisplay, engine.getLastGainReductionDb());
        }

        if (analyzer != nullptr)
            analyzer->pushNextSampleIntoFifo (monoInput);
    }
}

juce::AudioProcessorEditor* LearnerCompProcessor::createEditor()
{
    return new LearnerCompEditor (*this);
}

void LearnerCompProcessor::applyPreset (int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= (int) CompressorGuide::presets.size())
        return;

    const auto& preset = CompressorGuide::presets[(size_t) presetIndex];

    auto setParam = [this] (const char* paramId, float value)
    {
        if (auto* param = apvts.getParameter (paramId))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    setParam (thresholdParamId, preset.thresholdDb);
    setParam (ratioParamId, preset.ratio);
    setParam (attackParamId, preset.attackMs);
    setParam (releaseParamId, preset.releaseMs);
    setParam (kneeParamId, preset.kneeDb);
}

void LearnerCompProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void LearnerCompProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
