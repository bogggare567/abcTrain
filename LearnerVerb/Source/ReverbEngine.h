#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

// Room/Hall/Plate use juce::dsp::Reverb (Freeverb-derived); Spring is a
// cascade of resonant allpass filters - the same technique EarTrainer's
// ReverbGame uses for its Spring type, reimplemented here rather than
// literally shared, since ReverbGame's version is tightly coupled to its
// fixed-per-round game model and this one needs continuous, live
// parameter control instead.
//
// Freeverb's roomSize/damping aren't literally "decay in seconds" - the
// decay/size/damping -> Parameters mapping below is tuned by ear, not a
// physical model, the same "approximate, not measured" approach
// EarTrainer's CompressionGame/ReverbGame presets already take.
//
// Always renders 100% wet; PluginProcessor handles dry/wet blending
// externally, same division of responsibility as LearnerComp's
// CompressorEngine/processor split.
class ReverbEngine
{
public:
    enum class Type { room, hall, plate, spring };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        preDelayLine.setMaximumDelayInSamples ((int) (spec.sampleRate * 0.3) + 1);
        preDelayLine.prepare (spec);

        reverb.prepare (spec);

        for (auto& allpass : springAllpass)
            allpass.prepare (spec);
    }

    void reset()
    {
        preDelayLine.reset();
        reverb.reset();
        for (auto& allpass : springAllpass)
            allpass.reset();
    }

    void setParameters (Type newType, float decaySeconds, float preDelayMs,
                         float size, float damping, float width)
    {
        type = newType;

        preDelayLine.setDelay ((float) (preDelayMs * 0.001 * sampleRate));

        if (type == Type::spring)
        {
            const auto decayQ = juce::jmap (juce::jlimit (0.1f, 10.0f, decaySeconds), 0.1f, 10.0f, 1.0f, 8.0f);
            const auto q = juce::jmax (0.3f, decayQ * (1.0f - juce::jlimit (0.0f, 1.0f, damping) * 0.5f));
            const auto freqScale = 1.0f - juce::jlimit (0.0f, 1.0f, size) * 0.3f;

            for (int i = 0; i < (int) springAllpass.size(); ++i)
            {
                const auto freq = springFrequenciesHz[(size_t) i] * freqScale;
                *springAllpass[(size_t) i].state = *juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, freq, q);
            }
        }
        else
        {
            juce::dsp::Reverb::Parameters params;
            params.roomSize = juce::jlimit (0.0f, 1.0f, decaySeconds / 10.0f);
            params.damping = juce::jlimit (0.0f, 1.0f, damping);
            params.width = juce::jlimit (0.0f, 1.0f, width);
            params.wetLevel = 1.0f;
            params.dryLevel = 0.0f;

            switch (type)
            {
                case Type::room:  params.roomSize *= 0.5f; break;  // rooms are smaller than halls at the same decay setting
                case Type::plate: params.damping *= 0.3f; break;   // plates ring brighter
                default: break;
            }

            reverb.setParameters (params);
        }
    }

    // Processes one block in place (100% wet). The caller blends this
    // with the dry signal itself.
    void process (juce::dsp::AudioBlock<float>& block)
    {
        juce::dsp::ProcessContextReplacing<float> context (block);
        preDelayLine.process (context);

        if (type == Type::spring)
        {
            for (auto& allpass : springAllpass)
                allpass.process (context);
        }
        else
        {
            reverb.process (context);
        }
    }

private:
    static constexpr std::array<float, 4> springFrequenciesHz { 320.0f, 730.0f, 1400.0f, 2600.0f };

    double sampleRate = 44100.0;
    Type type = Type::room;

    juce::dsp::DelayLine<float> preDelayLine;
    juce::dsp::Reverb reverb;
    std::array<juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                               juce::dsp::IIR::Coefficients<float>>, 4> springAllpass;
};
