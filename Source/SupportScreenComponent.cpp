#include "SupportScreenComponent.h"
#include "../shared/AmbientInstruments.h"
#include "../shared/AbcTrainLookAndFeel.h"
#include "../shared/AbcTrainTheme.h"
#include <BrandBinaryData.h>

namespace
{
    constexpr int tickHz = 60;

    // juce::String's plain const char* constructor does NOT assume UTF-8 -
    // a known JUCE gotcha this project has already been bitten by once
    // (see decisions/011, where the language names mojibake'd the same
    // way). A raw "·" here rendered on screen as "Â·". Caught by looking
    // at a render; a test would never have noticed.
    juce::String middleDot()
    {
        return juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    }

    // The whole block, measured once so paint() and resized() cannot drift
    // apart - they both lay out the same vertical run, and two copies of
    // one layout is how a screen ends up with its text in a different
    // place from its buttons.
    constexpr int iconHeight = 76;
    constexpr int wordmarkHeight = 54;
    constexpr int wordsHeight = 26;
    constexpr int continueHeight = 36;
    constexpr int asksHeight = 32;

    // Each word starts 260ms after the one before, and takes 380ms to
    // arrive. The whole thing is over in about a second: long enough to
    // read as three separate ideas, short enough that nobody waits for it.
    constexpr double wordStaggerMs = 260.0;
    constexpr double wordArriveMs = 380.0;
    constexpr float riseDistance = 10.0f;

    AbcTrainTheme::Family familyForWord (int index)
    {
        // ambiance -> space, balance -> dynamics, clarity -> frequency.
        // The mapping is the whole point of the acronym, so it is spelled
        // out rather than derived from the index.
        switch (index)
        {
            case 0:  return AbcTrainTheme::Family::space;
            case 1:  return AbcTrainTheme::Family::dynamics;
            default: return AbcTrainTheme::Family::frequency;
        }
    }
}

SupportScreenComponent::SupportScreenComponent (LocalisationManager& localisationToUse)
    : localisation (localisationToUse)
{
    setOpaque (true);

    appIcon = juce::ImageCache::getFromMemory (BrandBinaryData::eartrainer_png,
                                                BrandBinaryData::eartrainer_pngSize);

    donateButton.onClick = []
    {
        juce::URL ("https://soundkorb.ru").launchInDefaultBrowser();
    };
    addAndMakeVisible (donateButton);

    starButton.onClick = []
    {
        juce::URL ("https://github.com/bogggare567/abcTrain").launchInDefaultBrowser();
    };
    addAndMakeVisible (starButton);

    continueButton.onClick = [this]
    {
        if (onDismissed != nullptr)
            onDismissed();
    };
    addAndMakeVisible (continueButton);

    // Both hidden unless setTourOffer says otherwise, so a returning player
    // never sees them.
    tourButton.onClick = [this]
    {
        if (onTourRequested != nullptr)
            onTourRequested();
    };
    tourButton.setColour (juce::TextButton::buttonColourId,
                           AbcTrainTheme::current().accent.withAlpha (0.9f));
    addChildComponent (tourButton);

    noTourButton.onClick = [this]
    {
        if (onDismissed != nullptr)
            onDismissed();
    };
    addChildComponent (noTourButton);

    addAndMakeVisible (repoLink);

    refresh();
    startTimerHz (tickHz);
}

SupportScreenComponent::~SupportScreenComponent()
{
    stopTimer();
}

void SupportScreenComponent::refresh()
{
    donateButton.setButtonText (localisation.getText ("ui.support"));
    starButton.setButtonText (localisation.getText ("ui.star"));
    continueButton.setButtonText (localisation.getText ("ui.continue"));

    // Restart the reveal: a language switch changes the three words, and
    // showing new text already faded in would look like a glitch.
    elapsedMs = 0.0;
    wordReveal = { { 0.0f, 0.0f, 0.0f } };
    bouncePhase = 0.0;

    repaint();
}

void SupportScreenComponent::completeReveal()
{
    elapsedMs = wordStaggerMs * 2.0 + wordArriveMs + 1.0;
    wordReveal = { { 1.0f, 1.0f, 1.0f } };

    // The wordmark's own sweep too, or a still frame shows no letters at
    // all - the same reason this method exists for the three words.
    bouncePhase = 2.0;

    // Part-way into the first scene rather than at zero, so the contact
    // sheet catches the background mid-figure instead of at its flattest.
    ambientPhase = 3.2;
    repaint();
}

void SupportScreenComponent::visibilityChanged()
{
    if (isVisible())
        refresh();
}

void SupportScreenComponent::timerCallback()
{
    if (! isVisible())
        return;

    // The reveal finishes; the bounce does not, so this no longer returns
    // early once the words have arrived.
    // The wordmark sweep runs once and stops; the background keeps its
    // own clock, which is what is still moving after that.
    if (bouncePhase < 2.0)
        bouncePhase += 1.0 / (double) tickHz;

    ambientPhase += 1.0 / (double) tickHz;

    elapsedMs += 1000.0 / (double) tickHz;

    auto revealing = false;

    for (size_t i = 0; i < wordReveal.size(); ++i)
    {
        const auto start = wordStaggerMs * (double) i;
        const auto target = (float) juce::jlimit (0.0, 1.0, (elapsedMs - start) / wordArriveMs);
        revealing = revealing || ! juce::approximatelyEqual (wordReveal[i], target);
        wordReveal[i] = target;
    }

    // Once the reveal has finished, the only thing still moving is the
    // bouncing wordmark - so only that strip is repainted. This screen
    // opens on every launch, and repainting the whole gradient-and-noise
    // window at 60 Hz forever was the app's first impression.
    // The background is always moving, so the whole screen repaints. It
    // is four thin figures at a few per cent alpha over a cached gradient,
    // not the every-frame full re-render the letters used to force.
    repaint();
}

void SupportScreenComponent::paintWordmark (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& theme = AbcTrainTheme::current();
    // Bigger and tighter than body text by a long way. Without a licensed
    // display face this is as much of a wordmark as a system font can be
    // made into: weight, size and negative-ish tracking doing the work a
    // drawn logotype would otherwise do.
    const auto font = AbcTrainLookAndFeel::titleFont();
    constexpr float tracking = 0.4f;

    // "abc" in the three family colours, "Train" in plain bright text.
    // Drawn glyph by glyph because the three letters need three colours
    // and JUCE has no rich-text drawText.
    const juce::String letters ("abcTrain");

    auto totalWidth = 0.0f;
    for (int i = 0; i < letters.length(); ++i)
        totalWidth += AbcTrainLookAndFeel::trackedTextWidth (letters.substring (i, i + 1), font, tracking);

    auto x = area.getCentreX() - totalWidth * 0.5f;

    for (int i = 0; i < letters.length(); ++i)
    {
        const auto letter = letters.substring (i, i + 1);
        const auto width = AbcTrainLookAndFeel::trackedTextWidth (letter, font, tracking);

        const auto colour = i < 3 ? AbcTrainTheme::accentFor (familyForWord (i))
                                  : theme.textBright;

        // Still. The hop is gone.
        //
        // It was the only moving thing on the screen, which put the whole
        // weight of "this is alive" on a gag - and one that read as a toy
        // beside the rest of the product. What is left is a single
        // left-to-right reveal on arrival, the way a needle sweeps once
        // and settles; after that the letters simply are. The motion moved
        // to the background, where a background belongs.
        const auto sweep = (float) juce::jlimit (0.0, 1.0,
            (bouncePhase - (double) i * 0.16) / 0.55);

        const auto eased = AbcTrainTheme::Ease::out (sweep);

        AbcTrainLookAndFeel::drawTrackedText (g, letter,
                                               area.withX (x).withWidth (width),
                                               font, colour.withAlpha (eased), tracking,
                                               juce::Justification::centred);
        x += width;
    }
}

void SupportScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

    // The instruments this product is about, drifting behind everything.
    // This screen used to have exactly one moving thing - three hopping
    // letters - which put the whole weight of "this is alive" on a gag.
    // The motion lives here now and the wordmark is still.
    // Full bounds, no inset: the figures are supposed to run to the
    // edges and past them, the way a backdrop does. The inset version
    // read as a small animated panel - "a microscope", per the report -
    // which is a widget, not weather.
    AmbientInstruments::paint (g, getLocalBounds().toFloat(), ambientPhase);

    auto area = getLocalBounds().reduced (AbcTrainTheme::Spacing::large * 2);

    // Centred vertically rather than pinned to the top: the first version
    // hung everything off the top edge and left 250px of empty window
    // under the buttons, which reads as an unfinished screen.
    area = contentArea (area);

    // The real app icon, not a stand-in glyph: the first thing seen should
    // be the same mark that is in the dock.
    const auto iconBox = area.removeFromTop (iconHeight).withSizeKeepingCentre (72, 72);

    if (appIcon.isValid())
        g.drawImage (appIcon, iconBox.toFloat(), juce::RectanglePlacement::centred);

    area.removeFromTop (AbcTrainTheme::Spacing::medium);

    // Remembered so the steady-state bounce can repaint just this strip.
    // Expanded past the lift height because a letter mid-hop paints above
    // the strip's own top edge.
    const auto wordmarkStrip = area.removeFromTop (wordmarkHeight);
    wordmarkRepaintArea = wordmarkStrip.expanded (0, 10);
    paintWordmark (g, wordmarkStrip.toFloat());

    area.removeFromTop (AbcTrainTheme::Spacing::small);

    // --- the three words, arriving one at a time -------------------------
    {
        const char* const wordKeys[] = { "brand.a", "brand.b", "brand.c" };
        const auto wordFont = AbcTrainLookAndFeel::titleFont();
        auto row = area.removeFromTop (wordsHeight).toFloat();

        // Laid out as one centred line, measured first so the words don't
        // shift sideways as later ones appear.
        std::array<juce::String, 3> words;
        auto totalWidth = 0.0f;

        for (size_t i = 0; i < words.size(); ++i)
        {
            words[i] = localisation.getText (wordKeys[i]);
            totalWidth += AbcTrainLookAndFeel::trackedTextWidth (words[i], wordFont, 1.0f);
        }

        const auto separatorWidth = AbcTrainLookAndFeel::trackedTextWidth (middleDot(), wordFont, 1.0f);
        totalWidth += separatorWidth * 2.0f;

        auto x = row.getCentreX() - totalWidth * 0.5f;

        for (size_t i = 0; i < words.size(); ++i)
        {
            const auto eased = AbcTrainTheme::Ease::out (wordReveal[i]);
            const auto width = AbcTrainLookAndFeel::trackedTextWidth (words[i], wordFont, 1.0f);

            AbcTrainLookAndFeel::drawTrackedText (
                g, words[i],
                row.withX (x).withWidth (width).translated (0.0f, (1.0f - eased) * riseDistance),
                wordFont,
                AbcTrainTheme::accentFor (familyForWord ((int) i)).withAlpha (eased),
                1.0f, juce::Justification::centred);

            x += width;

            if (i + 1 < words.size())
            {
                g.setColour (theme.textDim.withAlpha (0.5f * eased));
                g.setFont (wordFont);
                g.drawText (middleDot(), row.withX (x).withWidth (separatorWidth).toNearestInt(),
                             juce::Justification::centred, false);
                x += separatorWidth;
            }
        }
    }

    area.removeFromTop (AbcTrainTheme::Spacing::large);


    if (tourQuestion.isNotEmpty())
    {
        g.setColour (AbcTrainTheme::current().textDim);
        g.setFont (AbcTrainLookAndFeel::labelFont());
        g.drawText (tourQuestion, tourQuestionBounds, juce::Justification::centred, true);
    }
}

juce::Rectangle<int> SupportScreenComponent::contentArea (juce::Rectangle<int> bounds) const
{
    using namespace AbcTrainTheme;

    const auto total = iconHeight + Spacing::medium + wordmarkHeight
                           + Spacing::small + wordsHeight
                           + Spacing::large + continueHeight
                           + Spacing::medium + asksHeight;

    return bounds.withHeight (juce::jmin (bounds.getHeight(), total))
                 .withY (bounds.getY() + juce::jmax (0, (bounds.getHeight() - total) / 2));
}

void SupportScreenComponent::setTourOffer (juce::String question, juce::String accept,
                                            juce::String decline)
{
    tourQuestion = std::move (question);
    tourButton.setButtonText (accept);
    noTourButton.setButtonText (decline);

    const auto offering = tourQuestion.isNotEmpty();
    tourButton.setVisible (offering);
    noTourButton.setVisible (offering);
    continueButton.setVisible (! offering);

    resized();
    repaint();
}

void SupportScreenComponent::resized()
{
    using namespace AbcTrainTheme;

    auto area = contentArea (getLocalBounds().reduced (Spacing::large * 2));

    area.removeFromTop (iconHeight + Spacing::medium + wordmarkHeight
                        + Spacing::small + wordsHeight
                        + Spacing::large);

    // "Continue" is the primary action and sits alone, above the two asks
    // rather than below them: the screen is an offer, not a toll gate, and
    // the way onward should be the easiest thing to find.
    {
        auto primary = area.removeFromTop (continueHeight);

        if (tourQuestion.isNotEmpty())
        {
            // Two buttons of the same size, side by side. Making the accept
            // bigger would be the screen having an opinion about what you
            // should want.
            tourQuestionBounds = primary.withHeight (18).translated (0, -22);

            auto pair = primary.withSizeKeepingCentre (330, 38);
            tourButton.setBounds (pair.removeFromLeft (196));
            pair.removeFromLeft (10);
            noTourButton.setBounds (pair.withSizeKeepingCentre (124, 30));
        }
        else
        {
            continueButton.setBounds (primary.withSizeKeepingCentre (180, 36));
        }
    }

    area.removeFromTop (AbcTrainTheme::Spacing::medium);

    auto row = area.removeFromTop (asksHeight).withSizeKeepingCentre (
                   juce::jmin (area.getWidth(), 340), 32);
    donateButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
    row.removeFromLeft (8);
    starButton.setBounds (row);

    repoLink.setBounds (getLocalBounds().removeFromBottom (34)
                            .reduced (AbcTrainTheme::Spacing::large, 8));
}
