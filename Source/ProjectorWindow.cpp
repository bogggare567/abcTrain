#include "ProjectorWindow.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/QrCode.h"
#include "shared/i18n/LocalisationManager.h"

namespace
{
    using LnF = AbcTrainLookAndFeel;

    juce::String fill (juce::String s, const char* name, int value)
    {
        return LocalisationManager::resolvePlurals (s.replace (juce::String ("{{") + name + "}}", juce::String (value)));
    }

    juce::String fill2 (const juce::String& s, int n, int m)
    {
        return LocalisationManager::resolvePlurals (s.replace ("{{n}}", juce::String (n)).replace ("{{m}}", juce::String (m)));
    }
}

ProjectorView::ProjectorView (SeminarHost& h, std::function<juce::String()> a)
    : host (h), address (std::move (a))
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setWantsKeyboardFocus (true);

    cleanButton.onClick = [this] { host.setSound (true, false); if (onChanged) onChanged(); repaint(); };
    processedButton.onClick = [this] { host.setSound (true, true); if (onChanged) onChanged(); repaint(); };
    primaryButton.onClick = [this] { primaryAction(); };
    fullButton.onClick = [this] { if (onToggleFullScreen) onToggleFullScreen(); };
    LnF::makePrimary (primaryButton, true);

    for (auto* b : { &cleanButton, &processedButton, &primaryButton, &fullButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    setStrings ({});
    startTimerHz (4);
}

ProjectorView::~ProjectorView()
{
    setLookAndFeel (nullptr);
}

void ProjectorView::setStrings (Strings s)
{
    text = std::move (s);
    cleanButton.setButtonText ("A");
    processedButton.setButtonText ("B");
    fullButton.setButtonText (text.fullScreen);
    refreshButtons();
    repaint();
}

juce::String ProjectorView::joinUrl() const
{
    const auto base = address();

    if (base.isEmpty())
        return {};

    const auto snap = host.getRoom().snapshot();
    return "http://" + base + "/" + (snap.listOnly ? juce::String() : "?r=" + snap.roomCode);
}

void ProjectorView::primaryAction()
{
    switch (host.getStage())
    {
        case SeminarHost::Stage::lobby:     host.start(); break;
        case SeminarHost::Stage::listening: host.reveal(); break;
        case SeminarHost::Stage::revealed:  host.next(); break;
        case SeminarHost::Stage::finished:  host.again(); break;
        case SeminarHost::Stage::closed:    break;
    }

    if (onChanged)
        onChanged();

    refreshButtons();
    repaint();
}

void ProjectorView::refreshButtons()
{
    const auto stage = host.getStage();
    const auto label = stage == SeminarHost::Stage::lobby     ? text.start
                     : stage == SeminarHost::Stage::listening ? text.showAnswer
                     : stage == SeminarHost::Stage::revealed  ? text.next
                                                              : text.again;
    primaryButton.setButtonText (label);

    const auto round = stage == SeminarHost::Stage::listening || stage == SeminarHost::Stage::revealed;
    cleanButton.setVisible (round);
    processedButton.setVisible (round);
    LnF::makePrimary (cleanButton, round && host.isPlaying() && ! host.isProcessed());
    LnF::makePrimary (processedButton, round && host.isPlaying() && host.isProcessed());
}

bool ProjectorView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey || key == juce::KeyPress::rightKey
        || key == juce::KeyPress::pageDownKey)
    {
        primaryAction();
        return true;
    }

    const auto c = juce::CharacterFunctions::toLowerCase (key.getTextCharacter());

    if (c == 'a' || c == 'b')
    {
        host.setSound (true, c == 'b');
        if (onChanged) onChanged();
        refreshButtons();
        repaint();
        return true;
    }

    if (c == 's')
    {
        host.setSound (false, host.isProcessed());
        if (onChanged) onChanged();
        refreshButtons();
        repaint();
        return true;
    }

    if (c == 'f' || (key == juce::KeyPress::escapeKey && onToggleFullScreen != nullptr))
    {
        if (onToggleFullScreen)
            onToggleFullScreen();
        return true;
    }

    return false;
}

void ProjectorView::timerCallback()
{
    refreshButtons();
    repaint();
}

void ProjectorView::resized()
{
    auto bar = getLocalBounds().removeFromBottom (56).reduced (24, 10);
    fullButton.setBounds (bar.removeFromRight (170));
    bar.removeFromRight (12);
    primaryButton.setBounds (bar.removeFromRight (220));
    bar.removeFromRight (12);
    processedButton.setBounds (bar.removeFromRight (64));
    bar.removeFromRight (6);
    cleanButton.setBounds (bar.removeFromRight (64));
}

void ProjectorView::paintQr (juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto url = joinUrl();

    if (url.isEmpty())
        return;

    // Built once per address; a QR code of a constant string is constant.
    if (url != lastUrl || qr == nullptr)
    {
        auto& self = const_cast<ProjectorView&> (*this);
        self.qr = std::make_unique<QrCode> (url);
        self.lastUrl = url;
    }

    qr->paint (g, area);
}

void ProjectorView::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    g.fillAll (theme.windowBackground);

    const auto snap = host.getRoom().snapshot();
    auto area = getLocalBounds().reduced (juce::jmax (24, getWidth() / 30));
    area.removeFromBottom (56);

    // The header: the room and the way in, always.
    {
        auto top = area.removeFromTop (juce::jmax (40, getHeight() / 14));
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight ((float) top.getHeight() * 0.62f));
        LnF::fitText (g, snap.title, top.removeFromLeft (top.getWidth() / 2), juce::Justification::centredLeft, true);

        g.setColour (theme.textDim);
        g.setFont (LnF::monoFont().withHeight ((float) top.getHeight() * 0.45f));
        const auto where = address().isNotEmpty() ? "http://" + address()
                                                    + (snap.listOnly ? juce::String() : "   " + text.roomCode + " " + snap.roomCode)
                                                  : juce::String();
        LnF::fitText (g, where, top, juce::Justification::centredRight, true);
        area.removeFromTop (juce::jmax (12, getHeight() / 40));
    }

    switch (host.getStage())
    {
        case SeminarHost::Stage::closed:
        case SeminarHost::Stage::lobby:     paintLobby (g, area, snap); break;
        case SeminarHost::Stage::listening: paintRound (g, area, snap); break;
        case SeminarHost::Stage::revealed:  paintAnswer (g, area, snap); break;
        case SeminarHost::Stage::finished:  paintFinished (g, area, snap); break;
    }

    g.setColour (theme.textDim.withAlpha (0.7f));
    g.setFont (LnF::captionFont());
    LnF::fitText (g, text.keys, getLocalBounds().removeFromBottom (56).reduced (24, 10).withTrimmedRight (560),
                  juce::Justification::centredLeft, true);
}

void ProjectorView::paintLobby (juce::Graphics& g, juce::Rectangle<int> area, const LocalRoom::Snapshot& snap)
{
    const auto& theme = AbcTrainTheme::current();
    const auto side = juce::jmin (area.getHeight(), area.getWidth() / 2);
    paintQr (g, area.removeFromLeft (side).toFloat());
    area.removeFromLeft (juce::jmax (24, getWidth() / 30));

    const auto unit = (float) juce::jmax (16, getHeight() / 34);
    g.setColour (theme.textDim);
    g.setFont (LnF::bodyFont().withHeight (unit * 1.2f));
    LnF::fitText (g, text.scan, area.removeFromTop ((int) (unit * 2.0f)), juce::Justification::centredLeft, true);

    g.setColour (theme.textBright);
    g.setFont (LnF::monoFont().withHeight (unit * 2.4f));
    LnF::fitText (g, address().isNotEmpty() ? address() : "-", area.removeFromTop ((int) (unit * 3.2f)),
                  juce::Justification::centredLeft, true);

    if (! snap.listOnly)
    {
        g.setColour (theme.textDim);
        g.setFont (LnF::bodyFont().withHeight (unit * 1.2f));
        LnF::fitText (g, text.roomCode, area.removeFromTop ((int) (unit * 1.8f)), juce::Justification::centredLeft, true);
        g.setColour (theme.accent);
        g.setFont (LnF::monoFont().withHeight (unit * 3.6f));
        LnF::fitText (g, snap.roomCode, area.removeFromTop ((int) (unit * 4.2f)), juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (theme.accentWarm);
        g.setFont (LnF::bodyFont().withHeight (unit * 1.2f));
        LnF::fitText (g, text.personalCodes, area.removeFromTop ((int) (unit * 2.4f)), juce::Justification::centredLeft, true);
    }

    area.removeFromTop ((int) unit);
    g.setColour (theme.text);
    g.setFont (LnF::headingFont().withHeight (unit * 1.4f));
    const auto joinedLine = fill (text.joined, "n", (int) snap.players.size())
                          + (snap.listOnly && snap.invited > 0 ? " / " + juce::String (snap.invited) : juce::String());
    LnF::fitText (g, joinedLine, area.removeFromTop ((int) (unit * 2.0f)), juce::Justification::centredLeft, true);

    // The names, in columns, as they come.
    g.setFont (LnF::bodyFont().withHeight (unit * 1.05f));
    const auto rowH = (int) (unit * 1.6f);
    const auto columns = juce::jmax (1, area.getWidth() / 220);
    const auto colW = area.getWidth() / columns;
    int i = 0;

    for (const auto& p : snap.players)
    {
        const auto col = i % columns, row = i / columns;
        auto cell = juce::Rectangle<int> (area.getX() + col * colW, area.getY() + row * rowH, colW - 8, rowH);

        if (cell.getBottom() > area.getBottom())
            break;

        g.setColour (p.present ? theme.textBright : theme.textDim);
        LnF::fitText (g, p.name, cell, juce::Justification::centredLeft, true);
        ++i;
    }

    if (snap.players.empty())
    {
        g.setColour (theme.textDim);
        LnF::fitText (g, text.waitingForStart, area.removeFromTop (rowH), juce::Justification::centredLeft, true);
    }
}

void ProjectorView::paintRound (juce::Graphics& g, juce::Rectangle<int> area, const LocalRoom::Snapshot& snap)
{
    const auto& theme = AbcTrainTheme::current();
    const auto unit = (float) juce::jmax (16, getHeight() / 34);

    // Latecomers: a small code in the corner.
    {
        auto corner = area.withTrimmedLeft (area.getWidth() - (int) (unit * 9.0f)).removeFromTop ((int) (unit * 10.5f));
        paintQr (g, corner.removeFromTop ((int) (unit * 9.0f)).toFloat());
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont().withHeight (unit * 0.8f));
        LnF::fitText (g, text.lateJoin, corner, juce::Justification::centred, true);
        area.removeFromRight ((int) (unit * 10.0f));
    }

    g.setColour (theme.textDim);
    g.setFont (LnF::bodyFont().withHeight (unit * 1.2f));
    LnF::fitText (g, fill2 (text.round, snap.question.round, snap.question.totalRounds) + "  ·  " + snap.question.exercise,
                  area.removeFromTop ((int) (unit * 2.0f)), juce::Justification::centredLeft, true);

    g.setColour (theme.textBright);
    g.setFont (LnF::headingFont().withHeight (unit * 2.2f));
    LnF::fitLines (g, snap.question.prompt, area.removeFromTop ((int) (unit * 6.0f)), juce::Justification::topLeft, 3, 0.8f);

    // What is playing, big: the hall has to know which version it hears.
    area.removeFromTop ((int) unit);
    const auto playing = ! host.isPlaying() ? text.silent : host.isProcessed() ? text.playingProcessed : text.playingClean;
    g.setColour (! host.isPlaying() ? theme.textDim : host.isProcessed() ? theme.accentWarm : theme.accent);
    g.setFont (LnF::headingFont().withHeight (unit * 4.0f));
    LnF::fitText (g, playing, area.removeFromTop ((int) (unit * 5.0f)), juce::Justification::centredLeft, true);

    // Answered.
    area.removeFromTop ((int) unit);
    const auto total = juce::jmax (1, (int) snap.players.size());
    g.setColour (theme.text);
    g.setFont (LnF::bodyFont().withHeight (unit * 1.3f));
    LnF::fitText (g, fill2 (text.answered, snap.answered, (int) snap.players.size()),
                  area.removeFromTop ((int) (unit * 2.0f)), juce::Justification::centredLeft, true);
    auto bar = area.removeFromTop ((int) (unit * 0.8f)).withWidth (juce::jmin (area.getWidth(), (int) (unit * 36.0f))).toFloat();
    g.setColour (theme.displayBackground);
    g.fillRect (bar);
    g.setColour (theme.accent);
    g.fillRect (bar.withWidth (bar.getWidth() * (float) snap.answered / (float) total));
    g.setColour (theme.outline);
    g.drawRect (bar, 1.0f);
}

void ProjectorView::paintAnswer (juce::Graphics& g, juce::Rectangle<int> area, const LocalRoom::Snapshot& snap)
{
    const auto& theme = AbcTrainTheme::current();
    const auto unit = (float) juce::jmax (16, getHeight() / 34);
    const auto& q = snap.question;

    g.setColour (theme.textDim);
    g.setFont (LnF::bodyFont().withHeight (unit * 1.2f));
    LnF::fitText (g, fill2 (text.round, q.round, q.totalRounds) + "  ·  " + q.exercise,
                  area.removeFromTop ((int) (unit * 2.0f)), juce::Justification::centredLeft, true);

    g.setFont (LnF::bodyFont().withHeight (unit * 1.3f));
    LnF::fitText (g, text.theAnswer, area.removeFromTop ((int) (unit * 2.0f)), juce::Justification::centredLeft, true);
    g.setColour (theme.positive);
    g.setFont (LnF::headingFont().withHeight (unit * 4.2f));
    LnF::fitText (g, snap.answer.label, area.removeFromTop ((int) (unit * 5.0f)), juce::Justification::centredLeft, true);

    g.setColour (theme.text);
    g.setFont (LnF::bodyFont().withHeight (unit * 1.3f));
    LnF::fitText (g, snap.answered == 0 ? text.noVotes : fill2 (text.rightCount, snap.right, snap.answered),
                  area.removeFromTop ((int) (unit * 2.2f)), juce::Justification::centredLeft, true);
    area.removeFromTop ((int) unit);

    auto chart = area;

    if (! q.continuous)
    {
        // A bar per choice.
        const auto n = juce::jmax (1, q.choices.size());
        std::vector<int> counts ((size_t) n, 0);
        for (auto v : snap.votes)
            if (const auto i = juce::roundToInt (v); i >= 0 && i < n)
                ++counts[(size_t) i];

        const auto most = juce::jmax (1, *std::max_element (counts.begin(), counts.end()));
        const auto labelH = (int) (unit * 1.8f);
        const auto w = (float) chart.getWidth() / (float) n;

        for (int i = 0; i < n; ++i)
        {
            auto col = juce::Rectangle<float> ((float) chart.getX() + w * (float) i, (float) chart.getY(), w, (float) chart.getHeight()).reduced (w * 0.08f, 0);
            auto label = col.removeFromBottom ((float) labelH);
            auto countBox = col.removeFromTop ((float) labelH);
            const auto h = col.getHeight() * (float) counts[(size_t) i] / (float) most;
            const auto right = i == snap.answer.choice;

            g.setColour (right ? theme.positive : theme.widgetBackground);
            g.fillRect (col.removeFromBottom (h));
            g.setColour (right ? theme.positive : theme.textDim);
            g.setFont (LnF::monoFont().withHeight (unit * 1.2f));
            LnF::fitText (g, juce::String (counts[(size_t) i]), countBox.withY (label.getY() - h - (float) labelH), juce::Justification::centred, true);
            g.setColour (right ? theme.textBright : theme.text);
            g.setFont (LnF::labelFont().withHeight (unit * 1.0f));
            LnF::fitText (g, q.choices[i], label, juce::Justification::centred, true);
        }

        return;
    }

    // Continuous: the scale, the accept band, and every vote as a dot,
    // stacked where they land.
    auto axis = chart.removeFromBottom ((int) (unit * 2.4f)).toFloat();
    auto plot = chart.toFloat().reduced (unit, 0);
    axis = axis.reduced (unit, 0);
    const auto x = [&plot] (float v) { return plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, v); };

    g.setColour (theme.positive.withAlpha (0.28f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x (snap.answer.value - q.tolerance), plot.getY(),
                                                            x (snap.answer.value + q.tolerance), plot.getBottom()));
    g.setColour (theme.positive);
    g.fillRect (x (snap.answer.value) - 1.5f, plot.getY(), 3.0f, plot.getHeight());

    g.setColour (theme.outline);
    g.drawHorizontalLine ((int) plot.getBottom(), plot.getX(), plot.getRight());

    constexpr int bins = 60;
    std::vector<int> stack (bins, 0);
    const auto dot = juce::jmin (plot.getWidth() / (float) bins, unit * 0.9f);

    for (auto v : snap.votes)
    {
        const auto bin = juce::jlimit (0, bins - 1, (int) (v * bins));
        const auto cx = x (v);
        const auto cy = plot.getBottom() - dot * (0.6f + 1.1f * (float) stack[(size_t) bin]++);
        const auto right = std::abs (v - snap.answer.value) <= juce::jmax (0.005f, q.tolerance);
        g.setColour (right ? theme.positive : theme.accentWarm);
        g.fillEllipse (cx - dot * 0.45f, cy - dot * 0.45f, dot * 0.9f, dot * 0.9f);
    }

    g.setColour (theme.textDim);
    g.setFont (LnF::labelFont().withHeight (unit * 0.95f));

    for (const auto& m : q.marks)
        LnF::fitText (g, m.label, juce::Rectangle<float> (x (m.position) - unit * 3.0f, axis.getY(), unit * 6.0f, axis.getHeight()),
                      juce::Justification::centred, true);
}

void ProjectorView::paintFinished (juce::Graphics& g, juce::Rectangle<int> area, const LocalRoom::Snapshot& snap)
{
    const auto& theme = AbcTrainTheme::current();
    const auto unit = (float) juce::jmax (16, getHeight() / 34);

    g.setColour (theme.textBright);
    g.setFont (LnF::headingFont().withHeight (unit * 2.6f));
    LnF::fitText (g, text.finished, area.removeFromTop ((int) (unit * 3.4f)), juce::Justification::centredLeft, true);

    const auto rowH = (int) (unit * 2.1f);
    int place = 0, previous = -1, shown = 0;

    for (size_t i = 0; i < snap.players.size(); ++i)
    {
        const auto& p = snap.players[i];
        if (p.score != previous)
            place = (int) i + 1;
        previous = p.score;

        auto row = area.removeFromTop (rowH);
        if (row.getBottom() > area.getBottom() + rowH || ++shown > 12)
            break;

        g.setColour (place == 1 ? theme.accent : theme.textDim);
        g.setFont (LnF::monoFont().withHeight (unit * 1.4f));
        LnF::fitText (g, juce::String (place), row.removeFromLeft ((int) (unit * 3.0f)), juce::Justification::centredLeft, true);
        g.setColour (theme.textBright);
        g.setFont (LnF::bodyFont().withHeight (unit * 1.4f));
        auto scoreBox = row.removeFromRight ((int) (unit * 5.0f));
        LnF::fitText (g, p.name, row, juce::Justification::centredLeft, true);
        g.setFont (LnF::monoFont().withHeight (unit * 1.4f));
        LnF::fitText (g, fill (text.points, "n", p.score), scoreBox, juce::Justification::centredRight, true);
    }
}

// ---- the window ---------------------------------------------------------

ProjectorWindow::ProjectorWindow (SeminarHost& host, std::function<juce::String()> address, const ProjectorView::Strings& strings)
    : juce::DocumentWindow ("abcTrain - Live", AbcTrainTheme::current().windowBackground, juce::DocumentWindow::allButtons)
{
    view = new ProjectorView (host, std::move (address));
    view->setStrings (strings);
    view->onToggleFullScreen = [this] { setFullScreen (! isFullScreen()); };
    setUsingNativeTitleBar (true);
    setContentOwned (view, false);
    setResizable (true, false);
    centreWithSize (1280, 760);
    setVisible (true);
    view->grabKeyboardFocus();
}

void ProjectorWindow::closeButtonPressed()
{
    if (onClosed)
        onClosed();
}

void ProjectorWindow::placeOnBestDisplay()
{
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* main = displays.getPrimaryDisplay();

    for (const auto& d : displays.displays)
    {
        if (main != nullptr && d.totalArea == main->totalArea)
            continue;

        // A second screen: that is the projector. Fill it.
        setBounds (d.userArea.reduced (40));
        setFullScreen (true);
        return;
    }
}
