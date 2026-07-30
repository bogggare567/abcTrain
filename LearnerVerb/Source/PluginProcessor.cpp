#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../shared/WaveformDisplay.h"
#include "../../shared/SpectrumAnalyzer.h"
#include "ReverbGuide.h"

LearnerVerbProcessor::LearnerVerbProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LearnerVerbProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID (typeParamId, 1), "Type",
        juce::StringArray { "Room", "Hall", "Plate", "Spring" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (decayParamId, 1), "Decay",
        juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f, 0.3f), 1.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (preDelayParamId, 1), "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 250.0f, 1.0f), 20.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (sizeParamId, 1), "Size",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (dampingParamId, 1), "Damping",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (dryWetParamId, 1), "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 30.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (widthParamId, 1), "Width",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID (bypassParamId, 1), "Bypass", false));

    return { params.begin(), params.end() };
}

bool LearnerVerbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void LearnerVerbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The library only knows the real rate here, same as GameManager.
    practiceLibrary.prepare (sampleRate);
    practiceSource.prepare (sampleRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    engine.prepare (spec);
    engine.reset();

    wetBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);

    updateEngineParameters();
}

void LearnerVerbProcessor::updateEngineParameters()
{
    const auto typeIndex = (int) valueOf (typeParamId);
    const auto type = static_cast<ReverbEngine::Type> (juce::jlimit (0, 3, typeIndex));

    engine.setParameters (
        type,
        valueOf (decayParamId),
        valueOf (preDelayParamId),
        valueOf (sizeParamId) / 100.0f,
        valueOf (dampingParamId) / 100.0f,
        valueOf (widthParamId) / 100.0f);
}

void LearnerVerbProcessor::setCheckOverride (const juce::String& parameterID, float value)
{
    checkOverrideValue.store (value);
    checkOverrideTarget.store (apvts.getRawParameterValue (parameterID));
}

void LearnerVerbProcessor::clearCheckOverride()
{
    checkOverrideTarget.store (nullptr);
}

void LearnerVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Practice audio replaces the host's input before anything else
    // touches it, so every meter, curve and knob downstream behaves
    // exactly as it would on a real track. Off unless someone asked for
    // it; see shared/PracticeAudioSource.h.
    practiceSource.fillBlock (buffer);


    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    updateEngineParameters();

    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    const bool bypassed = valueOf (bypassParamId) > 0.5f;

    wetBuffer.makeCopyOf (buffer, true);
    juce::dsp::AudioBlock<float> wetBlock (wetBuffer);
    engine.process (wetBlock);

    // Bypass forces a pure dry passthrough by zeroing the effective wet
    // mix, rather than overwriting the Dry/Wet parameter itself, so
    // un-bypassing restores whatever Dry/Wet was dialled in before. The
    // engine still runs every block regardless of bypass so its internal
    // state (the reverb tail) stays warm - no click or cold-start thump
    // if bypass is toggled off mid-tail.
    const auto dryWetFraction = bypassed ? 0.0f
        : juce::jlimit (0.0f, 1.0f, valueOf (dryWetParamId) / 100.0f);

    auto* display = waveformDisplay.load();
    auto* analyzer = spectrumAnalyzer.load();

    for (int i = 0; i < numSamples; ++i)
    {
        float monoIn = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            monoIn += buffer.getSample (ch, i);
        if (numChannels > 0)
            monoIn /= (float) numChannels;

        const auto dryForDisplay = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;
        const auto wetForDisplay = numChannels > 0 ? wetBuffer.getSample (0, i) : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto dry = buffer.getSample (ch, i);
            const auto wet = wetBuffer.getSample (ch, i);
            buffer.setSample (ch, i, dryWetFraction * wet + (1.0f - dryWetFraction) * dry);
        }

        if (display != nullptr)
        {
            const auto outputForDisplay = dryWetFraction * wetForDisplay + (1.0f - dryWetFraction) * dryForDisplay;
            display->pushSample (dryForDisplay, outputForDisplay);
        }

        if (analyzer != nullptr)
            analyzer->pushNextSampleIntoFifo (monoIn);
    }
}

juce::AudioProcessorEditor* LearnerVerbProcessor::createEditor()
{
    return new LearnerVerbEditor (*this);
}

void LearnerVerbProcessor::applyPreset (int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= (int) ReverbGuide::presets.size())
        return;

    const auto& preset = ReverbGuide::presets[(size_t) presetIndex];

    auto setParam = [this] (const char* paramId, float value)
    {
        if (auto* param = apvts.getParameter (paramId))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    if (auto* typeParam = apvts.getParameter (typeParamId))
        typeParam->setValueNotifyingHost (typeParam->convertTo0to1 ((float) preset.typeIndex));

    setParam (decayParamId, preset.decaySeconds);
    setParam (preDelayParamId, preset.preDelayMs);
    setParam (sizeParamId, preset.size * 100.0f);
    setParam (dampingParamId, preset.damping * 100.0f);
    setParam (dryWetParamId, preset.dryWetPercent);
    setParam (widthParamId, preset.width * 100.0f);
}

void LearnerVerbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void LearnerVerbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
