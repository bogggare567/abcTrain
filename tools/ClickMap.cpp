// What can actually be pressed.
//
// EditorSnapshots answers "what does this look like". It cannot answer
// "what happens when I click there", and that turned out to be the more
// expensive question: a control can be visible in a snapshot and still be
// unreachable, because something transparent is lying on top of it, or it
// has been given empty bounds, or a parent has stopped intercepting mouse
// events. All three look identical in a picture, and all three read to a
// player as "the app is broken".
//
// So this walks the real editor's real component tree, and for every
// button, slider and combo box asks the editor itself the only question
// that matters: if a click landed in the middle of you, would you get it?
// Whatever answers instead is named in the report.
//
// It asserts nothing and returns non-zero only on a control that is on
// screen and cannot be reached - which is always a bug, never a design
// choice.

#include <juce_gui_extra/juce_gui_extra.h>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../shared/AbcTrainTheme.h"
#include "../shared/i18n/LocalisationManager.h"

#include <iostream>
#include <vector>

#if JUCE_MAC || JUCE_LINUX
 #include <cxxabi.h>
#endif

namespace
{
    juce::String classNameOf (const juce::Component& c)
    {
       #if JUCE_MAC || JUCE_LINUX
        int status = 0;
        if (char* d = abi::__cxa_demangle (typeid (c).name(), nullptr, nullptr, &status))
        {
            juce::String name (d);
            std::free (d);
            return name;
        }
       #endif
        return typeid (c).name();
    }

    // A label a person can find in the source. Prefer what is written on
    // the control, then whatever it was named, then its class.
    juce::String describe (const juce::Component& c)
    {
        juce::String text;

        if (auto* b = dynamic_cast<const juce::Button*> (&c))
            text = b->getButtonText();

        if (text.isEmpty())
            text = c.getName();

        if (text.isEmpty())
            text = c.getComponentID();

        auto label = classNameOf (c);

        if (text.isNotEmpty())
            label << " \"" << text << "\"";

        return label;
    }

    bool isInteractive (const juce::Component& c)
    {
        // The JUCE widgets, plus anything that handles its own mouse and
        // says so by carrying a component ID. There is no way to ask a
        // Component whether it overrides mouseDown, and the components
        // that do - the nav bar, the answer scale - are exactly the ones
        // worth checking, so they opt in by name.
        return dynamic_cast<const juce::Button*> (&c) != nullptr
            || dynamic_cast<const juce::Slider*> (&c) != nullptr
            || dynamic_cast<const juce::ComboBox*> (&c) != nullptr
            || dynamic_cast<const juce::TextEditor*> (&c) != nullptr
            || c.getComponentID().isNotEmpty();
    }

    void collect (juce::Component& c, std::vector<juce::Component*>& out)
    {
        for (auto* child : c.getChildren())
        {
            if (child->isVisible())
            {
                if (isInteractive (*child))
                    out.push_back (child);

                collect (*child, out);
            }
        }
    }

    // Everything between a component and the editor has to be showing and
    // has to let the event through, so an ancestor that stopped
    // intercepting clicks is reported against the child the player was
    // actually aiming at.
    bool isSelfOrAncestorOf (const juce::Component* maybeAncestor, const juce::Component* c)
    {
        for (auto* p = c; p != nullptr; p = p->getParentComponent())
            if (p == maybeAncestor)
                return true;

        return false;
    }

    struct Finding
    {
        juce::String control;
        juce::String reason;
        juce::String blocker;
    };

    // What this screen is supposed to be. A control that has stopped
    // responding is a bug in one state and the whole point in another, so
    // the difference has to be written down rather than eyeballed.
    enum class Expect
    {
        everythingLive,   // nothing on this screen may be dead
        runLocked         // a run is being played: most of it must be dead
    };

    int report (const juce::String& state, juce::Component& editor, Expect expect,
                 int allowedDead)
    {
        std::vector<juce::Component*> controls;
        collect (editor, controls);

        std::vector<Finding> faults;   // never acceptable
        std::vector<Finding> dead;     // disabled: right or wrong depending on the state
        int reachable = 0;
        int covered = 0;

        for (auto* c : controls)
        {
            const auto bounds = c->getBounds();

            // Zero-size is a real state in this codebase - several
            // controls are parked by giving them an empty rectangle
            // rather than by hiding them - so it is reported plainly
            // rather than treated as reachable or as a crash.
            if (bounds.isEmpty())
            {
                faults.push_back ({ describe (*c), "visible but has empty bounds", {} });
                continue;
            }

            const auto centre = editor.getLocalPoint (c, c->getLocalBounds().getCentre());

            if (! editor.getLocalBounds().contains (centre))
            {
                faults.push_back ({ describe (*c),
                                    "visible but its centre is outside the window at "
                                        + centre.toString(), {} });
                continue;
            }

            auto* hit = editor.getComponentAt (centre);

            if (hit != nullptr && isSelfOrAncestorOf (c, hit))
            {
                // Reaching the control is only half of it. A disabled
                // button still passes the hit test and still paints
                // itself in place - it simply ignores the click, which is
                // indistinguishable from a broken one unless something
                // says so. Same for a control faded to nothing.
                if (! c->isEnabled())
                {
                    dead.push_back ({ describe (*c), "disabled", {} });
                    continue;
                }

                if (c->getAlpha() < 0.02f)
                {
                    dead.push_back ({ describe (*c),
                                   "drawn at alpha " + juce::String (c->getAlpha(), 2), {} });
                    continue;
                }

                ++reachable;
                continue;
            }

            // A full-window overlay covering the screen beneath it is
            // what an overlay is for, so it is counted as covered rather
            // than as a fault. Anything else taking a click meant for
            // something else is a fault.
            const auto blockerName = hit != nullptr ? describe (*hit) : juce::String ("nothing");
            const auto blockerIsOverlay = hit != nullptr
                                       && hit->getParentComponent() == &editor
                                       && hit->getBounds() == editor.getLocalBounds();

            if (blockerIsOverlay)
            {
                ++covered;
                continue;
            }

            faults.push_back ({ describe (*c),
                                "on screen but a click at its centre goes elsewhere",
                                blockerName });
        }

        std::cout << "\n" << state << "\n"
                  << juce::String::repeatedString ("-", state.length()) << "\n"
                  << "  " << reachable << " live";

        if (! dead.empty())
            std::cout << ", " << dead.size() << " dead";

        if (covered > 0)
            std::cout << ", " << covered << " under an overlay";

        std::cout << "  (of " << controls.size() << ")\n";

        for (const auto& f : faults)
        {
            std::cout << "  BROKEN  " << f.control << "\n"
                      << "          " << f.reason << "\n";

            if (f.blocker.isNotEmpty())
                std::cout << "          it goes to: " << f.blocker << "\n";
        }

        int problems = (int) faults.size();

        // The rule, checked rather than described. Survival spends lives
        // and Blitz spends seconds, so a run has to be a closed room: if
        // the lock ever quietly stops being applied, this is what says so.
        if (expect == Expect::runLocked)
        {
            for (const auto& d : dead)
                std::cout << "  locked  " << d.control << "\n";

            if (dead.empty())
            {
                std::cout << "  BROKEN  a run is live and every control still responds\n"
                          << "          nothing but the answer should be pressable here\n";
                ++problems;
            }
        }
        else
        {
            // A control can be legitimately spent - the hint is gone for
            // the rest of a round once it has been used - so a case may
            // declare how many it expects. Any more than that is a bug,
            // and so is any fewer: a spent control that comes back is the
            // same broken promise seen from the other side.
            for (const auto& d : dead)
                std::cout << "  spent   " << d.control << "  (" << d.reason << ")\n";

            if ((int) dead.size() != allowedDead)
            {
                std::cout << "  BROKEN  expected " << allowedDead
                          << " control(s) to be dead here, found " << dead.size() << "\n";
                ++problems;
            }
        }

        return problems;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::ignoreUnused (argc, argv);

    // Same care EditorSnapshots takes: this constructs a real editor,
    // which reads and writes the player's own settings file. Put it aside
    // and give it back.
    const auto settings = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("abcTrain").getChildFile ("abcTrain.settings");
    const auto backup = settings.getSiblingFile ("abcTrain.settings.clickmap-backup");
    const auto hadSettings = settings.existsAsFile();

    if (hadSettings && ! settings.copyFileTo (backup))
    {
        std::cout << "couldn't back up " << settings.getFullPathName() << " - refusing to run\n";
        return 1;
    }

    int problems = 0;

    {
        AbcTrainTheme::setMode (AbcTrainTheme::Mode::dark);

        struct Case
        {
            const char* name;
            Expect expect;
            int allowedDead;   // controls legitimately spent in this state
            std::function<void (EarTrainerEditor&)> arrange;
        };

        const std::vector<Case> cases
        {
            { "Home", Expect::everythingLive, 0,                      [] (auto& e) { e.openHomeForSnapshot(); } },
            { "Training - Practice", Expect::everythingLive, 0,       [] (auto& e) { e.openTrainingForSnapshot (0); } },
            { "Training - answered", Expect::everythingLive, 0,       [] (auto& e) { e.openTrainingForSnapshot (0);
                                                          e.answerForSnapshot(); } },
            { "Training - Survival run", Expect::runLocked, 0,   [] (auto& e) { e.openTrainingForSnapshot (0);
                                                          e.startRunForSnapshot (SessionManager::Mode::survival); } },
            { "Training - Blitz run", Expect::runLocked, 0,      [] (auto& e) { e.openTrainingForSnapshot (0);
                                                          e.startRunForSnapshot (SessionManager::Mode::blitz); } },
            { "Training - hint shown", Expect::everythingLive, 1,     [] (auto& e) { e.openTrainingForSnapshot (0);
                                                          e.revealHintForSnapshot(); } },
            { "Run results", Expect::everythingLive, 0,               [] (auto& e) { e.showRunResultsForSnapshot(); } },
            { "Sounds", Expect::everythingLive, 0,                    [] (auto& e) { e.openSoundsForSnapshot(); } },
            { "Settings", Expect::everythingLive, 0,                  [] (auto& e) { e.openSettingsForSnapshot(); } },
            { "Achievements", Expect::everythingLive, 0,              [] (auto& e) { e.openAchievementsForSnapshot(); } },
        };

        for (const auto& c : cases)
        {
            EarTrainerProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            EarTrainerEditor editor (processor);

            // getComponentAt() starts by asking whether the component
            // itself is visible, and a top-level editor that was never
            // added to a window is not - so without this every control
            // reports as unreachable and the tool is a liar rather than a
            // check. EditorSnapshots never needed it because painting
            // does not consult the flag.
            editor.setVisible (true);

            editor.completeWelcomeReveal();
            editor.completeScreenFade();

            c.arrange (editor);

            editor.completeScreenFade();

            problems += report (c.name, editor, c.expect, c.allowedDead);
        }
    }

    if (hadSettings)
        backup.moveFileTo (settings);
    else
        settings.deleteFile();

    backup.deleteFile();

    std::cout << "\n" << (problems == 0
                              ? juce::String ("every screen behaves the way it says it does")
                              : juce::String (problems) + " problem(s)")
              << "\n";

    return problems == 0 ? 0 : 1;
}
