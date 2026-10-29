#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "shared/audio/SpectralContent.h"
#include "shared/audio/PinkNoiseGenerator.h"
#include "shared/audio/ReferenceAudioLibrary.h"
#include "Source/Games/EQGame.h"
#include "Source/Games/FrequencyRangeGame.h"
#include "Source/GameManager.h"
#include <array>

// The detector behind "ask only about what the clip has in it" (Bogdan,
// 2026-10-19: a hi-hat loop was asking "Sub-bass or Low-mids?"). Synthetic
// signals whose answer is known by construction first, then the real
// starter-pack clips the battles play, then the two exercises end to end.
namespace
{
    std::vector<float> pink (double sampleRate, double seconds, juce::int64 seed = 7)
    {
        PinkNoiseGenerator g { seed };
        std::vector<float> v ((size_t) (sampleRate * seconds));
        for (auto& s : v)
            s = g.nextSample();
        return v;
    }

    std::vector<float> sine (float hz, double sampleRate, double seconds)
    {
        std::vector<float> v ((size_t) (sampleRate * seconds));
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = 0.5f * std::sin (juce::MathConstants<float>::twoPi * hz * (float) i / (float) sampleRate);
        return v;
    }

    // A synthetic closed hi-hat: white noise through a steep high-pass at
    // 6 kHz, as short decaying hits eight times a second.
    std::vector<float> hiHats (double sampleRate, double seconds)
    {
        juce::Random r (99);
        juce::dsp::IIR::Filter<float> hp[4];
        for (auto& f : hp)
            f.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 6000.0f, 0.707f);

        std::vector<float> v ((size_t) (sampleRate * seconds));
        const auto period = (size_t) (sampleRate / 8.0);
        for (size_t i = 0; i < v.size(); ++i)
        {
            const auto t = (float) (i % period) / (float) sampleRate;
            auto x = (r.nextFloat() * 2.0f - 1.0f) * std::exp (-t * 60.0f);
            for (auto& f : hp)
                x = f.processSample (x);
            v[i] = x;
        }
        return v;
    }

    SpectralContent::Profile profileOf (const std::vector<float>& v, double sampleRate)
    {
        return SpectralContent::analyse (v.data(), (int) v.size(), sampleRate);
    }

    juce::PropertiesFile::Options tempOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "EarTrainerTests";
        options.filenameSuffix = "settings";
        options.folderName = "EarTrainerTests_spectralcontent";
        options.getDefaultFile().deleteFile();
        return options;
    }

    juce::File findBuiltIn (const ReferenceAudioLibrary& library, const juce::String& nameStart)
    {
        for (const auto& category : library.getCategories())
            for (const auto& file : category.files)
                if (file.getFileName().contains (nameStart))
                    return file;
        return {};
    }
}

class SpectralContentTest : public juce::UnitTest
{
public:
    SpectralContentTest() : juce::UnitTest ("SpectralContent", "Audio") {}

    void runTest() override
    {
        constexpr double sr = 44100.0;

        beginTest ("Nothing measured excludes nothing");
        {
            SpectralContent::Profile none;
            for (int b = 0; b < SpectralContent::numBands; ++b)
                expect (none.isPresent (b));
            expect (none.hasEnergyBetween (20.0f, 60.0f));
        }

        beginTest ("Pink noise has every band");
        {
            const auto p = profileOf (pink (sr, 4.0), sr);
            expect (p.analysed);
            for (int b = 0; b < SpectralContent::numBands - 1; ++b)
                expect (p.isPresent (b), "band " + juce::String (SpectralContent::bandCentres()[(size_t) b]) + " Hz: "
                                             + juce::String (p.relativeDb[(size_t) b], 1) + " dB");
            expect (! p.isHits(), "steady noise is not hits");
        }

        beginTest ("A sine is about one octave, nowhere near broadband");
        {
            const auto p = profileOf (sine (1000.0f, sr, 2.0), sr);
            expectLessThan (p.presentOctaves(), 2.0f);
            expect (p.isPresentAt (1000.0f));
            expect (! p.isPresentAt (100.0f));
            expect (! p.isPresentAt (10000.0f));
        }

        beginTest ("Hi-hats: air yes, sub-bass, bass and low-mids no, and they are hits");
        {
            const auto p = profileOf (hiHats (sr, 4.0), sr);
            expect (! p.hasEnergyBetween (20.0f, 60.0f));
            expect (! p.hasEnergyBetween (60.0f, 250.0f));
            expect (! p.hasEnergyBetween (250.0f, 500.0f));
            expect (p.hasEnergyBetween (6000.0f, 20000.0f));
            expect (p.isHits(), "onsets " + juce::String (p.onsetsPerSecond) + "/s, crest "
                                    + juce::String (p.envelopeCrestDb) + " dB");
        }

        beginTest ("The verdict does not depend on the sample rate");
        {
            // What a battle relies on: the same sound at 44.1 and 48 kHz must
            // name the same bands present (the library measures at the
            // file's own rate, but a pack may hold either).
            for (auto* make : { +[] (double rate) { return hiHats (rate, 3.0); },
                                +[] (double rate) { return pink (rate, 3.0); } })
            {
                const auto a = profileOf (make (44100.0), 44100.0);
                const auto b = profileOf (make (48000.0), 48000.0);
                for (int band = 0; band < SpectralContent::numBands - 1; ++band)
                    expect (a.isPresent (band) == b.isPresent (band),
                                  "band " + juce::String (SpectralContent::bandCentres()[(size_t) band]));
            }
        }

        beginTest ("Analysing a long clip is cheap enough for the message thread");
        {
            const auto v = pink (48000.0, 20.0);
            const auto start = juce::Time::getMillisecondCounterHiRes();
            const auto p = profileOf (v, 48000.0);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - start;
            logMessage ("20 s at 48 kHz analysed in " + juce::String (ms, 1) + " ms");
            expect (p.analysed);
            expectLessThan (ms, 250.0, "a round change must not stall the screen");
        }

        beginTest ("One sound, one profile");
        {
            const auto v = hiHats (sr, 2.0);
            const auto a = profileOf (v, sr);
            const auto b = profileOf (v, sr);
            for (int band = 0; band < SpectralContent::numBands; ++band)
                expectEquals (a.relativeDb[(size_t) band], b.relativeDb[(size_t) band]);
        }

        beginTest ("Empty, silent and nonsense input are a harmless miss");
        {
            expect (! SpectralContent::analyse (nullptr, 100, sr).analysed);
            expect (! SpectralContent::analyse (nullptr, 0, sr).analysed);
            std::vector<float> silence (44100, 0.0f);
            const auto p = profileOf (silence, sr);
            expect (! p.isPresent (17), "silence has nothing present");
            std::vector<float> tiny (100, 0.1f);
            expect (SpectralContent::analyse (tiny.data(), 100, 0.0).analysed == false);
        }

        beginTest ("The band table and the named ranges agree");
        {
            expectEquals (SpectralContent::Profile::bandIndexFor (1000.0f), 17);
            expectEquals (SpectralContent::Profile::bandIndexFor (20.0f), 0);
            expectEquals (SpectralContent::Profile::bandIndexFor (20000.0f), 30);
            expectEquals (SpectralContent::Profile::bandIndexFor (63.0f), 5);
            // Every named range has at least one third-octave centre in it,
            // or a range could never be judged present.
            for (const auto& r : FrequencyRangeGame::ranges)
            {
                auto any = false;
                for (const auto c : SpectralContent::bandCentres())
                    any = any || (c >= r.lowHz && c < r.highHz);
                expect (any, r.label);
            }
        }

        // ---- the real starter pack the battles play ------------------------
        auto options = tempOptions();
        juce::PropertiesFile properties (options);
        ReferenceAudioLibrary library (properties);
        library.setRootFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                   .getChildFile ("EarTrainerTests_spectralcontent_none"));

        if (juce::SystemStats::getEnvironmentVariable ("ABCTRAIN_PRINT_PROFILES", {}).isNotEmpty())
            printEveryBuiltIn (library);

        const auto hats = findBuiltIn (library, "hi-hats-closed");
        const auto kick = findBuiltIn (library, "acoustic-kicks");
        const auto tone = findBuiltIn (library, "Tone.wav");

        beginTest ("The starter pack's closed hi-hats have no low end");
        {
            expect (hats.existsAsFile(), "starter hi-hats clip");
            expect (library.selectFile (hats, 48000.0));
            const auto& p = library.getActiveContent();
            expect (p.analysed);
            expect (! p.hasEnergyBetween (20.0f, 60.0f), "hi-hats: no sub-bass");
            expect (! p.hasEnergyBetween (60.0f, 250.0f), "hi-hats: no bass");
            expect (p.hasEnergyBetween (6000.0f, 20000.0f), "hi-hats: air");
        }

        beginTest ("Name the Range on hi-hats never answers with a range they do not have");
        {
            FrequencyRangeGame game;
            game.setReferenceAudioLibrary (&library);
            game.prepare ({ 48000.0, 512, 2 });
            const auto& p = library.getActiveContent();

            for (int level = 1; level <= 10; level += 3)
            {
                game.setDifficulty (level);
                for (int round = 0; round < 60; ++round)
                {
                    game.newRound();
                    const auto& range = FrequencyRangeGame::ranges[(size_t) game.getCorrectRangeForTest()];
                    expect (p.hasEnergyBetween (range.lowHz, range.highHz),
                            juce::String ("level ") + juce::String (level) + " asked " + range.label);
                    expect (p.isPresentAt (game.getCorrectFrequencyHzForTest()),
                            "boost at " + juce::String (game.getCorrectFrequencyHzForTest()) + " Hz");
                    // Still inside its own range - a frequency that wandered
                    // out of it would score a right answer as wrong.
                    expect (game.getCorrectFrequencyHzForTest() >= range.lowHz
                            && game.getCorrectFrequencyHzForTest() < range.highHz);
                }
            }
        }

        beginTest ("Find the Frequency on hi-hats only boosts where there is sound");
        {
            EQGame game;
            game.setReferenceAudioLibrary (&library);
            game.prepare ({ 48000.0, 512, 2 });
            const auto& p = library.getActiveContent();

            for (int round = 0; round < 200; ++round)
            {
                game.newRound();
                const auto hz = EQGame::normalisedToFrequency (game.getCorrectNormalised());
                expect (p.isPresentAt (hz), "target " + juce::String (hz) + " Hz");
            }
        }

        beginTest ("A kick never asks for air");
        {
            expect (kick.existsAsFile(), "starter kick clip");
            expect (library.selectFile (kick, 48000.0));
            const auto& p = library.getActiveContent();
            FrequencyRangeGame game;
            game.setReferenceAudioLibrary (&library);
            game.prepare ({ 48000.0, 512, 2 });

            for (int round = 0; round < 80; ++round)
            {
                game.newRound();
                const auto& range = FrequencyRangeGame::ranges[(size_t) game.getCorrectRangeForTest()];
                expect (p.hasEnergyBetween (range.lowHz, range.highHz), juce::String ("kick asked ") + range.label);
            }
        }

        beginTest ("A single tone is skipped by the frequency exercises, not by the others");
        {
            expect (tone.existsAsFile(), "built-in tone");
            expect (library.selectFile (tone, 48000.0));
            expectLessThan (library.getActiveContent().presentOctaves(), TestSignalGenerator::minPresentOctaves);

            TestSignalGenerator forEq;
            forEq.setLibrary (&library);
            forEq.setNeed (TestSignalGenerator::Need::spectrum);
            expect (forEq.isSkippingChosenClip());
            expect (! forEq.activeContent().analysed, "the noise that plays instead has everything");

            TestSignalGenerator forPan;
            forPan.setLibrary (&library);
            expect (! forPan.isSkippingChosenClip());
        }

        beginTest ("A battle round is one round, on the app and on the judge");
        {
            // GameManager::startSeededRound is what the app, the server's
            // RoundOracle and BattleSmoke all call. Two apps at two device
            // rates must agree on the answer for every seed, now that the
            // answer depends on which clip the seed picks.
            auto optionsB = tempOptions();
            optionsB.folderName << "_b";
            juce::PropertiesFile propertiesB (optionsB);
            ReferenceAudioLibrary libraryB (propertiesB);
            libraryB.setRootFolder (library.getRootFolder());

            EQGame eqA, eqB;
            FrequencyRangeGame rangeA, rangeB;
            const std::array<std::pair<Game*, Game*>, 2> pairs { { { &eqA, &eqB }, { &rangeA, &rangeB } } };

            for (const auto& [gameA, gameB] : pairs)
            {
                gameA->setReferenceAudioLibrary (&library);
                gameB->setReferenceAudioLibrary (&libraryB);
                gameA->prepare ({ 44100.0, 512, 2 });
                gameB->prepare ({ 48000.0, 512, 2 });

                auto differentClips = 0;
                juce::String lastClip;

                for (juce::int64 seed = 1; seed <= 60; ++seed)
                {
                    GameManager::startSeededRound (library, *gameA, 5, seed * 7919, 44100.0);
                    GameManager::startSeededRound (libraryB, *gameB, 5, seed * 7919, 48000.0);
                    expectEquals (gameA->getCorrectChoiceIndex(), gameB->getCorrectChoiceIndex(), gameA->getName());
                    expectWithinAbsoluteError (gameA->getCorrectNormalised(), gameB->getCorrectNormalised(), 1.0e-6f,
                                               gameA->getName() + " seed " + juce::String (seed));

                    if (library.getSelectedFile().getFileName() != lastClip)
                        ++differentClips;
                    lastClip = library.getSelectedFile().getFileName();
                }

                expectGreaterThan (differentClips, 10, "the seeds really did play different clips");
            }

            libraryB.clearSelection();
            optionsB.getDefaultFile().deleteFile();
        }

        library.clearSelection();
        options.getDefaultFile().deleteFile();
    }

private:
    void printEveryBuiltIn (ReferenceAudioLibrary& library)
    {
        for (const auto& category : library.getCategories())
        {
            for (const auto& file : category.files)
            {
                if (! library.selectFile (file, 48000.0))
                    continue;

                const auto& p = library.getActiveContent();
                juce::String line;
                line << file.getFileName().paddedRight (' ', 36) << " oct " << juce::String (p.presentOctaves(), 1)
                     << (p.isHits() ? " hits " : " held ") << juce::String (p.onsetsPerSecond, 1) << "/s "
                     << juce::String (p.envelopeCrestDb, 0) << "dB sus " << juce::String (library.getActiveSustain(), 2) << " | ";
                for (const auto& r : FrequencyRangeGame::ranges)
                    line << (p.hasEnergyBetween (r.lowHz, r.highHz) ? juce::String (r.label).substring (0, 4) : juce::String ("----")) << " ";
                line << "| ";
                for (int b = 0; b < SpectralContent::numBands; b += 3)
                    line << juce::String (juce::roundToInt (p.relativeDb[(size_t) b])) << " ";
                logMessage (line);
            }
        }
    }
};

static SpectralContentTest spectralContentTest;
