#include "SupportScreenComponent.h"

#include <cstdlib>
#include "shared/audio/AmbientInstruments.h"
#include "shared/ui/Dimming.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

namespace
{
    constexpr int tickHz = 60;
    constexpr int idleTickHz = 30;

    // juce::String's plain const char* constructor does NOT assume UTF-8
    // (decisions/011). A raw "·" rendered as "Â·".
    juce::String middleDot()
    {
        return juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    }

    // Each item starts 260 ms after the one before and takes 380 ms to
    // arrive: long enough to read as three things, short enough that
    // nobody waits for it.
    constexpr double staggerMs = 260.0;
    constexpr double arriveMs = 380.0;
    constexpr float riseDistance = 10.0f;

    AbcTrainTheme::Family familyForWord (int index)
    {
        // ambiance -> space, balance -> dynamics, clarity -> frequency.
        switch (index)
        {
            case 0:  return AbcTrainTheme::Family::space;
            case 1:  return AbcTrainTheme::Family::dynamics;
            default: return AbcTrainTheme::Family::frequency;
        }
    }

    // Trainings are the frequency-blue of the app's first exercise, the
    // Studio the dynamics-orange of the Learner family; Live has no
    // colour yet because it does not exist yet.
    juce::Colour cardColour (int index)
    {
        if (index == 0) return AbcTrainTheme::accentFor (AbcTrainTheme::Family::frequency);
        if (index == 1) return AbcTrainTheme::accentFor (AbcTrainTheme::Family::dynamics);
        return AbcTrainTheme::current().textDim;
    }

    juce::Font wordmarkFont()
    {
        return AbcTrainLookAndFeel::displayFont().withHeight (54.0f);
    }

    juce::Font headingFont()
    {
        return AbcTrainLookAndFeel::displayFont().withHeight (40.0f);
    }

    void paintTag (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text, juce::Colour colour)
    {
        const auto font = AbcTrainLookAndFeel::microFont();
        const auto caps = AbcTrainLookAndFeel::toCaps (text);
        const auto width = AbcTrainLookAndFeel::trackedTextWidth (caps, font, 1.2f) + 12.0f;
        const auto box = area.withWidth (width).withSizeKeepingCentre (width, 17.0f);

        g.setColour (colour.withAlpha (0.8f));
        g.drawRect (box, 1.0f);
        AbcTrainLookAndFeel::drawTrackedText (g, caps, box, font, colour, 1.2f, juce::Justification::centred);
    }
}

SupportScreenComponent::SupportScreenComponent (LocalisationManager& localisationToUse)
    : localisation (localisationToUse)
{
    setOpaque (true);

    tourButton.onClick = [this]
    {
        if (onTourRequested != nullptr)
            onTourRequested();
    };
    addChildComponent (tourButton);

    nextButton.onClick = [this] { showStep (1); };
    addChildComponent (nextButton);

    donateLink.setURL (juce::URL ("https://soundkorb.ru"));
    starLink.setURL (juce::URL ("https://github.com/bogggare567/abcTrain"));
    for (auto* link : { &donateLink, &starLink })
    {
        link->setJustificationType (juce::Justification::centredRight);
        addChildComponent (*link);
    }

    backButton.onClick = [this] { showStep (0); };
    addChildComponent (backButton);

    continueButton.onClick = [this]
    {
        if (onDismissed != nullptr)
            onDismissed();
    };
    AbcTrainLookAndFeel::makePrimary (continueButton, true);
    addChildComponent (continueButton);

    for (auto* b : { &telegramButton, &sendCodeButton })
    {
        b->setEnabled (false);
        addChildComponent (*b);
    }

    emailField.setEnabled (false);
    emailField.setReadOnly (true);
    emailField.setFont (AbcTrainLookAndFeel::bodyFont());
    emailField.setIndents (12, 0);
    emailField.setJustification (juce::Justification::centredLeft);
    addChildComponent (emailField);

    // The buttons keep their own image: the background under them redraws
    // the whole screen thirty times a second, and a button's text is
    // shaped again on every one of those frames unless it is cached.
    for (juce::Component* c : { (juce::Component*) &tourButton, (juce::Component*) &nextButton,
                                (juce::Component*) &backButton, (juce::Component*) &continueButton,
                                (juce::Component*) &telegramButton, (juce::Component*) &sendCodeButton,
                                (juce::Component*) &donateLink, (juce::Component*) &starLink })
        c->setBufferedToImage (true);

    refresh();
    currentTickHz = tickHz;
    startTimerHz (tickHz);
}

SupportScreenComponent::~SupportScreenComponent()
{
    stopTimer();
}

void SupportScreenComponent::refresh()
{
    nextButton.setButtonText (localisation.getText ("welcome.next"));
    donateLink.setButtonText (localisation.getText ("ui.support"));
    starLink.setButtonText (localisation.getText ("ui.star"));
    backButton.setButtonText (localisation.getText ("welcome.back"));
    continueButton.setButtonText (localisation.getText ("welcome.continueNoAccount"));
    telegramButton.setButtonText (localisation.getText ("welcome.telegram"));
    sendCodeButton.setButtonText (localisation.getText ("welcome.sendCode"));
    emailField.setTextToShowWhenEmpty (localisation.getText ("welcome.emailPlaceholder"),
                                       AbcTrainTheme::current().textDim.withAlpha (0.6f));

    for (auto* link : { &donateLink, &starLink })
    {
        link->setColour (juce::HyperlinkButton::textColourId, AbcTrainTheme::current().accent);
        link->setFont (AbcTrainLookAndFeel::captionFont(), false, juce::Justification::centredRight);
    }

    invalidateForeground();

    // Restart the reveal: a language switch changes the words, and new
    // text appearing already faded in reads as a glitch.
    elapsedMs = 0.0;
    wordReveal = { { 0.0f, 0.0f, 0.0f } };
    sweepPhase = 0.0;

    showStep (0);
}

void SupportScreenComponent::showStep (int newStep)
{
    invalidateForeground();
    step = juce::jlimit (0, 1, newStep);
    const auto inside = (step == 0);

    tourButton.setVisible (inside && tourOffered);
    nextButton.setVisible (inside);
    AbcTrainLookAndFeel::makePrimary (nextButton, ! tourOffered);
    // No donation link in an iOS build: App Store rule 3.1.1 allows money
    // for an app only through the store. The GitHub star goes on a phone
    // too - one more link where there is no room for the cards.
   #if JUCE_IOS
    donateLink.setVisible (false);
   #else
    donateLink.setVisible (inside && getWidth() >= 600);
   #endif
    starLink.setVisible (inside && getWidth() >= 600);

    for (juce::Component* c : { (juce::Component*) &backButton, (juce::Component*) &continueButton,
                                (juce::Component*) &telegramButton, (juce::Component*) &sendCodeButton,
                                (juce::Component*) &emailField })
        c->setVisible (! inside);

    layout();
    repaint();
}

void SupportScreenComponent::completeReveal()
{
    elapsedMs = staggerMs * 2.0 + arriveMs + 1.0;
    wordReveal = { { 1.0f, 1.0f, 1.0f } };
    sweepPhase = 2.0;
    ambientPhase = 3.2;
    invalidateForeground();
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

    // Sixty a second while things are arriving, thirty after: the
    // background figures drift over seconds, and on a phone every frame of
    // this screen is a full-window redraw.
    const auto hz = currentTickHz > 0 ? currentTickHz : tickHz;

    if (sweepPhase < 2.0)
        sweepPhase += 1.0 / (double) hz;

    ambientPhase += 1.0 / (double) hz;
    elapsedMs += 1000.0 / (double) hz;

    for (size_t i = 0; i < wordReveal.size(); ++i)
    {
        const auto start = staggerMs * (double) i;
        wordReveal[i] = (float) juce::jlimit (0.0, 1.0, (elapsedMs - start) / arriveMs);
    }

    const auto wanted = revealDone() ? idleTickHz : tickHz;
    if (wanted != currentTickHz)
    {
        currentTickHz = wanted;
        startTimerHz (wanted);
    }

    // The background is always moving: four thin figures at a few per
    // cent alpha over a cached gradient.
    repaint();
}

void SupportScreenComponent::setTourOffer (juce::String question, juce::String accept,
                                            juce::String decline)
{
    // The question itself is no longer printed: "Show me around" beside
    // "Next" says the same thing without a sentence above it.
    juce::ignoreUnused (decline);
    tourOffered = question.isNotEmpty();
    invalidateForeground();
    tourButton.setButtonText (accept);
    AbcTrainLookAndFeel::makePrimary (tourButton, true);
    showStep (step);
}

//==============================================================================
void SupportScreenComponent::layout()
{
    using namespace AbcTrainTheme;

    lay = {};

    if (getWidth() < 600)
    {
        layoutCompact();
        return;
    }

    // A frame of fixed proportions, centred: at 940x620 it fills the
    // window, and on a big screen it stays a readable block rather than
    // flying apart to the corners.
    //
    // A phone on its side is the same two columns in under 400 points of
    // height: the 44-point margins and a 24-point link row took the room
    // the headphone note needed and left links a finger cannot hit
    // (tools/ClickMap). Short windows get thin margins and finger-sized
    // targets.
    const auto shortWindow = getHeight() < 500;
    const auto targetHeight = shortWindow ? 44 : 40;
    auto frame = getLocalBounds().reduced (shortWindow ? 28 : 60, shortWindow ? 10 : 44);
    frame = frame.withSizeKeepingCentre (juce::jmin (frame.getWidth(), 1100),
                                         juce::jmin (frame.getHeight(), 640));

    auto columns = frame;
    auto left = columns.removeFromLeft (juce::roundToInt ((float) columns.getWidth() * 0.43f));
    columns.removeFromLeft (Spacing::large * 2);
    auto right = columns;

    auto buttons = left.removeFromBottom (targetHeight);

    if (step == 0)
    {
        left.removeFromTop (shortWindow ? Spacing::small : Spacing::large);
        lay.wordmark = left.removeFromTop (64);
        lay.words = left.removeFromTop (28);
        left.removeFromTop (Spacing::medium);
        lay.tagline = left.removeFromTop (48);
        left.removeFromTop (Spacing::small);
        lay.stepLabel = left.removeFromTop (18);
        left.removeFromBottom (shortWindow ? Spacing::small : Spacing::large);
        lay.note = left.removeFromBottom (40);

        if (tourOffered)
        {
            tourButton.setBounds (buttons.removeFromLeft (200));
            buttons.removeFromLeft (Spacing::medium);
        }

        nextButton.setBounds (buttons.removeFromLeft (130));

        // Three cards stepping down and to the right, the way a staircase
        // does - it is the shape of every exercise in the app.
        auto links = right.removeFromBottom (shortWindow ? 44 : 24);
        starLink.setBounds (links.removeFromRight (130));
        links.removeFromRight (Spacing::medium);
        donateLink.setBounds (links.removeFromRight (160));

        right.removeFromBottom (shortWindow ? Spacing::small : Spacing::large);
        // The staircase's steps close up in a short window: the cards'
        // own text needs the height more than the gaps do.
        const auto minGap = shortWindow ? 6 : 28;
        const auto cardHeight = juce::jmin (112, (right.getHeight() - 2 * minGap) / 3);
        const auto step = juce::jmin (48, right.getWidth() / 8);
        const auto cardWidth = right.getWidth() - 2 * step;
        const auto gapY = (right.getHeight() - cardHeight * 3) / 2;

        for (int i = 0; i < 3; ++i)
            lay.cards[(size_t) i] = { right.getX() + step * i, right.getY() + (cardHeight + gapY) * i,
                                      cardWidth, cardHeight };
    }
    else
    {
        lay.stepLabel = left.removeFromTop (18);
        left.removeFromTop (Spacing::medium);
        {
            // As tall as the heading actually is: one line in English at
            // this width, two in German or Russian.
            juce::AttributedString text;
            text.append (localisation.getText ("welcome.account.title"), headingFont());
            juce::TextLayout measured;
            measured.createLayout (text, (float) left.getWidth());
            lay.heading = left.removeFromTop (juce::jlimit (48, 110, (int) std::ceil (measured.getHeight()) + 4));
        }

        left.removeFromTop (Spacing::medium);
        lay.body = left.removeFromTop (52);
        left.removeFromTop (Spacing::medium);
        lay.offline = left.removeFromTop (40);

        backButton.setBounds (buttons.removeFromLeft (100));
        buttons.removeFromLeft (Spacing::medium);
        continueButton.setBounds (buttons.removeFromLeft (juce::jmin (300, buttons.getWidth())));

        lay.panel = right;
        auto inner = right.reduced (Spacing::large);
        lay.signIn = inner.removeFromTop (20);
        inner.removeFromTop (Spacing::medium);
        telegramButton.setBounds (inner.removeFromTop (42));
        inner.removeFromTop (Spacing::small);
        lay.orLabel = inner.removeFromTop (22);
        lay.emailNote = inner.removeFromTop (22);
        inner.removeFromTop (Spacing::small);
        emailField.setBounds (inner.removeFromTop (40));
        inner.removeFromTop (Spacing::medium);
        sendCodeButton.setBounds (inner.removeFromTop (40));
        inner.removeFromTop (Spacing::medium);
        lay.privacy = inner.removeFromTop (40);

        // The panel ends where its content does, not at the bottom of the
        // window: an empty half-panel reads as something failed to load.
        lay.panel.setBottom (lay.privacy.getBottom() + Spacing::large);
    }
}

void SupportScreenComponent::resized()
{
    invalidateForeground();
    layout();

    // The links depend on the width (a phone has no room for them), so a
    // rotation re-decides them here as well as in showStep.
    const auto wide = getWidth() >= 600;
    starLink.setVisible (step == 0 && wide);
   #if ! JUCE_IOS
    donateLink.setVisible (step == 0 && wide);
   #endif
}

// A phone held upright (ADR 060): the same two steps in one column - the
// wordmark and the promise on top, the three cards one under another
// instead of stepping down a staircase that needs a second column, the
// buttons along the bottom where a thumb is.
void SupportScreenComponent::layoutCompact()
{
    using namespace AbcTrainTheme;

    auto column = getLocalBounds().reduced (Spacing::large, Spacing::large);
    auto buttons = column.removeFromBottom (44);
    column.removeFromBottom (Spacing::medium);

    starLink.setBounds ({});
    donateLink.setBounds ({});

    if (step == 0)
    {
        lay.wordmark = column.removeFromTop (56);
        lay.words = column.removeFromTop (26);
        column.removeFromTop (Spacing::small);
        lay.tagline = column.removeFromTop (48);
        column.removeFromTop (Spacing::small);
        lay.stepLabel = column.removeFromTop (18);
        column.removeFromTop (Spacing::medium);

        lay.note = column.removeFromBottom (36);
        column.removeFromBottom (Spacing::small);

        const auto cardHeight = juce::jlimit (60, 104, (column.getHeight() - 2 * Spacing::small) / 3);

        for (int i = 0; i < 3; ++i)
        {
            lay.cards[(size_t) i] = column.removeFromTop (cardHeight);
            column.removeFromTop (Spacing::small);
        }

        if (tourOffered)
        {
            tourButton.setBounds (buttons.removeFromLeft ((buttons.getWidth() - Spacing::small) / 2));
            buttons.removeFromLeft (Spacing::small);
        }

        nextButton.setBounds (buttons);
        return;
    }

    lay.stepLabel = column.removeFromTop (18);
    column.removeFromTop (Spacing::small);
    {
        juce::AttributedString text;
        text.append (localisation.getText ("welcome.account.title"), AbcTrainLookAndFeel::headingFont());
        juce::TextLayout measured;
        measured.createLayout (text, (float) column.getWidth());
        lay.heading = column.removeFromTop (juce::jlimit (40, 110, (int) std::ceil (measured.getHeight()) + 4));
    }
    lay.body = column.removeFromTop (60);
    column.removeFromTop (Spacing::small);
    lay.offline = column.removeFromTop (40);
    column.removeFromTop (Spacing::medium);

    lay.panel = column;
    auto inner = column.reduced (Spacing::medium);
    lay.signIn = inner.removeFromTop (20);
    inner.removeFromTop (Spacing::small);
    telegramButton.setBounds (inner.removeFromTop (44));
    inner.removeFromTop (Spacing::small);
    lay.orLabel = inner.removeFromTop (22);
    lay.emailNote = inner.removeFromTop (22);
    emailField.setBounds (inner.removeFromTop (44));
    inner.removeFromTop (Spacing::small);
    sendCodeButton.setBounds (inner.removeFromTop (44));
    inner.removeFromTop (Spacing::small);
    lay.privacy = inner.removeFromTop (40);
    lay.panel.setBottom (lay.privacy.getBottom() + Spacing::medium);

    backButton.setBounds (buttons.removeFromLeft (juce::jmin (110, buttons.getWidth() / 3)));
    buttons.removeFromLeft (Spacing::small);
    continueButton.setBounds (buttons);
}

void SupportScreenComponent::paintWordmark (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& theme = AbcTrainTheme::current();
    const auto font = wordmarkFont();
    constexpr float tracking = 0.0f;
    const juce::String letters ("abcTrain");

    auto x = area.getX();

    for (int i = 0; i < letters.length(); ++i)
    {
        const auto letter = letters.substring (i, i + 1);
        const auto width = AbcTrainLookAndFeel::trackedTextWidth (letter, font, tracking);
        const auto colour = i < 3 ? AbcTrainTheme::accentFor (familyForWord (i)) : theme.textBright;

        // One left-to-right sweep on arrival, the way a needle settles;
        // after that the letters simply are.
        const auto sweep = (float) juce::jlimit (0.0, 1.0, (sweepPhase - (double) i * 0.16) / 0.55);

        AbcTrainLookAndFeel::drawTrackedText (g, letter, area.withX (x).withWidth (width + 1.0f), font,
                                               colour.withAlpha (AbcTrainTheme::Ease::out (sweep)),
                                               tracking, juce::Justification::centredLeft);
        x += width;
    }
}

bool SupportScreenComponent::revealDone() const noexcept
{
    return sweepPhase >= 2.0 && wordReveal[0] >= 1.0f && wordReveal[1] >= 1.0f && wordReveal[2] >= 1.0f;
}

void SupportScreenComponent::paintForeground (juce::Graphics& g)
{
    if (step == 0)
        paintInside (g);
    else
        paintAccount (g);
}

void SupportScreenComponent::paint (juce::Graphics& g)
{
    backgroundCache.paint (g, getLocalBounds().toFloat());
    AmbientInstruments::paint (g, getLocalBounds().toFloat(), ambientPhase);

    // While the words and cards are still arriving every frame differs.
    if (! revealDone())
    {
        paintForeground (g);
        return;
    }

    const auto scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto width = juce::jmax (1, (int) std::ceil ((float) getWidth() * scale));
    const auto height = juce::jmax (1, (int) std::ceil ((float) getHeight() * scale));
    const auto key = localisation.getCurrentLanguage() + "|" + juce::String ((int) AbcTrainTheme::current().mode);

    if (foregroundDirty || key != foregroundKey || ! foregroundCache.isValid()
        || foregroundCache.getWidth() != width || foregroundCache.getHeight() != height)
    {
        foregroundCache = juce::Image (juce::Image::ARGB, width, height, true);
        juce::Graphics inner (foregroundCache);
        inner.addTransform (juce::AffineTransform::scale (scale));
        paintForeground (inner);
        foregroundKey = key;
        foregroundDirty = false;
    }

    g.drawImage (foregroundCache, getLocalBounds().toFloat());
}

namespace
{
    // A phone has no laptop speakers to warn about - its own speaker is
    // the thing to warn about.
    const char* headphoneNoteKey()
    {
       #if JUCE_IOS || JUCE_ANDROID
        return "ui.headphoneNotePhone";
       #else
        return std::getenv ("SNAP_SAFE") != nullptr ? "ui.headphoneNotePhone" : "ui.headphoneNote";
       #endif
    }
}

void SupportScreenComponent::paintInside (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();

    paintWordmark (g, lay.wordmark.toFloat());

    // The three words, arriving one at a time, each in its family colour.
    {
        const char* const wordKeys[] = { "brand.a", "brand.b", "brand.c" };
        const auto font = AbcTrainLookAndFeel::titleFont();
        auto x = (float) lay.words.getX();
        const auto separatorWidth = AbcTrainLookAndFeel::trackedTextWidth (middleDot(), font, 1.0f);

        for (size_t i = 0; i < 3; ++i)
        {
            const auto word = localisation.getText (wordKeys[i]);
            const auto eased = AbcTrainTheme::Ease::out (wordReveal[i]);
            const auto width = AbcTrainLookAndFeel::trackedTextWidth (word, font, 1.0f);

            AbcTrainLookAndFeel::drawTrackedText (
                g, word, lay.words.toFloat().withX (x).withWidth (width + 2.0f)
                             .translated (0.0f, (1.0f - eased) * riseDistance),
                font, AbcTrainTheme::accentFor (familyForWord ((int) i)).withAlpha (eased), 1.0f);
            x += width;

            if (i < 2)
            {
                g.setColour (theme.textDim.withAlpha (0.5f * eased));
                g.setFont (font);
                AbcTrainLookAndFeel::fitText (g, middleDot(), juce::Rectangle<float> (x, (float) lay.words.getY(), separatorWidth,
                                                                  (float) lay.words.getHeight()),
                            juce::Justification::centred, false);
                x += separatorWidth;
            }
        }
    }

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::bodyFont().withHeight (18.0f));
    AbcTrainLookAndFeel::fitLines (g, localisation.getText ("welcome.tagline"), lay.tagline,
                      juce::Justification::topLeft, 2, 1.0f);

    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (localisation.getText ("welcome.step1")),
                                           lay.stepLabel.toFloat(), AbcTrainLookAndFeel::microFont(),
                                           theme.textDim, 1.4f);

    // Said once, quietly but at body size: on laptop speakers several
    // exercises are answerable only by guessing, and it is better to say
    // so than to let somebody conclude their ears are the problem.
    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    AbcTrainLookAndFeel::fitLines (g, localisation.getText (headphoneNoteKey()), lay.note,
                      juce::Justification::bottomLeft, 2, 1.0f);

    const char* const titles[] = { "ui.trainings", "ui.studio", "welcome.live" };
    const char* const bodies[] = { "welcome.trainings.body", "welcome.studio.body", "welcome.live.body" };

    for (int i = 0; i < 3; ++i)
    {
        const auto eased = AbcTrainTheme::Ease::out (wordReveal[(size_t) i]);
        auto card = lay.cards[(size_t) i].toFloat().translated (0.0f, (1.0f - eased) * riseDistance);
        const auto soon = (i == 2);
        const auto colour = cardColour (i);

        // Through Dimming::paint, not beginTransparencyLayer: that opened
        // a layer the size of the window for every card on every frame
        // of the background's animation, long after the cards had
        // arrived - the bulk of what this screen cost on a phone.
        Dimming::paint (g, card.getSmallestIntegerContainer().expanded (2), eased * (soon ? 0.7f : 1.0f),
                        [&] (juce::Graphics& g)   // the card's own image, same name as the screen's
        {
            g.setColour (theme.panelBackground.withAlpha (0.94f));
            g.fillRect (card);
            g.setColour (soon ? theme.outline : colour.withAlpha (0.85f));
            g.drawRect (card, 1.0f);

            // A short card (a phone on its side) gives its padding to the
            // text: at the full 14 + 28 the body had 34 points for three
            // lines and was drawn squeezed to a smudge.
            const auto shortCard = card.getHeight() < 104.0f;
            auto inner = card.reduced (shortCard ? 14.0f : 18.0f, shortCard ? 7.0f : 14.0f);
            auto titleRow = inner.removeFromTop (shortCard ? 24.0f : 28.0f);
            g.setColour (colour);
            g.fillRect (titleRow.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 10.0f));
            titleRow.removeFromLeft (8.0f);

            const auto title = localisation.getText (titles[i]);
            const auto titleFont = AbcTrainLookAndFeel::titleFont();
            g.setColour (soon ? theme.textDim : theme.textBright);
            g.setFont (titleFont);
            AbcTrainLookAndFeel::fitText (g, title, titleRow, juce::Justification::centredLeft, false);

            if (soon)
            {
                const auto titleWidth = juce::GlyphArrangement::getStringWidth (titleFont, title);
                paintTag (g, titleRow.withTrimmedLeft (titleWidth + 10.0f),
                          localisation.getText ("welcome.soon"), theme.textDim);
            }

            inner.removeFromTop (shortCard ? 2.0f : 6.0f);
            g.setColour (soon ? theme.textDim : theme.text);
            g.setFont (AbcTrainLookAndFeel::captionFont());
            AbcTrainLookAndFeel::fitLines (g, localisation.getText (bodies[i]), inner.toNearestInt(),
                              juce::Justification::topLeft, 3, 1.0f);
        });
    }
}

void SupportScreenComponent::paintAccount (juce::Graphics& g)
{
    const auto& theme = AbcTrainTheme::current();
    const auto liveColour = AbcTrainTheme::accentFor (AbcTrainTheme::Family::dynamics);

    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (localisation.getText ("welcome.step2")),
                                           lay.stepLabel.toFloat(), AbcTrainLookAndFeel::microFont(),
                                           theme.textDim, 1.4f);

    g.setColour (theme.textBright);
    g.setFont (headingFont());
    AbcTrainLookAndFeel::fitLines (g, localisation.getText ("welcome.account.title"), lay.heading,
                      juce::Justification::topLeft, 3, 0.8f);

    g.setColour (theme.text);
    g.setFont (AbcTrainLookAndFeel::bodyFont());
    AbcTrainLookAndFeel::fitLines (g, localisation.getText ("welcome.account.body"), lay.body,
                      juce::Justification::topLeft, 3, 1.0f);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    AbcTrainLookAndFeel::fitLines (g, localisation.getText ("welcome.account.offline"), lay.offline,
                      juce::Justification::topLeft, 2, 1.0f);

    // The panel: what signing in will look like, greyed out.
    g.setColour (theme.panelBackground.withAlpha (0.6f));
    g.fillRect (lay.panel.toFloat());
    g.setColour (theme.outline);
    g.drawRect (lay.panel.toFloat(), 1.0f);

    {
        const auto caps = AbcTrainLookAndFeel::toCaps (localisation.getText ("welcome.signIn"));
        const auto font = AbcTrainLookAndFeel::microFont();
        const auto width = AbcTrainLookAndFeel::trackedTextWidth (caps, font, 1.4f);
        AbcTrainLookAndFeel::drawTrackedText (g, caps, lay.signIn.toFloat(), font, theme.textDim, 1.4f);
        paintTag (g, lay.signIn.toFloat().withTrimmedLeft (width + 12.0f),
                  localisation.getText ("welcome.comingWithLive"), liveColour);
    }

    AbcTrainLookAndFeel::drawTrackedText (g, AbcTrainLookAndFeel::toCaps (localisation.getText ("welcome.or")),
                                           lay.orLabel.toFloat(), AbcTrainLookAndFeel::microFont(),
                                           theme.textDim, 1.4f, juce::Justification::centred);

    g.setColour (theme.textDim);
    g.setFont (AbcTrainLookAndFeel::captionFont());
    AbcTrainLookAndFeel::fitLines (g, localisation.getText ("welcome.emailNote"), lay.emailNote,
                      juce::Justification::centredLeft, 1, 0.9f);
    AbcTrainLookAndFeel::fitLines (g, localisation.getText ("welcome.privacy"), lay.privacy,
                      juce::Justification::topLeft, 2, 1.0f);
}
