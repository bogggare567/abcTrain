// RoomDemo - a local seminar room with scripted rounds and no app around it,
// for checking the phone page in a real browser (tools/room_demo_check.mjs,
// Playwright) or on a real phone on the same Wi-Fi.
//
//   ./RoomDemo [port] [seconds]
//
// Phase every few seconds: lobby -> a discrete round -> its answer -> a
// continuous round -> its answer -> finished.
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <iostream>
#include "../Source/LocalRoom.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto port = argc > 1 ? juce::String (argv[1]).getIntValue() : 8930;
    const auto seconds = argc > 2 ? juce::String (argv[2]).getIntValue() : 60;

    LocalRoom room;
    room.configure ("Demo room", false, {});
    juce::String error;

    if (! room.open (port, error))
    {
        std::cerr << "cannot open: " << error << "\n";
        return 1;
    }

    std::cout << "PORT " << room.getPort() << " ROOM " << room.getRoomCode() << std::endl;
    const auto start = juce::Time::getMillisecondCounter();
    int step = 0;

    while (juce::Time::getMillisecondCounter() - start < (juce::uint32) seconds * 1000)
    {
        const auto t = (int) ((juce::Time::getMillisecondCounter() - start) / 1000);
        const auto want = t / 6;   // a new phase every six seconds

        if (want > step)
        {
            step = want;
            LocalRoom::Question q;
            q.totalRounds = 2;

            if (step == 1) { q.round = 1; q.exercise = "Guess the Reverb"; q.prompt = "Which space is this?"; q.choices = { "Room", "Hall", "Plate", "Spring" }; room.ask (q); }
            if (step == 2) room.reveal ({ 2, -1.0f, "Plate" });
            if (step == 3)
            {
                q.round = 2; q.exercise = "Guess the Band"; q.prompt = "Where is the boost?"; q.continuous = true; q.tolerance = 0.06f;
                for (int i = 0; i <= 100; ++i) q.scaleLabels.add (juce::String (juce::roundToInt (20.0 * std::pow (1000.0, i / 100.0))) + " Hz");
                q.marks = { { 0.0f, "20" }, { 0.333f, "200" }, { 0.667f, "2k" }, { 1.0f, "20k" } };
                room.ask (q);
            }
            if (step == 4) room.reveal ({ -1, 0.55f, "900 Hz" });
            if (step == 5) room.finish();
            std::cout << "STEP " << step << std::endl;
        }

        juce::Thread::sleep (100);
    }

    room.close();
    return 0;
}
