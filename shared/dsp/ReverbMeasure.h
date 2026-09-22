#pragma once

#include "shared/dsp/ReverbEngine.h"
#include <cmath>
#include <vector>

// What a reverb setting actually does, measured rather than assumed: its
// impulse response, and the RT60 read off that response. One copy, used by
// the echogram on screen and by tests/LearnerRedesignTest, so the picture
// and the check can never measure two different things.
namespace ReverbMeasure
{
    struct Setting
    {
        ReverbEngine::Type type = ReverbEngine::Type::room;
        float decaySeconds = 1.5f;
        float preDelayMs = 0.0f;
        float size = 0.5f;       // 0..1
        float damping = 0.4f;    // 0..1

        bool operator== (const Setting& o) const noexcept
        {
            return type == o.type && decaySeconds == o.decaySeconds && preDelayMs == o.preDelayMs
                && size == o.size && damping == o.damping;
        }

        bool operator!= (const Setting& o) const noexcept { return ! (*this == o); }
    };

    // The left channel's response to one unit impulse, `seconds` long.
    // A fresh engine each call: a warmed-up one would carry the last tail.
    inline std::vector<float> impulseResponse (const Setting& s, double sampleRate, double seconds)
    {
        ReverbEngine engine;
        juce::dsp::ProcessSpec spec { sampleRate, 512, 2 };
        engine.prepare (spec);
        engine.setParameters (s.type, s.decaySeconds, s.preDelayMs, s.size, s.damping, 1.0f);

        juce::AudioBuffer<float> buffer (2, 512);

        // Let the type-switch fade and the pre-delay glide settle on
        // silence first, or the picture shows the switch, not the room.
        for (int i = 0; i < 8; ++i)
        {
            buffer.clear();
            juce::dsp::AudioBlock<float> block (buffer);
            engine.process (block);
        }

        std::vector<float> out;
        const auto total = (int) (seconds * sampleRate);
        out.reserve ((size_t) total + 512);

        for (int start = 0; start < total; start += 512)
        {
            buffer.clear();

            if (start == 0)
            {
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
            }

            juce::dsp::AudioBlock<float> block (buffer);
            engine.process (block);

            for (int i = 0; i < 512; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    }

    // Schroeder backward integration, T30: the slope fitted between -5 and
    // -35 dB, extrapolated to 60. Zero when the response is too short or
    // too quiet to fit.
    inline double rt60 (const std::vector<float>& ir, double sampleRate)
    {
        std::vector<double> energy (ir.size());
        double acc = 0.0;

        for (size_t i = ir.size(); i-- > 0;)
        {
            acc += (double) ir[i] * ir[i];
            energy[i] = acc;
        }

        if (energy.empty() || energy[0] <= 0.0)
            return 0.0;

        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        int n = 0;

        for (size_t i = 0; i < energy.size(); i += 64)
        {
            const auto db = 10.0 * std::log10 (std::max (1.0e-30, energy[i] / energy[0]));

            if (db <= -5.0 && db >= -35.0)
            {
                const auto t = (double) i / sampleRate;
                sx += t; sy += db; sxx += t * t; sxy += t * db; ++n;
            }
        }

        if (n < 4)
            return 0.0;

        const auto slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
        return slope < 0.0 ? -60.0 / slope : 0.0;
    }

    // When the first sound after the impulse arrives: the first sample
    // within 30 dB of the loudest one.
    inline double onsetSeconds (const std::vector<float>& ir, double sampleRate)
    {
        float peak = 0.0f;

        for (auto v : ir)
            peak = std::max (peak, std::abs (v));

        if (peak <= 0.0f)
            return 0.0;

        for (size_t i = 0; i < ir.size(); ++i)
            if (std::abs (ir[i]) >= peak * 0.0316f)
                return (double) i / sampleRate;

        return 0.0;
    }
}
