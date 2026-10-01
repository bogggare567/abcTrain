#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "shared/analysis/WaveformDisplay.h"
#include "shared/analysis/SpectrumAnalyzer.h"
#include "ParameterGuide.h"

LearnerCompProcessor::LearnerCompProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                           .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
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

    // Appended, never inserted: hosts and saved sessions find parameters
    // by ID, and the existing eight keep theirs.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID (scSourceParamId, 1), "Sidechain", juce::StringArray { "Self", "External", "Kick" }, scSelf));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (scHpfParamId, 1), "Sidechain HPF",
        juce::NormalisableRange<float> (scHpfOffHz, 300.0f, 1.0f, 0.5f), scHpfOffHz));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID (scListenParamId, 1), "Sidechain Listen", false));

    return { params.begin(), params.end() };
}

bool LearnerCompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != mainOut)
        return false;

    // The sidechain may be off, mono or stereo; nothing else.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
    }

    return true;
}

void LearnerCompProcessor::KeyFilter::reset() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
        x1[ch] = x2[ch] = y1[ch] = y2[ch] = 0.0f;
}

void LearnerCompProcessor::KeyFilter::setHighPass (float hz, double sampleRate) noexcept
{
    if (hz == frequency)
        return;

    frequency = hz;
    // RBJ cookbook high-pass, Q = 0.707 (Butterworth).
    const auto w0 = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sampleRate * 0.45, (double) hz) / sampleRate;
    const auto cosw = std::cos (w0), alpha = std::sin (w0) / (2.0 * 0.7071067811865476);
    const auto a0 = 1.0 + alpha;
    b0 = (float) (((1.0 + cosw) * 0.5) / a0);
    b1 = (float) (-(1.0 + cosw) / a0);
    b2 = b0;
    a1 = (float) ((-2.0 * cosw) / a0);
    a2 = (float) ((1.0 - alpha) / a0);
}

float LearnerCompProcessor::KeyFilter::process (int ch, float x) noexcept
{
    ch = juce::jlimit (0, 1, ch);
    const auto y = b0 * x + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch];
    x2[ch] = x1[ch]; x1[ch] = x;
    y2[ch] = y1[ch]; y1[ch] = y;
    return y;
}

float LearnerCompProcessor::KickVoice::next() noexcept
{
    // 0.35 s of kick, then silence until the next beat.
    const auto t = position / sampleRate;
    float out = 0.0f;

    if (t < 0.35)
    {
        const auto freq = 48.0 + 92.0 * std::exp (-t / 0.03);
        phase += juce::MathConstants<double>::twoPi * freq / sampleRate;
        const auto body = std::exp (-t / 0.12);
        const auto click = t < 0.004 ? (1.0 - t / 0.004) * 0.3 : 0.0;
        out = (float) (0.9 * body * std::sin (phase) + click);
    }

    position += 1.0;
    if (position >= samplesPerBeat)
    {
        position -= samplesPerBeat;
        phase = 0.0;
    }

    return out;
}

void LearnerCompProcessor::prepareToPlay (double sampleRate, int)
{
    // The library only knows the real rate here, same as GameManager.
    practiceLibrary.prepare (sampleRate);
    practiceSource.prepare (sampleRate);

    engine.prepare (sampleRate);
    engine.reset();
    currentSampleRate = sampleRate;
    keyFilter.frequency = -1.0f;
    keyFilter.reset();
    kick.prepare (sampleRate);
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
    // shared/learning/PracticeAudioSource.h. Off by default.
    {
        auto main = getBusBuffer (buffer, true, 0);
        practiceSource.fillBlock (main);
    }

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Main bus only: with a sidechain bus enabled the host's buffer also
    // carries its channels, and those must neither be compressed nor
    // reach the output.
    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const auto sideBuffer = getBusCount (true) > 1 && getBus (true, 1)->isEnabled()
                              ? getBusBuffer (buffer, true, 1) : juce::AudioBuffer<float>();

    const auto source = (int) valueOf (scSourceParamId);
    const auto hpf = valueOf (scHpfParamId);
    const auto listen = valueOf (scListenParamId) > 0.5f;
    const auto useHpf = hpf > scHpfOffHz + 0.5f;
    if (useHpf)
        keyFilter.setHighPass (hpf, currentSampleRate);

    const auto external = source == scExternal && sideBuffer.getNumChannels() > 0;
    const auto kickKey = source == scKick;

    const auto numChannels = mainBuffer.getNumChannels();
    const auto numSamples = mainBuffer.getNumSamples();
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
        float keyMono = 0.0f;   // what Listen plays
        const auto kickSample = kickKey ? kick.next() : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto x = mainBuffer.getSample (ch, i);
            monoInput += x;
        }

        if (external)
        {
            for (int ch = 0; ch < sideBuffer.getNumChannels(); ++ch)
            {
                auto k = sideBuffer.getSample (ch, i);
                if (useHpf) k = keyFilter.process (ch, k);
                detection = juce::jmax (detection, std::abs (k));
                keyMono += k / (float) sideBuffer.getNumChannels();
            }
        }
        else if (kickKey)
        {
            const auto k = useHpf ? keyFilter.process (0, kickSample) : kickSample;
            detection = std::abs (k);
            keyMono = k;
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto k = mainBuffer.getSample (ch, i);
                if (useHpf) k = keyFilter.process (ch, k);
                detection = juce::jmax (detection, std::abs (k));
                keyMono += k / (float) numChannels;
            }
        }

        if (numChannels > 0)
            monoInput /= (float) numChannels;

        // Stereo-linked: one gain for every channel, so the image does not
        // pump sideways.
        const auto gain = engine.computeGain (detection) * makeupGain.getNextValue();
        const auto mix = mixAmount.getNextValue();
        const auto active = activeAmount.getNextValue();
        const auto inputForDisplay = numChannels > 0 ? mainBuffer.getSample (0, i) : 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto dry = mainBuffer.getSample (ch, i);
            const auto processed = mix * (dry * gain) + (1.0f - mix) * dry;
            auto out = active >= 1.0f ? processed
                     : active <= 0.0f ? dry
                                      : dry + active * (processed - dry);

            // The teaching kick is heard as well as used: ducking only
            // makes sense against the thing that ducks it.
            if (kickKey)
                out += 0.5f * kickSample;

            mainBuffer.setSample (ch, i, listen ? keyMono : out);
        }

        if (display != nullptr)
        {
            const auto outputForDisplay = numChannels > 0 ? mainBuffer.getSample (0, i) : 0.0f;
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
