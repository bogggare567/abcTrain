#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/AudiometryProcedure.h"
#include "../Source/ProbeTone.h"
#include "shared/audio/HearingProfile.h"
#include "shared/dsp/HearingCompensation.h"
#include <iostream>
#include <random>

// ADR 051: the hearing calibration - the procedure, the profile, the
// compensation it turns into.
class HearingCalibrationTest : public juce::UnitTest
{
public:
    HearingCalibrationTest() : juce::UnitTest ("HearingCalibration", "Hearing") {}

    // A listener with known thresholds. Hears a tone with a logistic
    // probability around the threshold (1.5 dB spread - a real ear's
    // psychometric function for pure tones is that steep), never "hears"
    // a silence unless told to lie.
    struct Listener
    {
        std::array<std::array<float, HearingProfile::numFrequencies>, 2> truth {};
        std::mt19937 random;
        bool lieOnSilenceLeft = false;
        int lieBudget = 1 << 30;          // how many silent trials it lies on
        float retestShift = 0.0f;         // added to 1 kHz on the retest: fatigue
        float spreadDb = 1.5f;            // 0: hears exactly at and above threshold

        explicit Listener (juce::int64 seed) : random ((std::mt19937::result_type) seed) {}

        bool answer (const AudiometryProcedure::Presentation& p)
        {
            if (p.silent)
            {
                if (p.ear == AudiometryProcedure::Ear::left && lieOnSilenceLeft && lieBudget > 0)
                {
                    --lieBudget;
                    return true;
                }

                return false;
            }

            auto t = truth[(size_t) p.ear][(size_t) HearingProfile::indexOf (p.freqHz)];

            if (p.retest)
                t += retestShift;

            if (spreadDb <= 0.0f)
                return p.levelDb >= t;

            const auto probability = 1.0 / (1.0 + std::exp (-((double) p.levelDb - (double) t) / (double) spreadDb));
            return std::uniform_real_distribution<double> (0.0, 1.0) (random) < probability;
        }
    };

    static void run (AudiometryProcedure& procedure, Listener& listener)
    {
        for (int guard = 0; guard < 5000 && ! procedure.isFinished(); ++guard)
            procedure.answer (listener.answer (procedure.current()));
    }

    static Listener typicalListener (juce::int64 seed)
    {
        Listener l (seed);
        //              250     500     1k      2k      3k      4k      6k      8k
        l.truth[0] = { -61.0f, -67.5f, -72.0f, -70.0f, -66.0f, -63.0f, -59.0f, -57.5f };
        l.truth[1] = { -59.0f, -66.0f, -71.0f, -68.5f, -62.0f, -58.0f, -48.0f, -43.0f };
        return l;
    }

    void runTest() override
    {
        beginTest ("a consistent listener: every threshold within 5 dB of the truth");
        {
            // Hears every tone at or above its threshold and none below:
            // what is left is the procedure's own 5 dB grid.
            for (juce::int64 seed = 1; seed <= 10; ++seed)
            {
                auto listener = typicalListener (seed);
                listener.spreadDb = 0.0f;
                AudiometryProcedure procedure (seed);
                run (procedure, listener);
                expect (procedure.isFinished(), "the procedure finishes");

                HearingProfile p;
                procedure.fillProfile (p);

                for (int ear = 0; ear < 2; ++ear)
                    for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
                    {
                        const auto measured = (ear == 0 ? p.left : p.right).threshold[i];
                        const auto truth = listener.truth[(size_t) ear][i];
                        expect (std::abs (measured - truth) <= 5.0f,
                                "seed " + juce::String (seed) + ", ear " + juce::String (ear) + ", "
                                    + juce::String (HearingProfile::frequencies[i]) + " Hz: measured "
                                    + juce::String (measured) + ", truth " + juce::String (truth));
                    }

                expect (p.retestAgrees(), "a consistent listener agrees with himself at 1 kHz");
                expect (p.isReliable());
                expectEquals (p.left.falseAlarms + p.right.falseAlarms, 0);
                expect (p.left.catchTrials + p.right.catchTrials > 0, "catch trials happen");
            }
        }

        beginTest ("a noisy listener: most within 5 dB, none beyond 10, the method's own repeatability");
        {
            // A logistic psychometric function, 1.5 dB spread (about 17 %
            // per dB at the 50 % point). With 5 dB steps, "two of three
            // ascents" lands one step high whenever the level just above
            // the 50 % point is missed twice - about one measurement in
            // ten with these thresholds, which sit just under a grid level
            // on purpose (the worst case). That is the method's known
            // +-5 dB repeatability (BSA 2018; ISO 8253-1), and the reason
            // the compensation ignores differences under 5 dB - not a
            // fault to tune away. Measured: 89 % within 5 dB, worst 8.5,
            // mean +2.7 (ascending methods read slightly high).
            auto within5 = 0, total = 0;
            auto worst = 0.0f;
            auto bias = 0.0;

            for (juce::int64 seed = 1; seed <= 25; ++seed)
            {
                auto listener = typicalListener (seed * 31);
                AudiometryProcedure procedure (seed);

                if (std::getenv ("HP_TRACE") != nullptr && seed == juce::String (std::getenv ("HP_TRACE")).getIntValue())
                {
                    while (! procedure.isFinished())
                    {
                        const auto pr = procedure.current();
                        const auto a = listener.answer (pr);
                        std::cout << (int) pr.ear << " " << pr.freqHz << " " << pr.levelDb << (pr.silent ? " S" : "") << " -> " << a << "\n";
                        procedure.answer (a);
                    }
                }

                run (procedure, listener);
                expect (procedure.isFinished(), "the procedure finishes");

                HearingProfile p;
                procedure.fillProfile (p);

                for (int ear = 0; ear < 2; ++ear)
                    for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
                    {
                        const auto error = (ear == 0 ? p.left : p.right).threshold[i] - listener.truth[(size_t) ear][i];
                        worst = juce::jmax (worst, std::abs (error));
                        bias += (double) error;
                        within5 += std::abs (error) <= 5.0f ? 1 : 0;
                        ++total;
                    }

                expectEquals (p.left.falseAlarms + p.right.falseAlarms, 0);
            }

            const auto share = (double) within5 / (double) total;
            bias /= total;
            logMessage ("noisy listener: " + juce::String (share * 100.0, 1) + " % within 5 dB, worst "
                        + juce::String (worst, 1) + " dB, mean error " + juce::String (bias, 1) + " dB");
            expect (share >= 0.85, "share within 5 dB: " + juce::String (share));
            expect (worst <= 10.0f, "worst error: " + juce::String (worst));
            expect (bias > -2.0 && bias < 5.0, "mean error: " + juce::String (bias));
        }

        beginTest ("the short run: octaves 1, 2, 4, 8 kHz, 500, 250 Hz, left then right");
        {
            AudiometryProcedure::Config config;
            config.catchOneIn = 0;
            AudiometryProcedure procedure (3, config);
            auto listener = typicalListener (5);
            juce::Array<float> seen;
            auto lastStep = -1;
            auto lastEar = AudiometryProcedure::Ear::left;

            while (! procedure.isFinished())
            {
                const auto& p = procedure.current();

                if (p.step != lastStep || p.ear != lastEar)
                {
                    seen.add (p.freqHz);
                    expectEquals (p.levelDb, -40.0f, "each frequency starts at a comfortable level");
                    lastStep = p.step;
                    lastEar = p.ear;
                }

                procedure.answer (listener.answer (p));
            }

            const float expected[] { 1000, 2000, 4000, 8000, 500, 250,
                                     1000, 2000, 4000, 8000, 500, 250 };
            expectEquals (seen.size(), 12);

            for (int i = 0; i < juce::jmin (12, seen.size()); ++i)
                expectEquals (seen[i], expected[i]);
        }

        beginTest ("down 10 after heard, up 5 after not heard, within -100..-10");
        {
            AudiometryProcedure::Config config;
            config.catchOneIn = 0;
            AudiometryProcedure procedure (1, config);

            expectEquals (procedure.current().levelDb, -40.0f);
            procedure.answer (true);
            expectEquals (procedure.current().levelDb, -50.0f);
            procedure.answer (false);
            expectEquals (procedure.current().levelDb, -45.0f);

            // Never heard: climbs to -10 and records "not heard" there.
            AudiometryProcedure deaf (2, config);
            auto top = -200.0f;

            while (deaf.current().step == 0 && ! deaf.isFinished())
            {
                top = juce::jmax (top, deaf.current().levelDb);
                deaf.answer (false);
            }

            expectEquals (top, -10.0f, "never louder than -10 dBFS");
            expect (HearingProfile::isNotHeard (deaf.result (AudiometryProcedure::Ear::left).first1k));

            // Always heard: goes no quieter than -100 and stops there.
            AudiometryProcedure keen (3, config);
            auto bottom = 0.0f;

            while (keen.current().step == 0 && ! keen.isFinished())
            {
                bottom = juce::jmin (bottom, keen.current().levelDb);
                keen.answer (true);
            }

            expectEquals (bottom, -100.0f, "never quieter than -100 dBFS");
            expectEquals (keen.result (AudiometryProcedure::Ear::left).first1k, -100.0f);
        }

        beginTest ("\"heard\" on silence twice: the ear is measured again; twice more: unreliable");
        {
            {
                auto listener = typicalListener (77);
                listener.lieOnSilenceLeft = true;
                listener.lieBudget = 2;            // two lies, then honest

                AudiometryProcedure procedure (11);
                run (procedure, listener);

                const auto& left = procedure.result (AudiometryProcedure::Ear::left);
                expect (left.remeasured, "the left ear starts over");
                expect (! left.unreliable, "and is fine the second time");

                HearingProfile p;
                procedure.fillProfile (p);
                expect (p.isReliable() || ! p.retestAgrees());
            }

            {
                auto listener = typicalListener (78);
                listener.lieOnSilenceLeft = true;  // lies every time

                AudiometryProcedure procedure (12);
                run (procedure, listener);
                expect (procedure.isFinished(), "a liar does not loop forever");

                const auto& left = procedure.result (AudiometryProcedure::Ear::left);
                const auto& right = procedure.result (AudiometryProcedure::Ear::right);
                expect (left.remeasured);
                expect (left.unreliable);
                expect (! right.remeasured && ! right.unreliable, "the other ear is not blamed");

                HearingProfile p;
                procedure.fillProfile (p);
                expect (! p.isReliable());
            }
        }

        beginTest ("3 and 6 kHz come from their octave neighbours; no retest is not a disagreement");
        {
            auto listener = typicalListener (91);
            AudiometryProcedure::Config config;
            config.catchOneIn = 0;
            AudiometryProcedure procedure (21, config);
            run (procedure, listener);

            HearingProfile p;
            procedure.fillProfile (p);
            for (const auto* ear : { &p.left, &p.right })
            {
                expect (HearingProfile::isMeasured (ear->threshold[4]) && HearingProfile::isMeasured (ear->threshold[6]));
                if (! HearingProfile::isNotHeard (ear->threshold[4]))
                    expectWithinAbsoluteError (ear->threshold[4], 0.5f * (ear->threshold[3] + ear->threshold[5]), 0.01f);
            }
            expect (p.retestAgrees(), "nothing measured twice, nothing disagrees");

            // A profile that does carry a retest 15 dB off is still flagged.
            HearingProfile q;
            q.left.first1k = -70.0f;  q.left.retest1k = -55.0f;
            expect (! q.retestAgrees());
            expect (! q.isReliable());
        }

        beginTest ("compensation: dead zone, amount, cap, the better ear as reference, never a cut");
        {
            expectEquals (HearingProfile::gainFor (4.0f, 100), 0.0f);
            expectEquals (HearingProfile::gainFor (5.0f, 100), 0.0f);
            expectEquals (HearingProfile::gainFor (10.0f, 100), 5.0f);
            expectEquals (HearingProfile::gainFor (10.0f, 50), 2.5f);
            expectEquals (HearingProfile::gainFor (14.0f, 50), 4.5f);
            expectEquals (HearingProfile::gainFor (40.0f, 100), 12.0f);
            expectEquals (HearingProfile::gainFor (30.0f, 50), 12.0f);
            expectEquals (HearingProfile::gainFor (20.0f, 0), 0.0f);
            expectEquals (HearingProfile::gainFor (-20.0f, 100), 0.0f);

            HearingProfile p;
            p.left.threshold.fill (-60.0f);
            p.right.threshold.fill (-60.0f);
            p.left.threshold[5] = -60.0f;  p.right.threshold[5] = -50.0f;   // 4 kHz: right 10 dB worse
            p.left.threshold[1] = -48.0f;  p.right.threshold[1] = -60.0f;   // 500 Hz: left 12 dB worse
            p.left.threshold[7] = -40.0f;  p.right.threshold[7] = HearingProfile::notHeard;   // 8 kHz
            p.left.threshold[0] = -60.0f;  p.right.threshold[0] = -57.0f;   // 250 Hz: 3 dB, ignored
            p.amountPercent = 50;

            const auto l = p.compensationDb (0);
            const auto r = p.compensationDb (1);

            expectEquals (l[5], 0.0f);   expectEquals (r[5], 2.5f);
            expectEquals (l[1], 3.5f);   expectEquals (r[1], 0.0f);
            expectEquals (l[7], 0.0f);   expectEquals (r[7], 12.0f);   // 0.5 x (35 - 5) = 15, capped
            expectEquals (l[0], 0.0f);   expectEquals (r[0], 0.0f);

            for (auto g : l) expect (g >= 0.0f);
            for (auto g : r) expect (g >= 0.0f);

            p.amountPercent = 100;
            expectEquals (p.compensationDb (1)[5], 5.0f);
        }

        beginTest ("the filters hit the asked gain at each test frequency within 0.5 dB");
        {
            for (auto rate : { 44100.0, 48000.0, 96000.0 })
            {
                // One band at a time...
                for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
                {
                    std::array<float, HearingProfile::numFrequencies> gains {};
                    gains[i] = 6.0f;
                    const auto bands = HearingCompensation::makeBands (HearingProfile::frequencies, gains);
                    const auto db = HearingCompensation::responseDb (bands, HearingProfile::frequencies[i], rate);
                    expectWithinAbsoluteError (db, 6.0, 0.5, juce::String (HearingProfile::frequencies[i]) + " Hz at " + juce::String (rate));
                }

                // ...and neighbours together, where single bells would pile up.
                const std::array<float, HearingProfile::numFrequencies> gains { 0, 0, 0, 0, 3.0f, 6.0f, 9.0f, 12.0f };
                const auto bands = HearingCompensation::makeBands (HearingProfile::frequencies, gains);

                for (size_t i = 4; i < (size_t) HearingProfile::numFrequencies; ++i)
                    expectWithinAbsoluteError (HearingCompensation::responseDb (bands, HearingProfile::frequencies[i], rate),
                                               (double) gains[i], 0.5,
                                               juce::String (HearingProfile::frequencies[i]) + " Hz in a group, at " + juce::String (rate));

                // Far from the boosted bands nothing happens.
                expectWithinAbsoluteError (HearingCompensation::responseDb (bands, 250.0, rate), 0.0, 0.3);
            }
        }

        beginTest ("the processor: right channel boosted as designed, left untouched, off is bit-exact");
        {
            const double rate = 48000.0;
            std::array<float, HearingProfile::numFrequencies> none {}, right {};
            right[3] = 6.0f;   // 2 kHz

            HearingCompensation::Processor processor;
            processor.prepare (rate);
            processor.setBands (HearingCompensation::makeBands (HearingProfile::frequencies, none),
                                HearingCompensation::makeBands (HearingProfile::frequencies, right), true);

            const auto rmsAfter = [&] (int channel)
            {
                juce::AudioBuffer<float> buffer (2, 512);
                double sum = 0.0, sumIn = 0.0;

                for (int block = 0; block < 40; ++block)
                {
                    for (int i = 0; i < 512; ++i)
                        for (int ch = 0; ch < 2; ++ch)
                            buffer.setSample (ch, i, 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * 2000.0
                                                                               * (double) (block * 512 + i) / rate));

                    const auto input = buffer.getSample (channel, 100);
                    juce::ignoreUnused (input);

                    if (block >= 10)
                        for (int i = 0; i < 512; ++i)
                            sumIn += std::pow (buffer.getSample (channel, i), 2.0);

                    processor.process (buffer);

                    if (block >= 10)
                        for (int i = 0; i < 512; ++i)
                            sum += std::pow (buffer.getSample (channel, i), 2.0);
                }

                return 10.0 * std::log10 (sum / sumIn);
            };

            expectWithinAbsoluteError (rmsAfter (1), 6.0, 0.5);
            expectWithinAbsoluteError (rmsAfter (0), 0.0, 0.05);

            processor.setBands (HearingCompensation::makeBands (HearingProfile::frequencies, none),
                                HearingCompensation::makeBands (HearingProfile::frequencies, right), false);

            juce::AudioBuffer<float> buffer (2, 256), copy (2, 256);
            juce::Random random (4);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    buffer.setSample (ch, i, random.nextFloat() - 0.5f);

            copy.makeCopyOf (buffer);
            processor.process (buffer);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 256; ++i)
                    expectEquals (buffer.getSample (ch, i), copy.getSample (ch, i));
        }

        beginTest ("the probe: three 250 ms pulses after the pause, in one ear, at the level asked for");
        {
            const double rate = 48000.0;
            ProbeTone probe;
            probe.prepare (rate);
            probe.present (1, 1000.0f, -20.0f, 0.5, false);

            const auto total = (int) (rate * (0.5 + ProbeTone::stimulusSeconds + 0.2));
            juce::AudioBuffer<float> all (2, total);
            juce::AudioBuffer<float> block (2, 480);

            for (int pos = 0; pos < total; pos += 480)
            {
                probe.render (block);
                const auto n = juce::jmin (480, total - pos);

                for (int ch = 0; ch < 2; ++ch)
                    all.copyFrom (ch, pos, block, ch, 0, n);
            }

            expectEquals (all.getMagnitude (0, 0, total), 0.0f, "nothing in the other ear");
            expectEquals (all.getMagnitude (1, 0, (int) (rate * 0.5)), 0.0f, "nothing during the pause");
            expectWithinAbsoluteError (all.getMagnitude (1, 0, total), juce::Decibels::decibelsToGain (-20.0f), 0.002f);

            // Pulses: count the runs of sound.
            auto runs = 0;
            auto inRun = false;
            auto quietFor = 0;

            for (int i = 0; i < total; ++i)
            {
                const auto loud = std::abs (all.getSample (1, i)) > 1.0e-4f;

                if (loud && ! inRun) { ++runs; inRun = true; }
                quietFor = loud ? 0 : quietFor + 1;
                if (inRun && quietFor > (int) (rate * 0.01)) inRun = false;
            }

            expectEquals (runs, 3);

            // A catch trial is silence for just as long.
            probe.present (0, 1000.0f, -20.0f, 0.1, true);
            probe.render (block);
            expectEquals (block.getMagnitude (0, 0, 480), 0.0f);
        }

        beginTest ("a profile survives JSON and the settings file");
        {
            HearingProfile p;
            p.id = HearingProfileStore::newId();
            p.person = juce::String::fromUTF8 ("Алёна");
            p.headphones = "DT 770 Pro";
            p.created = juce::Time (2026, 9, 2, 14, 30);
            p.amountPercent = 70;

            for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
            {
                p.left.threshold[i] = -70.0f + (float) i;
                p.right.threshold[i] = -65.0f - (float) i;
            }

            p.right.threshold[7] = HearingProfile::notHeard;
            p.left.threshold[0] = std::nanf ("");
            p.left.first1k = -70.0f;  p.left.retest1k = -66.0f;
            p.right.falseAlarms = 1;  p.right.catchTrials = 9;
            p.right.remeasured = true;

            const auto back = HearingProfile::fromVar (juce::JSON::parse (juce::JSON::toString (p.toVar())));

            expectEquals (back.id, p.id);
            expectEquals (back.person, p.person);
            expectEquals (back.headphones, p.headphones);
            expect (back.created == p.created);
            expectEquals (back.amountPercent, 70);
            expect (! HearingProfile::isMeasured (back.left.threshold[0]));
            expect (HearingProfile::isNotHeard (back.right.threshold[7]));

            for (size_t i = 1; i < (size_t) HearingProfile::numFrequencies; ++i)
                expectEquals (back.left.threshold[i], p.left.threshold[i]);

            expectEquals (back.left.retest1k, -66.0f);
            expectEquals (back.right.falseAlarms, 1);
            expectEquals (back.right.catchTrials, 9);
            expect (back.right.remeasured);
            expect (! back.right.unreliable);

            // Through the store and the file, as the app and Learner EQ see it.
            juce::PropertiesFile::Options options;
            options.applicationName = "EarTrainerTests";
            options.filenameSuffix = "settings";
            options.folderName = "EarTrainerTests_HearingProfiles";
            options.getDefaultFile().deleteFile();

            const auto now = juce::Time (2026, 9, 10, 10, 0);

            {
                juce::PropertiesFile file (options);
                HearingProfileStore store (file);
                expect (store.shouldOffer (now), "no profile: offer the test");
                store.declineOffer (now);
                expect (! store.shouldOffer (now + juce::RelativeTime::days (6)), "not now: quiet for a week");
                expect (store.shouldOffer (now + juce::RelativeTime::days (7)));

                store.save (p);
                store.setApplying (false);
                store.rememberNames (p.person, p.headphones);
            }

            {
                juce::PropertiesFile file (options);
                HearingProfileStore store (file);
                expectEquals ((int) store.getProfiles().size(), 1);
                expect (store.getActive() != nullptr && store.getActive()->id == p.id);
                expect (! store.isApplying());
                expect (! store.currentCompensation().active, "switch off: nothing applied");
                expectEquals (store.lastPerson(), p.person);
                expect (! store.shouldOffer (now + juce::RelativeTime::days (30)), "a profile exists: no offer");

                store.setApplying (true);
                const auto c = store.currentCompensation();
                expect (c.active);
                expectEquals (c.right[5], back.compensationDb (1)[5]);
            }

            options.getDefaultFile().deleteFile();
        }
    }
};

static HearingCalibrationTest hearingCalibrationTest;
