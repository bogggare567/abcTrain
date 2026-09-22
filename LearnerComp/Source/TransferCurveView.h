#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CompressorEngine.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

// Level in against level out: the one picture that *is* a compressor.
// Below the threshold the line runs at 45 degrees; above it, it bends
// over by the ratio, and the knee rounds the corner. A dot rides the line
// at the level going in right now, so turning Threshold down visibly
// pushes the music into the bend.
//
// Drawn from CompressorEngine::staticReductionDb, the formula the DSP
// runs, so the bend on screen is the bend in the audio. Makeup lifts the
// whole line and is drawn as that. This took the place of the live
// spectrum, which in a compressor showed the source and not the process.
class TransferCurveView : public juce::Component
{
public:
    struct Strings
    {
        juce::String in = "in, dB";
        juce::String out = "out, dB";
    };

    void setStrings (Strings s)           { text = std::move (s); repaint(); }
    void setAccentColour (juce::Colour c) { accent = c; repaint(); }

    void setParameters (float thresholdDb, float ratio, float kneeDb, float makeupDb)
    {
        if (juce::approximatelyEqual (thresholdDb, threshold) && juce::approximatelyEqual (ratio, this->ratio)
            && juce::approximatelyEqual (kneeDb, knee) && juce::approximatelyEqual (makeupDb, makeup))
            return;

        threshold = thresholdDb;
        this->ratio = ratio;
        knee = kneeDb;
        makeup = makeupDb;
        repaint();
    }

    // The input peak, in dB; below the floor the dot is hidden.
    void setInputLevel (float db)
    {
        if (std::abs (db - inputDb) < 0.05f)
            return;

        inputDb = db;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto bounds = getLocalBounds().toFloat();

        AbcTrainLookAndFeel::paintDisplayWell (g, bounds);
        AbcTrainLookAndFeel::drawRegistrationMarks (g, bounds, theme.outline);

        const auto plot = bounds.reduced (12.0f);
        const auto toX = [&] (float db) { return plot.getX() + (db - minDb) / (maxDb - minDb) * plot.getWidth(); };
        const auto toY = [&] (float db) { return plot.getBottom() - (db - minDb) / (maxDb - minDb) * plot.getHeight(); };

        // Grid every 12 dB.
        g.setFont (AbcTrainLookAndFeel::microFont());

        for (float db = minDb + 12.0f; db < maxDb; db += 12.0f)
        {
            g.setColour (theme.divider.withAlpha (0.55f));
            g.fillRect (toX (db), plot.getY(), 1.0f, plot.getHeight());
            g.fillRect (plot.getX(), toY (db), plot.getWidth(), 1.0f);
        }

        g.setColour (theme.textDim);
        g.drawText (text.out, plot.withHeight (14.0f).translated (4.0f, 2.0f), juce::Justification::centredLeft, false);
        g.drawText (text.in, plot.withTrimmedTop (plot.getHeight() - 16.0f).translated (0.0f, -2.0f),
                    juce::Justification::centredRight, false);

        // The unity line, dashed: what "no compression" would be.
        {
            juce::Path unity;
            unity.startNewSubPath (toX (minDb), toY (minDb));
            unity.lineTo (toX (maxDb), toY (maxDb));
            const float pattern[] = { 4.0f, 4.0f };
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, unity, pattern, 2);
            g.setColour (theme.textDim.withAlpha (0.5f));
            g.fillPath (dashed);
        }

        // The threshold, where the bend begins.
        g.setColour (accent.withAlpha (0.35f));
        g.fillRect (toX (threshold), plot.getY(), 1.0f, plot.getHeight());

        juce::Path curve;

        for (int i = 0; i <= 120; ++i)
        {
            const auto in = minDb + (maxDb - minDb) * (float) i / 120.0f;
            const auto out = juce::jlimit (minDb - 12.0f, maxDb + 12.0f, outputFor (in));
            const auto p = juce::Point<float> (toX (in), toY (out));

            if (i == 0) curve.startNewSubPath (p);
            else        curve.lineTo (p);
        }

        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (plot.toNearestInt());
            g.setColour (accent);
            g.strokePath (curve, juce::PathStrokeType (2.0f));

            if (inputDb > minDb + 1.0f)
            {
                const auto in = juce::jmin (maxDb, inputDb);
                const auto centre = juce::Point<float> (toX (in), toY (outputFor (in)));
                g.setColour (theme.textBright);
                g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (centre));
            }
        }
    }

private:
    static constexpr float minDb = -60.0f, maxDb = 0.0f;

    float outputFor (float in) const noexcept
    {
        return in - CompressorEngine::staticReductionDb (in, threshold, ratio, knee) + makeup;
    }

    Strings text;
    juce::Colour accent { juce::Colours::orange };
    float threshold = -12.0f, ratio = 4.0f, knee = 0.0f, makeup = 0.0f;
    float inputDb = -100.0f;
};
