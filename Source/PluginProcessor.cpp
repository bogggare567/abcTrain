#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LearnerEQ/Source/PluginProcessor.h"
#include "LearnerComp/Source/PluginProcessor.h"
#include "LearnerVerb/Source/PluginProcessor.h"

EarTrainerProcessor::EarTrainerProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    studio[0] = std::make_unique<LearnerEQProcessor>();
    studio[1] = std::make_unique<LearnerCompProcessor>();
    studio[2] = std::make_unique<LearnerVerbProcessor>();

    // Before any audio device exists, so the editor (and the snapshot
    // tool, which never has one) can already convert levels.
    measureCalibrationNoise (48000.0);
}

void EarTrainerProcessor::measureCalibrationNoise (double sampleRate)
{
    // Two seconds of the exact generator the calibration row plays, once
    // unscaled to find the gain that puts it at -20 dBFS RMS, then scaled
    // and A-weighted to find what the meter will read for it.
    const auto length = juce::roundToInt (sampleRate * 2.0);
    juce::AudioBuffer<float> noise (1, length);

    PinkNoiseGenerator generator { 0x5eed };
    double sum = 0.0;

    for (int i = 0; i < length; ++i)
    {
        const auto x = generator.nextSample();
        noise.setSample (0, i, x);
        sum += (double) x * x;
    }

    const auto rms = std::sqrt (sum / length);
    calibrationGain = rms > 0.0 ? (float) (juce::Decibels::decibelsToGain ((double) calibrationLevelDbFs) / rms)
                                : 0.0f;
    noise.applyGain (calibrationGain);

    calibrationReference = AWeightedMeter::meanSquareOf (noise, sampleRate);
}

bool EarTrainerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOut;
}

void EarTrainerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    gameManager.prepare (spec);

    outputMeter.prepare (sampleRate, getTotalNumOutputChannels());
    measureCalibrationNoise (sampleRate);

    // 40 ms is long enough that a dragged slider never zippers and short
    // enough that the control still feels immediate.
    outputGain.reset (sampleRate, 0.04);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGainDb.load(),
                                                                         minOutputGainDb));

    // All three, not just the one on show, so switching in the Studio is
    // a pointer swap on the audio thread and never a prepare.
    for (auto& effect : studio)
    {
        effect->setPlayConfigDetails (getTotalNumInputChannels(), getTotalNumOutputChannels(),
                                      sampleRate, samplesPerBlock);
        effect->prepareToPlay (sampleRate, samplesPerBlock);
    }
}

void EarTrainerProcessor::releaseResources()
{
    for (auto& effect : studio)
        effect->releaseResources();
}

void EarTrainerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // The Studio: the Learner's own processBlock, then the same last two
    // steps as the trainer - one output level, and the hearing meter,
    // because an hour of EQ on headphones is an hour of sound. Calibration
    // noise still wins over it, as it does over everything.
    if (const auto effect = studioEffect.load(); effect >= 0 && ! calibrationNoise.load())
    {
        studio[(size_t) effect]->processBlock (buffer, midi);
        applyOutputGain (buffer);
        outputMeter.process (buffer);
        return;
    }

    // The trainer ignores whatever the host feeds in and generates its own test signal.
    gameManager.process (buffer);

    if (calibrationNoise.load())
    {
        // The same noise in every channel: the player measures what both
        // monitors together put at their head, which is what they hear.
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto x = calibrationGenerator.nextSample() * calibrationGain;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, x);
        }

        outputMeter.process (buffer);
        return;
    }

    if (! signalEnabled.load())
    {
        // Still ran the game's process() above so its internal state (burst
        // envelopes, delay tails, filter memory) stays continuous - only
        // the output is silenced. Clearing without processing would make
        // every return from the menu start mid-burst.
        buffer.clear();

        // Silence is metered too: quiet time on the menus is what makes a
        // break count as one.
        outputMeter.process (buffer);
        return;
    }

    // Feed the hint scopes from the generated signal. Both are optional -
    // they only exist while the editor's hint panel is open.
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (auto* scope = vectorscope.load())
    {
        // A vectorscope needs two genuinely different channels; a mono
        // buffer would just draw a vertical line forever, which is
        // correct but useless, so feed the same sample to both and let it
        // say so.
        const auto* left = numChannels > 0 ? buffer.getReadPointer (0) : nullptr;
        const auto* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

        if (left != nullptr)
            for (int i = 0; i < numSamples; ++i)
                scope->pushSample (left[i], right[i]);
    }

    if (auto* analyzer = spectrum.load())
    {
        const auto* left = numChannels > 0 ? buffer.getReadPointer (0) : nullptr;
        const auto* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

        if (left != nullptr)
            for (int i = 0; i < numSamples; ++i)
                analyzer->pushNextSampleIntoFifo (0.5f * (left[i] + right[i]));
    }

    if (auto* display = waveform.load())
    {
        const auto* left = numChannels > 0 ? buffer.getReadPointer (0) : nullptr;
        const auto* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

        // Both traces get the same signal. WaveformDisplay was built for
        // the Learner plugins, where the two are dry and wet - here there
        // is only one signal and the point is its *shape over time*, so
        // feeding one trace and leaving the other silent would draw a flat
        // line beside the answer for no reason.
        if (left != nullptr)
            for (int i = 0; i < numSamples; ++i)
            {
                const auto mono = 0.5f * (left[i] + right[i]);
                display->pushSample (mono, mono);
            }
    }

    // Output level, last of all.
    //
    // After the scopes on purpose: the hint views are about what the
    // processing does to the signal, not about how loud the player has set
    // their monitoring - a waveform that shrank as you turned the volume
    // down would be answering a question nobody asked.
    //
    // And after the game, which is what keeps it honest: each exercise
    // levels its treated signal against its untreated one so loudness
    // cannot be the tell, and one gain applied to everything downstream
    // moves both sides of every A/B by the same amount.
    applyOutputGain (buffer);

    // What actually leaves, for the hearing dose.
    outputMeter.process (buffer);
}

void EarTrainerProcessor::applyOutputGain (juce::AudioBuffer<float>& buffer) noexcept
{
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainDb.load(),
                                                                minOutputGainDb));

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = outputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

void EarTrainerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("abcTrainStudio");

    for (size_t i = 0; i < studio.size(); ++i)
    {
        juce::MemoryBlock block;
        studio[i]->getStateInformation (block);
        state.setProperty ("effect" + juce::String ((int) i), block.toBase64Encoding(), nullptr);
    }

    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void EarTrainerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);

    if (! state.hasType ("abcTrainStudio"))
        return;

    for (size_t i = 0; i < studio.size(); ++i)
    {
        juce::MemoryBlock block;

        if (block.fromBase64Encoding (state.getProperty ("effect" + juce::String ((int) i)).toString())
            && block.getSize() > 0)
            studio[i]->setStateInformation (block.getData(), (int) block.getSize());
    }
}

juce::AudioProcessorEditor* EarTrainerProcessor::createEditor()
{
    return new EarTrainerEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EarTrainerProcessor();
}
