#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>

// Scrolling the way a finger expects it (ADR 060): the list follows the
// finger, keeps going with the speed it was let go at and slows down on
// its own, and stretches a little past either end before settling back.
//
// The owner keeps drawing from its own offset; this only decides what the
// offset is. Wire mouseDown / mouseDrag / mouseUp / mouseWheelMove through
// and set onChange to store the offset and repaint.
class TouchScroll : private juce::Timer
{
public:
    std::function<void (float)> onChange;

    void setLimit (float newMaximum)
    {
        maximum = juce::jmax (0.0f, newMaximum);

        if (! isTimerRunning() && ! dragging && (offset < 0.0f || offset > maximum))
            set (juce::jlimit (0.0f, maximum, offset));
    }

    float getLimit() const noexcept { return maximum; }
    float getOffset() const noexcept { return offset; }

    // A jump from code (a new page, a reset): no motion carried over.
    void setOffset (float newOffset)
    {
        stopTimer();
        velocity = 0.0f;
        set (juce::jlimit (0.0f, maximum, newOffset));
    }

    void mouseDown (const juce::MouseEvent& e)
    {
        stopTimer();
        velocity = 0.0f;
        dragging = false;
        startOffset = offset;
        lastY = (float) e.getScreenY();
        lastTime = juce::Time::getMillisecondCounterHiRes();
    }

    void mouseDrag (const juce::MouseEvent& e)
    {
        // Under the click threshold it is still a tap on a row.
        if (e.mouseWasClicked())
            return;

        dragging = true;
        auto target = startOffset - (float) e.getDistanceFromDragStartY();

        // Past an end the list moves a third as far as the finger does.
        if (target < 0.0f)
            target *= rubber;
        else if (target > maximum)
            target = maximum + (target - maximum) * rubber;

        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto y = (float) e.getScreenY();
        const auto dt = (float) juce::jmax (1.0, now - lastTime) / 1000.0f;

        // Smoothed, so one jittery sample does not decide the fling.
        velocity = 0.6f * ((lastY - y) / dt) + 0.4f * velocity;
        lastY = y;
        lastTime = now;

        set (target);
    }

    void mouseUp (const juce::MouseEvent&)
    {
        if (! dragging)
            return;

        dragging = false;

        // A finger held still before lifting means "stop here".
        if (juce::Time::getMillisecondCounterHiRes() - lastTime > 90.0)
            velocity = 0.0f;

        velocity = juce::jlimit (-maxVelocity, maxVelocity, velocity);
        lastTick = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }

    bool wasDragged() const noexcept { return dragging; }

    void mouseWheelMove (const juce::MouseWheelDetails& wheel, float pixelsPerUnit)
    {
        stopTimer();
        velocity = 0.0f;
        set (juce::jlimit (0.0f, maximum, offset - wheel.deltaY * pixelsPerUnit));
    }

private:
    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto dt = (float) juce::jlimit (0.001, 0.05, (now - lastTick) / 1000.0);
        lastTick = now;

        auto next = offset + velocity * dt;

        if (next < 0.0f || next > maximum)
        {
            // Out past an end: a spring pulls it back, and the fling dies.
            const auto edge = next < 0.0f ? 0.0f : maximum;
            velocity *= std::pow (0.02f, dt);
            next += (edge - next) * juce::jmin (1.0f, dt * 12.0f);

            if (std::abs (next - edge) < 0.5f && std::abs (velocity) < 40.0f)
            {
                stopTimer();
                velocity = 0.0f;
                next = edge;
            }
        }
        else
        {
            // Friction: about a second and a half from a fast fling to rest.
            velocity *= std::pow (0.05f, dt);

            if (std::abs (velocity) < 12.0f)
            {
                stopTimer();
                velocity = 0.0f;
            }
        }

        set (next);
    }

    void set (float newOffset)
    {
        if (std::abs (newOffset - offset) < 0.01f)
            return;

        offset = newOffset;

        if (onChange != nullptr)
            onChange (offset);
    }

    static constexpr float rubber = 0.35f;
    static constexpr float maxVelocity = 6000.0f;

    float offset = 0.0f, maximum = 0.0f, startOffset = 0.0f;
    float velocity = 0.0f, lastY = 0.0f;
    double lastTime = 0.0, lastTick = 0.0;
    bool dragging = false;
};
