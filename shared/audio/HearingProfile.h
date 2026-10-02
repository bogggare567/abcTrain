#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <cmath>
#include <vector>

// A hearing profile: one person in one pair of headphones (ADR 051).
//
// What the audiometry test in the app measured - the quietest level each
// ear heard at eight frequencies, in dBFS at the app's output - and how
// much of the difference between the ears the Studio should make up.
//
// The numbers are relative on purpose. dBFS is not dB HL: nobody here
// knows what a headphone does with a full-scale sine, so a threshold of
// -72 dBFS says nothing about a person's hearing in the clinic's sense.
// What it does say, reliably, is how the two ears compare *in this pair at
// this volume*, and that comparison is all the compensation uses: each
// ear is measured against the better ear at the same frequency.
//
// Pure model: no GUI and no audio. The app writes profiles; Learner EQ in
// a DAW only reads them, to draw what the Studio would do.
struct HearingProfile
{
    // The frequencies a profile holds, low to high. The test plays them in
    // the BSA order (1, 2, 3, 4, 6, 8 kHz, 1 kHz again, 500, 250 Hz); this
    // is only how they are stored.
    static constexpr int numFrequencies = 8;
    static constexpr std::array<float, numFrequencies> frequencies { 250.0f, 500.0f, 1000.0f, 2000.0f,
                                                                     3000.0f, 4000.0f, 6000.0f, 8000.0f };

    static int indexOf (float frequencyHz) noexcept
    {
        for (int i = 0; i < numFrequencies; ++i)
            if (std::abs (frequencies[(size_t) i] - frequencyHz) < 1.0f)
                return i;

        return -1;
    }

    // The loudest level the test plays. A frequency an ear did not hear
    // even there is stored as `notHeard`; for the compensation it counts as
    // a little above this level, so the far ear gets the full boost and no
    // more.
    static constexpr float maxLevelDb = -10.0f;
    static constexpr float minLevelDb = -100.0f;
    static constexpr float notHeard = 1000.0f;

    // The compensation rule (ADR 051): differences under deadZoneDb are
    // left alone - they are inside the test's own step size - and no boost
    // is ever larger than maxBoostDb.
    static constexpr float deadZoneDb = 5.0f;
    static constexpr float maxBoostDb = 12.0f;
    static constexpr int defaultAmountPercent = 50;

    static bool isMeasured (float threshold) noexcept { return ! std::isnan (threshold); }
    static bool isNotHeard (float threshold) noexcept { return threshold >= notHeard; }

    struct Ear
    {
        // dBFS; NaN = not measured; notHeard = not heard at maxLevelDb.
        std::array<float, numFrequencies> threshold;

        // The two 1 kHz measurements (start and end of the ear) - the
        // test-retest check. NaN until both exist.
        float first1k = std::nanf (""), retest1k = std::nanf ("");

        int falseAlarms = 0;        // "heard" on a silent catch trial
        int catchTrials = 0;
        bool remeasured = false;    // two false alarms: the ear was measured again
        bool unreliable = false;    // ...and it happened again

        Ear() { threshold.fill (std::nanf ("")); }

        // |first - retest| at 1 kHz, or NaN.
        float retestDifference() const noexcept;
    };

    juce::String id, person, headphones;
    juce::Time created;
    Ear left, right;
    int amountPercent = defaultAmountPercent;

    // "Анна × HD 600"
    juce::String pairName() const;

    // The 1 kHz retest agreed within deadZoneDb on both ears.
    bool retestAgrees() const noexcept;

    // Retest agrees and neither ear was left flagged by false alarms.
    bool isReliable() const noexcept;

    // Per test frequency, the boost the Studio applies to `channel` (0 left,
    // 1 right), in dB, never negative. See gainFor().
    std::array<float, numFrequencies> compensationDb (int channel) const;

    // amount x max(0, D - deadZone), capped at maxBoostDb, where D is how
    // many dB worse this ear is than the better one.
    static float gainFor (float differenceDb, int amountPercent) noexcept;

    juce::var toVar() const;
    static HearingProfile fromVar (const juce::var&);
};

// Saved profiles, which one is active, and the Studio's switch.
//
// In a file of its own (hearing.settings in the shared abcTrain folder),
// not in the library's settings file every plugin also holds open: each
// open juce::PropertiesFile writes back everything it read at load, so a
// Learner in a DAW that saved its practice source would have put back
// the profile list as it was when the DAW started. One writer (the app),
// readers that reload when the file changes.
class HearingProfileStore
{
public:
    explicit HearingProfileStore (juce::PropertiesFile&);

    static juce::PropertiesFile::Options makeDefaultOptions();

    // Re-reads the file (another process may have written it).
    void reload();

    const std::vector<HearingProfile>& getProfiles() const noexcept { return profiles; }
    const HearingProfile* find (const juce::String& id) const;

    // The active profile, or nullptr.
    const HearingProfile* getActive() const;
    void setActive (const juce::String& id);   // empty: none

    // Adds the profile, or replaces the one with the same id, and makes it
    // active.
    void save (const HearingProfile&);

    // The Studio switch. On by default: a pair is saved to be used.
    bool isApplying() const;
    void setApplying (bool);

    // The active profile's boosts if a profile is active and the switch is
    // on; empty (all zero, active = false) otherwise.
    struct Compensation
    {
        bool active = false;
        std::array<float, HearingProfile::numFrequencies> left {}, right {};
    };

    Compensation currentCompensation() const;

    // The names typed into the test's form last time.
    juce::String lastPerson() const;
    juce::String lastHeadphones() const;
    void rememberNames (const juce::String& person, const juce::String& headphones);

    // The offer before training: shown while there is no profile, unless
    // declined in the last offerSnoozeDays days.
    static constexpr int offerSnoozeDays = 7;
    bool shouldOffer (juce::Time now) const;
    void declineOffer (juce::Time now);

    // A new id for a profile.
    static juce::String newId();

private:
    void load();
    void store();

    juce::PropertiesFile& file;
    std::vector<HearingProfile> profiles;
};
