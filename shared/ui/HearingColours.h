#pragma once

#include "shared/ui/AbcTrainTheme.h"

// The two ears, in the audiogram's own colours (ADR 051): left blue,
// right red - the convention every audiogram since the 1940s uses, so the
// test's result page and the line on Learner EQ read the way a chart from
// a clinic would. A shade per theme so both carry on dark and on light.
namespace HearingColours
{
    inline juce::Colour left()
    {
        return AbcTrainTheme::getMode() == AbcTrainTheme::Mode::dark ? juce::Colour (0xff6fa8ff) : juce::Colour (0xff2c64c4);
    }

    inline juce::Colour right()
    {
        return AbcTrainTheme::getMode() == AbcTrainTheme::Mode::dark ? juce::Colour (0xffff7d6e) : juce::Colour (0xffc0392b);
    }
}
