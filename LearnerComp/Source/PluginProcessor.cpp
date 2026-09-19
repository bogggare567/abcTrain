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
    // The library only knows the real rate here, same as GameManager.
    practiceLibrary.prepare (sampleRate);
    practiceSource.prepare (sampleRate);

    engine.prepare (sampleRate);
    engine.reset();
    updateEngineParameters();

    makeupGain.reset (sampleRate, 0.03);
    mixAmount.reset (sampleRate, 0.03);
    activeAmount.reset (sampleRate, 0.02);
    makeupGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (valueOf (makeupParamId)));
    mixAmount.setCurrentAndTargetValue (valueOf (dryWetParamId) / 100.0f);
    activeAmount.setCurrentAndTargetValue (valueOf (bypassParamId) > 0.5f ? 0.0f : 1.0f);
}

void LearnerCompProcessor::updateEngineParameters()
{
    engine.setParameters (
        valueOf (thresholdParamId),
        valueOf (ratioParamId),
        valueOf (attackParamId),
        valueOf (releaseParamId),
        valueOf (kneeParamId),
        valueOf (makeupParamId));
}

void LearnerCompProcessor::setCheckOverride (const juce::String& parameterID, float value)
{
    checkOverrideValue.store (value);
    checkOverrideTarget.store (apvts.getRawParameterValue (parameterID));
}

void LearnerCompProcessor::clearCheckOverride()
{
    checkOverrideTarget.store (nullptr);
}

void LearnerCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // When no host is feeding us, play the practice clip instead - see
    // shared/PracticeAudioSource.h. Off by default.
    practiceSource.fillBlock (buffer);

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    auto* display = waveformDisplay.load();
    auto* analyzer = spectrumAnalyzer.load();

    // Three things glide rather than step (ADR 037): makeup, the mix, and
    // bypass itself. A makeup knob dragged or a bypass pressed used to jump
    // the output level between one sample and the next, which is a click -
    // and a click is exactly the thing a person learning to listen will
    // notice first and trust least.
    engine.setParameters (valueOf (thresholdParamId), valueOf (ratioParamId), valueOf (attackParamId),
                          valueOf (releaseParamId), valueOf (kneeParamId), 0.0f);
    makeupGain.setTargetValue (juce::Decibels::decibelsToGain (valueOf (makeupParamId)));
    mixAmount.setTargetValue (juce::jlimit (0.0f, 1.0f, valueOf (dryWetParamId) / 100.0f));
    activeAmount.setTargetValue (valueOf (bypassParamId) > 0.5f ? 0.0f : 1.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        float detection = 0.0f;
        float monoInput = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto x = buffer.getSample (ch, i);
            detection = juce::jmax (detection, std::abs (x));
            monoInput += x;
        }

        if (numChannels > 0)
            monoInput /= (float) numChannels;

        // Stereo-linked: one gain for every channel, so the image does not
        // pump sideways.
        const auto gain = engine.computeGain (detection) * makeupGain.getNextValue();
        const auto mix = mixAmount.getNextValue();
        const auto active = activeAmount.getNextValue();
        const auto inputForDisplay = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto dry = buffer.getSample (ch, i);
            const auto processed = mix * (dry * gain) + (1.0f - mix) * dry;
            buffer.setSample (ch, i, active >= 1.0f ? processed
                                     : active <= 0.0f ? dry
                                                      : dry + active * (processed - dry));
        }

        if (display != nullptr)
        {
            const auto outputForDisplay = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;
            display->pushSample (inputForDisplay, outputForDisplay, engine.getLastGainReductionDb() * active);
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

    // All seven, not five: a preset that leaves makeup and mix wherever
    // they were sounds different depending on what you did before it.
    setParam (makeupParamId, preset.makeupDb);
    setParam (dryWetParamId, preset.dryWetPercent);
    setParam (bypassParamId, 0.0f);
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
