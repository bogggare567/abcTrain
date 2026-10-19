#pragma once

// A click under the finger (ADR 062): one per step crossed on a scale.
// Strength 0 is off, 1 light, 2 strong - the "Vibration on scales"
// setting. Only a phone or a tablet has anything to click; elsewhere it
// does nothing.
namespace Haptics
{
    void tick (int strength);
}
