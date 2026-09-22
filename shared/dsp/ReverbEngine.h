#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>

// Learner Verb's reverb: four algorithms that are actually different, and
// knobs that do what they say (ADR 037).
//
// What was here: Room, Hall and Plate were one Freeverb with its room size
// rescaled, the Size knob did nothing to any of them, "Decay (s)" was
// roomSize = decay / 10 and so not seconds at all, and Spring was four
// all-pass filters with no feedback - a short ring, not a tank. A plugin
// that teaches reverb cannot have a Size knob a sound engineer turns and
// hears nothing from.
//
//   Room / Hall   an 8-line feedback delay network (Jot) with a Householder
//                 matrix, early reflections from a tapped delay, and
//                 per-line absorption filters. Decay is RT60 in seconds -
//                 each line's gain is 10^(-3 * length / (RT60 * fs)) - and
//                 Size scales every delay length, so it changes echo density
//                 and the spacing of the first reflections without changing
//                 how long the tail lasts. Room: short lines, strong early
//                 reflections. Hall: long lines, early reflections far apart
//                 and quieter.
//   Plate         Dattorro's plate (J. Audio Eng. Soc., 1997): input
//                 diffusion into a two-branch figure-eight tank with a
//                 modulated all-pass, taps from inside the tank. Dense from
//                 the first instant, no early reflections - a sheet of metal
//                 has no walls. Size scales the tank.
//   Spring        two springs, each a feedback loop whose transit time is
//                 the spring length (Size) with a long cascade of first-order
//                 all-passes in it. That cascade is dispersion: high
//                 frequencies travel faster along a coil than low ones, which
//                 is what makes the "drip" and the chirp on every repeat.
//
// Damping in all four is the ratio of the high-frequency decay time to the
// low one: 0 keeps the top end as long as the bottom, 100% makes it die
// about seven times faster. Width is mid/side on the wet signal only.
//
// Always renders 100% wet; the processor blends.
class ReverbEngine
{
public:
    enum class Type { room, hall, plate, spring };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
        coefficientsValid = false;   // lengths and gains depend on the rate

        const auto seconds = [this] (double s) { return (int) std::ceil (s * sampleRate) + 4; };

        preDelay.assign ((size_t) seconds (0.26), 0.0f);
        preDelaySmoothed.reset (sampleRate, 0.08);

        for (auto& line : fdn)
            line.assign ((size_t) seconds (0.16), 0.0f);

        early.assign ((size_t) seconds (0.12), 0.0f);

        const auto plateScale = sampleRate / 29761.0 * 1.35;

        for (size_t i = 0; i < plateInputLengths.size(); ++i)
            plateIn[i].assign ((size_t) std::ceil (plateInputLengths[i] * plateScale) + 4, 0.0f);

        for (size_t i = 0; i < 8; ++i)
            plateTank[i].assign ((size_t) std::ceil ((plateTankLengths[i] + 40.0) * plateScale) + 4, 0.0f);

        for (auto& spring : springs)
        {
            spring.loop.assign ((size_t) seconds (0.08), 0.0f);
            spring.allpassState.fill (0.0f);
        }

        fadeGain = 1.0f;
        reset();
    }

    void reset()
    {
        std::fill (preDelay.begin(), preDelay.end(), 0.0f);
        preWrite = 0;
        resetTank();
    }

    // Everything after the pre-delay. Called on the audio thread when the
    // type changes, so it only ever clears - it never allocates.
    void resetTank()
    {
        std::fill (early.begin(), early.end(), 0.0f);

        for (auto& line : fdn)   std::fill (line.begin(), line.end(), 0.0f);
        for (auto& b : plateIn)  std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : plateTank) std::fill (b.begin(), b.end(), 0.0f);

        for (auto& spring : springs)
        {
            std::fill (spring.loop.begin(), spring.loop.end(), 0.0f);
            spring.allpassState.fill (0.0f);
            spring.lowpass = 0.0f;
            spring.writePos = 0;
        }

        fdnLowpass.fill (0.0f);
        plateLowpass = { 0.0f, 0.0f };
        plateBandwidth = 0.0f;
        plateTailL = plateTailR = 0.0f;
        writePos = 0;
        earlyWrite = 0;
        plateInWrite.fill (0);
        plateTankWrite.fill (0);
        lfoPhase = 0.0;
    }

    void setParameters (Type newType, float decaySeconds, float preDelayMs,
                        float size, float damping, float width)
    {
        if (newType != type && ! switching)
        {
            // Fade the old algorithm out, clear it, fade the new one in:
            // switching a tail mid-flight is a click.
            pendingType = newType;
            switching = true;
        }

        const auto newRt60 = juce::jlimit (0.1f, 10.0f, decaySeconds);
        const auto newSize = juce::jlimit (0.0f, 1.0f, size);
        const auto newDamping = juce::jlimit (0.0f, 1.0f, damping);
        const auto newWidth = juce::jlimit (0.0f, 1.0f, width);

        preDelaySmoothed.setTargetValue ((float) (juce::jlimit (0.0f, 250.0f, preDelayMs) * 0.001 * sampleRate));

        // The processor calls this every block. Recomputing the gains and
        // absorption filters (pow/exp per delay line) is only needed when
        // something they depend on moved - the review in ADR 038 was right
        // that doing it unconditionally is wasted work on the audio thread.
        if (coefficientsValid && newRt60 == rt60 && newSize == sizeAmount
            && newDamping == dampingAmount && newWidth == widthAmount)
            return;

        rt60 = newRt60;
        sizeAmount = newSize;
        dampingAmount = newDamping;
        widthAmount = newWidth;
        coefficientsValid = true;

        updateCoefficients();
    }

    Type getType() const noexcept { return type; }

    // Switches algorithm at once, with an empty tank - no crossfade. For a
    // caller that is between sounds anyway: the trainer changes its space
    // only when a round starts, and fading the previous round's space into
    // the new one would play the answer to the last question over the start
    // of the next. Message thread or audio thread; never allocates.
    void setTypeNow (Type newType) noexcept
    {
        type = pendingType = newType;
        switching = false;
        fadeGain = 1.0f;
        resetTank();
        coefficientsValid = false;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const auto numSamples = (int) block.getNumSamples();
        const auto numChannels = (int) block.getNumChannels();

        if (numChannels == 0 || preDelay.empty())
            return;

        auto* left = block.getChannelPointer (0);
        auto* right = numChannels > 1 ? block.getChannelPointer (1) : nullptr;

        const auto fadeStep = 1.0f / (float) (0.02 * sampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto inL = left[i];
            const auto inR = right != nullptr ? right[i] : inL;

            // Pre-delay, smoothed and read fractionally, so turning the knob
            // glides instead of clicking.
            preDelay[(size_t) preWrite] = 0.5f * (inL + inR);
            const auto delayed = readFractional (preDelay, preWrite, preDelaySmoothed.getNextValue());
            preWrite = (preWrite + 1) % (int) preDelay.size();

            float outL = 0.0f, outR = 0.0f;

            switch (type)
            {
                case Type::room:
                case Type::hall:   processFdn (delayed, inL - inR, outL, outR); break;
                case Type::plate:  processPlate (delayed, outL, outR); break;
                case Type::spring: processSprings (delayed, outL, outR); break;
            }

            if (switching)
            {
                fadeGain -= fadeStep;

                if (fadeGain <= 0.0f)
                {
                    fadeGain = 0.0f;
                    type = pendingType;
                    switching = false;
                    resetTank();
                    updateCoefficients();
                }
            }
            else if (fadeGain < 1.0f)
            {
                fadeGain = juce::jmin (1.0f, fadeGain + fadeStep);
            }

            // Width on the wet signal only: mid stays, side scales.
            const auto mid = 0.5f * (outL + outR);
            const auto side = 0.5f * (outL - outR) * widthAmount;

            left[i] = (mid + side) * fadeGain;

            if (right != nullptr)
                right[i] = (mid - side) * fadeGain;

            for (int ch = 2; ch < numChannels; ++ch)
                block.getChannelPointer ((size_t) ch)[i] = left[i];
        }
    }

    // For tests: the RT60 a line of this length gets.
    static float gainForRt60 (double lengthSamples, double rt60Seconds, double fs) noexcept
    {
        return (float) std::pow (10.0, -3.0 * lengthSamples / (rt60Seconds * fs));
    }

private:
    // ---- shared helpers ----

    static float readFractional (const std::vector<float>& buffer, int writeIndex, float delaySamples) noexcept
    {
        const auto size = (int) buffer.size();
        const auto d = juce::jlimit (0.0f, (float) (size - 2), delaySamples);
        const auto whole = (int) d;
        const auto frac = d - (float) whole;
        auto a = writeIndex - whole;
        if (a < 0) a += size;
        auto b = a - 1;
        if (b < 0) b += size;
        return buffer[(size_t) a] * (1.0f - frac) + buffer[(size_t) b] * frac;
    }

    static float readAt (const std::vector<float>& buffer, int writeIndex, int delaySamples) noexcept
    {
        const auto size = (int) buffer.size();
        auto i = writeIndex - juce::jlimit (0, size - 1, delaySamples);
        if (i < 0) i += size;
        return buffer[(size_t) i];
    }

    // One-pole absorption, per Jot: DC gain g, Nyquist gain g * r, where r
    // is the ratio the damping asks for over this line's length.
    struct Absorption { float gain = 0.0f; float pole = 0.0f; };

    Absorption absorptionFor (double lengthSamples) const noexcept
    {
        const auto rtLow = (double) rt60;
        const auto rtHigh = rtLow * (1.0 - 0.86 * (double) dampingAmount);
        const auto gDc = std::pow (10.0, -3.0 * lengthSamples / (rtLow * sampleRate));
        const auto gNy = std::pow (10.0, -3.0 * lengthSamples / (rtHigh * sampleRate));
        const auto ratio = juce::jlimit (0.0, 1.0, gNy / gDc);

        return { (float) gDc, (float) ((1.0 - ratio) / (1.0 + ratio)) };
    }

    void updateCoefficients()
    {
        // FDN: eight mutually-prime-ish lengths, scaled by the space and by
        // Size. A hall's lines are about three times a room's.
        const auto hall = type == Type::hall;
        const auto base = hall ? 0.042 : 0.013;                   // seconds, shortest line
        const auto sizeScale = 0.55 + 1.1 * (double) sizeAmount;  // 0.55x .. 1.65x

        for (size_t i = 0; i < 8; ++i)
        {
            fdnLength[i] = juce::jlimit (8.0, (double) fdn[i].size() - 4.0,
                                         base * fdnRatios[i] * sizeScale * sampleRate);
            fdnAbsorb[i] = absorptionFor (fdnLength[i]);
        }

        // Early reflections: a room's arrive close and loud, a hall's far
        // apart and quieter. Size spreads them out.
        const auto erSpan = (hall ? 0.080 : 0.028) * sizeScale;

        for (size_t i = 0; i < earlyTaps.size(); ++i)
            earlyDelay[i] = (int) juce::jlimit (1.0, (double) early.size() - 2.0, earlyTaps[i] * erSpan * sampleRate);

        earlyLevel = hall ? 0.35f : 0.8f;

        // Plate: the tank's length scales with Size; its loss per branch is
        // what gives the requested RT60.
        plateScale = (float) (sampleRate / 29761.0 * (0.6 + 0.7 * (double) sizeAmount));
        const auto branchSeconds = (plateTankLengths[0] + plateTankLengths[1] + plateTankLengths[2] + plateTankLengths[3])
                                   * (double) plateScale / sampleRate;
        // The loss is applied twice per branch (inside it and on the way
        // across to the other), so each takes half of the branch's share.
        plateDecay = (float) std::pow (10.0, -1.5 * branchSeconds / (double) rt60);
        plateDamp = 0.05f + 0.7f * dampingAmount;

        // Springs: transit time is the spring's length.
        for (size_t s = 0; s < springs.size(); ++s)
        {
            auto& spring = springs[s];
            spring.length = juce::jlimit (16.0, (double) spring.loop.size() - 4.0,
                                          (0.024 + 0.036 * (double) sizeAmount) * (s == 0 ? 1.0 : 1.17) * sampleRate);
            spring.feedback = gainForRt60 (spring.length, rt60, sampleRate);
            const auto cutoff = 7000.0 - 4500.0 * (double) dampingAmount;
            spring.lowpassCoeff = (float) std::exp (-juce::MathConstants<double>::twoPi * cutoff / sampleRate);
        }
    }

    // ---- Room / Hall ----

    void processFdn (float monoIn, float sideIn, float& outL, float& outR)
    {
        // Early reflections from a tapped line.
        early[(size_t) earlyWrite] = monoIn;
        float erL = 0.0f, erR = 0.0f;

        for (size_t i = 0; i < earlyTaps.size(); ++i)
        {
            const auto tap = readAt (early, earlyWrite, earlyDelay[i]) * earlyGains[i];
            (i % 2 == 0 ? erL : erR) += tap;
        }

        earlyWrite = (earlyWrite + 1) % (int) early.size();

        // Read the eight lines, with a slow wobble on two of them so the
        // tail does not ring at fixed modes - the metallic sound of a
        // static network.
        lfoPhase += 0.37 / sampleRate;
        if (lfoPhase > 1.0) lfoPhase -= 1.0;
        const auto wobble = (float) std::sin (juce::MathConstants<double>::twoPi * lfoPhase) * (float) (0.0006 * sampleRate);

        std::array<float, 8> y {};

        for (size_t i = 0; i < 8; ++i)
        {
            const auto length = (float) fdnLength[i] + (i == 1 ? wobble : i == 5 ? -wobble : 0.0f);
            auto v = readFractional (fdn[i], writePos, length);

            // Absorption: gain for the RT60, one pole for the damping.
            fdnLowpass[i] = (1.0f - fdnAbsorb[i].pole) * v + fdnAbsorb[i].pole * fdnLowpass[i];
            y[i] = fdnLowpass[i] * fdnAbsorb[i].gain;
        }

        // Householder feedback: x - (2/N) * sum(x). Lossless, fully mixing.
        float sum = 0.0f;
        for (auto v : y) sum += v;
        const auto mix = sum * (2.0f / 8.0f);

        const auto diffuseIn = monoIn * 0.35f + (erL + erR) * 0.15f;

        for (size_t i = 0; i < 8; ++i)
        {
            const auto input = diffuseIn + (i % 2 == 0 ? 0.08f : -0.08f) * sideIn;
            fdn[i][(size_t) writePos] = y[i] - mix + input;
        }

        writePos = (writePos + 1) % (int) fdn[0].size();

        outL = (y[0] - y[2] + y[4] - y[6]) * 0.5f + erL * earlyLevel;
        outR = (y[1] - y[3] + y[5] - y[7]) * 0.5f + erR * earlyLevel;
    }

    // ---- Plate (Dattorro) ----

    float allpass (std::vector<float>& buffer, int& write, int length, float g, float in) noexcept
    {
        const auto delayed = readAt (buffer, write, length);
        const auto v = in + g * delayed;
        buffer[(size_t) write] = v;
        write = (write + 1) % (int) buffer.size();
        return delayed - g * v;
    }

    float delay (std::vector<float>& buffer, int& write, int length, float in) noexcept
    {
        const auto out = readAt (buffer, write, length);
        buffer[(size_t) write] = in;
        write = (write + 1) % (int) buffer.size();
        return out;
    }

    void processPlate (float monoIn, float& outL, float& outR)
    {
        const auto len = [this] (double l) { return (int) std::round (l * (double) plateScale); };

        // Input bandwidth and four diffusers.
        plateBandwidth = 0.9995f * monoIn + 0.0005f * plateBandwidth;
        auto x = plateBandwidth;
        x = allpass (plateIn[0], plateInWrite[0], len (142), 0.75f, x);
        x = allpass (plateIn[1], plateInWrite[1], len (107), 0.75f, x);
        x = allpass (plateIn[2], plateInWrite[2], len (379), 0.625f, x);
        x = allpass (plateIn[3], plateInWrite[3], len (277), 0.625f, x);

        lfoPhase += 1.0 / sampleRate;
        if (lfoPhase > 1.0) lfoPhase -= 1.0;
        const auto excursion = (int) std::round (8.0 * (double) plateScale
                                                 * (1.0 + std::sin (juce::MathConstants<double>::twoPi * lfoPhase)));

        // The figure-eight: each branch feeds the other.
        const auto leftIn = x + plateDecay * plateTailR;
        const auto rightIn = x + plateDecay * plateTailL;

        // Left branch.
        auto l = allpass (plateTank[0], plateTankWrite[0], len (672) + excursion, -0.7f, leftIn);
        l = delay (plateTank[1], plateTankWrite[1], len (4453), l);
        plateLowpass[0] = (1.0f - plateDamp) * l + plateDamp * plateLowpass[0];
        l = plateLowpass[0] * plateDecay;
        l = allpass (plateTank[2], plateTankWrite[2], len (1800), 0.5f, l);
        plateTailL = delay (plateTank[3], plateTankWrite[3], len (3720), l);

        // Right branch.
        auto r = allpass (plateTank[4], plateTankWrite[4], len (908) + excursion / 2, -0.7f, rightIn);
        r = delay (plateTank[5], plateTankWrite[5], len (4217), r);
        plateLowpass[1] = (1.0f - plateDamp) * r + plateDamp * plateLowpass[1];
        r = plateLowpass[1] * plateDecay;
        r = allpass (plateTank[6], plateTankWrite[6], len (2656), 0.5f, r);
        plateTailR = delay (plateTank[7], plateTankWrite[7], len (3163), r);

        // Output taps from inside the tank, per the paper.
        const auto tap = [this, &len] (int line, double at) { return readAt (plateTank[(size_t) line], plateTankWrite[(size_t) line], len (at)); };

        outL = 0.6f * (tap (5, 266) + tap (5, 2974) - tap (6, 1913) + tap (7, 1996) - tap (1, 1990) - tap (2, 187) - tap (3, 1066));
        outR = 0.6f * (tap (1, 353) + tap (1, 3627) - tap (2, 1228) + tap (3, 2673) - tap (5, 2111) - tap (6, 335) - tap (7, 121));
    }

    // ---- Spring ----

    struct Spring
    {
        std::vector<float> loop;
        int writePos = 0;
        double length = 1000.0;
        float feedback = 0.5f;
        float lowpass = 0.0f;
        float lowpassCoeff = 0.5f;
        std::array<float, 40> allpassState {};
    };

    void processSprings (float monoIn, float& outL, float& outR)
    {
        for (size_t s = 0; s < springs.size(); ++s)
        {
            auto& spring = springs[s];

            // What comes back round the coil.
            auto v = readFractional (spring.loop, spring.writePos, (float) spring.length);

            // Dispersion: a long chain of first-order all-passes delays low
            // frequencies more than high ones, so every trip round the loop
            // arrives as a chirp rather than a copy.
            const auto a = s == 0 ? 0.62f : 0.58f;

            for (auto& state : spring.allpassState)
            {
                const auto out = state - a * v;
                state = v + a * out;
                v = out;
            }

            spring.lowpass = (1.0f - spring.lowpassCoeff) * v + spring.lowpassCoeff * spring.lowpass;

            spring.loop[(size_t) spring.writePos] = monoIn * 0.6f + spring.lowpass * spring.feedback;
            spring.writePos = (spring.writePos + 1) % (int) spring.loop.size();

            (s == 0 ? outL : outR) = spring.lowpass * 1.4f;
        }
    }

    // ---- state ----

    static constexpr std::array<double, 8> fdnRatios { 1.0, 1.1487, 1.3195, 1.4142, 1.5874, 1.7818, 1.9601, 2.1544 };
    static constexpr std::array<double, 8> earlyTaps { 0.043, 0.113, 0.197, 0.281, 0.389, 0.521, 0.683, 0.887 };
    static constexpr std::array<float, 8> earlyGains { 0.84f, 0.72f, 0.66f, 0.55f, 0.47f, 0.38f, 0.31f, 0.24f };
    static constexpr std::array<double, 4> plateInputLengths { 142, 107, 379, 277 };
    static constexpr std::array<double, 8> plateTankLengths { 672, 4453, 1800, 3720, 908, 4217, 2656, 3163 };

    double sampleRate = 44100.0;
    Type type = Type::room, pendingType = Type::room;
    bool switching = false;
    float fadeGain = 1.0f;

    bool coefficientsValid = false;
    float rt60 = 1.5f, sizeAmount = 0.5f, dampingAmount = 0.4f, widthAmount = 1.0f;

    std::vector<float> preDelay;
    int preWrite = 0;
    juce::SmoothedValue<float> preDelaySmoothed;

    std::array<std::vector<float>, 8> fdn;
    std::array<double, 8> fdnLength {};
    std::array<Absorption, 8> fdnAbsorb {};
    std::array<float, 8> fdnLowpass {};
    int writePos = 0;
    double lfoPhase = 0.0;

    std::vector<float> early;
    std::array<int, 8> earlyDelay {};
    int earlyWrite = 0;
    float earlyLevel = 0.6f;

    std::array<std::vector<float>, 4> plateIn;
    std::array<int, 4> plateInWrite {};
    std::array<std::vector<float>, 8> plateTank;
    std::array<int, 8> plateTankWrite {};
    std::array<float, 2> plateLowpass {};
    float plateBandwidth = 0.0f, plateTailL = 0.0f, plateTailR = 0.0f;
    float plateScale = 1.0f, plateDecay = 0.5f, plateDamp = 0.3f;

    std::array<Spring, 2> springs;
};
