#include "UiSounds.h"

namespace
{
    constexpr float pitchCents = 35.0f;
    constexpr float levelJitterDb = 1.5f;
    constexpr double burstWindowS = 1.5;
    constexpr float burstStepDb = 2.0f;
    constexpr float burstFloorDb = -8.0f;
}

const char* UiSounds::idOf (Event e)
{
    switch (e)
    {
        case Event::correct:     return "correct";
        case Event::wrong:       return "wrong";
        case Event::stepUp:      return "step-up";
        case Event::newRecord:   return "new-record";
        case Event::achievement: return "achievement";
        case Event::runEnd:      return "run-end";
        case Event::battleWon:   return "battle-won";
        case Event::battleLost:  return "battle-lost";
        case Event::roundStart:  return "round-start";
        case Event::open:        return "open";
        case Event::numEvents:   break;
    }
    return "";
}

int UiSounds::eventFromId (const juce::String& id)
{
    for (int i = 0; i < numEvents; ++i)
        if (id == idOf ((Event) i))
            return i;
    return -1;
}

UiSounds::UiSounds()
{
    formats.registerBasicFormats();
    lastTake.fill (-1);
    lastTime.fill (-1.0e9);
}

void UiSounds::addTake (Event e, const void* data, size_t size)
{
    auto stream = std::make_unique<juce::MemoryInputStream> (data, size, false);
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (std::move (stream)));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return;

    Take t;
    t.originalRate = reader->sampleRate;
    t.original.setSize ((int) juce::jmin (2u, reader->numChannels), (int) reader->lengthInSamples);
    reader->read (&t.original, 0, (int) reader->lengthInSamples, 0, true, t.original.getNumChannels() > 1);
    takes[(size_t) e].push_back (std::move (t));
}

void UiSounds::prepare (double sampleRate)
{
    deviceRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (auto& list : takes)
        for (auto& t : list)
        {
            const auto ratio = t.originalRate / deviceRate;
            const auto outLength = (int) std::ceil (t.original.getNumSamples() / ratio);
            t.ready.setSize (2, outLength);
            t.ready.clear();

            for (int ch = 0; ch < 2; ++ch)
            {
                juce::LagrangeInterpolator interpolator;
                const auto src = juce::jmin (ch, t.original.getNumChannels() - 1);
                interpolator.process (ratio, t.original.getReadPointer (src), t.ready.getWritePointer (ch), outLength);
            }
        }

    for (auto& v : voices)
        v.buffer = nullptr;
}

void UiSounds::trigger (Event e)
{
    const auto i = (size_t) e;
    if (! enabled.load() || takes[i].empty())
        return;

    // A take, never the last one for this event (when there is a choice).
    const auto n = (int) takes[i].size();
    auto take = random.nextInt (n);
    if (n > 1 && take == lastTake[i])
        take = (take + 1 + random.nextInt (n - 1)) % n;
    lastTake[i] = take;

    // A burst of the same event fades back, so ten right answers in a row
    // do not hammer the same spot.
    const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    burst[i] = now - lastTime[i] < burstWindowS ? burst[i] + 1 : 0;
    lastTime[i] = now;

    const auto db = levelDb.load()
                  + (random.nextFloat() * 2.0f - 1.0f) * levelJitterDb
                  + juce::jmax (burstFloorDb, -burstStepDb * (float) burst[i]);
    const auto cents = (random.nextFloat() * 2.0f - 1.0f) * pitchCents;

    Shot shot { (int) e, take, juce::Decibels::decibelsToGain (db), std::pow (2.0f, cents / 1200.0f) };
    last = shot;

    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
        queue[(size_t) scope.startIndex1] = shot;
}

void UiSounds::render (juce::AudioBuffer<float>& buffer) noexcept
{
    // Start whatever was triggered since the last block.
    {
        const auto scope = fifo.read (fifo.getNumReady());
        const auto start = [this] (int index)
        {
            const auto& shot = queue[(size_t) index];
            const auto& list = takes[(size_t) shot.event];
            if (shot.take >= (int) list.size() || list[(size_t) shot.take].ready.getNumSamples() == 0)
                return;

            // A free voice, or the one nearest its end.
            auto* best = &voices[0];
            for (auto& v : voices)
            {
                if (v.buffer == nullptr) { best = &v; break; }
                if (v.pos > best->pos) best = &v;
            }
            *best = { &list[(size_t) shot.take].ready, 0.0, shot.gain, shot.rate };
        };

        for (int k = 0; k < scope.blockSize1; ++k) start (scope.startIndex1 + k);
        for (int k = 0; k < scope.blockSize2; ++k) start (scope.startIndex2 + k);
    }

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    for (auto& v : voices)
    {
        if (v.buffer == nullptr)
            continue;

        const auto length = v.buffer->getNumSamples();
        const auto* l = v.buffer->getReadPointer (0);
        const auto* r = v.buffer->getReadPointer (1);

        for (int s = 0; s < numSamples; ++s)
        {
            const auto i0 = (int) v.pos;
            if (i0 + 1 >= length)
            {
                v.buffer = nullptr;
                break;
            }

            const auto frac = (float) (v.pos - i0);
            const auto left = (l[i0] + (l[i0 + 1] - l[i0]) * frac) * v.gain;
            const auto right = (r[i0] + (r[i0 + 1] - r[i0]) * frac) * v.gain;

            if (numChannels > 0) buffer.addSample (0, s, left);
            if (numChannels > 1) buffer.addSample (1, s, right);
            v.pos += v.rate;
        }
    }
}
