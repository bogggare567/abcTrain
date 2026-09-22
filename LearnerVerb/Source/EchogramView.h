#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "shared/dsp/ReverbMeasure.h"
#include "shared/ui/AbcTrainLookAndFeel.h"
#include "shared/ui/AbcTrainTheme.h"

// The reverb as a picture of what it does to one click: the dry hit, the
// gap the pre-delay leaves, the first reflections, and the tail dying
// away, with its length measured off that same response.
//
// Not a drawing of what the knobs claim. The engine is run on the message
// thread from scratch whenever a setting changes, so a Size that did
// nothing or a Decay that lied would show here as plainly as it would in
// a test (ADR 037). This replaces the live spectrum, which in a reverb
// mostly showed the source - the room is in time, not in frequency.
class EchogramView : public juce::Component
{
public:
    struct Strings
    {
        juce::String dry = "dry";
        juce::String firstReflections = "first reflections after {{ms}} ms";
        juce::String tail = "tail {{s}} s";
        juce::String seconds = "s";
    };

    void setStrings (Strings s)             { text = std::move (s); repaint(); }
    void setAccentColour (juce::Colour c)   { accent = c; repaint(); }
    void setDecimalSeparator (juce::String s) { decimal = std::move (s); repaint(); }

    // Cheap to call every frame: only a changed setting recomputes, and
    // never more often than every 80 ms while a knob is being dragged.
    void update (const ReverbMeasure::Setting& setting, bool immediately = false)
    {
        if (computed && setting == shown)
            return;

        pending = setting;
        const auto now = juce::Time::getMillisecondCounter();

        if (computed && ! immediately && now - lastCompute < 80)
            return;

        lastCompute = now;
        compute (pending);
    }

    double getMeasuredRt60() const noexcept { return measuredRt60; }

    void paint (juce::Graphics& g) override
    {
        const auto& theme = AbcTrainTheme::current();
        const auto bounds = getLocalBounds().toFloat();

        AbcTrainLookAndFeel::paintDisplayWell (g, bounds);
        AbcTrainLookAndFeel::drawRegistrationMarks (g, bounds, theme.outline);

        if (bins.empty())
            return;

        auto plot = bounds.reduced (18.0f, 26.0f);
        const auto xFor = [&] (double seconds) { return plot.getX() + (float) (seconds / span) * plot.getWidth(); };

        // Time grid, in steps a reader can count.
        const auto step = span > 4.0 ? 1.0 : span > 1.6 ? 0.5 : span > 0.6 ? 0.2 : 0.1;
        g.setFont (AbcTrainLookAndFeel::microFont());

        for (double tick = step; tick < span; tick += step)
        {
            const auto x = xFor (tick);
            g.setColour (theme.divider.withAlpha (0.6f));
            g.fillRect (x, bounds.getY() + 1.0f, 1.0f, bounds.getHeight() - 2.0f);
            g.setColour (theme.textDim.withAlpha (0.8f));
            g.drawText (number (tick, 1) + " " + text.seconds, juce::Rectangle<float> (x + 3.0f, bounds.getBottom() - 18.0f, 48.0f, 14.0f),
                        juce::Justification::centredLeft, false);
        }

        // The dry hit at zero.
        const auto x0 = xFor (0.0);
        g.setColour (accent);
        g.fillRect (x0 - 1.0f, plot.getY(), 2.0f, plot.getHeight());

        // Where the first reflection lands, dashed: the gap before it is
        // the pre-delay, and it is the thing that separates a voice from
        // its room.
        const auto xOnset = xFor (onset);
        {
            juce::Path dash;
            dash.startNewSubPath (xOnset, plot.getY());
            dash.lineTo (xOnset, plot.getBottom());
            const float pattern[] = { 3.0f, 3.0f };
            juce::PathStrokeType stroke (1.0f);
            juce::Path dashed;
            stroke.createDashedStroke (dashed, dash, pattern, 2);
            g.setColour (theme.textDim);
            g.fillPath (dashed);
        }

        // The energy, as bars centred on the middle line - the shape of a
        // tail reads better as something shrinking than as a curve falling.
        const auto binWidth = plot.getWidth() / (float) bins.size();
        const auto barWidth = juce::jmax (1.2f, binWidth * 0.55f);
        const auto mid = plot.getCentreY();

        for (size_t i = 0; i < bins.size(); ++i)
        {
            const auto db = bins[i];

            if (db <= floorDb)
                continue;

            const auto h = (db - floorDb) / -floorDb * plot.getHeight() * 0.92f;
            const auto x = plot.getX() + ((float) i + 0.5f) * binWidth;
            g.setColour (accent.withAlpha (0.35f + 0.6f * (db - floorDb) / -floorDb));
            g.fillRect (x - barWidth * 0.5f, mid - h * 0.5f, barWidth, h);
        }

        // Captions.
        g.setFont (AbcTrainLookAndFeel::captionFont());
        g.setColour (theme.text);
        g.drawText (text.dry, juce::Rectangle<float> (x0 + 5.0f, bounds.getY() + 6.0f, 80.0f, 16.0f),
                    juce::Justification::centredLeft, false);

        const auto onsetText = text.firstReflections.replace ("{{ms}}", juce::String (juce::roundToInt (onset * 1000.0)));
        const auto onsetX = juce::jmax (x0 + 60.0f, xOnset + 5.0f);
        g.drawText (onsetText, juce::Rectangle<float> (onsetX, bounds.getY() + 6.0f, bounds.getRight() - onsetX - 8.0f, 16.0f),
                    juce::Justification::centredLeft, true);

        if (measuredRt60 > 0.0)
        {
            g.setColour (theme.textBright);
            g.drawText (text.tail.replace ("{{s}}", number (measuredRt60, 2)),
                        juce::Rectangle<float> (bounds.getRight() - 220.0f, bounds.getBottom() - 20.0f, 208.0f, 16.0f),
                        juce::Justification::centredRight, false);
        }
    }

private:
    static constexpr double sampleRate = 22050.0;
    static constexpr float floorDb = -60.0f;

    juce::String number (double v, int decimals) const
    {
        return juce::String (v, decimals).replace (".", decimal);
    }

    void compute (const ReverbMeasure::Setting& setting)
    {
        shown = setting;
        computed = true;

        // Long enough to see the tail fall through the floor, short enough
        // that a 10-second hall does not become a flat line at the left.
        span = juce::jlimit (0.3, 6.0, setting.preDelayMs * 0.001 + setting.decaySeconds * 1.15 + 0.05);

        // The measurement runs longer than the picture, so the fit has the
        // whole decay to work with.
        const auto ir = ReverbMeasure::impulseResponse (setting, sampleRate, juce::jmax (span, setting.decaySeconds * 1.6 + 0.3));
        measuredRt60 = ReverbMeasure::rt60 (ir, sampleRate);
        onset = ReverbMeasure::onsetSeconds (ir, sampleRate);

        constexpr int numBins = 90;
        const auto perBin = juce::jmax (1, (int) (span * sampleRate / numBins));
        bins.assign ((size_t) numBins, floorDb);

        std::vector<double> energy ((size_t) numBins, 0.0);
        double peak = 0.0;

        for (int b = 0; b < numBins; ++b)
        {
            for (int i = b * perBin; i < (b + 1) * perBin && i < (int) ir.size(); ++i)
                energy[(size_t) b] += (double) ir[(size_t) i] * ir[(size_t) i];

            peak = juce::jmax (peak, energy[(size_t) b]);
        }

        if (peak > 0.0)
            for (int b = 0; b < numBins; ++b)
                bins[(size_t) b] = juce::jmax (floorDb, (float) (10.0 * std::log10 (juce::jmax (1.0e-12, energy[(size_t) b] / peak))));

        repaint();
    }

    Strings text;
    juce::Colour accent { juce::Colours::seagreen };
    juce::String decimal { "." };

    ReverbMeasure::Setting shown, pending;
    bool computed = false;
    juce::uint32 lastCompute = 0;

    std::vector<float> bins;
    double span = 1.0, measuredRt60 = 0.0, onset = 0.0;
};
