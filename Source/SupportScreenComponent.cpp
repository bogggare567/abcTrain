#include "SupportScreenComponent.h"
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
    constexpr int wordmarkHeight = 40;
    constexpr int wordsHeight = 26;
    constexpr int bodyHeight = 76;
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

    repaint();
}

void SupportScreenComponent::completeReveal()
{
    elapsedMs = wordStaggerMs * 2.0 + wordArriveMs + 1.0;
    wordReveal = { { 1.0f, 1.0f, 1.0f } };
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

    const auto total = wordStaggerMs * 2.0 + wordArriveMs;

    if (elapsedMs > total)
        return;

    elapsedMs += 1000.0 / (double) tickHz;

    for (size_t i = 0; i < wordReveal.size(); ++i)
    {
        const auto start = wordStaggerMs * (double) i;
        wordReveal[i] = (float) juce::jlimit (0.0, 1.0, (elapsedMs - start) / wordArriveMs);
    }

    repaint();
}

void SupportScreenComponent::paintWordmark (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& theme = AbcTrainTheme::current();
    const auto font = juce::Font (juce::FontOptions (34.0f).withStyle ("Bold"));
    constexpr float tracking = 1.5f;

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

        AbcTrainLookAndFeel::drawTrackedText (g, letter,
                                               area.withX (x).withWidth (width),
                                               font, colour, tracking,
                                               juce::Justification::centred);
        x += width;
    }
}

void SupportScreenComponent::paint (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    AbcTrainLookAndFeel::paintPanelBackground (g, getLocalBounds().toFloat());

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

    paintWordmark (g, area.removeFromTop (wordmarkHeight).toFloat());

    area.removeFromTop (AbcTrainTheme::Spacing::small);

    // --- the three words, arriving one at a time -------------------------
    {
        const char* const wordKeys[] = { "brand.a", "brand.b", "brand.c" };
        const auto wordFont = juce::Font (juce::FontOptions (15.0f));
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

    g.setColour (theme.textDim);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawFittedText (localisation.getText ("ui.supportBody"),
                       area.removeFromTop (bodyHeight), juce::Justification::centredTop, 5);
}

juce::Rectangle<int> SupportScreenComponent::contentArea (juce::Rectangle<int> bounds) const
{
    using namespace AbcTrainTheme;

    const auto total = iconHeight + Spacing::medium + wordmarkHeight
                           + Spacing::small + wordsHeight
                           + Spacing::large + bodyHeight
                           + Spacing::large + continueHeight
                           + Spacing::medium + asksHeight;

    return bounds.withHeight (juce::jmin (bounds.getHeight(), total))
                 .withY (bounds.getY() + juce::jmax (0, (bounds.getHeight() - total) / 2));
}

void SupportScreenComponent::resized()
{
    using namespace AbcTrainTheme;

    auto area = contentArea (getLocalBounds().reduced (Spacing::large * 2));

    area.removeFromTop (iconHeight + Spacing::medium + wordmarkHeight
                        + Spacing::small + wordsHeight
                        + Spacing::large + bodyHeight
                        + Spacing::large);

    // "Continue" is the primary action and sits alone, above the two asks
    // rather than below them: the screen is an offer, not a toll gate, and
    // the way onward should be the easiest thing to find.
    continueButton.setBounds (area.removeFromTop (continueHeight).withSizeKeepingCentre (180, 36));
    area.removeFromTop (AbcTrainTheme::Spacing::medium);

    auto row = area.removeFromTop (asksHeight).withSizeKeepingCentre (
                   juce::jmin (area.getWidth(), 340), 32);
    donateButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
    row.removeFromLeft (8);
    starButton.setBounds (row);

    repoLink.setBounds (getLocalBounds().removeFromBottom (34)
                            .reduced (AbcTrainTheme::Spacing::large, 8));
}
