#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <chrono>
#include <cstdio>
#include "../Source/Games/ReverbGame.h"
#include "../Source/Games/DelayGame.h"
#include "../Source/Games/EQGame.h"
#include "../Source/Games/CompressionGame.h"
#include "../Source/Games/DistortionGame.h"
#include "../Source/Games/FrequencyRangeGame.h"

template <typename G>
static void bench (const char* name)
{
    G game;
    juce::dsp::ProcessSpec spec { 48000.0, 512, 2 };
    game.prepare (spec);

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; ++i)
        game.newRound();
    const auto ms = std::chrono::duration<double, std::milli> (
                        std::chrono::steady_clock::now() - t0).count();

    std::printf ("%-20s %8.2f ms per newRound()\n", name, ms / 20.0);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    bench<ReverbGame> ("ReverbGame");
    bench<DelayGame> ("DelayGame");
    bench<CompressionGame> ("CompressionGame");
    bench<DistortionGame> ("DistortionGame");
    bench<EQGame> ("EQGame");
    bench<FrequencyRangeGame> ("FrequencyRangeGame");
    return 0;
}
