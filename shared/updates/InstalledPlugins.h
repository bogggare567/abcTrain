#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// What of this product is actually on this machine, and where.
//
// The update flow used to say "a newer version exists" and nothing else,
// which leaves the one question anybody actually has unanswered: *what am
// I updating?* Someone with the VST3 in one place, the AU in another and a
// stale copy in a folder they forgot about has no way to know which of
// them the installer is about to replace - and no way to know afterwards
// whether it worked.
//
// This scans the standard locations for the four plugins and reports what
// it finds. It only ever reads directory names; nothing here writes,
// moves, or deletes anything, because an updater that tidies up after you
// unasked is an updater that eventually deletes the wrong thing.
namespace InstalledPlugins
{
    struct Found
    {
        juce::String product;    // "ABC Learner EQ"
        juce::String format;     // "VST3" / "AU" / "Standalone"
        juce::File location;
        juce::String version;    // from the bundle's plist where readable, else empty
    };

    inline juce::StringArray productNames()
    {
        return { "ABC Ear Trainer", "ABC Learner EQ", "ABC Learner Comp", "ABC Learner Verb" };
    }

    // The places each platform's installers actually write to - the same
    // list installer/macos/distribution.xml, installer/windows_setup.iss
    // and installer/linux/install.sh use. Both the shared and the per-user
    // roots, because the macOS installer offers that choice and someone
    // who picked "install for me only" is otherwise told nothing is
    // installed.
    inline std::vector<std::pair<juce::File, juce::String>> searchRoots()
    {
        using juce::File;
        std::vector<std::pair<File, juce::String>> roots;

       #if JUCE_MAC
        const auto userHome = File::getSpecialLocation (File::userHomeDirectory);
        roots.emplace_back (File ("/Library/Audio/Plug-Ins/VST3"), "VST3");
        roots.emplace_back (userHome.getChildFile ("Library/Audio/Plug-Ins/VST3"), "VST3");
        roots.emplace_back (File ("/Library/Audio/Plug-Ins/Components"), "AU");
        roots.emplace_back (userHome.getChildFile ("Library/Audio/Plug-Ins/Components"), "AU");
        roots.emplace_back (File ("/Applications"), "Standalone");
       #elif JUCE_WINDOWS
        roots.emplace_back (File::getSpecialLocation (File::commonApplicationDataDirectory)
                                .getSiblingFile ("Common Files").getChildFile ("VST3"), "VST3");
        roots.emplace_back (File::getSpecialLocation (File::globalApplicationsDirectory)
                                .getChildFile ("abcTrain"), "Standalone");
       #else
        const auto userHome = File::getSpecialLocation (File::userHomeDirectory);
        roots.emplace_back (userHome.getChildFile (".vst3"), "VST3");
        roots.emplace_back (File ("/usr/lib/vst3"), "VST3");
        roots.emplace_back (userHome.getChildFile (".local/bin"), "Standalone");
       #endif

        return roots;
    }

    // Best-effort, and deliberately so: a bundle whose plist cannot be read
    // reports an empty version rather than a guess. "Version unknown" is a
    // true statement; a wrong number is not.
    inline juce::String readBundleVersion (const juce::File& bundle)
    {
        const auto plist = bundle.getChildFile ("Contents/Info.plist");

        if (! plist.existsAsFile())
            return {};

        if (auto xml = juce::XmlDocument::parse (plist))
        {
            // A plist <dict> is a flat alternating list of <key> and its
            // value, so the version is the element *after* the matching key
            // rather than a child of it.
            if (auto* dict = xml->getChildByName ("dict"))
            {
                auto wanted = false;

                for (auto* child : dict->getChildIterator())
                {
                    if (wanted)
                        return child->getAllSubText().trim();

                    if (child->hasTagName ("key")
                        && child->getAllSubText().trim() == "CFBundleShortVersionString")
                        wanted = true;
                }
            }
        }

        return {};
    }

    inline std::vector<Found> scan()
    {
        std::vector<Found> found;
        const auto products = productNames();

        for (const auto& [root, format] : searchRoots())
        {
            if (! root.isDirectory())
                continue;

            for (const auto& product : products)
            {
                // One pattern per format rather than a directory walk: these
                // folders can hold hundreds of other people's plugins, and
                // scanning all of them to find four is rude with someone
                // else's disk.
                for (const auto* extension : { ".vst3", ".component", ".app", ".exe", "" })
                {
                    const auto candidate = root.getChildFile (product + extension);

                    if (! candidate.exists())
                        continue;

                    Found entry;
                    entry.product = product;
                    entry.format = format;
                    entry.location = candidate;
                    entry.version = readBundleVersion (candidate);
                    found.push_back (std::move (entry));
                    break;
                }
            }
        }

        return found;
    }
}
