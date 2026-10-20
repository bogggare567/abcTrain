#include <cstdlib>
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
#include "../Source/LiveLink.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/i18n/LocalisationManager.h"

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

    // The window a screen is checked in. The layout changes shape at
    // three widths and one height (EarTrainerEditor::compactLayout,
    // sideways), and every one of those shapes has broken on its own while
    // the design size kept passing - "each fix makes a new problem
    // somewhere else" was exactly that: one size was checked, four were
    // shipped. A phone is the snapshot tools' phone: SNAP_SAFE sets the
    // insets (top, left, bottom, right) and switches the editor into touch
    // mode, see EarTrainerEditor::isPhone.
    struct Window
    {
        const char* name;
        int width, height;
        const char* safe;   // nullptr: a desktop window
    };

    const std::vector<Window>& windows()
    {
        static const std::vector<Window> all {
            { "desktop",          1180, 880, nullptr },
            { "desktop, smallest", 940, 620, nullptr },
            { "iPhone",            393, 852, "59,0,34,0" },
            { "iPhone sideways",   852, 393, "0,59,21,59" },
            { "iPhone SE",         375, 667, "20,0,0,0" },
        };
        return all;
    }

    // Apple's minimum for something a finger presses. Slightly under is
    // survivable and gets a note; under minimumTouch is a control people
    // will miss, and is a fault.
    constexpr int comfortableTouch = 44;
    constexpr int minimumTouch = 30;

    // The part of a component nothing above it has cut off - what is
    // actually on screen - in the editor's coordinates.
    juce::Rectangle<int> visiblePart (juce::Component& editor, juce::Component& c)
    {
        auto area = editor.getLocalArea (&c, c.getLocalBounds());

        for (auto* p = c.getParentComponent(); p != nullptr && p != &editor; p = p->getParentComponent())
            area = area.getIntersection (editor.getLocalArea (p, p->getLocalBounds()));

        return area.getIntersection (editor.getLocalBounds());
    }

    // ---- what a screen costs when nobody touches it (--frames) ---------
    //
    // A phone that "terribly lags" is a phone repainting far more than it
    // shows. Two numbers per screen: what one full frame costs to draw
    // through the platform renderer at a phone's 3x (CoreGraphics on a Mac,
    // the same engine JUCE uses on iOS), and how much of the window asks to
    // be repainted per second while the screen sits idle. The product is
    // the screen's idle bill.
    //
    // The second is counted without a window: JUCE hands every repaint()
    // of a component or any of its children to the cached image of the
    // nearest ancestor that has one, so a cache on the editor that only
    // counts sees all of them.
    struct CountingCache : public juce::CachedComponentImage
    {
        explicit CountingCache (juce::Component& c) : owner (c) {}
        juce::Component& owner;
        double area = 0.0;
        int calls = 0;

        void paint (juce::Graphics&) override {}
        bool invalidateAll() override
        {
            ++calls;
            area += (double) owner.getWidth() * (double) owner.getHeight();
            return true;
        }
        bool invalidate (const juce::Rectangle<int>& r) override
        {
            ++calls;
            area += (double) r.getWidth() * (double) r.getHeight();
            return true;
        }
        void releaseResources() override {}
    };

    struct FrameCost
    {
        double frameMs = 0.0;     // one full frame at 3x
        double repaintsPerSecond = 0.0;
        double screensPerSecond = 0.0;   // repainted area / window area
    };

    FrameCost measureFrames (juce::Component& editor)
    {
        FrameCost cost;
        constexpr float scale = 3.0f;
        const auto w = editor.getWidth(), h = editor.getHeight();
        juce::Image image (juce::Image::ARGB, (int) (w * scale), (int) (h * scale), true, juce::NativeImageType());

        // FRAME_LOOP=n: paint n frames, for a profiler to attach to.
        const auto loops = juce::jmax (6, juce::String (std::getenv ("FRAME_LOOP")).getIntValue());

        // One untimed frame for the caches (fonts, the background image).
        for (int i = 0; i < loops; ++i)
        {
            const auto start = juce::Time::getMillisecondCounterHiRes();
            {
                juce::Graphics g (image);
                g.addTransform (juce::AffineTransform::scale (scale));
                editor.paintEntireComponent (g, true);
            }
            if (i > 0)
                cost.frameMs += (juce::Time::getMillisecondCounterHiRes() - start) / (loops - 1);
        }

        auto* counter = new CountingCache (editor);
        editor.setCachedComponentImage (counter);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);   // let entry animations settle
        counter->area = 0.0;
        counter->calls = 0;
        constexpr int windowMs = 2000;
        juce::MessageManager::getInstance()->runDispatchLoopUntil (windowMs);
        cost.repaintsPerSecond = counter->calls * 1000.0 / windowMs;
        cost.screensPerSecond = counter->area / ((double) w * h) * 1000.0 / windowMs;
        editor.setCachedComponentImage (nullptr);
        return cost;
    }

    int report (const juce::String& state, juce::Component& editor, Expect expect,
                 int allowedDead, const Window& window, juce::Rectangle<int> safeArea)
    {
        std::vector<juce::Component*> controls;
        collect (editor, controls);

        std::vector<Finding> faults;   // never acceptable
        std::vector<Finding> dead;     // disabled: right or wrong depending on the state
        std::vector<juce::Component*> live;   // reachable, for the overlap check below
        int reachable = 0;
        int covered = 0;
        int parked = 0;

        for (auto* c : controls)
        {
            const auto bounds = c->getBounds();

            // Zero-size is a real state in this codebase - several
            // controls are parked by giving them an empty rectangle
            // rather than by hiding them - so it is reported plainly
            // rather than treated as reachable or as a crash.
            //
            // Parked, then, not broken: nothing is drawn and nothing can be
            // pressed. It used to count as a fault, and with the mode pills
            // and Answer parked on purpose since ADR 061/062 every desktop
            // screen "failed" - a check that always fails is a check nobody
            // reads, which is how the phone sizes went unchecked. A control
            // that ought to be on screen and is parked shows up in the
            // snapshots, and as a missing control in the live counts below.
            if (bounds.isEmpty())
            {
                ++parked;
                continue;
            }

            // A page that scrolls clips its children to itself: what is
            // checked is the part of the control actually showing. Nothing
            // showing is scrolled out of view, which is neither reachable
            // nor broken.
            const auto shown = visiblePart (editor, *c);

            if (shown.isEmpty() && c->getParentComponent() != &editor)
            {
                ++parked;
                continue;
            }

            const auto centre = shown.isEmpty() ? editor.getLocalPoint (c, c->getLocalBounds().getCentre())
                                                : shown.getCentre();

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
                    // On a touch screen Answer is dead until a finger has
                    // placed something on the scale (ADR 062) - by design,
                    // and in every training state, so not counted.
                    if (c->getComponentID() == "training.answer")
                        continue;

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
                live.push_back (c);
                continue;
            }

            // A full-window overlay covering the screen beneath it is
            // what an overlay is for, so it is counted as covered rather
            // than as a fault. Anything else taking a click meant for
            // something else is a fault.
            const auto blockerName = hit != nullptr ? describe (*hit) : juce::String ("nothing");
            // On a phone an overlay stops at the safe area rather than at
            // the window's edge, and is just as much an overlay.
            // The overlay is the editor's child the hit belongs to - the
            // click often lands on one of the overlay's own buttons.
            auto* top = hit;
            while (top != nullptr && top->getParentComponent() != &editor)
                top = top->getParentComponent();

            const auto blockerIsOverlay = top != nullptr
                                       && ! isSelfOrAncestorOf (top, c)
                                       && (top->getBounds() == editor.getLocalBounds()
                                           || top->getBounds().contains (safeArea));

            if (blockerIsOverlay)
            {
                ++covered;
                continue;
            }

            faults.push_back ({ describe (*c),
                                "on screen but a click at its centre goes elsewhere",
                                blockerName });
        }

        // On a phone: inside the safe area, and big enough for a finger.
        std::vector<Finding> notes;

        if (window.safe != nullptr)
        {
            for (auto* c : live)
            {
                const auto shown = visiblePart (editor, *c);

                if (shown.isEmpty())
                    continue;

                if (! safeArea.contains (shown) && ! (bool) c->getProperties()["extendsUnderSystemBars"])
                {
                    const auto under = shown.getIntersection (safeArea);
                    faults.push_back ({ describe (*c),
                                        "reaches under the notch, the rounded corner or the home bar: "
                                            + shown.toString() + " against the safe area " + safeArea.toString(),
                                        under.isEmpty() ? juce::String ("entirely outside it") : juce::String() });
                }

                // Sliders and the answer scale are measured along their
                // length; a slider is a thin thing on purpose.
                if (dynamic_cast<juce::Slider*> (c) != nullptr)
                    continue;

                const auto w = c->getWidth(), h = c->getHeight();

                if (w < minimumTouch || h < minimumTouch)
                    faults.push_back ({ describe (*c), "too small to press: " + juce::String (w) + "x" + juce::String (h)
                                                        + " (a finger needs " + juce::String (comfortableTouch) + ")", {} });
                else if (w < comfortableTouch || h < comfortableTouch)
                    notes.push_back ({ describe (*c), juce::String (w) + "x" + juce::String (h), {} });
            }
        }

        // Two live controls on top of each other. The centre hit test
        // cannot see this when both centres stay clear - which is how the
        // seminar's host controls sat on top of the open room (2026-09-24)
        // with every click still landing somewhere.
        for (size_t i = 0; i < live.size(); ++i)
            for (size_t j = i + 1; j < live.size(); ++j)
            {
                auto* a = live[i];
                auto* b = live[j];

                if (isSelfOrAncestorOf (a, b) || isSelfOrAncestorOf (b, a))
                    continue;

                const auto ra = visiblePart (editor, *a);
                const auto rb = visiblePart (editor, *b);
                const auto overlap = ra.getIntersection (rb);

                if (overlap.getWidth() > 3 && overlap.getHeight() > 3)
                    faults.push_back ({ describe (*a), "overlaps another control by "
                                            + juce::String (overlap.getWidth()) + "x" + juce::String (overlap.getHeight()),
                                        describe (*b) });
            }

        std::cout << "\n" << state << "\n"
                  << juce::String::repeatedString ("-", state.length()) << "\n"
                  << "  " << reachable << " live";

        if (! dead.empty())
            std::cout << ", " << dead.size() << " dead";

        if (covered > 0)
            std::cout << ", " << covered << " under an overlay";

        if (parked > 0)
            std::cout << ", " << parked << " parked";

        std::cout << "  (of " << controls.size() << ")\n";

        for (const auto& n : notes)
            std::cout << "  small   " << n.control << "  " << n.reason << "\n";

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
    LiveLink::networkAllowed = false;   // no network from a test tool

    // Always the design size, whatever the virtual display is (shared/ui/WindowFit.h).
   #if JUCE_WINDOWS
    _putenv_s ("ABC_DESIGN_SIZE", "1");   // Windows has no setenv (CI caught it)
   #else
    setenv ("ABC_DESIGN_SIZE", "1", 1);
   #endif

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto framesOnly = argc > 1 && juce::String (argv[1]) == "--frames";

    // Same care EditorSnapshots takes: this constructs a real editor,
    // which reads and writes the player's own settings file. Put it aside
    // and give it back.
    const auto settings = LocalisationManager::makeDefaultOptions().getDefaultFile();
    const auto backup = settings.getSiblingFile ("abcTrain.settings.clickmap-backup");
    const auto hadSettings = settings.existsAsFile();

    if (hadSettings && ! settings.copyFileTo (backup))
    {
        std::cout << "couldn't back up " << settings.getFullPathName() << " - refusing to run\n";
        return 1;
    }

    // The progress file (rounds, levels, records) is ProgressManager's own,
    // apart from the settings above: clicking through a round answers it.
    const auto progressFile = []
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainer";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainer";
        options.osxLibrarySubFolder = "Application Support";
        return options.getDefaultFile();
    }();
    const auto progressExisted = progressFile.existsAsFile();
    const auto progressBefore = progressExisted ? progressFile.loadFileAsString() : juce::String();

    // The hearing profiles (ADR 051): put aside, and an empty store with the
    // offer declined, so the training cases see no strip.
    const auto hearingFile = HearingProfileStore::makeDefaultOptions().getDefaultFile();
    const auto hearingExisted = hearingFile.existsAsFile();
    const auto hearingBefore = hearingExisted ? hearingFile.loadFileAsString() : juce::String();
    {
        hearingFile.deleteFile();
        juce::PropertiesFile file (HearingProfileStore::makeDefaultOptions());
        HearingProfileStore (file).declineOffer (juce::Time::getCurrentTime());
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
            { "Welcome", Expect::everythingLive, 0,                   [] (auto& e) { e.offerTourForSnapshot(); } },
            { "Welcome - account", Expect::everythingLive, 3,         [] (auto& e) { e.openWelcomeAccountForSnapshot(); } },
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
            { "Settings - Pro", Expect::everythingLive, 0,            [] (auto& e) { e.openSettingsPageForSnapshot (SettingsScreenComponent::Page::training, true); } },
            { "Settings - Hearing", Expect::everythingLive, 0,        [] (auto& e) { e.openSettingsPageForSnapshot (SettingsScreenComponent::Page::hearing, true, true); } },
            { "Hearing notice", Expect::everythingLive, 0,            [] (auto& e) { e.showHearingNoticeForSnapshot(); } },
            // The form's Start is dead until both names are typed - right.
            { "Hearing test - form", Expect::everythingLive, 1,       [] (auto& e) { e.openHearingTestForSnapshot (0); } },
            { "Hearing test - answering", Expect::everythingLive, 0,  [] (auto& e) { e.openHearingTestForSnapshot (1); } },
            { "Hearing test - result", Expect::everythingLive, 0,     [] (auto& e) { e.openHearingTestForSnapshot (2); } },
            { "Hearing test - offer", Expect::everythingLive, 0,      [] (auto& e) { e.offerHearingTestForSnapshot(); } },
            { "Achievements", Expect::everythingLive, 0,              [] (auto& e) { e.openAchievementsForSnapshot(); } },
            { "Studio - EQ", Expect::everythingLive, 0,               [] (auto& e) { e.openStudioForSnapshot (StudioScreenComponent::Effect::eq); } },
            { "Studio - Comp", Expect::everythingLive, 0,             [] (auto& e) { e.openStudioForSnapshot (StudioScreenComponent::Effect::comp); } },
            { "Studio - Verb", Expect::everythingLive, 0,             [] (auto& e) { e.openStudioForSnapshot (StudioScreenComponent::Effect::verb); } },
            { "Live - seminar", Expect::everythingLive, 0,            [] (auto& e) { e.openLiveForSnapshot (0); } },
            { "Live - battle", Expect::everythingLive, 0,             [] (auto& e) { e.openLiveForSnapshot (1); } },
            { "Live - rating", Expect::everythingLive, 0,             [] (auto& e) { e.openLiveForSnapshot (2); } },
            { "Live - local room open", Expect::everythingLive, 0,    [] (auto& e) { e.openLiveForSnapshot (3); } },
            { "Live - online room open", Expect::everythingLive, 0,   [] (auto& e) { e.openLiveForSnapshot (8); } },
            { "Live - no internet", Expect::everythingLive, 0,        [] (auto& e) { e.openLiveForSnapshot (6); } },
        };

        for (const auto& window : windows())
        for (const auto& c : cases)
        {
            if (framesOnly)
                if (const auto* only = std::getenv ("FRAME_CASE"); only != nullptr && juce::String (c.name) != only)
                    continue;

           #if JUCE_WINDOWS
            _putenv_s ("SNAP_SAFE", window.safe != nullptr ? window.safe : "");
           #else
            if (window.safe != nullptr)
                setenv ("SNAP_SAFE", window.safe, 1);
            else
                unsetenv ("SNAP_SAFE");
           #endif

            EarTrainerProcessor processor;
            processor.prepareToPlay (44100.0, 512);

            EarTrainerEditor editor (processor);
            editor.setSize (window.width, window.height);

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

            editor.setSize (window.width, window.height);
            editor.completeScreenFade();
            editor.completeWelcomeReveal();

            if (framesOnly)
            {
                if (juce::String (window.name) != "iPhone")
                    continue;

                if (const auto* only = std::getenv ("FRAME_CASE"); only != nullptr && juce::String (c.name) != only)
                    continue;

                // What forces a transparency layer or an effect pass: the
                // usual reason one frame costs several.
                std::function<void (juce::Component&)> layers = [&] (juce::Component& comp)
                {
                    for (auto* ch : comp.getChildren())
                    {
                        if (! ch->isVisible())
                            continue;
                        if (ch->getAlpha() < 1.0f || ch->getComponentEffect() != nullptr || ch->isPaintingUnclipped()
                            || ch->getCachedComponentImage() != nullptr)
                            std::cout << "    layer: " << describe (*ch) << " alpha " << ch->getAlpha()
                                      << (ch->getComponentEffect() != nullptr ? " effect" : "")
                                      << (ch->isPaintingUnclipped() ? " unclipped" : "")
                                      << (ch->getCachedComponentImage() != nullptr ? " cached" : "")
                                      << " " << editor.getLocalArea (ch, ch->getLocalBounds()).toString() << "\n";
                        if (std::getenv ("FRAME_NOALPHA") != nullptr)
                            ch->setAlpha (1.0f);
                        layers (*ch);
                    }
                };
                layers (editor);

                const auto f = measureFrames (editor);
                std::cout << juce::String (c.name).paddedRight (' ', 28)
                          << juce::String (f.frameMs, 1).paddedLeft (' ', 7) << " ms/frame"
                          << juce::String (f.repaintsPerSecond, 0).paddedLeft (' ', 6) << " repaints/s"
                          << juce::String (f.screensPerSecond, 1).paddedLeft (' ', 7) << " screens/s"
                          << juce::String (f.frameMs * juce::jmin (60.0, f.screensPerSecond) / 10.0, 1).paddedLeft (' ', 7)
                          << " % of a core idle\n";
                continue;
            }

            const auto state = juce::String (c.name) + "  @ " + window.name
                             + " " + juce::String (window.width) + "x" + juce::String (window.height);
            problems += report (state, editor, c.expect, c.allowedDead, window, editor.safeBoundsForTools());

            // Text that did not fit even after the look-and-feel squeezed
            // it as far as it allows (AbcTrainLookAndFeel::fitText) - a cut
            // word is a layout fault like any other, and it is size-specific.
            AbcTrainLookAndFeel::enableTextAudit (true);
            AbcTrainLookAndFeel::setTextAuditContext (state);
            const auto picture = editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f);
            AbcTrainLookAndFeel::enableTextAudit (false);

            // CLICKMAP_PNG=folder: keep the picture of exactly what was
            // checked, named after the screen and the window.
            if (const auto* folder = std::getenv ("CLICKMAP_PNG"))
            {
                const auto file = juce::File (folder).getChildFile (juce::File::createLegalFileName (
                                      juce::String (c.name) + " @ " + window.name) + ".png");
                file.deleteFile();
                if (auto out = file.createOutputStream())
                    juce::PNGImageFormat().writeImageToStream (picture, *out);
            }

            for (const auto& line : AbcTrainLookAndFeel::takeTextAudit())
            {
                // kind, context, text, "needed > available"
                const auto parts = juce::StringArray::fromTokens (line, "\t", "");
                const auto cut = parts[0] == "CUT";
                std::cout << (cut ? "  BROKEN  text cut: \"" : "  small   text squeezed: \"")
                          << parts[juce::jmin (2, parts.size() - 1)] << "\"  " << parts[parts.size() - 1] << "\n";
                problems += cut ? 1 : 0;
            }
        }

       #if ! JUCE_WINDOWS
        unsetenv ("SNAP_SAFE");
       #endif
    }

    // Behaviour the screens cannot show in one still frame.
    {
        std::cout << "\nStudio - every switch, twice\n------------------------------\n";
        EarTrainerProcessor processor;
        processor.prepareToPlay (44100.0, 512);
        EarTrainerEditor editor (processor);
        editor.setVisible (true);
        editor.openStudioForSnapshot (StudioScreenComponent::Effect::eq);

        const StudioScreenComponent::Effect order[] { StudioScreenComponent::Effect::comp, StudioScreenComponent::Effect::eq,
                                                      StudioScreenComponent::Effect::verb, StudioScreenComponent::Effect::comp,
                                                      StudioScreenComponent::Effect::verb, StudioScreenComponent::Effect::eq };
        auto blank = 0;

        for (auto e : order)
        {
            editor.switchStudioForSnapshot (e);
            blank += editor.isStudioShowingEditorForSnapshot() ? 0 : 1;
        }

        std::cout << "  " << (blank == 0 ? "every plugin opens again" : juce::String (blank) + " switch(es) left an empty page") << "\n";
        problems += blank;
    }

    {
        std::cout << "\nTraining -> Training sounds\n---------------------------\n";
        EarTrainerProcessor processor;
        processor.prepareToPlay (44100.0, 512);
        EarTrainerEditor editor (processor);
        editor.setVisible (true);
        editor.openTrainingForSnapshot (0);
        editor.openSoundsFromTrainingForSnapshot();
        const auto silent = ! editor.isSignalEnabledForSnapshot();
        std::cout << "  " << (silent ? "the exercise goes quiet" : "the exercise keeps playing under the page") << "\n";
        problems += silent ? 0 : 1;
    }

    {
        std::cout << "\nHearing test -> another tab\n---------------------------\n";
        EarTrainerProcessor processor;
        processor.prepareToPlay (44100.0, 512);
        EarTrainerEditor editor (processor);
        editor.setVisible (true);
        editor.openHearingTestForSnapshot (1);
        processor.setHearingTest (true);   // as if the test were running
        editor.openSoundsFromTrainingForSnapshot();
        const auto stopped = ! processor.isRunningHearingTest();
        std::cout << "  " << (stopped ? "the probe stops" : "the probe keeps playing under another page") << "\n";
        problems += stopped ? 0 : 1;
    }

    if (hearingExisted)
        hearingFile.replaceWithText (hearingBefore);
    else
        hearingFile.deleteFile();

    if (progressExisted)
        progressFile.replaceWithText (progressBefore);
    else
        progressFile.deleteFile();

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
