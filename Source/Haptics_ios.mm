#include "Haptics.h"

#import <UIKit/UIKit.h>

namespace Haptics
{
    void tick (int strength)
    {
        if (strength <= 0)
            return;

        // Kept alive between calls: a generator made per click misses the
        // first ones while the Taptic Engine wakes up.
        static UIImpactFeedbackGenerator* light = [[UIImpactFeedbackGenerator alloc] initWithStyle: UIImpactFeedbackStyleLight];
        static UIImpactFeedbackGenerator* medium = [[UIImpactFeedbackGenerator alloc] initWithStyle: UIImpactFeedbackStyleMedium];

        auto* generator = strength >= 2 ? medium : light;
        [generator impactOccurred];
        [generator prepare];
    }
}
