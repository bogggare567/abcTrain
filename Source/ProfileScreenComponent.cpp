#include "ProfileScreenComponent.h"

#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

namespace
{
    using LnF = AbcTrainLookAndFeel;

    constexpr int pad = 16;
    constexpr int captionHeight = 22;
    constexpr int sectionGap = 18;
    constexpr int familyHeight = 50;
    constexpr int activityHeight = 22;
    constexpr int linkHeight = 58;
    constexpr int historyHeight = 120;
}

ProfileScreenComponent::ProfileScreenComponent()
{
    setOpaque (true);
    scroller.onChange = [this] (float o) { scroll = o; repaint(); };
}

void ProfileScreenComponent::setContent (Content newContent)
{
    content = std::move (newContent);
    resized();
    repaint();
}

void ProfileScreenComponent::showSupportPage (bool shouldShow)
{
    supportShown = shouldShow;
    scroller.setOffset (0.0f);
    scroll = 0.0f;
    resized();
    repaint();
}

juce::Rectangle<int> ProfileScreenComponent::column() const
{
    auto area = getLocalBounds().reduced (pad, 0);
    return area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 720), area.getHeight());
}

ProfileScreenComponent::Layout ProfileScreenComponent::layout() const
{
    Layout l;
    auto c = column();
    auto y = pad - juce::roundToInt (scroll);
    const auto take = [&c, &y] (int height)
    {
        juce::Rectangle<int> r (c.getX(), y, c.getWidth(), height);
        y += height;
        return r;
    };

    if (supportShown)
    {
        l.back = take (44).withWidth (juce::jmin (220, c.getWidth()));
        y += 10;
        l.title = take (34);
        y += 10;

        juce::AttributedString text;
        text.append (content.supportBody, LnF::bodyFont());
        juce::TextLayout measured;
        measured.createLayout (text, (float) c.getWidth());
        l.body = take (juce::jlimit (40, 260, (int) std::ceil (measured.getHeight()) + 6));
        y += sectionGap;

        if (content.donationAllowed)
        {
            l.donateCaption = take (captionHeight);
            y += 6;
            l.donateButton = take (54).withWidth (juce::jmin (c.getWidth(), 420));
            y += 6;
            l.donateNote = take (36);
            y += sectionGap;
        }

        l.freeCaption = take (captionHeight);
        y += 4;
        for (int i = 0; i < content.freeItems.size(); ++i)
            l.freeRows.push_back (take (48));

        l.bottom = y + pad + juce::roundToInt (scroll);
        return l;
    }

    l.header = take (48);
    y += 12;
    l.totals = take (60);
    y += sectionGap;

    l.historyCaption = take (captionHeight + 6);

    // Week / month / all, at the caption's right end.
    {
        auto chips = l.historyCaption.withTrimmedLeft (l.historyCaption.getWidth() / 2);
        const auto w = juce::jmin (64, chips.getWidth() / juce::jmax (1, content.periods.size()));
        for (int i = content.periods.size() - 1; i >= 0; --i)
            l.periodChips.insert (l.periodChips.begin(), chips.removeFromRight (w));
    }
    y += 6;
    l.history = take (content.history.size() >= 2 ? historyHeight : 40);
    y += sectionGap;

    l.familiesCaption = take (captionHeight);
    for (size_t i = 0; i < content.families.size(); ++i)
        l.families.push_back (take (familyHeight));
    y += sectionGap;

    l.activityCaption = take (captionHeight);
    y += 4;
    for (int i = 0; i < content.activity.size(); ++i)
        l.activity.push_back (take (activityHeight));
    y += sectionGap;

    if (content.showPageLinks)
    {
        l.achievementsRow = take (linkHeight);
        y += 8;
        l.settingsRow = take (linkHeight);
        y += 8;
    }

    l.supportRow = take (linkHeight);
    l.bottom = y + pad + juce::roundToInt (scroll);
    return l;
}

void ProfileScreenComponent::resized()
{
    // Measured with no scroll applied, so the limit is the content's own.
    const auto saved = scroll;
    scroll = 0.0f;
    const auto bottom = layout().bottom;
    scroll = saved;

    scroller.setLimit ((float) juce::jmax (0, bottom - getHeight()));
    scroll = scroller.getOffset();
}

void ProfileScreenComponent::paintLinkRow (juce::Graphics& g, juce::Rectangle<int> row, const juce::String& title,
                                           const juce::String& detail, juce::Colour accent)
{
    const auto& theme = AbcTrainTheme::current();
    g.setColour (accent);
    g.drawRect (row, 1);

    auto inner = row.reduced (14, 8);
    auto chevron = inner.removeFromRight (14).toFloat().withSizeKeepingCentre (7.0f, 12.0f);
    juce::Path p;
    p.startNewSubPath (chevron.getX(), chevron.getY());
    p.lineTo (chevron.getRight(), chevron.getCentreY());
    p.lineTo (chevron.getX(), chevron.getBottom());
    g.strokePath (p, juce::PathStrokeType (1.6f));

    g.setColour (theme.textBright);
    g.setFont (LnF::headingFont().withHeight (18.0f));
    LnF::fitText (g, title, inner.removeFromTop (22), juce::Justification::centredLeft, true);
    g.setColour (theme.textDim);
    g.setFont (LnF::captionFont());
    LnF::fitText (g, detail, inner, juce::Justification::centredLeft, true);
}

void ProfileScreenComponent::paint (juce::Graphics& g)
{
    LnF::paintPanelBackground (g, getLocalBounds().toFloat());
    const auto l = layout();

    if (supportShown)
        paintSupport (g, l);
    else
        paintProfile (g, l);
}

void ProfileScreenComponent::paintProfile (juce::Graphics& g, const Layout& l)
{
    const auto& theme = AbcTrainTheme::current();

    // --- who --------------------------------------------------------------
    {
        auto header = l.header;
        auto badge = header.removeFromLeft (48);
        g.setColour (theme.accent);
        g.drawRect (badge, 1);
        g.setFont (LnF::titleFont());
        LnF::fitText (g, content.name.substring (0, 1).toUpperCase(), badge, juce::Justification::centred, false);
        header.removeFromLeft (12);

        g.setColour (theme.textBright);
        g.setFont (LnF::titleFont());
        LnF::fitText (g, content.name, header.removeFromTop (28), juce::Justification::centredLeft, true);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, content.subtitle, header, juce::Justification::centredLeft, true);
    }

    // --- three facts --------------------------------------------------------
    {
        auto row = l.totals;
        g.setColour (theme.divider);
        g.fillRect (row.getX(), row.getY(), row.getWidth(), 1);
        g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);

        const auto w = row.getWidth() / 3;

        for (int i = 0; i < 3; ++i)
        {
            auto cell = i < 2 ? row.removeFromLeft (w) : row;

            if (i > 0)
            {
                g.setColour (theme.divider);
                g.fillRect (cell.getX(), cell.getY() + 8, 1, cell.getHeight() - 16);
                cell.removeFromLeft (12);
            }

            const auto& total = content.totals[(size_t) i];
            cell = cell.reduced (0, 8);
            g.setColour (total.colour.isTransparent() ? theme.textBright : total.colour);
            g.setFont (LnF::monoFont().withHeight (20.0f));
            LnF::fitText (g, total.value, cell.removeFromTop (26), juce::Justification::centredLeft, true);
            g.setColour (theme.textDim);
            g.setFont (LnF::captionFont());
            LnF::fitText (g, total.caption, cell, juce::Justification::centredLeft, true);
        }
    }

    // --- Decibelo over time ------------------------------------------------
    LnF::drawTrackedText (g, LnF::toCaps (content.historyCaption), l.historyCaption.toFloat(),
                          LnF::labelFont(), theme.text, 2.2f);

    for (int i = 0; i < (int) l.periodChips.size() && i < content.periods.size(); ++i)
    {
        const auto chip = l.periodChips[(size_t) i];
        const auto on = i == content.period;
        g.setColour (on ? theme.accent : theme.outline);

        if (on)
            g.fillRect (chip);
        else
            g.drawRect (chip, 1);

        LnF::drawTrackedText (g, LnF::toCaps (content.periods[i]), chip.toFloat(), LnF::microFont(),
                              on ? LnF::labelColourOn (theme.accent) : theme.textDim, 1.2f,
                              juce::Justification::centred);
    }

    if (content.history.size() < 2)
    {
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, content.historyEmpty, l.history, juce::Justification::topLeft, 2, 0.9f);
    }
    else
    {
        auto plot = l.history.toFloat().reduced (0.0f, 6.0f);
        int lo = 100000, hi = 0;

        for (const auto& day : content.history)
            for (auto v : day)
                if (v > 0)
                {
                    lo = juce::jmin (lo, v);
                    hi = juce::jmax (hi, v);
                }

        if (hi <= lo)
        {
            lo -= 25;
            hi += 25;
        }

        g.setColour (theme.divider);
        for (int k = 0; k <= 2; ++k)
            g.drawHorizontalLine (juce::roundToInt (plot.getY() + plot.getHeight() * (float) k * 0.5f),
                                  plot.getX(), plot.getRight());

        g.setColour (theme.textDim);
        g.setFont (LnF::monoFont().withHeight (10.0f));
        LnF::fitText (g, juce::String (hi), plot.withHeight (12.0f).toNearestInt(), juce::Justification::topRight, false);
        LnF::fitText (g, juce::String (lo), plot.withTop (plot.getBottom() - 12.0f).toNearestInt(),
                      juce::Justification::bottomRight, false);

        const auto n = (int) content.history.size();

        for (size_t f = 0; f < 4; ++f)
        {
            juce::Path line;
            auto started = false;

            for (int i = 0; i < n; ++i)
            {
                const auto v = content.history[(size_t) i][f];
                if (v <= 0)
                    continue;

                const juce::Point<float> p { plot.getX() + plot.getWidth() * (float) i / (float) (n - 1),
                                             plot.getBottom() - plot.getHeight() * (float) (v - lo) / (float) (hi - lo) };
                if (! started)
                    line.startNewSubPath (p);
                else
                    line.lineTo (p);
                started = true;
            }

            g.setColour (content.familyColours[f]);
            g.strokePath (line, juce::PathStrokeType (2.0f));
        }
    }

    // --- the families -------------------------------------------------------
    LnF::drawTrackedText (g, LnF::toCaps (content.familiesCaption), l.familiesCaption.toFloat(),
                          LnF::labelFont(), theme.text, 2.2f);

    for (size_t i = 0; i < content.families.size() && i < l.families.size(); ++i)
    {
        const auto& f = content.families[i];
        auto row = l.families[i];
        g.setColour (theme.divider);
        g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);

        g.setColour (f.colour);
        g.fillRect (row.removeFromLeft (10).withSizeKeepingCentre (10, 10));
        row.removeFromLeft (12);

        auto delta = row.removeFromRight (52);
        auto rating = row.removeFromRight (70);

        if (f.delta.isNotEmpty())
        {
            g.setColour (f.deltaUp ? theme.positive : theme.negative);
            g.setFont (LnF::monoFont().withHeight (13.0f));
            LnF::fitText (g, f.delta, delta, juce::Justification::centredRight, false);
        }

        g.setColour (theme.textBright);
        g.setFont (LnF::monoFont().withHeight (18.0f));
        LnF::fitText (g, f.rating, rating, juce::Justification::centredRight, false);

        g.setColour (theme.textBright);
        g.setFont (LnF::headingFont().withHeight (17.0f));
        LnF::fitText (g, f.name, row.removeFromTop (row.getHeight() / 2 + 3).withTrimmedTop (4),
                      juce::Justification::bottomLeft, true);
        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitText (g, f.detail, row, juce::Justification::topLeft, true);
    }

    // --- today ----------------------------------------------------------------
    LnF::drawTrackedText (g, LnF::toCaps (content.activityCaption), l.activityCaption.toFloat(),
                          LnF::labelFont(), theme.text, 2.2f);

    for (int i = 0; i < content.activity.size() && i < (int) l.activity.size(); ++i)
    {
        g.setColour (i == 0 ? theme.text : theme.textDim);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, content.activity[i], l.activity[(size_t) i], juce::Justification::centredLeft, true);
    }

    if (content.showPageLinks)
    {
        paintLinkRow (g, l.achievementsRow, content.achievementsTitle, content.achievementsDetail, theme.outline);
        paintLinkRow (g, l.settingsRow, content.settingsTitle, content.settingsDetail, theme.outline);
    }

    paintLinkRow (g, l.supportRow, content.supportTitle, content.supportDetail, theme.accentWarm);
}

void ProfileScreenComponent::paintSupport (juce::Graphics& g, const Layout& l)
{
    const auto& theme = AbcTrainTheme::current();

    {
        auto back = l.back;
        g.setColour (theme.outline);
        g.drawRect (back, 1);
        LnF::drawTrackedText (g, LnF::toCaps (juce::String::fromUTF8 ("\xe2\x80\xb9  ") + content.backLabel),
                              back.reduced (12, 0).toFloat(), LnF::labelFont(), theme.text, 1.8f);
    }

    g.setColour (theme.textBright);
    g.setFont (LnF::titleFont());
    LnF::fitText (g, content.supportPageTitle, l.title, juce::Justification::centredLeft, true);

    g.setColour (theme.text);
    g.setFont (LnF::bodyFont());
    LnF::fitLines (g, content.supportBody, l.body, juce::Justification::topLeft, 10, 0.9f);

    if (content.donationAllowed)
    {
        LnF::drawTrackedText (g, LnF::toCaps (content.donateCaption), l.donateCaption.toFloat(),
                              LnF::labelFont(), theme.textDim, 2.2f);

        g.setColour (theme.accentWarm);
        g.fillRect (l.donateButton);
        LnF::drawTrackedText (g, LnF::toCaps (content.donateButton), l.donateButton.toFloat(),
                              LnF::headingFont().withHeight (18.0f), LnF::labelColourOn (theme.accentWarm), 2.2f,
                              juce::Justification::centred);

        g.setColour (theme.textDim);
        g.setFont (LnF::captionFont());
        LnF::fitLines (g, content.donateNote, l.donateNote, juce::Justification::topLeft, 2, 0.9f);
    }

    LnF::drawTrackedText (g, LnF::toCaps (content.freeCaption), l.freeCaption.toFloat(),
                          LnF::labelFont(), theme.textDim, 2.2f);

    for (int i = 0; i < content.freeItems.size() && i < (int) l.freeRows.size(); ++i)
    {
        auto row = l.freeRows[(size_t) i];
        g.setColour (theme.divider);
        g.fillRect (row.getX(), row.getBottom() - 1, row.getWidth(), 1);
        g.setColour (theme.text);
        g.setFont (LnF::bodyFont());
        LnF::fitText (g, content.freeItems[i], row.withTrimmedRight (24), juce::Justification::centredLeft, true);
        g.setColour (theme.textDim);
        LnF::fitText (g, juce::String::fromUTF8 ("\xe2\x80\xba"), row.removeFromRight (16), juce::Justification::centred, false);
    }
}

void ProfileScreenComponent::mouseDown (const juce::MouseEvent& e)
{
    scroller.mouseDown (e);
}

void ProfileScreenComponent::mouseDrag (const juce::MouseEvent& e)
{
    scroller.mouseDrag (e);
}

void ProfileScreenComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scroller.mouseWheelMove (wheel, 120.0f);
}

void ProfileScreenComponent::mouseUp (const juce::MouseEvent& e)
{
    scroller.mouseUp (e);

    if (! e.mouseWasClicked())
        return;

    const auto p = e.getPosition();
    const auto l = layout();
    const auto call = [] (const std::function<void()>& f) { if (f != nullptr) f(); };

    if (supportShown)
    {
        if (l.back.contains (p))
            showSupportPage (false);
        else if (content.donationAllowed && l.donateButton.contains (p))
            call (onDonate);
        else
            for (int i = 0; i < (int) l.freeRows.size(); ++i)
                if (l.freeRows[(size_t) i].contains (p) && onFreeItem != nullptr)
                    onFreeItem (i);
        return;
    }

    for (int i = 0; i < (int) l.periodChips.size(); ++i)
        if (l.periodChips[(size_t) i].contains (p))
        {
            if (onPeriodChosen != nullptr)
                onPeriodChosen (i);
            return;
        }

    if (l.supportRow.contains (p))
        showSupportPage (true);
    else if (content.showPageLinks && l.achievementsRow.contains (p))
        call (onAchievements);
    else if (content.showPageLinks && l.settingsRow.contains (p))
        call (onSettings);
}
