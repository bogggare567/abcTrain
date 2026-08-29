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

    // 40 ms is long enough that a dragged slider never zippers and short
    // enough that the control still feels immediate.
    outputGain.reset (sampleRate, 0.04);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGainDb.load(),
                                                                         minOutputGainDb));
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
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainDb.load(),
                                                                minOutputGainDb));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = outputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
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
