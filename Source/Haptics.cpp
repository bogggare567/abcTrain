#include "Haptics.h"

#include <juce_core/juce_core.h>

// The iOS build compiles Haptics_ios.mm instead (CMakeLists.txt).
#if ! JUCE_IOS
namespace Haptics
{
    void tick (int) {}
}
#endif
