#include "OwnPluginPlayer.h"
#include <cmath>
#include <thread>

OwnPluginPlayer::~OwnPluginPlayer()
{
    setRig (nullptr);
}

void OwnPluginPlayer::prepare (double sampleRate, int maximumBlockSize)
{
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    block = juce::jmax (32, maximumBlockSize);
    scratchA.setSize (maxChannels, block, false, true, false);
    scratchB.setSize (maxChannels, block, false, true, false);
    scratchDry.setSize (2, block, false, true, false);
    noMidi.ensureSize (256);
}

void OwnPluginPlayer::setRig (std::unique_ptr<Rig> next)
{
    auto* old = rig.exchange (next.release());

    if (old == nullptr)
        return;

    // Wait for the audio thread to leave the old rig: at most one block,
    // and never while it is stopped (inUse is cleared after every block).
    for (int i = 0; i < 400 && inUse.load() == old; ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));

    delete old;
}

void OwnPluginPlayer::process (juce::AudioBuffer<float>& buffer) noexcept
{
    auto* r = rig.load();
    inUse.store (r);

    if (r == nullptr || r->a == nullptr || r->b == nullptr || r->loop.getNumSamples() == 0)
    {
        buffer.clear();
        inUse.store (nullptr);
        return;
    }

    for (int start = 0; start < buffer.getNumSamples(); start += block)
        runChunk (*r, buffer, start, juce::jmin (block, buffer.getNumSamples() - start));

    inUse.store (nullptr);
}

void OwnPluginPlayer::runChunk (Rig& r, juce::AudioBuffer<float>& out, int start, int n) noexcept
{
    const auto chans = juce::jmax (2, r.a->getTotalNumInputChannels(), r.a->getTotalNumOutputChannels());

    if (chans > maxChannels)
    {
        out.clear (start, n);
        return;
    }

    // The loop, into the dry buffer and both plugin buffers.
    const auto loopLen = r.loop.getNumSamples();
    for (int done = 0; done < n;)
    {
        const auto take = juce::jmin (n - done, loopLen - loopPos);
        for (int ch = 0; ch < 2; ++ch)
            scratchDry.copyFrom (ch, done, r.loop, juce::jmin (ch, r.loop.getNumChannels() - 1), loopPos, take);
        done += take;
        loopPos = (loopPos + take) % loopLen;
    }

    for (int ch = 0; ch < chans; ++ch)
    {
        scratchA.copyFrom (ch, 0, scratchDry, juce::jmin (ch, 1), 0, n);
        scratchB.copyFrom (ch, 0, scratchDry, juce::jmin (ch, 1), 0, n);
    }

    // Views over the first n samples: no allocation (AudioBuffer keeps up
    // to 32 channel pointers in place).
    juce::AudioBuffer<float> viewA (scratchA.getArrayOfWritePointers(), chans, n);
    juce::AudioBuffer<float> viewB (scratchB.getArrayOfWritePointers(), chans, n);
    noMidi.clear();
    r.a->processBlock (viewA, noMidi);
    noMidi.clear();
    r.b->processBlock (viewB, noMidi);

    // Loudness followers over at least the whole loop: a shorter window
    // follows the beat instead of the level, and a long reverb decay then
    // came out 5 dB louder than A (OwnPluginSmoke on ValhallaRoom).
    const auto window = juce::jmax (1.5, (double) r.loop.getNumSamples() / rate);
    const auto coeff = (float) std::exp (-(double) n / (window * rate));
    auto sumA = 0.0f, sumB = 0.0f, sumD = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* a = viewA.getReadPointer (ch);
        const auto* b = viewB.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            sumA += a[i] * a[i];
            sumB += b[i] * b[i];
            sumD += (a[i] - b[i]) * (a[i] - b[i]);
        }
    }
    meanSqA = coeff * meanSqA + (1.0f - coeff) * (sumA / (float) (2 * n));
    meanSqB = coeff * meanSqB + (1.0f - coeff) * (sumB / (float) (2 * n));

    // The round check measures on its own, faster window (0.3 s), restarted
    // when a round begins: the matching follower above spans the loop and
    // would still be half-way to the new level when the check reads it.
    const auto quick = (float) std::exp (-(double) n / (0.3 * rate));
    const auto inA = sumA / (float) (2 * n), inB = sumB / (float) (2 * n), inD = sumD / (float) (2 * n);
    if (resetDiff.exchange (false))
    {
        meanSqDiff = inD; checkSqA = inA; checkSqB = inB;
    }
    else
    {
        meanSqDiff = quick * meanSqDiff + (1.0f - quick) * inD;
        checkSqA = quick * checkSqA + (1.0f - quick) * inA;
        checkSqB = quick * checkSqB + (1.0f - quick) * inB;
    }

    differenceDb.store (10.0f * std::log10 ((meanSqDiff + 1.0e-20f) / (checkSqA + 1.0e-20f)));
    levelGapDb.store (10.0f * std::log10 ((checkSqB + 1.0e-20f) / (checkSqA + 1.0e-20f)));

    const auto wantMatch = match.load() && meanSqA > 1.0e-9f && meanSqB > 1.0e-9f
                               ? juce::jlimit (0.25f, 4.0f, std::sqrt (meanSqA / meanSqB))   // ±12 dB at most
                               : 1.0f;

    // 25 ms crossfade towards the chosen output.
    const auto target = (Listen) listen.load();
    const auto step = (float) n / (float) (0.025 * rate);
    const auto approach = [step] (float& g, float t) { g = g < t ? juce::jmin (t, g + step) : juce::jmax (t, g - step); };

    const auto a0 = gainA, b0 = gainB, d0 = gainDry, m0 = matchGain;
    approach (gainA, target == Listen::a ? 1.0f : 0.0f);
    approach (gainB, target == Listen::b ? 1.0f : 0.0f);
    approach (gainDry, target == Listen::dry ? 1.0f : 0.0f);
    matchGain += (wantMatch - matchGain) * juce::jmin (1.0f, step);

    for (int ch = 0; ch < out.getNumChannels(); ++ch)
    {
        auto* dst = out.getWritePointer (ch, start);
        const auto src = juce::jmin (ch, 1);
        const auto* a = viewA.getReadPointer (src);
        const auto* b = viewB.getReadPointer (src);
        const auto* d = scratchDry.getReadPointer (src);

        for (int i = 0; i < n; ++i)
        {
            const auto t = (float) i / (float) n;
            const auto ga = a0 + (gainA - a0) * t;
            const auto gb = (b0 + (gainB - b0) * t) * (m0 + (matchGain - m0) * t);
            const auto gd = d0 + (gainDry - d0) * t;
            dst[i] = ga * a[i] + gb * b[i] + gd * d[i];
        }
    }
}
