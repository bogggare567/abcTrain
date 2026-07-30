#include "UpdatePrompt.h"
#include "Version.h"

namespace UpdatePrompt
{
    namespace
    {
        // "0.0.0-dev+sha4221e0d" is true and useless. It happens when a
        // build came from a clone with no tags fetched, so `git describe`
        // had nothing to describe against - which is a normal thing to
        // happen to somebody building from source, and no reason to show
        // them a hash.
        juce::String describeCurrentVersion (const juce::String& devBuildText)
        {
            const juce::String current (CurrentVersion::string);

            return current.startsWith ("0.0.0-dev") || current == "unknown"
                       ? devBuildText
                       : current;
        }
    }

    bool launchInstaller (const juce::File& downloaded)
    {
        if (! downloaded.existsAsFile())
            return false;

       #if JUCE_MAC
        // hdiutil first, then open the package it contains. Opening the
        // .dmg alone would leave a Finder window and one more step; this
        // way Installer.app comes up already pointed at our package.
        juce::ChildProcess mount;

        if (! mount.start (juce::StringArray { "/usr/bin/hdiutil", "attach", "-nobrowse",
                                                downloaded.getFullPathName() }))
            return false;

        const auto output = mount.readAllProcessOutput();
        mount.waitForProcessToFinish (30000);

        // hdiutil's last column is the mount point, and the volume name
        // can contain spaces - so take everything from "/Volumes" on.
        for (const auto& line : juce::StringArray::fromLines (output))
        {
            const auto at = line.indexOf ("/Volumes/");

            if (at < 0)
                continue;

            const juce::File volume (line.substring (at).trim());

            for (const auto& pkg : volume.findChildFiles (juce::File::findFiles, false, "*.pkg"))
                return pkg.startAsProcess();
        }

        return false;
       #elif JUCE_WINDOWS
        // Inno Setup handles its own elevation prompt, which is where that
        // question belongs - not in a plugin's UI.
        return downloaded.startAsProcess();
       #else
        // Linux: extract and install into the *user's* plugin folder, which
        // needs no root. The tarball's own install.sh is interactive and
        // offers system-wide locations; running a downloaded shell script
        // unattended is not something this will do, so it does the one part
        // that is unambiguous and safe.
        const auto staging = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("abcTrain-update");

        staging.deleteRecursively();
        staging.createDirectory();

        juce::ChildProcess untar;

        if (! untar.start (juce::StringArray { "tar", "-xzf", downloaded.getFullPathName(),
                                                "-C", staging.getFullPathName() }))
            return false;

        untar.waitForProcessToFinish (60000);

        const auto vst3Folder = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                    .getChildFile (".vst3");
        vst3Folder.createDirectory();

        auto installed = 0;

        for (const auto& bundle : staging.findChildFiles (juce::File::findDirectories, true, "*.vst3"))
        {
            const auto destination = vst3Folder.getChildFile (bundle.getFileName());

            destination.deleteRecursively();

            if (bundle.copyDirectoryTo (destination))
                ++installed;
        }

        staging.deleteRecursively();

        if (installed == 0)
        {
            downloaded.revealToUser();
            return false;
        }

        return true;
       #endif
    }

    void offer (const UpdateChecker::ReleaseInfo& release, const Strings& strings,
                juce::Component* parent, std::function<void (juce::String)> say)
    {
        const auto haveInstaller = release.assetUrl.isNotEmpty();

        // Megabytes, because "31.8 MB" is a decision somebody can make and
        // "33362739 bytes" is not.
        const auto megabytes = juce::String (release.assetBytes / 1048576.0, 1);

        const auto body = strings.body
                              .replace ("{{latest}}", release.tagName)
                              .replace ("{{current}}", describeCurrentVersion (strings.devBuild));

        const auto detail = haveInstaller
                                ? strings.offerInstall.replace ("{{file}}", release.assetName)
                                                       .replace ("{{size}}", megabytes)
                                : strings.noAsset;

        const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
            juce::MessageBoxIconType::InfoIcon,
            strings.title,
            body + "\n\n" + detail,
            haveInstaller ? strings.updateNow : strings.openPage,
            strings.later,
            parent);

        juce::AlertWindow::showAsync (options, [release, strings, say, haveInstaller] (int result)
        {
            // makeOptionsOkCancel's documented mapping: first button 1,
            // second 0.
            if (result != 1)
                return;

            if (! haveInstaller)
            {
                juce::URL (release.htmlUrl).launchInDefaultBrowser();
                return;
            }

            const auto report = [say] (const juce::String& text)
            {
                if (say != nullptr)
                    say (text);
            };

            report (strings.downloading.replace ("{{percent}}", "0"));

            UpdateChecker::downloadReleaseAsync (release,
                [report, strings] (float progress)
                {
                    report (strings.downloading.replace ("{{percent}}",
                        juce::String (juce::roundToInt (progress * 100.0f))));
                },
                [report, strings] (juce::File downloaded)
                {
                    if (! downloaded.existsAsFile())
                    {
                        report (strings.failed);
                        return;
                    }

                    report (strings.opening);

                    if (launchInstaller (downloaded))
                        report (strings.installed);
                    else
                    {
                        // Could not start it - hand over the file rather
                        // than leaving somebody with nothing.
                        downloaded.revealToUser();
                        report (strings.installed);
                    }
                });
        });
    }
}
