#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "shared/analysis/WaveformDisplay.h"
#include "shared/analysis/SpectrumAnalyzer.h"
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

    // Appended: saved sessions find parameters by ID.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID (routingParamId, 1), "Routing", juce::StringArray { "Insert", "Send" }, insert));

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

    // Headroom: a host sending a larger block than it announced must not
    // make makeCopyOf reallocate on the audio thread.
    wetBuffer.setSize (juce::jmax (2, getTotalNumOutputChannels()), juce::jmax (samplesPerBlock, 8192));
    wetAmount.reset (sampleRate, 0.03);
    dryAmount.reset (sampleRate, 0.03);
    const auto [wet, dry] = targetLevels();
    wetAmount.setCurrentAndTargetValue (wet);
    dryAmount.setCurrentAndTargetValue (dry);

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

std::pair<float, float> LearnerVerbProcessor::targetLevels() const noexcept
{
    if (valueOf (bypassParamId) > 0.5f)
        return { 0.0f, 1.0f };

    const auto mix = juce::jlimit (0.0f, 1.0f, valueOf (dryWetParamId) / 100.0f);

    if ((int) valueOf (routingParamId) == send)
        return simulateSendBus.load() ? std::pair { mix, 1.0f } : std::pair { 1.0f, 0.0f };

    return { mix, 1.0f - mix };
}

void LearnerVerbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Practice audio replaces the host's input before anything else
    // touches it, so every meter, curve and knob downstream behaves
    // exactly as it would on a real track. Off unless someone asked for
    // it; see shared/learning/PracticeAudioSource.h.
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
    // Smoothed per sample: a mix knob dragged, or bypass pressed, must
    // glide rather than step - a step in the wet level mid-tail clicks.
    // Insert: one Mix knob crossfades dry and wet. Send in a DAW: this is
    // the return channel, so wet only. Send in the app's Studio: the dry
    // track stays at unity and Mix is the return fader.
    juce::ignoreUnused (bypassed);
    const auto [wetTarget, dryTarget] = targetLevels();
    wetAmount.setTargetValue (wetTarget);
    dryAmount.setTargetValue (dryTarget);

    auto* display = waveformDisplay.load();
    auto* analyzer = spectrumAnalyzer.load();

    for (int i = 0; i < numSamples; ++i)
    {
        float monoIn = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            monoIn += buffer.getSample (ch, i);
        if (numChannels > 0)
            monoIn /= (float) numChannels;

        const auto dryWetFraction = wetAmount.getNextValue();
        const auto dryFraction = dryAmount.getNextValue();
        const auto dryForDisplay = numChannels > 0 ? buffer.getSample (0, i) : 0.0f;
        const auto wetForDisplay = numChannels > 0 ? wetBuffer.getSample (0, i) : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto dry = buffer.getSample (ch, i);
            const auto wet = wetBuffer.getSample (ch, i);
            buffer.setSample (ch, i, dryWetFraction * wet + dryFraction * dry);
        }

        if (display != nullptr)
        {
            const auto outputForDisplay = dryWetFraction * wetForDisplay + dryFraction * dryForDisplay;
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
