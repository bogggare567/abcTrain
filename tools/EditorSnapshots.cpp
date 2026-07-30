// Renders every editor in the project to a PNG, with no plugin host and
// no window server involvement beyond what JUCE needs to lay out fonts.
//
// Why this exists: this project has a documented history (ADR 014, 015,
// 016, 019, 022) of UI bugs that compiled, passed all 172 test groups, and
// were obvious within ten seconds of *looking* at the thing - a slider
// groove the same colour as its panel, text clipped by its own container,
// a layout computed from a 1px-high rectangle. The test suite structurally
// cannot see any of those, and until now the only way to catch them was to
// launch the standalone build and look, which needs a desktop session a CI
// runner doesn't have and a contributor may not want to set up for a
// one-line change.
//
// So: construct each real editor, force a layout, snapshot it, write a
// PNG. It is not a golden-file test - nothing here asserts, because a
// pixel-exact expectation would fail on every legitimate design change and
// on every machine with different font rendering. It is a *contact sheet*:
// run it, open the folder, look at four pictures.
//
//     cmake --build build --target EditorSnapshots
//     ./build/EditorSnapshots_artefacts/EditorSnapshots [output-dir]
//
// Both themes are rendered for every editor, because "invisible in the
// other theme" is the single most repeated bug class in this codebase.

#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <type_traits>

#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../LearnerEQ/Source/PluginEditor.h"
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../LearnerComp/Source/PluginEditor.h"
#include "../LearnerVerb/Source/PluginProcessor.h"
#include "../LearnerVerb/Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../shared/AbcTrainTheme.h"
#include "../shared/i18n/LocalisationManager.h"

namespace
{
    // An editor sizes itself in its constructor (setSize) but only lays
    // its children out once resized() runs, which normally happens when a
    // peer is attached. There is no peer here, so call it directly.
    //
    // Deliberately no message-loop pumping: without it no Timer fires, so
    // every eased value - hover, the bypass veil, the guide card's rise -
    // is captured at its resting state. That is exactly what a still image
    // should show, and it makes the output reproducible instead of
    // depending on how long the process happened to take.
    void settle (juce::Component& component)
    {
        component.resized();
    }

    bool writeSnapshot (juce::Component& component, const juce::File& destination)
    {
        settle (component);

        const auto image = component.createComponentSnapshot (component.getLocalBounds(), true, 2.0f);

        if (! image.isValid())
            return false;

        destination.deleteFile();

        if (auto stream = destination.createOutputStream())
        {
            juce::PNGImageFormat png;
            return png.writeImageToStream (image, *stream);
        }

        return false;
    }

    enum class Extra { none, training, sounds, settings, results, achievements,
                       moduleShelf, moduleCheck, tourOffer, tour, screensaver };

    template <typename ProcessorType, typename EditorType>
    int renderOne (const juce::File& outputDir, const juce::String& name,
                   int openTraining = -1, Extra extra = Extra::none)
    {
        int failures = 0;

        for (const auto mode : { AbcTrainTheme::Mode::dark, AbcTrainTheme::Mode::light })
        {
            // Every editor reads the *persisted* preference in its own
            // constructor and calls setMode() from that, so setting the
            // mode here directly would simply be overwritten. Drive the
            // preference instead; main() puts the original value back.
            {
                juce::PropertiesFile properties (LocalisationManager::makeDefaultOptions());
                properties.setValue ("themeMode", mode == AbcTrainTheme::Mode::light ? "light" : "dark");

                // English, so the shots are readable to whoever finds the
                // repo. The player's own language is put back with
                // everything else in main().
                properties.setValue ("language", "en");
                properties.saveIfNeeded();
            }

            AbcTrainTheme::setMode (mode);

            ProcessorType processor;
            processor.prepareToPlay (44100.0, 512);

            EditorType editor (processor);

            // The welcome screen's word reveal is the one animation whose
            // resting state is "nothing yet", so a still frame of it at
            // rest is a blank. Fast-forward it; everything else stays at
            // rest deliberately.
            if constexpr (std::is_same_v<EditorType, EarTrainerEditor>)
            {
                editor.completeWelcomeReveal();

                if (openTraining >= 0)
                    editor.openTrainingForSnapshot (openTraining);

                if (extra == Extra::sounds)
                    editor.openSoundsForSnapshot();

                if (extra == Extra::settings)
                    editor.openSettingsForSnapshot();

                if (extra == Extra::results)
                    editor.showRunResultsForSnapshot();

                if (extra == Extra::achievements)
                    editor.openAchievementsForSnapshot();

                if (extra == Extra::tourOffer)
                    editor.offerTourForSnapshot();

                if (extra == Extra::tour)
                    editor.openTourForSnapshot();

                if (extra == Extra::screensaver)
                    editor.openScreensaverForSnapshot();
            }

            if constexpr (std::is_same_v<EditorType, LearnerCompEditor>
                          || std::is_same_v<EditorType, LearnerVerbEditor>)
            {
                // A plain shot of these two shows a preset engaged rather
                // than every knob at its default - a picture of a plugin
                // nobody has touched is a picture of nothing.
                if (extra == Extra::none)
                    editor.applyPresetForSnapshot (1);

                if (extra == Extra::moduleShelf)
                    editor.openModuleShelfForSnapshot();

                if (extra == Extra::moduleCheck)
                    editor.openModuleCheckForSnapshot();
            }

            const auto suffix = mode == AbcTrainTheme::Mode::light ? "-light" : "-dark";
            const auto file = outputDir.getChildFile (name + suffix + ".png");

            if (writeSnapshot (editor, file))
            {
                std::cout << "  " << file.getFileName() << "  ("
                          << editor.getWidth() << "x" << editor.getHeight() << ")\n";
            }
            else
            {
                std::cout << "  FAILED: " << file.getFileName() << "\n";
                ++failures;
            }
        }

        return failures;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto outputDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                    : juce::File::getCurrentWorkingDirectory().getChildFile ("editor-snapshots");
    outputDir.createDirectory();

    std::cout << "Rendering editors to " << outputDir.getFullPathName() << "\n";

    // Both themes are rendered by temporarily driving the shared
    // preference file the editors read - so remember what it said and put
    // it back, or running the snapshot tool would silently flip the theme
    // of the app the user actually uses.
    juce::String originalThemeMode, originalLanguage;
    {
        juce::PropertiesFile properties (LocalisationManager::makeDefaultOptions());
        originalThemeMode = properties.getValue ("themeMode", "dark");
        originalLanguage = properties.getValue ("language");
    }

    auto failures = 0;
    failures += renderOne<LearnerEQProcessor,   LearnerEQEditor>   (outputDir, "LearnerEQ");
    failures += renderOne<LearnerCompProcessor, LearnerCompEditor> (outputDir, "LearnerComp");
    failures += renderOne<LearnerVerbProcessor, LearnerVerbEditor> (outputDir, "LearnerVerb");
    failures += renderOne<LearnerCompProcessor, LearnerCompEditor> (outputDir, "LearnerComp-Modules",
                                                                     -1, Extra::moduleShelf);
    failures += renderOne<LearnerCompProcessor, LearnerCompEditor> (outputDir, "LearnerComp-Check",
                                                                     -1, Extra::moduleCheck);

    // EarTrainer's editor owns a ProgressManager writing to the real
    // per-user settings file, so rendering it *does* touch a player's
    // saved progress. Copy the file aside first and put it back after -
    // and skip EarTrainer entirely if that copy fails, rather than
    // rendering anyway and risking someone's record for a screenshot.
    {
        const auto settings = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                  .getChildFile ("abcTrain").getChildFile ("abcTrain.settings");
        const auto backup = settings.getSiblingFile ("abcTrain.settings.snapshot-backup");

        const auto hadSettings = settings.existsAsFile();
        const auto backedUp = ! hadSettings || settings.copyFileTo (backup);

        if (! backedUp)
        {
            std::cout << "  skipped EarTrainer: couldn't back up " << settings.getFullPathName() << "\n";
        }
        else
        {
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer");

            // The screen people actually spend their time on. Index 0 is
            // the EQ exercise - a continuous scale, so the shot shows the
            // answer slider rather than a row of named choices.
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Training", 0);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Sounds", -1, Extra::sounds);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Settings", -1, Extra::settings);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Results", -1, Extra::results);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Achievements", -1, Extra::achievements);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Welcome", -1, Extra::tourOffer);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Tour", -1, Extra::tour);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Screensaver", -1, Extra::screensaver);

            if (hadSettings)
            {
                backup.copyFileTo (settings);
                backup.deleteFile();
            }
            else
            {
                settings.deleteFile();
            }
        }
    }

    {
        juce::PropertiesFile properties (LocalisationManager::makeDefaultOptions());
        properties.setValue ("themeMode", originalThemeMode);
        properties.setValue ("language", originalLanguage);
        properties.saveIfNeeded();
    }

    if (failures > 0)
        std::cout << failures << " snapshot(s) failed to render.\n";

    return failures > 0 ? 1 : 0;
}
