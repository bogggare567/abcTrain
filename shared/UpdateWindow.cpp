#include "UpdateWindow.h"
#include "UpdatePrompt.h"
#include "AbcTrainTheme.h"
#include "AbcTrainLookAndFeel.h"
#include "Version.h"

namespace
{
    constexpr int buttonHeight = 32;

    juce::String megabytes (double bytes)
    {
        return juce::String (bytes / (1024.0 * 1024.0), 1);
    }
}

UpdateWindow::UpdateWindow()
{
    // Modal in effect, not in mechanism: it covers the editor and takes
    // clicks inside the panel, but never runs a nested message loop - a
    // plugin that blocks the host's event loop is a plugin that hangs the
    // DAW.
    setInterceptsMouseClicks (true, true);

    for (auto* button : { &installButton, &laterButton, &cancelButton, &doneButton })
        addChildComponent (*button);

    installButton.onClick = [this]
    {
        if (release.assetUrl.isEmpty())
        {
            juce::URL (release.htmlUrl).launchInDefaultBrowser();
            return;
        }

        beginDownload();
    };

    laterButton.onClick = [this]
    {
        setVisible (false);

        if (onClosed != nullptr)
            onClosed();
    };

    doneButton.onClick = laterButton.onClick;

    // Cancelling mid-download closes the window and stops reporting. The
    // transfer itself is owned by UpdateChecker and finishes into a
    // temporary file that nothing then opens - there is no safe way to
    // abort a juce::URL download from here, and pretending otherwise
    // would be a button that lies.
    cancelButton.onClick = [this]
    {
        phase = Phase::offer;
        progress = 0.0f;
        setVisible (false);

        if (onClosed != nullptr)
            onClosed();
    };

    startTimerHz (30);
}

UpdateWindow::~UpdateWindow() { stopTimer(); }

void UpdateWindow::setStrings (Strings newStrings)
{
    text = std::move (newStrings);

    installButton.setButtonText (text.install);
    laterButton.setButtonText (text.later);
    cancelButton.setButtonText (text.cancel);
    doneButton.setButtonText (text.later);

    layoutButtons();
    repaint();
}

void UpdateWindow::show (UpdateChecker::ReleaseInfo newRelease, bool runningStandalone)
{
    release = std::move (newRelease);
    standalone = runningStandalone;
    phase = Phase::offer;
    progress = 0.0f;
    appear = 0.0f;
    status = {};

    // Scanned once, when the window opens, rather than continuously: this
    // touches the filesystem, and the answer cannot change while somebody
    // is looking at a dialogue about it.
    installed = InstalledPlugins::scan();

    installButton.setButtonText (release.assetUrl.isEmpty() ? text.openPage : text.install);

    setVisible (true);
    toFront (true);
    layoutButtons();
    repaint();
}

void UpdateWindow::beginDownload()
{
    phase = Phase::downloading;
    progress = 0.0f;
    bytesTotal = (double) release.assetBytes;
    status = {};
    layoutButtons();
    repaint();

    juce::Component::SafePointer<UpdateWindow> safeThis (this);

    UpdateChecker::downloadReleaseAsync (release,
        [safeThis] (float p)
        {
            if (safeThis != nullptr)
            {
                safeThis->progress = juce::jlimit (0.0f, 1.0f, p);
                safeThis->repaint();
            }
        },
        [safeThis] (juce::File downloaded)
        {
            if (safeThis != nullptr)
                safeThis->finish (downloaded);
        });
}

void UpdateWindow::finish (juce::File downloaded)
{
    if (! downloaded.existsAsFile())
    {
        phase = Phase::failed;
        status = text.failed;
        layoutButtons();
        repaint();
        return;
    }

    phase = Phase::opening;
    status = text.opening;
    repaint();

    if (! UpdatePrompt::launchInstaller (downloaded))
        downloaded.revealToUser();

    phase = Phase::done;

    // The one place an automatic restart is honest. A plugin cannot
    // restart the host that loaded it, and no program can replace a
    // dynamic library the host already has mapped - but the standalone app
    // is its own process and can go away and come back.
    if (standalone)
    {
        status = text.finishedApp;
        repaint();

        juce::Component::SafePointer<UpdateWindow> safeThis (this);

        // Long enough for the installer to have taken over the screen, so
        // the app does not vanish while somebody is still reading it.
        juce::Timer::callAfterDelay (2500, [safeThis]
        {
            if (safeThis == nullptr)
                return;

            if (auto* app = juce::JUCEApplicationBase::getInstance())
                app->systemRequestedQuit();
        });
    }
    else
    {
        status = text.finishedPlugin;
    }

    layoutButtons();
    repaint();
}

juce::Rectangle<int> UpdateWindow::panelBounds() const
{
    const auto width = juce::jmin (getWidth() - 40, 520);
    const auto height = juce::jmin (getHeight() - 40, 340);

    return getLocalBounds().withSizeKeepingCentre (juce::jmax (240, width),
                                                    juce::jmax (200, height));
}

bool UpdateWindow::hitTest (int x, int y)
{
    // Clicks outside the panel fall through to the editor beneath, the
    // same rule ModuleScreenComponent follows - a scrim that eats clicks
    // is a scrim that traps people.
    return panelBounds().contains (x, y);
}

void UpdateWindow::timerCallback()
{
    if (appear >= 1.0f)
        return;

    appear = juce::jmin (1.0f, appear + (float) (1000.0 / 30.0 / AbcTrainTheme::Duration::transition));
    repaint();
}

void UpdateWindow::layoutButtons()
{
    for (auto* button : { &installButton, &laterButton, &cancelButton, &doneButton })
        button->setVisible (false);

    auto row = panelBounds().reduced (AbcTrainTheme::Spacing::large)
                            .removeFromBottom (buttonHeight);

    const auto place = [&row] (juce::TextButton& button, int width)
    {
        button.setVisible (true);
        button.setBounds (row.removeFromRight (width));
        row.removeFromRight (AbcTrainTheme::Spacing::small);
    };

    switch (phase)
    {
        case Phase::offer:
            place (installButton, 190);
            place (laterButton, 96);
            break;

        case Phase::downloading:
        case Phase::opening:
            place (cancelButton, 110);
            break;

        case Phase::done:
        case Phase::failed:
            place (doneButton, 110);
            break;
    }
}

void UpdateWindow::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto eased = AbcTrainTheme::Ease::out (appear);

    // The scrim only dims; hitTest above is what makes it pass clicks.
    g.setColour (theme.windowBackground.withAlpha (0.62f * eased));
    g.fillAll();

    auto panel = panelBounds().toFloat();
    AbcTrainLookAndFeel::paintRaisedCard (g, panel);

    auto inner = panel.reduced ((float) AbcTrainTheme::Spacing::large);
    inner.removeFromBottom ((float) (buttonHeight + AbcTrainTheme::Spacing::medium));

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::titleFont());
    g.drawText (text.title, inner.removeFromTop (30.0f), juce::Justification::centredLeft, false);

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::small);

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    g.drawFittedText (text.body.replace ("{{latest}}", release.tagName)
                               .replace ("{{current}}", CurrentVersion::string),
                      inner.removeFromTop (40.0f).toNearestInt(),
                      juce::Justification::topLeft, 2);

    inner.removeFromTop ((float) AbcTrainTheme::Spacing::medium);

    if (phase == Phase::offer)
    {
        // What is on this machine, and where. The question anybody has
        // when told an update exists is "which copy is this replacing" -
        // and until now nothing answered it.
        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.drawText (AbcTrainLookAndFeel::toCaps (text.installedHere),
                    inner.removeFromTop (16.0f), juce::Justification::topLeft, false);

        auto list = inner;

        if (installed.empty())
        {
            g.setColour (theme.textDim);
            g.setFont (AbcTrainLookAndFeel::bodyFont());
            g.drawFittedText (text.nothingFound, list.toNearestInt(),
                              juce::Justification::topLeft, 2);
        }
        else
        {
            g.setFont (AbcTrainLookAndFeel::microFont());

            for (const auto& entry : installed)
            {
                if (list.getHeight() < 15.0f)
                    break;

                auto line = list.removeFromTop (15.0f);

                g.setColour (theme.text);
                g.drawText (entry.product + "  ·  " + entry.format,
                            line.removeFromLeft (line.getWidth() * 0.62f),
                            juce::Justification::centredLeft, true);

                g.setColour (theme.textDim);
                g.drawText (entry.version.isNotEmpty() ? entry.version : text.versionUnknown,
                            line, juce::Justification::centredRight, true);
            }
        }

        if (release.assetUrl.isEmpty())
        {
            g.setColour (theme.accentWarm);
            g.setFont (AbcTrainLookAndFeel::bodyFont());
            g.drawFittedText (text.noAsset, inner.removeFromBottom (34.0f).toNearestInt(),
                              juce::Justification::bottomLeft, 2);
        }

        return;
    }

    // --- everything from here is the part that used to be invisible ---
    if (phase == Phase::downloading)
    {
        const auto done = bytesTotal * (double) progress;

        g.setColour (theme.text);
        g.setFont (AbcTrainLookAndFeel::bodyFont());
        g.drawText (text.downloading.replace ("{{done}}", megabytes (done))
                                     .replace ("{{total}}", megabytes (bytesTotal)),
                    inner.removeFromTop (22.0f), juce::Justification::centredLeft, false);

        inner.removeFromTop ((float) AbcTrainTheme::Spacing::small);

        auto track = inner.removeFromTop (10.0f);
        AbcTrainLookAndFeel::paintRecessedWell (g, track, 5.0f);

        // The fill can never be narrower than its own corner radius, or it
        // stops being a rounded shape - the same rule the level bar
        // follows, and the reason the first percent still looks like a bar.
        const auto minWidth = track.getHeight();
        auto fill = track.withWidth (juce::jmax (minWidth, track.getWidth() * progress));

        g.setColour (theme.accent);
        g.fillRoundedRectangle (fill, 5.0f);

        inner.removeFromTop ((float) AbcTrainTheme::Spacing::small);

        g.setColour (theme.textDim);
        g.setFont (AbcTrainLookAndFeel::microFont());
        g.drawText (release.assetName, inner.removeFromTop (16.0f),
                    juce::Justification::centredLeft, true);

        return;
    }

    g.setColour (phase == Phase::failed ? theme.negative : theme.text);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    g.drawFittedText (status, inner.toNearestInt(), juce::Justification::topLeft, 4);
}

void UpdateWindow::resized()
{
    layoutButtons();
}
