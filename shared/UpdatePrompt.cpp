#include "UpdatePrompt.h"
#include "Version.h"

namespace UpdatePrompt
{
    void offer (const UpdateChecker::ReleaseInfo& release, juce::Component* parent,
                std::function<void (juce::String)> say)
    {
        const auto haveInstaller = release.assetUrl.isNotEmpty();

        // Megabytes, because "83.4 MB" is a decision someone can make and
        // "87445504 bytes" is not.
        const auto sizeText = release.assetBytes > 0
                                  ? " (" + juce::String (release.assetBytes / 1048576.0, 1) + " MB)"
                                  : juce::String();

        const auto options = juce::MessageBoxOptions::makeOptionsOkCancel (
            juce::MessageBoxIconType::InfoIcon,
            "Update available",
            "Version " + release.tagName + " is out - you have "
                + juce::String (CurrentVersion::string) + ".\n\n"
                + (haveInstaller
                       ? "Download " + release.assetName + sizeText + " to your Downloads folder? "
                         "Your levels, achievements and settings are kept - the installer does not "
                         "touch them."
                       : juce::String ("This release has no installer for your system yet.")),
            haveInstaller ? "Download" : "Open release page", "Later",
            parent);

        juce::AlertWindow::showAsync (options, [release, say, haveInstaller] (int result)
        {
            // makeOptionsOkCancel's documented mapping: the first button
            // returns 1, the second 0.
            if (result != 1)
                return;

            if (! haveInstaller)
            {
                juce::URL (release.htmlUrl).launchInDefaultBrowser();
                return;
            }

            if (say != nullptr)
                say ("Downloading " + release.tagName + "...");

            UpdateChecker::downloadReleaseAsync (release,
                [say] (float progress)
                {
                    if (say != nullptr)
                        say ("Downloading " + juce::String (juce::roundToInt (progress * 100.0f)) + "%");
                },
                [say] (juce::File downloaded)
                {
                    if (! downloaded.existsAsFile())
                    {
                        if (say != nullptr)
                            say ("Download failed - the release page is still there.");

                        return;
                    }

                    // Reveal rather than open: the person should see what
                    // arrived and where, and decide to run it themselves.
                    downloaded.revealToUser();

                    if (say != nullptr)
                        say ("Downloaded to " + downloaded.getFileName() + " - it is in Finder now.");
                });
        });
    }
}
