#include "LessonsPanel.h"
#include <algorithm>
#include <map>
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"
#include "shared/audio/LessonAudioBed.h"
#include "shared/audio/BuiltInSynth.h"
#include "../LearnerEQ/Source/PluginProcessor.h"
#include "../LearnerEQ/Source/PluginEditor.h"
#include "../LearnerComp/Source/PluginProcessor.h"
#include "../LearnerVerb/Source/PluginProcessor.h"

using LnF = AbcTrainLookAndFeel;

namespace
{
    constexpr int pad = 20;
    constexpr int headerHeight = 64;
    constexpr int courseHeadHeight = 40;
    constexpr int lessonRowHeight = 46;
    constexpr int stepRowHeight = 30;
    constexpr int footerHeight = 72;

    // What a lesson plays: a real voice for the vocal ones, the app's own
    // synthesized instruments and loops for the rest - the same sounds the
    // trainer uses, so nothing in a lesson needs a library to be imported.
    juce::AudioBuffer<float> renderMaterial (const juce::String& name, double sampleRate)
    {
        using Bed = LessonAudioBed::Bed;

        const std::pair<const char*, Bed> beds[] {
            { "vocal", Bed::vocal }, { "drums", Bed::drumLoop }, { "hit", Bed::singleHit },
            { "chord", Bed::chord }, { "pink", Bed::pinkNoise } };

        for (const auto& [key, bed] : beds)
            if (name == key)
                return LessonAudioBed::render (bed, sampleRate, 2);

        const std::pair<const char*, const char*> synth[] {
            { "kick", "Kick tight" }, { "snare", "Snare crisp" }, { "bass", "Pick bass" },
            { "guitar", "Guitar" }, { "keys", "E-piano" }, { "mix", "Mini mix 120" } };

        for (const auto& [key, sound] : synth)
            if (name == key)
                for (const auto& s : BuiltInSynth::all())
                    if (juce::String (s.name) == sound)
                    {
                        auto mono = s.render (sampleRate);
                        juce::AudioBuffer<float> stereo (2, mono.getNumSamples());
                        stereo.copyFrom (0, 0, mono, 0, 0, mono.getNumSamples());
                        stereo.copyFrom (1, 0, mono, 0, 0, mono.getNumSamples());
                        return stereo;
                    }

        return LessonAudioBed::render (Bed::pinkNoise, sampleRate, 2);
    }

    // The books, short: "R. Izhaki, «Mixing Audio»" for each reference -
    // the chapters are in the lesson file for whoever wants them, and the
    // full line ran to three widths of the window.
    juce::String shortSources (const juce::String& sources)
    {
        juce::StringArray out;
        for (auto part : juce::StringArray::fromTokens (sources, ";", ""))
        {
            part = part.trim();
            const auto close = part.indexOf (juce::String (juce::CharPointer_UTF8 ("\xc2\xbb")));
            out.addIfNotAlreadyThere (close > 0 ? part.substring (0, close + 1) : part.upToFirstOccurrenceOf (",", false, false));
        }
        out.removeEmptyStrings();
        return out.joinIntoString ("; ");
    }

    // The first sentence of a step, for its row in the list.
    juce::String firstSentence (const juce::String& text)
    {
        for (int i = 0; i < text.length() - 1; ++i)
            if ((text[i] == '.' || text[i] == '!' || text[i] == '?') && text[i + 1] == ' ' && i > 12)
                return text.substring (0, i + 1);
        return text;
    }
}

// ---------------------------------------------------------------- the rail

class LessonsPanel::Rail : public juce::Component
{
public:
    explicit Rail (LessonsPanel& p) : owner (p) {}

    struct Row { juce::Rectangle<int> bounds; const LessonFile::Lesson* lesson = nullptr; juce::String course; };
    std::vector<Row> rows;
    int hovered = -1;

    void layout (int width)
    {
        rows.clear();
        int y = 6;

        for (const auto& c : owner.courses)
        {
            rows.push_back ({ { 0, y, width, courseHeadHeight }, nullptr, c.id });
            y += courseHeadHeight;

            for (const auto* l : c.lessons)
            {
                rows.push_back ({ { 0, y, width, lessonRowHeight }, l, c.id });
                y += lessonRowHeight;
            }
        }

        setSize (width, y + 12);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();

        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto& r = rows[(size_t) i];

            if (r.lesson == nullptr)
            {
                // Course heading and how far through it.
                const auto& course = *std::find_if (owner.courses.begin(), owner.courses.end(),
                                                    [&r] (const auto& c) { return c.id == r.course; });
                int done = 0;
                for (const auto* l : course.lessons)
                    done += owner.isDone (l->id) ? 1 : 0;

                auto area = r.bounds.reduced (18, 0).withTrimmedTop (12);
                g.setColour (theme.textDim);
                g.setFont (LnF::monoFont().withHeight (11.0f));
                LnF::fitText (g, juce::String (done) + "/" + juce::String ((int) course.lessons.size()),
                              area.removeFromRight (40), juce::Justification::centredRight, false);
                LnF::drawTrackedText (g, LnF::toCaps (owner.host.text ("lessons.course." + r.course)),
                                      area.toFloat(), LnF::microFont(), theme.textDim, 1.3f);
                continue;
            }

            const auto selected = owner.lesson == r.lesson;
            auto area = r.bounds.reduced (8, 1);

            if (selected || hovered == i)
            {
                g.setColour (selected ? theme.accent.withAlpha (0.18f) : theme.widgetBackground.withAlpha (0.6f));
                g.fillRect (area);
            }
            if (selected)
            {
                g.setColour (theme.accent);
                g.fillRect (area.withWidth (3).reduced (0, 6));
            }

            area.removeFromLeft (12);
            const auto done = owner.isDone (r.lesson->id);

            g.setColour (theme.textDim);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, owner.host.textWith ("lessons.minutes", { { "n", juce::String (r.lesson->minutes) } }),
                          area.removeFromRight (50), juce::Justification::centredRight, false);

            g.setColour (done ? theme.positive : theme.textDim);
            g.setFont (LnF::monoFont().withHeight (12.0f));
            LnF::fitText (g, done ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93")) : juce::String (r.lesson->order),
                          area.removeFromLeft (20), juce::Justification::centredLeft, false);

            // Two lines rather than a shrunk one: a long title in a
            // smaller font read as a different kind of row.
            g.setColour (selected ? theme.textBright : theme.text);
            g.setFont (LnF::bodyFont());
            LnF::fitLines (g, r.lesson->title (owner.host.language), area, juce::Justification::centredLeft, 2);
        }
    }

    int rowAt (juce::Point<int> p) const
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].lesson != nullptr && rows[(size_t) i].bounds.contains (p))
                return i;
        return -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto r = rowAt (e.getPosition());
        if (r != hovered) { hovered = r; repaint(); }
        setMouseCursor (r >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto r = rowAt (e.getPosition());
        if (r >= 0)
            owner.openLesson (rows[(size_t) r].lesson->id);
    }

    LessonsPanel& owner;
};

// ---------------------------------------------------------------- panel

LessonsPanel::LessonsPanel (Host h)
    : host (std::move (h)), courses (LessonLibrary::courses())
{
    if (host.lookAndFeel != nullptr)
        setLookAndFeel (host.lookAndFeel);

    rail = std::make_unique<Rail> (*this);
    railView.setViewedComponent (rail.get(), false);
    railView.setScrollBarsShown (true, false);
    railView.setScrollBarThickness (6);
    addAndMakeVisible (railView);

    backButton.setButtonText (host.text ("lessons.back"));
    nextButton.setButtonText (host.text ("lessons.next"));
    coursesButton.setButtonText (host.text ("lessons.allCourses"));
    LnF::makePrimary (nextButton, true);

    backButton.onClick = [this] { goToStep (step - 1); };
    nextButton.onClick = [this]
    {
        if (lesson == nullptr)
            return;
        if (step + 1 < (int) lesson->steps.size())
            goToStep (step + 1);
        else
        {
            markDone (lesson->id);
            closeLesson();
        }
    };
    coursesButton.onClick = [this] { closeLesson(); };

    for (auto* b : { &backButton, &nextButton, &coursesButton })
        addChildComponent (b);

    setSize (1000, 660);
}

LessonsPanel::~LessonsPanel()
{
    runner.finish();
    setLookAndFeel (nullptr);
}

bool LessonsPanel::isDone (const juce::String& id) const
{
    return host.properties != nullptr
        && juce::StringArray::fromTokens (host.properties->getValue (doneKey), ",", "").contains (id);
}

void LessonsPanel::markDone (const juce::String& id)
{
    if (host.properties == nullptr || isDone (id))
        return;

    auto done = juce::StringArray::fromTokens (host.properties->getValue (doneKey), ",", "");
    done.add (id);
    done.removeEmptyStrings();
    host.properties->setValue (doneKey, done.joinIntoString (","));
    host.properties->saveIfNeeded();
}

int LessonsPanel::effectIndex() const
{
    return lesson == nullptr ? 0 : LessonFile::plugins().indexOf (lesson->plugin);
}

void LessonsPanel::openLesson (const juce::String& id)
{
    lesson = LessonLibrary::find (id);
    if (lesson == nullptr)
        return;

    step = 0;
    playingMaterial = {};
    applyStep();
    resized();
    repaint();
}

void LessonsPanel::closeLesson()
{
    runner.finish();
    setHighlight ({});
    lesson = nullptr;
    resized();
    repaint();
}

void LessonsPanel::goToStep (int index)
{
    if (lesson == nullptr || ! juce::isPositiveAndBelow (index, (int) lesson->steps.size()))
        return;

    step = index;
    applyStep();
    resized();
    repaint();
}

void LessonsPanel::applyMaterial (const juce::String& material)
{
    if (! host.showEffect)
        return;

    playingMaterial = material;
    auto& processor = host.showEffect (effectIndex());
    const auto sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0;
    auto buffer = renderMaterial (material, sampleRate);

    const auto publish = [&buffer] (PracticeAudioSource& source)
    {
        source.setEnabled (true);
        source.publishOverrideBuffer (std::move (buffer));
    };

    if (auto* eq = dynamic_cast<LearnerEQProcessor*> (&processor))
        publish (eq->getPracticeSource());
    else if (auto* comp = dynamic_cast<LearnerCompProcessor*> (&processor))
        publish (comp->getPracticeSource());
    else if (auto* verb = dynamic_cast<LearnerVerbProcessor*> (&processor))
        publish (verb->getPracticeSource());
}

void LessonsPanel::setHighlight (juce::Range<float> hz)
{
    if (! host.currentEditor)
        return;

    if (auto* eq = dynamic_cast<LearnerEQEditor*> (host.currentEditor()))
    {
        const auto hzText = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 0 : 1) + "k" : juce::String ((int) f); };
        const auto unit = host.language.startsWithIgnoreCase ("ru") || host.language.startsWithIgnoreCase ("uk")
                              ? juce::String (juce::CharPointer_UTF8 (" \xd0\x93\xd1\x86")) : juce::String (" Hz");
        eq->setLessonHighlight (hz, hz.isEmpty() ? juce::String()
                                                 : hzText (hz.getStart()) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + hzText (hz.getEnd()) + unit);
    }
}

void LessonsPanel::applyStep()
{
    if (lesson == nullptr || ! host.showEffect)
        return;

    auto& processor = host.showEffect (effectIndex());

    // A step says what changes, not the whole state, so the state at step
    // n is every step up to n played in order - which is what makes
    // jumping back to step 2 after step 5 sound like step 2.
    std::map<juce::String, float> state;
    juce::Range<float> highlight;
    auto material = lesson->material;

    for (int i = 0; i <= step; ++i)
    {
        const auto& s = lesson->steps[(size_t) i];

        for (const auto& [id, value] : LessonRunner::targetsFor (s, lesson->plugin))
            state[id] = value;

        for (const auto& a : s.actions)
        {
            if (a.verb == "highlight")
                highlight = { a.number (0, 20.0f), a.number (1, 20000.0f) };
            else if (a.verb == "material")
                material = a.args[0];
        }
    }

    // A highlight belongs to the step that sets it.
    const auto& current = lesson->steps[(size_t) step];
    const auto lit = std::any_of (current.actions.begin(), current.actions.end(),
                                  [] (const auto& a) { return a.verb == "highlight"; });
    setHighlight (lit ? highlight : juce::Range<float>());

    if (material != playingMaterial)
        applyMaterial (material);

    juce::AudioProcessorValueTreeState* apvts = nullptr;
    if (auto* eq = dynamic_cast<LearnerEQProcessor*> (&processor))        apvts = &eq->apvts;
    else if (auto* comp = dynamic_cast<LearnerCompProcessor*> (&processor)) apvts = &comp->apvts;
    else if (auto* verb = dynamic_cast<LearnerVerbProcessor*> (&processor)) apvts = &verb->apvts;

    if (apvts != nullptr)
        runner.apply (*apvts, LessonRunner::Targets (state.begin(), state.end()));
}

// ---------------------------------------------------------------- layout

juce::Rectangle<int> LessonsPanel::readerBounds() const
{
    return getLocalBounds().withTrimmedLeft (railWidth).withTrimmedTop (headerHeight).reduced (pad + 8, pad);
}

juce::Rectangle<int> LessonsPanel::stepListBounds() const
{
    if (lesson == nullptr)
        return {};

    auto r = readerBounds();
    r.removeFromBottom (footerHeight);
    const auto listHeight = stepRowHeight * (int) lesson->steps.size();
    return r.removeFromBottom (juce::jmin (listHeight, r.getHeight() / 2));
}

void LessonsPanel::resized()
{
    railView.setBounds (getLocalBounds().withTrimmedTop (headerHeight).removeFromLeft (railWidth));
    rail->layout (railWidth - 8);

    const auto inLesson = lesson != nullptr;
    for (auto* b : { &backButton, &nextButton, &coursesButton })
        b->setVisible (inLesson);

    if (! inLesson)
        return;

    auto footer = readerBounds().removeFromBottom (36);   // under the two lines of sources
    nextButton.setBounds (footer.removeFromRight (150));
    footer.removeFromRight (8);
    backButton.setBounds (footer.removeFromRight (110));
    coursesButton.setBounds (footer.removeFromLeft (140));

    backButton.setEnabled (step > 0);
    nextButton.setButtonText (step + 1 < (int) lesson->steps.size() ? host.text ("lessons.next")
                                                                       : host.text ("lessons.finish"));
}

void LessonsPanel::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    g.fillAll (theme.windowBackground);

    // Header across the window.
    {
        auto header = getLocalBounds().removeFromTop (headerHeight).reduced (pad, 0);
        g.setColour (theme.textBright);
        g.setFont (LnF::titleFont());
        LnF::fitText (g, host.text ("lessons.window"), header.removeFromLeft (railWidth - pad), juce::Justification::centredLeft, false);
        g.setColour (theme.textDim);
        g.setFont (LnF::labelFont());
        LnF::fitLines (g, host.text ("lessons.tagline"), header.withTrimmedLeft (8 + pad), juce::Justification::centredLeft, 2);

        g.setColour (theme.divider);
        g.fillRect (0, headerHeight - 1, getWidth(), 1);
    }

    // Rail backing, as in Settings and Sounds.
    {
        auto railArea = getLocalBounds().withTrimmedTop (headerHeight).removeFromLeft (railWidth);
        g.setColour (theme.windowBackground.darker (0.15f));
        g.fillRect (railArea);
        g.setColour (theme.divider);
        g.fillRect (railArea.getRight() - 1, railArea.getY(), 1, railArea.getHeight());
    }

    auto r = readerBounds();

    if (lesson == nullptr)
    {
        // No lesson open: what this is, in one paragraph, and the books.
        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont());
        LnF::fitText (g, host.text ("lessons.chooseTitle"), r.removeFromTop (30), juce::Justification::centredLeft, true);
        r.removeFromTop (6);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitLines (g, host.text ("lessons.chooseBody"), r.removeFromTop (110), juce::Justification::topLeft, 5);
        r.removeFromTop (12);
        g.setColour (theme.textDim);
        g.setFont (LnF::labelFont());
        LnF::fitLines (g, host.text ("lessons.booksNote"), r.removeFromTop (60), juce::Justification::topLeft, 3);
        return;
    }

    const auto& s = lesson->steps[(size_t) step];
    const auto courseIndex = std::find_if (courses.begin(), courses.end(), [this] (const auto& c) { return c.id == lesson->course; });
    const auto ofCourse = courseIndex != courses.end() ? (int) courseIndex->lessons.size() : 0;

    // Where we are: course, lesson n of m, minutes, which plugin it turns.
    LnF::drawTrackedText (g, LnF::toCaps (host.text ("lessons.course." + lesson->course) + "  ·  "
                                            + host.textWith ("lessons.lessonOf", { { "n", juce::String (lesson->order) },
                                                                                   { "m", juce::String (ofCourse) } })
                                            + "  ·  " + host.textWith ("lessons.minutes", { { "n", juce::String (lesson->minutes) } })
                                            + "  ·  " + lesson->plugin.toUpperCase()),
                          r.removeFromTop (16).toFloat(), LnF::microFont(), theme.textDim, 1.3f);
    r.removeFromTop (6);

    g.setColour (theme.textBright);
    g.setFont (LnF::titleFont());
    LnF::fitText (g, lesson->title (host.language), r.removeFromTop (32), juce::Justification::centredLeft, true);
    r.removeFromTop (14);

    // The step being read, large.
    auto footerArea = r.removeFromBottom (footerHeight);
    auto list = stepListBounds();
    r.setBottom (list.getY() - 14);

    g.setColour (theme.textDim);
    g.setFont (LnF::labelFont());
    auto stepLine = host.textWith ("lessons.stepOf", { { "n", juce::String (step + 1) },
                                                       { "m", juce::String ((int) lesson->steps.size()) } });
    if (! host.language.startsWithIgnoreCase ("ru") && ! host.language.startsWithIgnoreCase ("en"))
        stepLine << juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) << host.text ("lessons.englishOnly");
    LnF::fitText (g, stepLine, r.removeFromTop (20), juce::Justification::centredLeft, true);
    r.removeFromTop (4);

    g.setColour (theme.text);
    g.setFont (LnF::bodyFont().withHeight (LnF::bodyFont().getHeight() * 1.12f));
    LnF::fitLines (g, lesson->text (s, host.language), r, juce::Justification::topLeft, juce::jmax (2, r.getHeight() / 20));

    // Every step as a row: done, current, still to come.
    {
        g.setColour (theme.divider);
        g.fillRect (list.getX(), list.getY() - 7, list.getWidth(), 1);

        for (int i = 0; i < (int) lesson->steps.size(); ++i)
        {
            auto row = juce::Rectangle<int> (list.getX(), list.getY() + i * stepRowHeight, list.getWidth(), stepRowHeight);
            if (row.getBottom() > list.getBottom())
                break;

            const auto current = i == step;
            if (current || hoveredStep == i)
            {
                g.setColour (current ? theme.accent.withAlpha (0.16f) : theme.widgetBackground.withAlpha (0.5f));
                g.fillRect (row);
            }

            auto mark = row.removeFromLeft (30);
            g.setColour (i < step ? theme.positive : current ? theme.accent : theme.textDim);
            g.setFont (LnF::monoFont().withHeight (12.0f));
            LnF::fitText (g, i < step ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93")) : juce::String (i + 1),
                          mark, juce::Justification::centred, false);

            g.setColour (current ? theme.textBright : i < step ? theme.textDim : theme.text);
            g.setFont (LnF::labelFont());
            LnF::fitText (g, firstSentence (lesson->text (lesson->steps[(size_t) i], host.language)),
                          row.withTrimmedRight (6), juce::Justification::centredLeft, true);
        }
    }

    // Sources, small, above the buttons - two lines, then cut.
    {
        auto sources = footerArea.removeFromTop (30);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, host.text ("lessons.sources") + ": " + shortSources (lesson->sources),
                       sources, juce::Justification::topLeft, 2);
    }
}

int LessonsPanel::stepRowAt (juce::Point<int> p) const
{
    const auto list = stepListBounds();
    if (lesson == nullptr || ! list.contains (p))
        return -1;

    const auto i = (p.y - list.getY()) / stepRowHeight;
    return juce::isPositiveAndBelow (i, (int) lesson->steps.size()) ? i : -1;
}

void LessonsPanel::mouseMove (const juce::MouseEvent& e)
{
    const auto i = stepRowAt (e.getPosition());
    if (i != hoveredStep) { hoveredStep = i; repaint(); }
    setMouseCursor (i >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void LessonsPanel::mouseExit (const juce::MouseEvent&)
{
    hoveredStep = -1;
    repaint();
}

void LessonsPanel::mouseUp (const juce::MouseEvent& e)
{
    // Any step, in any order: applyStep rebuilds that step's state.
    const auto i = stepRowAt (e.getPosition());
    if (i >= 0)
        goToStep (i);
}

// ---------------------------------------------------------------- window

LessonsWindow::LessonsWindow (const juce::String& title, LessonsPanel::Host host)
    : juce::DocumentWindow (title, AbcTrainTheme::current().windowBackground, juce::DocumentWindow::closeButton)
{
    content = new LessonsPanel (std::move (host));
    setUsingNativeTitleBar (true);
    setContentOwned (content, true);
    setResizable (true, false);
    setResizeLimits (820, 560, 1600, 1200);
}
