#pragma once

#include "PluginProcessor.h"
#include <initializer_list>
#include <utility>
#include <vector>

// A whole EQ state for a lesson or module step: the bands named here
// switched on with their type and values, every other band switched off,
// bypass off.
//
// The two original lessons were written for the old fixed four-band layout
// and only ever moved gains on bands 1-3 - which, since only band 0 is on by
// default now, did nothing at all you could hear. A step that says "boost
// 3 kHz" has to be the whole state, or what it sounds like depends on what
// happened before.
namespace EQSetup
{
    struct Band
    {
        EQCoefficients::BandType type;
        float freq;
        float gain = 0.0f;
        float q = 0.7f;
    };

    using Params = std::vector<std::pair<juce::String, float>>;

    inline Params bands (std::initializer_list<Band> wanted)
    {
        using P = LearnerEQProcessor;
        Params out;
        int index = 0;

        for (const auto& b : wanted)
        {
            out.push_back ({ P::onParamId (index), 1.0f });
            out.push_back ({ P::typeParamId (index), (float) (int) b.type });
            out.push_back ({ P::freqParamId (index), b.freq });
            out.push_back ({ P::gainParamId (index), b.gain });
            out.push_back ({ P::qParamId (index), b.q });
            ++index;
        }

        for (; index < P::maxBands; ++index)
            out.push_back ({ P::onParamId (index), 0.0f });

        out.push_back ({ P::bypassParamId, 0.0f });
        return out;
    }
}
