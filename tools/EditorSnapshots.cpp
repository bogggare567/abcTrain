#include <cstdlib>
#include <vector>
// Renders every editor in the project to a PNG, with no plugin host and
// no window server involvement beyond what JUCE needs to lay out fonts.
//
// Why this exists: this project has a documented history (ADR 014, 015,
// 016, 019, 022, 031) of UI bugs that compiled, passed every test group, and
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
#include "shared/ui/AbcTrainTheme.h"
#include "shared/i18n/LocalisationManager.h"
#include "../Source/LiveLink.h"

namespace
{
    // A small library for the Sounds shots: the player's own folders sorted
    // by instrument, and a pack split by instrument - the shape a real
    // library has after an import and a pack install. Synthesized, made
    // once per run in the temp folder.
    juce::File snapshotLibrary()
    {
        static const juce::File root = []
        {
            const auto base = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("abcTrainSnapshotLibrary");
            base.deleteRecursively();
            const auto library = base.getChildFile ("Training Sounds");
            constexpr double rate = 44100.0;

            const auto write = [] (const juce::File& file, double seconds, double bpm, float pitch, float noise, float wide)
            {
                juce::AudioBuffer<float> audio (2, (int) (seconds * rate));
                juce::Random random (file.getFileName().hashCode());
                const auto beat = 60.0 / bpm * rate;

                for (int i = 0; i < audio.getNumSamples(); ++i)
                {
                    const auto t = std::fmod ((double) i, beat) / rate;
                    const auto env = (float) std::exp (-t * 9.0);
                    const auto body = std::sin (juce::MathConstants<double>::twoPi * pitch * (double) i / rate);
                    const auto phrase = 0.55f + 0.45f * (float) std::sin ((double) i / rate * 0.35);
                    const auto n = random.nextFloat() * 2.0f - 1.0f;
                    const auto v = phrase * (env * (0.7f * (float) body + noise * n));
                    audio.setSample (0, i, v);
                    audio.setSample (1, i, v * (1.0f - wide) + wide * (random.nextFloat() * 2.0f - 1.0f) * 0.2f);
                }

                file.getParentDirectory().createDirectory();
                juce::WavAudioFormat wav;
                auto stream = file.createOutputStream();
                std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate, 2, 16, {}, 0));
                if (writer != nullptr)
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
                }
            };

            write (library.getChildFile ("Kick/kick in 1.wav"), 8.0, 92.0, 55.0f, 0.05f, 0.0f);
            write (library.getChildFile ("Kick/kick in 2.wav"), 8.0, 92.0, 60.0f, 0.05f, 0.0f);
            write (library.getChildFile ("Kick/kick out 1.wav"), 8.0, 92.0, 50.0f, 0.1f, 0.0f);
            write (library.getChildFile ("Full Mix/80 1.wav"), 9.9, 97.0, 110.0f, 0.4f, 0.4f);
            write (library.getChildFile ("Other/Audio 3 1.wav"), 8.5, 120.0, 330.0f, 0.1f, 0.1f);

            const auto pack = library.getChildFile ("bogdan-own");
            juce::StringArray clips;
            const auto add = [&] (const juce::String& file, double bpm, float pitch, float noise, float wide)
            {
                write (pack.getChildFile (file), 4 * 4 * 60.0 / bpm, bpm, pitch, noise, wide);
                clips.add ("{\"file\":\"" + file + "\",\"source\":{\"author\":\"Bogdan Korablev\",\"license\":\"CC-BY-4.0\"}}");
            };

            add ("kick/live-01-kick-in.wav", 92.0, 55.0f, 0.05f, 0.0f);
            add ("kick/live-02-kick-out.wav", 92.0, 52.0f, 0.08f, 0.0f);
            add ("kick/beat-01-kick-bus.wav", 110.0, 58.0f, 0.1f, 0.0f);
            add ("snare/live-01-snare-top.wav", 92.0, 200.0f, 0.7f, 0.0f);
            add ("hihat/live-01-hh.wav", 92.0, 4000.0f, 0.9f, 0.0f);
            add ("bass/live-01-bass.wav", 92.0, 65.0f, 0.02f, 0.0f);
            add ("guitar/live-01-guitar.wav", 92.0, 196.0f, 0.1f, 0.0f);
            add ("mix/2-10-01.wav", 110.0, 110.0f, 0.4f, 0.5f);
            add ("mix/2-14-01.wav", 140.0, 98.0f, 0.4f, 0.5f);
            add ("other/1-2-01.wav", 118.0, 440.0f, 0.1f, 0.05f);

            pack.getChildFile ("pack.json").replaceWithText (
                "{\"abcTrainPack\":1,\"id\":\"bogdan-own\",\"version\":\"1.0.0\","
                "\"title\":{\"en\":\"Bogdan Korablev's recordings\",\"ru\":\"" + juce::String::fromUTF8 ("Записи Богдана Кораблёва") + "\"},"
                "\"clips\":[" + clips.joinIntoString (",") + "]}");

            write (base.getChildFile ("Rehearsal take 3.wav"), 75.0, 104.0, 82.0f, 0.3f, 0.3f);
            return library;
        }();

        return root;
    }

    // Second CLI argument; "en" when absent. See the language note below.
    juce::String& snapshotLanguage()
    {
        static juce::String language { "en" };
        return language;
    }

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
                       moduleShelf, moduleCheck, tourOffer, tour, screensaver, stretched,
                       answered, survivalRun, duelRun, duelResults, home, homeWithRecords, hint,
                       settingsPro, settingsHearing, settingsAbout, settingsAppearance, hearingNotice, moduleResult,
                       studioEQ, studioComp, studioVerb, welcomeAccount, soundClips, eqKick, studioRevisit, companion, companionApp,
                       liveSeminar, liveBattle, liveRating, liveRoom, liveInvites, liveSignIn, settingsLive, settingsLiveSignedIn, liveBattleSignedIn, eqSlope,
                       liveNoInternet, liveNoLan, soundsLibrary, soundsSelect, soundsDelete, soundsTrack,
                       liveRoundOpen, liveRoundAnswer, soundsChecked };

    template <typename ProcessorType, typename EditorType>
    int renderOne (const juce::File& outputDir, const juce::String& name,
                   int openTraining = -1, Extra extra = Extra::none, int moduleIndex = 2)
    {
        int failures = 0;

        std::vector<AbcTrainTheme::Mode> modes { AbcTrainTheme::Mode::dark, AbcTrainTheme::Mode::light };

        // SNAP_DARK=1: one theme, for quick layout passes.
        if (std::getenv ("SNAP_DARK") != nullptr)
            modes = { AbcTrainTheme::Mode::dark };

        // SNAP_ONLY=substring: render only screens whose name contains it.
        if (const auto* only = std::getenv ("SNAP_ONLY"))
            if (! name.containsIgnoreCase (only))
                return 0;

        for (const auto mode : modes)
        {
            // Every editor reads the *persisted* preference in its own
            // constructor and calls setMode() from that, so setting the
            // mode here directly would simply be overwritten. Drive the
            // preference instead; main() puts the original value back.
            {
                juce::PropertiesFile properties (LocalisationManager::makeDefaultOptions());
                properties.setValue ("themeMode", mode == AbcTrainTheme::Mode::light ? "light" : "dark");

                // English by default, so the committed shots are readable
                // to whoever finds the repo - but overridable, because
                // "the zones are translated now" is a claim only a
                // non-English render can check. The player's own language
                // is put back with everything else in main().
                properties.setValue ("language", snapshotLanguage());
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
                editor.completeScreenFade();

                if (openTraining >= 0)
                    editor.openTrainingForSnapshot (openTraining);

                if (extra == Extra::home)
                    editor.openHomeForSnapshot();

                if (extra == Extra::hint)
                    editor.revealHintForSnapshot();

                if (extra == Extra::answered)
                    editor.answerForSnapshot();

                if (extra == Extra::survivalRun)
                    editor.startRunForSnapshot (SessionManager::Mode::survival);

                if (extra == Extra::duelRun || extra == Extra::duelResults)
                    editor.duelForSnapshot (extra == Extra::duelResults);

                if (extra == Extra::sounds)
                    editor.openSoundsForSnapshot();

                if (extra == Extra::settings)
                    editor.openSettingsForSnapshot();

                if (extra == Extra::settingsPro)
                    editor.openSettingsPageForSnapshot (SettingsScreenComponent::Page::training, true);

                if (extra == Extra::settingsHearing)
                    editor.openSettingsPageForSnapshot (SettingsScreenComponent::Page::hearing, true, true);

                if (extra == Extra::liveSeminar) editor.openLiveForSnapshot (0);
                if (extra == Extra::liveBattle)  editor.openLiveForSnapshot (1);
                if (extra == Extra::liveRating)  editor.openLiveForSnapshot (2);
                if (extra == Extra::liveRoom)    editor.openLiveForSnapshot (3);
                if (extra == Extra::liveInvites) editor.openLiveForSnapshot (4);
                if (extra == Extra::liveSignIn)  editor.openLiveForSnapshot (5);
                if (extra == Extra::liveNoInternet) editor.openLiveForSnapshot (6);
                if (extra == Extra::liveNoLan)   editor.openLiveForSnapshot (7);
                if (extra == Extra::liveRoundOpen)   editor.openLiveForSnapshot (9);
                if (extra == Extra::liveRoundAnswer) editor.openLiveForSnapshot (10);

                if (extra == Extra::soundsLibrary || extra == Extra::soundsSelect
                    || extra == Extra::soundsDelete || extra == Extra::soundsTrack || extra == Extra::soundsChecked)
                {
                    const auto root = snapshotLibrary();
                    editor.openSoundsLibraryForSnapshot (root, root.getSiblingFile ("Rehearsal take 3.wav"),
                                                         extra == Extra::soundsDelete || extra == Extra::soundsChecked ? "Kick" : "bogdan-own/kick",
                                                         extra == Extra::soundsSelect ? 1
                                                         : extra == Extra::soundsDelete ? 2
                                                         : extra == Extra::soundsTrack ? 3
                                                         : extra == Extra::soundsChecked ? 4 : 0);
                }
                if (extra == Extra::settingsLive)
                    editor.openSettingsPageForSnapshot (SettingsScreenComponent::Page::live, false);

                if (extra == Extra::settingsLiveSignedIn || extra == Extra::liveBattleSignedIn)
                {
                    processor.getLiveAccount().signInForSnapshot ("bogdan");
                    if (extra == Extra::settingsLiveSignedIn)
                        editor.openSettingsPageForSnapshot (SettingsScreenComponent::Page::live, false);
                    else
                        editor.openLiveForSnapshot (1);
                }

                if (extra == Extra::settingsAppearance)
                    editor.openSettingsPageForSnapshot (SettingsScreenComponent::Page::appearance, false);

                if (extra == Extra::settingsAbout)
                    editor.openSettingsPageForSnapshot (SettingsScreenComponent::Page::about, false);

                if (extra == Extra::hearingNotice)
                    editor.showHearingNoticeForSnapshot();

                if (extra == Extra::studioEQ)
                    editor.openStudioForSnapshot (StudioScreenComponent::Effect::eq);

                if (extra == Extra::studioRevisit)
                {
                    editor.openStudioForSnapshot (StudioScreenComponent::Effect::eq);
                    editor.switchStudioForSnapshot (StudioScreenComponent::Effect::comp);
                    editor.switchStudioForSnapshot (StudioScreenComponent::Effect::eq);
                }

                if (extra == Extra::studioComp)
                    editor.openStudioForSnapshot (StudioScreenComponent::Effect::comp);

                if (extra == Extra::studioVerb)
                    editor.openStudioForSnapshot (StudioScreenComponent::Effect::verb);

                if (extra == Extra::results)
                    editor.showRunResultsForSnapshot();

                if (extra == Extra::achievements)
                    editor.openAchievementsForSnapshot();

                if (extra == Extra::tourOffer)
                    editor.offerTourForSnapshot();

                if (extra == Extra::soundClips)
                    editor.openSoundClipsForSnapshot();

                if (extra == Extra::welcomeAccount)
                    editor.openWelcomeAccountForSnapshot();

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
            }

            if constexpr (std::is_same_v<EditorType, LearnerEQEditor>)
                if (extra == Extra::eqKick || extra == Extra::companion)
                    editor.kickLessonForSnapshot();

            if constexpr (std::is_same_v<EditorType, LearnerEQEditor>)
                if (extra == Extra::eqSlope)
                    editor.slopeForSnapshot();

            // The companion window's content, rendered on its own next to
            // the plugin it was taken out of.
            juce::Component* companionShot = nullptr;

            if constexpr (std::is_same_v<EditorType, EarTrainerEditor>)
                if (extra == Extra::liveRoom || extra == Extra::liveRoundOpen || extra == Extra::liveRoundAnswer)
                    companionShot = editor.projectorForSnapshot();

            if constexpr (std::is_base_of_v<LearnerEditorBase, EditorType>)
            {
                if (extra == Extra::companionApp)
                    editor.setHearingProvider ([]
                    {
                        CompanionHearing h;
                        h.sessionMinutes = 38;
                        h.levelDbA = 74.0;
                        h.calibrated = true;
                        h.weekFraction = 0.36;
                        h.minutesUntilBreak = 12;
                        return h;
                    });

                if (extra == Extra::companion || extra == Extra::companionApp)
                    companionShot = &editor.openCompanionForSnapshot();
            }

            if constexpr (std::is_same_v<EditorType, LearnerCompEditor>
                          || std::is_same_v<EditorType, LearnerVerbEditor>
                          || std::is_same_v<EditorType, LearnerEQEditor>)
            {
                if (extra == Extra::moduleShelf)
                    editor.openModuleShelfForSnapshot();

                if (extra == Extra::moduleCheck)
                    editor.openModuleCheckForSnapshot (moduleIndex);

                if (extra == Extra::moduleResult)
                    editor.openModuleResultForSnapshot (moduleIndex, true);
            }

            // "Adaptive" is a claim, and the only way to check a claim about
            // layout is to render it at a size it was not designed at.
            if (extra == Extra::stretched)
                editor.setSize ((int) (editor.getWidth() * 1.35),
                                 (int) (editor.getHeight() * 1.3));

            // SNAP_SIZE=WxH renders every screen at that window size instead
            // of the design size - how a laptop display actually sees it.
            if (const auto* size = std::getenv ("SNAP_SIZE"))
            {
                const auto parts = juce::StringArray::fromTokens (size, "x", "");

                if (parts.size() == 2)
                    editor.setSize (parts[0].getIntValue(), parts[1].getIntValue());
            }

            if constexpr (std::is_same_v<EditorType, EarTrainerEditor>)
                editor.completeScreenFade();   // after every screen change above

            const auto suffix = mode == AbcTrainTheme::Mode::light ? "-light" : "-dark";
            const auto file = outputDir.getChildFile (name + suffix + ".png");

            // TEXT_AUDIT=1: list every line that had to be squeezed hard or
            // cut while this screen painted (AbcTrainLookAndFeel::fitText).
            const auto auditing = std::getenv ("TEXT_AUDIT") != nullptr;
            AbcTrainLookAndFeel::enableTextAudit (auditing);
            AbcTrainLookAndFeel::setTextAuditContext (snapshotLanguage() + "\t" + name);

            const auto written = writeSnapshot (editor, file);

            if (companionShot != nullptr)
            {
                const auto windowFile = outputDir.getChildFile (name + "-Window" + suffix + ".png");

                if (writeSnapshot (*companionShot, windowFile))
                    std::cout << "  " << windowFile.getFileName() << "\n";
                else
                    ++failures;
            }

            for (const auto& line : AbcTrainLookAndFeel::takeTextAudit())
                std::cout << "OVERFLOW\t" << line << "\n";

            if (written)
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
    // Always the design size, whatever the virtual display is (shared/ui/WindowFit.h).
   #if JUCE_WINDOWS
    _putenv_s ("ABC_DESIGN_SIZE", "1");   // Windows has no setenv (CI caught it)
   #else
    setenv ("ABC_DESIGN_SIZE", "1", 1);
   #endif

    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    // A picture must not depend on the network of whatever machine draws it.
    LiveLink::networkAllowed = false;

    // The Sounds shots point the library at a temporary folder, and that
    // choice is saved. Keep the player's own settings file to put back.
    const auto librarySettingsFile = ReferenceAudioLibrary::makeDefaultOptions().getDefaultFile();
    const auto librarySettingsExisted = librarySettingsFile.existsAsFile();
    const auto librarySettingsBefore = librarySettingsExisted ? librarySettingsFile.loadFileAsString() : juce::String();

    const auto outputDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                    : juce::File::getCurrentWorkingDirectory().getChildFile ("editor-snapshots");
    outputDir.createDirectory();

    if (argc > 2)
        snapshotLanguage() = argv[2];

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
    failures += renderOne<LearnerEQProcessor,   LearnerEQEditor>   (outputDir, "LearnerEQ-Kick", -1, Extra::eqKick);
    failures += renderOne<LearnerEQProcessor,   LearnerEQEditor>   (outputDir, "LearnerEQ-Companion", -1, Extra::companion);
    failures += renderOne<LearnerEQProcessor,   LearnerEQEditor>   (outputDir, "LearnerEQ-Slope", -1, Extra::eqSlope);
    failures += renderOne<LearnerCompProcessor, LearnerCompEditor> (outputDir, "LearnerComp-Companion", -1, Extra::companionApp);
    failures += renderOne<LearnerCompProcessor, LearnerCompEditor> (outputDir, "LearnerComp");
    failures += renderOne<LearnerVerbProcessor, LearnerVerbEditor> (outputDir, "LearnerVerb");
    failures += renderOne<LearnerCompProcessor, LearnerCompEditor> (outputDir, "LearnerComp-Stretched",
                                                                     -1, Extra::stretched);
    failures += renderOne<LearnerCompProcessor, LearnerCompEditor> (outputDir, "LearnerComp-Modules",
                                                                     -1, Extra::moduleShelf);
    // The module shots answer a check, which writes a step to the practice
    // library's settings file - the player's own. Put it aside first.
    {
        const auto library = ReferenceAudioLibrary::makeDefaultOptions().getDefaultFile();
        const auto backup = library.getSiblingFile (library.getFileName() + ".snapshot-backup");
        const auto hadLibrary = library.existsAsFile();

        if (! hadLibrary || library.copyFileTo (backup))
        {
            failures += renderOne<LearnerVerbProcessor, LearnerVerbEditor> (outputDir, "LearnerVerb-Modules", -1, Extra::moduleShelf);
            failures += renderOne<LearnerEQProcessor,   LearnerEQEditor>   (outputDir, "LearnerEQ-Modules", -1, Extra::moduleShelf);

            if (hadLibrary)
            {
                backup.copyFileTo (library);
                backup.deleteFile();
            }
            else
            {
                library.deleteFile();
            }
        }
    }

    // EarTrainer's editor owns a ProgressManager writing to the real
    // per-user settings file, so rendering it *does* touch a player's
    // saved progress. Copy the file aside first and put it back after -
    // and skip EarTrainer entirely if that copy fails, rather than
    // rendering anyway and risking someone's record for a screenshot.
    {
        const auto settings = LocalisationManager::makeDefaultOptions().getDefaultFile();
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
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Home", -1, Extra::home);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Training", 0);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Answered", 0, Extra::answered);

            // Game 2 is Guess the Reverb - a zoned exercise, so the answer
            // panel is two regions rather than a ruler. Both mechanics now
            // need a shot: the pair-of-zones layout is where "always two"
            // is most likely to look wrong.
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Zoned", 2);

            // Reported missing their mode pills: delay, distortion, range.
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Delay", 4);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Distortion", 5);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Range", 8);

            // The zoned panel *after* an answer. Worth its own shot: the
            // verdict used to be carried by the tick line down each zone,
            // and once that went the only things left saying right from
            // wrong are the zone tint and the readout's colour.
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-ZonedAnswered", 2, Extra::answered);
            // One shot per hint view, since the whole point is that the
            // three are different pictures: 0 is the frequency exercise
            // (spectrum), 3 is pan (stereo), 1 is compression (envelope).
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-HintSpectrum", 0, Extra::hint);

            // Every exercise with its hint bought - SNAP_ONLY=HintAll.
            for (int game = 0; game < 9; ++game)
                failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-HintAll" + juce::String (game), game, Extra::hint);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-HintStereo", 3, Extra::hint);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-HintEnvelope", 1, Extra::hint);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SurvivalRun", 0, Extra::survivalRun);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-DuelRun", 3, Extra::duelRun);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-DuelResults", 3, Extra::duelResults);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Sounds", -1, Extra::sounds);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SoundClips", -1, Extra::soundClips);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Settings", -1, Extra::settings);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-StudioEQ", -1, Extra::studioEQ);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-StudioComp", -1, Extra::studioComp);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-StudioRevisit", -1, Extra::studioRevisit);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-StudioVerb", -1, Extra::studioVerb);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveSeminar", -1, Extra::liveSeminar);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveNoInternet", -1, Extra::liveNoInternet);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveNoLan", -1, Extra::liveNoLan);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SoundsLibrary", -1, Extra::soundsLibrary);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SoundsSelect", -1, Extra::soundsSelect);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SoundsDelete", -1, Extra::soundsDelete);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SoundsTrack", -1, Extra::soundsTrack);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveRoom", -1, Extra::liveRoom);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveInvites", -1, Extra::liveInvites);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveRoundOpen", -1, Extra::liveRoundOpen);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveRoundAnswer", -1, Extra::liveRoundAnswer);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SoundsChecked", -1, Extra::soundsChecked);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveBattle", -1, Extra::liveBattle);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveRating", -1, Extra::liveRating);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveSignIn", -1, Extra::liveSignIn);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SettingsLive", -1, Extra::settingsLive);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SettingsLiveSignedIn", -1, Extra::settingsLiveSignedIn);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-LiveBattleSignedIn", -1, Extra::liveBattleSignedIn);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Results", -1, Extra::results);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Achievements", -1, Extra::achievements);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Welcome", -1, Extra::tourOffer);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-WelcomeAccount", -1, Extra::welcomeAccount);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Tour", -1, Extra::tour);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Screensaver", -1, Extra::screensaver);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-Stretched", 0, Extra::stretched);

            // Last, because these write Pro mode, a calibration and a week
            // of dose into the settings file every later shot would inherit.
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-HearingNotice", -1, Extra::hearingNotice);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SettingsPro", -1, Extra::settingsPro);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SettingsHearing", -1, Extra::settingsHearing);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SettingsAbout", -1, Extra::settingsAbout);
            failures += renderOne<EarTrainerProcessor, EarTrainerEditor> (outputDir, "EarTrainer-SettingsAppearance", -1, Extra::settingsAppearance);

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

    // The Sounds shots pointed the library at a temporary folder; put the
    // player's own setting back, as the theme and language are above.
    {
        const auto file = ReferenceAudioLibrary::makeDefaultOptions().getDefaultFile();

        if (librarySettingsExisted)
            file.replaceWithText (librarySettingsBefore);
        else
            file.deleteFile();

        snapshotLibrary().getParentDirectory().deleteRecursively();
    }

    if (failures > 0)
        std::cout << failures << " snapshot(s) failed to render.\n";

    return failures > 0 ? 1 : 0;
}
