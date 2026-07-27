#include "WidgetStateRegistry.h"
#include "AbcTrainTheme.h"

namespace
{
    // 60 Hz. The rest of this project's visualisations run at 30 Hz, which
    // is plenty for a meter, but pointer-tracking feedback is the one place
    // where the difference between 30 and 60 is genuinely felt.
    constexpr int tickHz = 60;
    constexpr float frameMs = 1000.0f / (float) tickHz;

    // Below this, an animation is close enough to its target that another
    // repaint would be wasted work.
    constexpr float settleEpsilon = 0.004f;
}

WidgetStateRegistry::WidgetStateRegistry()
{
    startTimerHz (tickHz);
}

WidgetStateRegistry::~WidgetStateRegistry()
{
    stopTimer();
}

float WidgetStateRegistry::approach (float currentValue, float target, double durationMs) noexcept
{
    if (durationMs <= 0.0)
        return target;

    const auto step = frameMs / (float) durationMs;
    if (std::abs (target - currentValue) <= step)
        return target;

    return currentValue + (target > currentValue ? step : -step);
}

WidgetStateRegistry::Entry& WidgetStateRegistry::entryFor (juce::Component& component)
{
    for (auto& e : entries)
        if (e.component == &component)
            return e;

    // A linear scan is fine here: an editor has on the order of tens of
    // interactive widgets, and only ones that have actually been hovered
    // or pressed ever get an entry. A hash map would cost more in
    // bookkeeping than it saves.
    Entry fresh;
    fresh.component = &component;
    entries.push_back (fresh);
    return entries.back();
}

float WidgetStateRegistry::hoverAmount (juce::Component& component, bool isHoveredNow)
{
    auto& e = entryFor (component);
    e.hoverTarget = isHoveredNow ? 1.0f : 0.0f;
    return e.hover;
}

float WidgetStateRegistry::pressAmount (juce::Component& component, bool isPressedNow)
{
    auto& e = entryFor (component);
    e.pressTarget = isPressedNow ? 1.0f : 0.0f;
    return e.press;
}

float WidgetStateRegistry::trackValue (juce::Component& component, int slot, float target,
                                        double durationMs, bool snapNow)
{
    if (slot < 0 || slot >= numValueSlots)
        return target;

    auto& entry = entryFor (component);

    entry.valueTargets[slot] = target;
    entry.valueDurations[slot] = durationMs;

    // First sight of a widget is not a transition. Without this every knob
    // would sweep up from zero the moment its editor opened, which reads as
    // the plugin loading rather than as the value changing.
    if (snapNow || ! entry.valueStarted[slot])
    {
        entry.valueStarted[slot] = true;
        entry.values[slot] = target;
    }

    return entry.values[slot];
}

void WidgetStateRegistry::timerCallback()
{
    for (size_t i = 0; i < entries.size();)
    {
        auto& e = entries[i];

        if (e.component == nullptr)
        {
            // The widget (or its whole editor) was destroyed while we were
            // still animating it - drop the entry. SafePointer nulling is
            // exactly what makes this safe to do lazily rather than needing
            // every component to deregister itself.
            entries.erase (entries.begin() + (long) i);
            continue;
        }

        const auto previousHover = e.hover;
        const auto previousPress = e.press;

        e.hover = approach (e.hover, e.hoverTarget, AbcTrainTheme::Duration::hover);
        e.press = approach (e.press, e.pressTarget,
                            e.pressTarget > e.press ? AbcTrainTheme::Duration::press
                                                    : AbcTrainTheme::Duration::release);

        auto valueMoved = false;

        for (int slot = 0; slot < numValueSlots; ++slot)
        {
            if (! e.valueStarted[slot] || e.valueDurations[slot] <= 0.0)
                continue;

            const auto previous = e.values[slot];
            e.values[slot] = approach (e.values[slot], e.valueTargets[slot], e.valueDurations[slot]);

            // Tracked values are not all 0..1 - a knob position is, but a
            // frequency in Hz is not - so the settle test is relative to
            // how far the value still has to go.
            const auto span = std::abs (e.valueTargets[slot]) + 1.0f;
            valueMoved = valueMoved || std::abs (e.values[slot] - previous) > settleEpsilon * span;
        }

        if (std::abs (e.hover - previousHover) > settleEpsilon
            || std::abs (e.press - previousPress) > settleEpsilon
            || valueMoved)
        {
            e.component->repaint();
        }

        ++i;
    }
}
