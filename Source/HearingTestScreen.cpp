#include "HearingTestScreen.h"
#include "ProbeTone.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/ui/HearingColours.h"

namespace
{
    constexpr float chartLowHz = 125.0f, chartHighHz = 16000.0f;
    constexpr float chartTopDb = HearingProfile::minLevelDb;     // quieter = better = up
    constexpr float chartBottomDb = HearingProfile::maxLevelDb;

    float effective (float t)
    {
        return HearingProfile::isNotHeard (t) ? HearingProfile::maxLevelDb + HearingProfile::deadZoneDb : t;
    }

    juce::String n (int v) { return juce::String (v); }
}

HearingTestScreen::HearingTestScreen (LocalisationManager& localisationToUse, HearingProfileStore& storeToUse, Host hostToUse)
    : localisation (localisationToUse), store (storeToUse), host (std::move (hostToUse)),
      pauseRandom (juce::Time::currentTimeMillis())
{
    setOpaque (true);

    for (auto* e : { &personEditor, &headphonesEditor })
    {
        e->setFont (AbcTrainLookAndFeel::bodyFont());
        e->setIndents (10, 7);
        e->onTextChange = [this] { refreshButtons(); };
        e->onReturnKey = [this] { if (startButton.isEnabled()) startButton.triggerClick(); };
        addChildComponent (e);
    }

    personEditor.setComponentID ("hearing.person");
    headphonesEditor.setComponentID ("hearing.headphones");

    startButton.onClick = [this]
    {
        person = personEditor.getText().trim();
        headphones = headphonesEditor.getText().trim();
        store.rememberNames (person, headphones);
        startTest();
    };

    cancelButton.onClick = [this]
    {
        abort();

        if (host.leave != nullptr)
            host.leave();
    };

    heardButton.onClick = [this] { answer (true); };
    notHeardButton.onClick = [this] { answer (false); };
    pauseButton.onClick = [this] { togglePause(); };

    saveButton.onClick = [this]
    {
        draft.amountPercent = juce::roundToInt (amountSlider.getValue());

        if (host.save != nullptr)
            host.save (draft);
    };

    retakeButton.onClick = [this] { startTest(); };

    for (auto* b : { &startButton, &cancelButton, &heardButton, &notHeardButton, &pauseButton, &saveButton, &retakeButton })
        addChildComponent (b);

    AbcTrainLookAndFeel::makePrimary (startButton);
    AbcTrainLookAndFeel::makePrimary (saveButton);

    amountSlider.setRange (0.0, 100.0, 10.0);
    amountSlider.setValue (HearingProfile::defaultAmountPercent, juce::dontSendNotification);
    amountSlider.setDoubleClickReturnValue (true, HearingProfile::defaultAmountPercent);
    amountSlider.onValueChange = [this]
    {
        draft.amountPercent = juce::roundToInt (amountSlider.getValue());
        refreshCompensationCurve();
        repaint (amountLabelArea);
    };
    addChildComponent (amountSlider);

    // The result page's curve is Learner EQ's own display: same grid, same
    // bells, same scale - only without bands of its own to drag.
    compensationCurve.setResponseCurveVisible (false);
    compensationCurve.setZonesVisible (false);
    compensationCurve.setInterceptsMouseClicks (false, false);
    addChildComponent (compensationCurve);

    refreshText();
    showView (View::setup);
}

HearingTestScreen::~HearingTestScreen()
{
    abort();
}

// ---- flow -----------------------------------------------------------------

void HearingTestScreen::openSetup()
{
    abort();
    personEditor.setText (store.lastPerson(), false);
    headphonesEditor.setText (store.lastHeadphones(), false);
    showView (View::setup);
}

void HearingTestScreen::abort()
{
    stopTimer();

    if (view == View::test)
    {
        if (host.stopTone != nullptr)
            host.stopTone();

        if (host.setTestActive != nullptr)
            host.setTestActive (false);

        procedure.reset();
        answering = false;
        paused = false;
        showView (View::setup);
    }
}

void HearingTestScreen::startTest()
{
    procedure = std::make_unique<AudiometryProcedure> (juce::Time::currentTimeMillis());
    paused = false;
    showView (View::test);

    if (host.setTestActive != nullptr)
        host.setTestActive (true);

    presentCurrent();
    startTimerHz (20);
}

void HearingTestScreen::presentCurrent()
{
    if (procedure == nullptr || procedure->isFinished())
        return;

    answering = false;

    // 0.6 to 1.6 s of silence first, never the same twice in a row: a pulse
    // that always comes on the beat is one the person can answer without
    // hearing it. (1-3 s made the whole test feel endless.)
    const auto delay = 0.6 + 1.0 * pauseRandom.nextDouble();
    const auto& p = procedure->current();

    if (host.present != nullptr)
        host.present ((int) p.ear, p.freqHz, p.levelDb, delay, p.silent);

    windowEndsMs = juce::Time::getMillisecondCounterHiRes() + (delay + ProbeTone::stimulusSeconds) * 1000.0 + 150.0;
    refreshButtons();
    repaint();
}

void HearingTestScreen::timerCallback()
{
    if (view != View::test || paused || answering)
        return;

    if (juce::Time::getMillisecondCounterHiRes() >= windowEndsMs)
    {
        answering = true;
        refreshButtons();
        repaint();
    }
}

void HearingTestScreen::answer (bool heard)
{
    if (! answering || procedure == nullptr)
        return;

    procedure->answer (heard);

    if (procedure->isFinished())
    {
        finish();
        return;
    }

    presentCurrent();
}

void HearingTestScreen::togglePause()
{
    if (view != View::test)
        return;

    paused = ! paused;

    if (paused)
    {
        answering = false;

        if (host.stopTone != nullptr)
            host.stopTone();

        refreshButtons();
        repaint();
        return;
    }

    // Time has passed: the presentation starts again from its pause.
    presentCurrent();
}

void HearingTestScreen::finish()
{
    stopTimer();

    if (host.setTestActive != nullptr)
        host.setTestActive (false);

    draft = HearingProfile();
    draft.id = HearingProfileStore::newId();
    draft.person = person;
    draft.headphones = headphones;
    draft.created = juce::Time::getCurrentTime();
    procedure->fillProfile (draft);
    procedure.reset();

    amountSlider.setValue (draft.amountPercent, juce::dontSendNotification);
    showView (View::result);
}

void HearingTestScreen::showView (View newView)
{
    view = newView;
    scroller.setOffset (0.0f);   // every page opens at its top

    for (auto* c : { (juce::Component*) &personEditor, (juce::Component*) &headphonesEditor,
                     (juce::Component*) &startButton, (juce::Component*) &cancelButton })
        c->setVisible (view == View::setup);

    for (auto* c : { &heardButton, &notHeardButton, &pauseButton })
        c->setVisible (view == View::test);

    for (auto* c : { (juce::Component*) &saveButton, (juce::Component*) &retakeButton,
                     (juce::Component*) &amountSlider, (juce::Component*) &compensationCurve })
        c->setVisible (view == View::result);

    if (view == View::result)
        refreshCompensationCurve();

    refreshButtons();
    resized();
    repaint();
}

void HearingTestScreen::refreshButtons()
{
    startButton.setEnabled (personEditor.getText().trim().isNotEmpty()
                            && headphonesEditor.getText().trim().isNotEmpty());

    heardButton.setEnabled (answering && ! paused);
    notHeardButton.setEnabled (answering && ! paused);
    pauseButton.setButtonText (t (paused ? "hp.resume" : "hp.pause"));
}

void HearingTestScreen::refreshCompensationCurve()
{
    std::vector<SpectrumAnalyserComponent::OverlayCurve> curves;
    curves.push_back ({ HearingCompensation::makeBands (HearingProfile::frequencies, draft.compensationDb (0)), HearingColours::left() });
    curves.push_back ({ HearingCompensation::makeBands (HearingProfile::frequencies, draft.compensationDb (1)), HearingColours::right() });
    compensationCurve.setSampleRate (48000.0);
    compensationCurve.setOverlayCurves (std::move (curves), {}, true);
}

juce::String HearingTestScreen::pairName() const
{
    HearingProfile p;
    p.person = view == View::result ? draft.person : person;
    p.headphones = view == View::result ? draft.headphones : headphones;
    return p.pairName();
}

void HearingTestScreen::refreshText()
{
    const auto& theme = AbcTrainTheme::current();

    startButton.setButtonText (t ("hp.setup.start"));
    cancelButton.setButtonText (t ("hp.setup.cancel"));
    heardButton.setButtonText (t ("hp.heard"));
    notHeardButton.setButtonText (t ("hp.notHeard"));
    saveButton.setButtonText (t ("hp.save"));
    retakeButton.setButtonText (t ("hp.retake"));

    for (auto* e : { &personEditor, &headphonesEditor })
    {
        e->setColour (juce::TextEditor::backgroundColourId, theme.displayBackground);
        e->setColour (juce::TextEditor::textColourId, theme.textBright);
        e->setColour (juce::TextEditor::outlineColourId, theme.outline);
        e->setColour (juce::TextEditor::focusedOutlineColourId, theme.accent);
        e->applyColourToAllText (theme.textBright);
    }

    personEditor.setTextToShowWhenEmpty (t ("hp.setup.personHint"), theme.textDim);
    headphonesEditor.setTextToShowWhenEmpty (t ("hp.setup.headphonesHint"), theme.textDim);

    if (view == View::result)
        refreshCompensationCurve();

    refreshButtons();
    repaint();
}

// ---- words ----------------------------------------------------------------

juce::String HearingTestScreen::frequencyText (float hz, const LocalisationManager& loc)
{
    if (hz < 1000.0f)
        return juce::String (juce::roundToInt (hz)) + " " + loc.getText ("unit.Hz");

    const auto k = hz / 1000.0f;
    const auto text = std::abs (k - std::round (k)) < 0.01f ? juce::String (juce::roundToInt (k)) : juce::String (k, 1);
    return text + " " + loc.getText ("unit.kHz");
}

juce::String HearingTestScreen::summaryOf (const HearingProfile& p, const LocalisationManager& loc)
{
    struct Item { float freq, diff; };
    std::vector<Item> rightWorse, leftWorse;

    for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
    {
        const auto l = p.left.threshold[i], r = p.right.threshold[i];

        if (! HearingProfile::isMeasured (l) || ! HearingProfile::isMeasured (r)
            || (HearingProfile::isNotHeard (l) && HearingProfile::isNotHeard (r)))
            continue;

        const auto diff = effective (r) - effective (l);

        if (diff >= HearingProfile::deadZoneDb)
            rightWorse.push_back ({ HearingProfile::frequencies[i], diff });
        else if (diff <= -HearingProfile::deadZoneDb)
            leftWorse.push_back ({ HearingProfile::frequencies[i], -diff });
    }

    if (rightWorse.empty() && leftWorse.empty())
        return loc.getText ("hp.summary.same");

    const auto sentence = [&loc] (const std::vector<Item>& items, const char* key)
    {
        auto fMin = items.front().freq, fMax = items.front().freq;
        auto dMin = items.front().diff, dMax = items.front().diff;

        for (const auto& item : items)
        {
            fMin = juce::jmin (fMin, item.freq);  fMax = juce::jmax (fMax, item.freq);
            dMin = juce::jmin (dMin, item.diff);  dMax = juce::jmax (dMax, item.diff);
        }

        juce::String range;

        if (std::abs (fMax - fMin) < 1.0f)
            range = loc.getText ("hp.range.at", { { "f", frequencyText (fMin, loc) } });
        else if (fMax >= HearingProfile::frequencies.back() && fMin >= 2000.0f)
            range = loc.getText ("hp.range.above", { { "f", frequencyText (fMin, loc) } });
        else
            range = loc.getText ("hp.range.between", { { "a", frequencyText (fMin, loc) }, { "b", frequencyText (fMax, loc) } });

        const auto a = juce::roundToInt (dMin), b = juce::roundToInt (dMax);
        const auto db = a == b ? n (a) : n (a) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + n (b);
        return loc.getText (key, { { "range", range }, { "db", db } });
    };

    juce::StringArray parts;

    if (! rightWorse.empty()) parts.add (sentence (rightWorse, "hp.summary.rightWorse"));
    if (! leftWorse.empty())  parts.add (sentence (leftWorse, "hp.summary.leftWorse"));

    // Frequencies an ear did not hear at the loudest level: said in words,
    // because the chart can only draw them as an arrow off its edge.
    for (int e = 0; e < 2; ++e)
    {
        const auto& ear = e == 0 ? p.left : p.right;
        juce::StringArray missed;

        for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
            if (HearingProfile::isNotHeard (ear.threshold[i]))
                missed.add (frequencyText (HearingProfile::frequencies[i], loc));

        if (! missed.isEmpty())
            parts.add (loc.getText ("hp.summary.notHeard", { { "ear", loc.getText (e == 0 ? "hp.ear.left" : "hp.ear.right") },
                                                            { "list", missed.joinIntoString (", ") } }));
    }

    return parts.joinIntoString (" ");
}

juce::String HearingTestScreen::reliabilityOf (const HearingProfile& p, const LocalisationManager& loc)
{
    auto worst = 0.0f;
    auto hasRetest = false;   // the short run (no 1 kHz retest) has none

    for (const auto* ear : { &p.left, &p.right })
        if (const auto d = ear->retestDifference(); HearingProfile::isMeasured (d))
        {
            worst = juce::jmax (worst, d);
            hasRetest = true;
        }

    const auto falseAlarms = p.left.falseAlarms + p.right.falseAlarms;
    const auto remeasured = p.left.remeasured || p.right.remeasured;
    const auto unreliable = p.left.unreliable || p.right.unreliable;
    const auto db = n (juce::roundToInt (worst));

    if (p.retestAgrees() && falseAlarms == 0 && ! remeasured)
        return loc.getText (hasRetest ? "hp.rel.good" : "hp.rel.goodShort", { { "db", db } });

    juce::StringArray parts;
    if (hasRetest)
        parts.add (loc.getText (p.retestAgrees() ? "hp.rel.retestOk" : "hp.rel.retestBad", { { "db", db } }));

    if (unreliable)
        parts.add (loc.getText ("hp.rel.falseAlarms"));
    else if (remeasured)
        parts.add (loc.getText ("hp.rel.remeasured"));
    else if (falseAlarms > 0)
        parts.add (loc.getText ("hp.rel.fewFalseAlarms", { { "n", n (falseAlarms) } }));

    return parts.joinIntoString (" ");
}

HearingProfile HearingTestScreen::exampleProfile()
{
    HearingProfile p;
    p.id = "example";
    p.person = juce::String::fromUTF8 ("Богдан");
    p.headphones = "HD 600";
    p.created = juce::Time (2026, 9, 2, 12, 0);

    //                       250    500    1k     2k     3k     4k     6k     8k
    const float left[]  { -62.0f, -68.0f, -72.0f, -70.0f, -66.0f, -64.0f, -60.0f, -58.0f };
    const float right[] { -60.0f, -67.0f, -71.0f, -69.0f, -63.0f, -60.0f, -50.0f, -44.0f };

    for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
    {
        p.left.threshold[i] = left[i];
        p.right.threshold[i] = right[i];
    }

    p.left.first1k = -72.0f;   p.left.retest1k = -70.0f;
    p.right.first1k = -71.0f;  p.right.retest1k = -71.0f;
    p.left.catchTrials = 7;    p.right.catchTrials = 6;
    return p;
}

// ---- snapshots ------------------------------------------------------------

void HearingTestScreen::showTestForSnapshot()
{
    person = juce::String::fromUTF8 ("Богдан");
    headphones = "HD 600";

    // Partway into the right ear, answering - a frozen moment of a real
    // procedure, so the frequency and the step agree with each other.
    AudiometryProcedure::Config config;
    config.catchOneIn = 0;
    procedure = std::make_unique<AudiometryProcedure> (7, config);

    while (! (procedure->current().ear == AudiometryProcedure::Ear::right && procedure->current().step == 2))
        procedure->answer (procedure->current().levelDb > -60.0f);

    paused = false;
    showView (View::test);
    answering = true;
    refreshButtons();
}

void HearingTestScreen::showResultForSnapshot (const HearingProfile& profile)
{
    draft = profile;
    amountSlider.setValue (draft.amountPercent, juce::dontSendNotification);
    showView (View::result);
}

// ---- layout ---------------------------------------------------------------

juce::Rectangle<int> HearingTestScreen::column (int width) const
{
    auto area = getLocalBounds().reduced (AbcTrainTheme::Spacing::large + 8, AbcTrainTheme::Spacing::large);
    return area.withSizeKeepingCentre (juce::jmin (width, area.getWidth()), area.getHeight());
}

int HearingTestScreen::linesFor (const juce::String& text, const juce::Font& font, int width, int most)
{
    juce::GlyphArrangement layout;
    layout.addJustifiedText (font, text, 0.0f, 0.0f, (float) juce::jmax (40, width), juce::Justification::left);
    return juce::jlimit (1, most, juce::roundToInt (layout.getBoundingBox (0, -1, true).getHeight() / font.getHeight()));
}

juce::Rectangle<int> HearingTestScreen::scrolled (juce::Rectangle<int> area, int needed)
{
    const auto overflow = juce::jmax (0, needed - area.getHeight());
    if (scroller.getLimit() != (float) overflow)
        scroller.setLimit ((float) overflow);

    scroll = juce::jlimit (0.0f, (float) overflow, scroller.getOffset());
    return area.withHeight (juce::jmax (area.getHeight(), needed)).translated (0, -juce::roundToInt (scroll));
}

void HearingTestScreen::mouseDown (const juce::MouseEvent& e)  { scroller.mouseDown (e); }
void HearingTestScreen::mouseDrag (const juce::MouseEvent& e)  { scroller.mouseDrag (e); }
void HearingTestScreen::mouseUp (const juce::MouseEvent& e)    { scroller.mouseUp (e); }
void HearingTestScreen::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scroller.mouseWheelMove (wheel, 220.0f);
}

void HearingTestScreen::resized()
{
    constexpr int controlHeight = 34;

    if (scroller.onChange == nullptr)
        scroller.onChange = [this] (float) { resized(); repaint(); };

    if (view == View::test)
        scroller.setLimit (0.0f);

    if (view == View::setup)
    {
        auto c = column (560);
        introLines = linesFor (t ("hp.setup.intro"), AbcTrainLookAndFeel::bodyFont(), c.getWidth(), 5);
        const auto introHeight = juce::jmax (66, juce::roundToInt ((float) introLines * AbcTrainLookAndFeel::bodyFont().getHeight()) + 6);
        c = scrolled (c, 24 + 34 + 8 + introHeight + 16 + 22 + controlHeight + 12 + 22 + controlHeight + 10 + 22 + 16 + controlHeight + 2 + 8)
                .withTrimmedTop (24);
        titleArea = c.removeFromTop (34);
        c.removeFromTop (8);
        introArea = c.removeFromTop (introHeight);
        c.removeFromTop (16);
        personLabelArea = c.removeFromTop (22);
        personEditor.setBounds (c.removeFromTop (controlHeight));
        c.removeFromTop (12);
        headphonesLabelArea = c.removeFromTop (22);
        headphonesEditor.setBounds (c.removeFromTop (controlHeight));
        c.removeFromTop (10);
        timeArea = c.removeFromTop (22);
        c.removeFromTop (16);
        auto row = c.removeFromTop (controlHeight + 2);
        startButton.setBounds (row.removeFromLeft (200));
        row.removeFromLeft (AbcTrainTheme::Spacing::small);
        cancelButton.setBounds (row.removeFromLeft (150));
        return;
    }

    if (view == View::test)
    {
        // The column is about 430 px tall; centred in the page rather than
        // hung from its top edge, where it left half the window empty.
        auto c = column (640);

        // A phone on its side has about 350 points: the same column with
        // the gaps closed up and the note about the pauses left out (it is
        // on the setup page and in the result too).
        const auto tight = c.getHeight() < 470;
        const auto gap = [tight] (int normal) { return tight ? normal / 3 : normal; };

        c = c.withTrimmedTop (tight ? 4 : juce::jmax (24, (c.getHeight() - 430) / 2 - 20));
        titleArea = c.removeFromTop (34);
        c.removeFromTop (gap (12));
        progressArea = c.removeFromTop (6);
        c.removeFromTop (gap (34));
        earArea = c.removeFromTop (48);
        frequencyArea = c.removeFromTop (24);
        c.removeFromTop (gap (26));
        promptArea = c.removeFromTop (30);
        c.removeFromTop (gap (18));
        auto row = c.removeFromTop (tight ? 52 : 60);
        const auto w = (row.getWidth() - AbcTrainTheme::Spacing::medium) / 2;
        heardButton.setBounds (row.removeFromLeft (w));
        notHeardButton.setBounds (row.removeFromRight (w));
        c.removeFromTop (gap (22));
        noteArea = tight ? juce::Rectangle<int>() : c.removeFromTop (66);
        c.removeFromTop (gap (12));
        pauseButton.setBounds (c.removeFromTop (controlHeight).withSizeKeepingCentre (180, controlHeight));
        return;
    }

    auto c = column (1000);

    // Narrow (a phone held upright): the two charts one above the other,
    // each the whole width - side by side at 160 points each their axis
    // labels ran into one another ("1225500k") and the two legends into
    // their captions - and the buttons on a row of their own under the
    // amount, which otherwise had no width left at all.
    const auto narrow = c.getWidth() < 640;
    const auto labelFont = AbcTrainLookAndFeel::labelFont();
    noteLines = linesFor (t ("hp.note.halfGain"), labelFont, c.getWidth(), 6);
    disclaimerLines = linesFor (t ("hp.note.disclaimer"), labelFont, c.getWidth(), 5);
    const auto lineHeight = labelFont.getHeight() + 1.0f;
    const auto noteHeight = juce::roundToInt ((float) noteLines * lineHeight) + 4;
    const auto disclaimerHeight = juce::roundToInt ((float) disclaimerLines * lineHeight) + 4;
    const auto summaryHeight = narrow ? 52 : 44;

    const auto chartHeight = narrow ? 170 : juce::jlimit (150, 300, c.getHeight() - (8 + 34 + 4 + summaryHeight + 10 + 20 + 10 + 42 + 8
                                                                                     + controlHeight + 12 + noteHeight + 4 + disclaimerHeight) - 20);
    const auto curveHeight = narrow ? 110 : chartHeight;
    const auto charts = narrow ? 20 + chartHeight + 10 + 20 + curveHeight : 20 + chartHeight;
    const auto controls = narrow ? controlHeight + 8 + 44 : controlHeight;

    c = scrolled (c, 8 + 34 + 4 + summaryHeight + 10 + charts + 10 + 42 + 8 + controls + 12 + noteHeight + 4 + disclaimerHeight + 8)
            .withTrimmedTop (8);
    titleArea = c.removeFromTop (34);
    c.removeFromTop (4);
    summaryArea = c.removeFromTop (summaryHeight);
    c.removeFromTop (10);

    if (narrow)
    {
        thresholdCaption = c.removeFromTop (20);
        thresholdArea = c.removeFromTop (chartHeight);
        c.removeFromTop (10);
        curveCaption = c.removeFromTop (20);
        compensationCurve.setBounds (c.removeFromTop (curveHeight));
    }
    else
    {
        auto chartsArea = c.removeFromTop (charts);
        const auto half = (chartsArea.getWidth() - AbcTrainTheme::Spacing::large) / 2;
        auto leftChart = chartsArea.removeFromLeft (half);
        auto rightChart = chartsArea.removeFromRight (half);
        thresholdCaption = leftChart.removeFromTop (20);
        thresholdArea = leftChart;
        curveCaption = rightChart.removeFromTop (20);
        compensationCurve.setBounds (rightChart);
    }

    c.removeFromTop (10);
    reliabilityArea = c.removeFromTop (42);
    c.removeFromTop (8);

    if (narrow)
    {
        auto row = c.removeFromTop (controlHeight);
        amountLabelArea = row.removeFromLeft (juce::jmin (190, row.getWidth() / 2));
        amountSlider.setBounds (row.withSizeKeepingCentre (row.getWidth(), 24));
        c.removeFromTop (8);
        auto buttons = c.removeFromTop (44);
        const auto w = (buttons.getWidth() - AbcTrainTheme::Spacing::small) / 2;
        saveButton.setBounds (buttons.removeFromLeft (w));
        retakeButton.setBounds (buttons.removeFromRight (w));
    }
    else
    {
        auto row = c.removeFromTop (controlHeight);
        retakeButton.setBounds (row.removeFromRight (170));
        row.removeFromRight (AbcTrainTheme::Spacing::small);
        saveButton.setBounds (row.removeFromRight (190));
        row.removeFromRight (AbcTrainTheme::Spacing::large);
        amountLabelArea = row.removeFromLeft (juce::jmin (260, row.getWidth() / 2));
        amountSlider.setBounds (row.withSizeKeepingCentre (row.getWidth(), 24));
    }

    c.removeFromTop (12);
    halfGainArea = c.removeFromTop (noteHeight);
    c.removeFromTop (4);
    disclaimerArea = c.removeFromTop (disclaimerHeight);
}

// ---- painting -------------------------------------------------------------

void HearingTestScreen::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    backgroundCache.paint (g, getLocalBounds().toFloat());
    g.setColour (theme.panelBackground);
    g.fillRect (getLocalBounds());

    switch (view)
    {
        case View::setup:  paintSetup (g);  break;
        case View::test:   paintTest (g);   break;
        case View::result: paintResult (g); break;
    }
}

void HearingTestScreen::paintSetup (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::titleFont());
    AbcTrainLookAndFeel::fitText (g, t ("hp.setup.title"), titleArea, juce::Justification::centredLeft, true);

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    AbcTrainLookAndFeel::fitLines (g, t ("hp.setup.intro"), introArea, juce::Justification::topLeft, juce::jmax (3, introLines), 0.9f);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    AbcTrainLookAndFeel::fitText (g, t ("hp.setup.person"), personLabelArea, juce::Justification::centredLeft, true);
    AbcTrainLookAndFeel::fitText (g, t ("hp.setup.headphones"), headphonesLabelArea, juce::Justification::centredLeft, true);
    AbcTrainLookAndFeel::fitText (g, t ("hp.setup.time"), timeArea, juce::Justification::centredLeft, true);
}

void HearingTestScreen::paintTest (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::titleFont());
    AbcTrainLookAndFeel::fitText (g, t ("hp.test.title").replace ("{{pair}}", pairName()), titleArea,
                                  juce::Justification::centred, true);

    if (procedure == nullptr)
        return;

    const auto& p = procedure->current();
    const auto left = p.ear == AudiometryProcedure::Ear::left;
    const auto earColour = left ? HearingColours::left() : HearingColours::right();

    AbcTrainLookAndFeel::drawSegmentedBar (g, progressArea.toFloat(), AudiometryProcedure::totalSteps,
                                           (float) procedure->stepsDone() / (float) AudiometryProcedure::totalSteps,
                                           theme.accent, theme.outline, 3.0f);

    g.setColour (earColour);
    g.setFont (AbcTrainLookAndFeel::displayFont().withHeight (AbcTrainLookAndFeel::displayFontHeight
                                                              * AbcTrainLookAndFeel::getTextScale() * 0.9f));
    AbcTrainLookAndFeel::fitText (g, t (left ? "hp.ear.left" : "hp.ear.right"), earArea, juce::Justification::centred, true);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    AbcTrainLookAndFeel::fitText (g, localisation.getText ("hp.test.freq", { { "freq", frequencyText (p.freqHz, localisation) },
                                                                           { "n", n (procedure->stepsDone() + 1) },
                                                                           { "total", n (AudiometryProcedure::totalSteps) } }),
                                  frequencyArea, juce::Justification::centred, true);

    const auto prompt = paused ? t ("hp.test.paused")
                      : procedure->justRestartedEar() ? t ("hp.test.restarted")
                      : answering ? t ("hp.test.question")
                                  : t ("hp.test.listen");

    g.setColour (answering ? theme.textBright : theme.text);
    g.setFont (AbcTrainLookAndFeel::headingFont());
    AbcTrainLookAndFeel::fitText (g, prompt, promptArea, juce::Justification::centred, true);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    AbcTrainLookAndFeel::fitLines (g, t ("hp.test.note"), noteArea, juce::Justification::centredTop, 3, 0.9f);
}

void HearingTestScreen::paintThresholdChart (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintRecessedWell (g, bounds, AbcTrainTheme::Radius::well);

    auto plot = bounds.reduced (12.0f, 10.0f).withTrimmedLeft (34.0f).withTrimmedBottom (16.0f);

    const auto xFor = [plot] (float hz)
    {
        const auto p = std::log (hz / chartLowHz) / std::log (chartHighHz / chartLowHz);
        return plot.getX() + plot.getWidth() * p;
    };

    const auto yFor = [plot] (float db)
    {
        const auto p = (juce::jlimit (chartTopDb, chartBottomDb, db) - chartTopDb) / (chartBottomDb - chartTopDb);
        return plot.getY() + plot.getHeight() * p;
    };

    g.setFont (AbcTrainLookAndFeel::captionFont());

    for (const auto hz : { 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f })
    {
        const auto x = xFor (hz);
        g.setColour (theme.textDim.withAlpha (0.13f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        g.setColour (theme.textDim.withAlpha (0.6f));
        const auto label = hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String (juce::roundToInt (hz));
        AbcTrainLookAndFeel::fitText (g, label, juce::Rectangle<float> (x - 16.0f, plot.getBottom() + 2.0f, 32.0f, 13.0f),
                                      juce::Justification::centred, false);
    }

    for (int db = -100; db <= -20; db += 20)
    {
        const auto y = yFor ((float) db);
        g.setColour (theme.textDim.withAlpha (0.10f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (theme.textDim.withAlpha (0.6f));
        AbcTrainLookAndFeel::fitText (g, juce::String (db), juce::Rectangle<float> (bounds.getX() + 6.0f, y - 7.0f, 34.0f, 14.0f),
                                      juce::Justification::centredLeft, false);
    }

    // One ear: a line through the measured points, x or o at each, and an
    // arrow off the bottom edge where nothing was heard.
    const auto drawEar = [&] (const HearingProfile::Ear& ear, juce::Colour colour, bool cross)
    {
        juce::Path line;
        auto started = false;

        for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
        {
            const auto thr = ear.threshold[i];

            if (! HearingProfile::isMeasured (thr) || HearingProfile::isNotHeard (thr))
            {
                started = false;
                continue;
            }

            const juce::Point<float> p (xFor (HearingProfile::frequencies[i]), yFor (thr));

            if (! started) line.startNewSubPath (p);
            else           line.lineTo (p);

            started = true;
        }

        g.setColour (colour.withAlpha (0.6f));
        g.strokePath (line, juce::PathStrokeType (1.5f));
        g.setColour (colour);

        for (size_t i = 0; i < (size_t) HearingProfile::numFrequencies; ++i)
        {
            const auto thr = ear.threshold[i];

            if (! HearingProfile::isMeasured (thr))
                continue;

            const auto x = xFor (HearingProfile::frequencies[i]);
            const auto r = 5.0f;

            if (HearingProfile::isNotHeard (thr))
            {
                const auto y = plot.getBottom() - 4.0f;
                g.drawArrow (juce::Line<float> (x, y - 16.0f, x, y), 1.5f, 7.0f, 6.0f);
                continue;
            }

            const auto y = yFor (thr);

            if (cross)
            {
                g.drawLine (x - r, y - r, x + r, y + r, 2.0f);
                g.drawLine (x - r, y + r, x + r, y - r, 2.0f);
            }
            else
            {
                g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 2.0f);
            }
        }
    };

    drawEar (draft.left, HearingColours::left(), true);
    drawEar (draft.right, HearingColours::right(), false);
}

void HearingTestScreen::paintResult (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::titleFont());
    AbcTrainLookAndFeel::fitText (g, t ("hp.result.title").replace ("{{pair}}", pairName()), titleArea,
                                  juce::Justification::centredLeft, true);

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    AbcTrainLookAndFeel::fitLines (g, summaryOf (draft, localisation), summaryArea, juce::Justification::centredLeft, 2, 0.9f);

    // Captions: what each chart is, and which mark is which ear, in the
    // ear's own colour.
    const auto caption = [&] (juce::Rectangle<int> area, const juce::String& text, bool withEars)
    {
        g.setFont (AbcTrainLookAndFeel::labelFont());

        if (withEars)
        {
            const auto legendWidth = juce::jmin (area.getWidth() / 2, 190);
            auto legend = area.removeFromRight (legendWidth);
            const auto leftText = t ("hp.chart.left"), rightText = t ("hp.chart.right");
            auto l = legend.removeFromLeft (legend.getWidth() / 2);
            g.setColour (HearingColours::left());
            AbcTrainLookAndFeel::fitText (g, leftText, l, juce::Justification::centredRight, true);
            g.setColour (HearingColours::right());
            AbcTrainLookAndFeel::fitText (g, rightText, legend, juce::Justification::centredRight, true);
        }

        g.setColour (theme.textDim);
        AbcTrainLookAndFeel::fitText (g, text, area, juce::Justification::centredLeft, true);
    };

    caption (thresholdCaption, t ("hp.chart.thresholds"), true);
    caption (curveCaption, t ("hp.chart.compensation"), true);

    paintThresholdChart (g, thresholdArea.toFloat());

    g.setColour (draft.isReliable() ? theme.positive : theme.accentWarm);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    AbcTrainLookAndFeel::fitLines (g, reliabilityOf (draft, localisation), reliabilityArea,
                                   juce::Justification::centredLeft, 2, 0.9f);

    g.setColour (theme.textBright);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    AbcTrainLookAndFeel::fitText (g, localisation.getText ("hp.amount", { { "n", n (juce::roundToInt (amountSlider.getValue())) } }),
                                  amountLabelArea, juce::Justification::centredLeft, true);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::labelFont());
    AbcTrainLookAndFeel::fitLines (g, t ("hp.note.halfGain"), halfGainArea, juce::Justification::topLeft, noteLines, 0.9f);
    AbcTrainLookAndFeel::fitLines (g, t ("hp.note.disclaimer"), disclaimerArea, juce::Justification::topLeft, disclaimerLines, 0.9f);
}
