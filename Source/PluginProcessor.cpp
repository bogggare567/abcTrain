#include "PluginProcessor.h"
#include "PluginEditor.h"

EarTrainerProcessor::EarTrainerProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
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
}

void EarTrainerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // The trainer ignores whatever the host feeds in and generates its own test signal.
    gameManager.process (buffer);

    if (! signalEnabled.load())
    {
        // Still ran the game's process() above so its internal state (burst
        // envelopes, delay tails, filter memory) stays continuous - only
        // the output is silenced. Clearing without processing would make
        // every return from the menu start mid-burst.
        buffer.clear();
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
