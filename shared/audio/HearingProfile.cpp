#include "HearingProfile.h"

namespace
{
    constexpr const char* profilesKey   = "hearingProfiles";
    constexpr const char* activeKey     = "hearingProfileActive";
    constexpr const char* applyKey      = "hearingApplyInStudio";
    constexpr const char* personKey     = "hearingLastPerson";
    constexpr const char* headphonesKey = "hearingLastHeadphones";
    constexpr const char* declinedKey   = "hearingOfferDeclinedAt";

    // A threshold as JSON: a number, "nh" for not heard, null for not
    // measured.
    juce::var thresholdToVar (float t)
    {
        if (! HearingProfile::isMeasured (t))
            return {};

        if (HearingProfile::isNotHeard (t))
            return "nh";

        return (double) t;
    }

    float thresholdFromVar (const juce::var& v)
    {
        if (v.isString() && v.toString() == "nh")
            return HearingProfile::notHeard;

        if (v.isDouble() || v.isInt() || v.isInt64())
            return (float) (double) v;

        return std::nanf ("");
    }

    juce::var earToVar (const HearingProfile::Ear& ear)
    {
        juce::Array<juce::var> thresholds;

        for (auto t : ear.threshold)
            thresholds.add (thresholdToVar (t));

        auto* o = new juce::DynamicObject();
        o->setProperty ("thresholds", thresholds);
        o->setProperty ("first1k", thresholdToVar (ear.first1k));
        o->setProperty ("retest1k", thresholdToVar (ear.retest1k));
        o->setProperty ("falseAlarms", ear.falseAlarms);
        o->setProperty ("catchTrials", ear.catchTrials);
        o->setProperty ("remeasured", ear.remeasured);
        o->setProperty ("unreliable", ear.unreliable);
        return juce::var (o);
    }

    HearingProfile::Ear earFromVar (const juce::var& v)
    {
        HearingProfile::Ear ear;

        if (const auto* list = v["thresholds"].getArray())
            for (int i = 0; i < juce::jmin (list->size(), HearingProfile::numFrequencies); ++i)
                ear.threshold[(size_t) i] = thresholdFromVar (list->getReference (i));

        ear.first1k = thresholdFromVar (v["first1k"]);
        ear.retest1k = thresholdFromVar (v["retest1k"]);
        ear.falseAlarms = (int) v["falseAlarms"];
        ear.catchTrials = (int) v["catchTrials"];
        ear.remeasured = (bool) v["remeasured"];
        ear.unreliable = (bool) v["unreliable"];
        return ear;
    }

    // For the compensation: "not heard" counts as just above the loudest
    // level played.
    float effective (float t)
    {
        return HearingProfile::isNotHeard (t) ? HearingProfile::maxLevelDb + HearingProfile::deadZoneDb : t;
    }
}

// ---- HearingProfile -------------------------------------------------------

float HearingProfile::Ear::retestDifference() const noexcept
{
    if (! isMeasured (first1k) || ! isMeasured (retest1k))
        return std::nanf ("");

    if (isNotHeard (first1k) && isNotHeard (retest1k))
        return 0.0f;

    return std::abs (effective (first1k) - effective (retest1k));
}

juce::String HearingProfile::pairName() const
{
    return person.trim() + juce::String (juce::CharPointer_UTF8 (" \xc3\x97 ")) + headphones.trim();
}

bool HearingProfile::retestAgrees() const noexcept
{
    for (const auto* ear : { &left, &right })
    {
        const auto d = ear->retestDifference();

        if (! isMeasured (d) || d > deadZoneDb)
            return false;
    }

    return true;
}

bool HearingProfile::isReliable() const noexcept
{
    return retestAgrees() && ! left.unreliable && ! right.unreliable;
}

float HearingProfile::gainFor (float differenceDb, int amountPercent) noexcept
{
    const auto amount = (float) juce::jlimit (0, 100, amountPercent) / 100.0f;
    return juce::jlimit (0.0f, maxBoostDb, amount * juce::jmax (0.0f, differenceDb - deadZoneDb));
}

std::array<float, HearingProfile::numFrequencies> HearingProfile::compensationDb (int channel) const
{
    std::array<float, numFrequencies> gains {};

    for (size_t i = 0; i < (size_t) numFrequencies; ++i)
    {
        const auto l = left.threshold[i];
        const auto r = right.threshold[i];

        if (! isMeasured (l) || ! isMeasured (r) || (isNotHeard (l) && isNotHeard (r)))
            continue;

        const auto better = juce::jmin (effective (l), effective (r));
        const auto mine = effective (channel == 0 ? l : r);
        gains[i] = gainFor (mine - better, amountPercent);
    }

    return gains;
}

juce::var HearingProfile::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("id", id);
    o->setProperty ("person", person);
    o->setProperty ("headphones", headphones);
    o->setProperty ("created", created.toISO8601 (true));

    juce::Array<juce::var> freqs;
    for (auto f : frequencies)
        freqs.add ((double) f);

    o->setProperty ("frequencies", freqs);
    o->setProperty ("left", earToVar (left));
    o->setProperty ("right", earToVar (right));
    o->setProperty ("amount", amountPercent);
    return juce::var (o);
}

HearingProfile HearingProfile::fromVar (const juce::var& v)
{
    HearingProfile p;
    p.id = v["id"].toString();
    p.person = v["person"].toString();
    p.headphones = v["headphones"].toString();
    p.created = juce::Time::fromISO8601 (v["created"].toString());
    p.left = earFromVar (v["left"]);
    p.right = earFromVar (v["right"]);
    p.amountPercent = v.hasProperty ("amount") ? juce::jlimit (0, 100, (int) v["amount"]) : defaultAmountPercent;
    return p;
}

// ---- HearingProfileStore -------------------------------------------------

HearingProfileStore::HearingProfileStore (juce::PropertiesFile& fileToUse)
    : file (fileToUse)
{
    load();
}

juce::PropertiesFile::Options HearingProfileStore::makeDefaultOptions()
{
    // The same folder as the sound library's settings
    // (ReferenceAudioLibrary::makeDefaultOptions), so every plugin finds
    // it; a file of its own - see the class comment.
    juce::PropertiesFile::Options options;
    options.applicationName = "hearing";
    options.filenameSuffix = "settings";
    options.folderName = "abcTrain";
    options.osxLibrarySubFolder = "Application Support";
    options.millisecondsBeforeSaving = 0;
    return options;
}

void HearingProfileStore::reload()
{
    file.reload();
    load();
}

void HearingProfileStore::load()
{
    profiles.clear();
    const auto parsed = juce::JSON::parse (file.getValue (profilesKey));

    if (const auto* list = parsed.getArray())
        for (const auto& item : *list)
        {
            auto p = HearingProfile::fromVar (item);

            if (p.id.isNotEmpty())
                profiles.push_back (std::move (p));
        }
}

void HearingProfileStore::store()
{
    juce::Array<juce::var> list;

    for (const auto& p : profiles)
        list.add (p.toVar());

    file.setValue (profilesKey, juce::JSON::toString (juce::var (list), true));
    file.saveIfNeeded();
}

const HearingProfile* HearingProfileStore::find (const juce::String& id) const
{
    for (const auto& p : profiles)
        if (p.id == id)
            return &p;

    return nullptr;
}

const HearingProfile* HearingProfileStore::getActive() const
{
    const auto id = file.getValue (activeKey);
    return id.isEmpty() ? nullptr : find (id);
}

void HearingProfileStore::setActive (const juce::String& id)
{
    file.setValue (activeKey, find (id) != nullptr ? id : juce::String());
    file.saveIfNeeded();
}

void HearingProfileStore::save (const HearingProfile& profile)
{
    auto replaced = false;

    for (auto& p : profiles)
        if (p.id == profile.id)
        {
            p = profile;
            replaced = true;
        }

    if (! replaced)
        profiles.push_back (profile);

    store();
    setActive (profile.id);
}

bool HearingProfileStore::isApplying() const
{
    return file.getBoolValue (applyKey, true);
}

void HearingProfileStore::setApplying (bool shouldApply)
{
    file.setValue (applyKey, shouldApply);
    file.saveIfNeeded();
}

HearingProfileStore::Compensation HearingProfileStore::currentCompensation() const
{
    Compensation c;

    if (const auto* p = getActive(); p != nullptr && isApplying())
    {
        c.active = true;
        c.left = p->compensationDb (0);
        c.right = p->compensationDb (1);
    }

    return c;
}

juce::String HearingProfileStore::lastPerson() const      { return file.getValue (personKey); }
juce::String HearingProfileStore::lastHeadphones() const  { return file.getValue (headphonesKey); }

void HearingProfileStore::rememberNames (const juce::String& person, const juce::String& headphones)
{
    file.setValue (personKey, person.trim());
    file.setValue (headphonesKey, headphones.trim());
    file.saveIfNeeded();
}

bool HearingProfileStore::shouldOffer (juce::Time now) const
{
    if (! profiles.empty())
        return false;

    const auto declined = file.getValue (declinedKey).getLargeIntValue();

    if (declined <= 0)
        return true;

    return now.toMilliseconds() - declined >= (juce::int64) offerSnoozeDays * 24 * 60 * 60 * 1000;
}

void HearingProfileStore::declineOffer (juce::Time now)
{
    file.setValue (declinedKey, juce::String (now.toMilliseconds()));
    file.saveIfNeeded();
}

juce::String HearingProfileStore::newId()
{
    return juce::Uuid().toDashedString();
}
