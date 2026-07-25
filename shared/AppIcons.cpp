#include "AppIcons.h"

namespace AppIcons
{
    namespace
    {
        juce::Path eqPath()
        {
            juce::Path p;
            p.startNewSubPath (5.0f, 3.0f); p.lineTo (5.0f, 21.0f);
            p.addEllipse (3.0f, 7.0f, 4.0f, 4.0f);
            p.startNewSubPath (12.0f, 3.0f); p.lineTo (12.0f, 21.0f);
            p.addEllipse (10.0f, 13.0f, 4.0f, 4.0f);
            p.startNewSubPath (19.0f, 3.0f); p.lineTo (19.0f, 21.0f);
            p.addEllipse (17.0f, 4.0f, 4.0f, 4.0f);
            return p;
        }

        juce::Path compressionPath()
        {
            juce::Path p;
            p.startNewSubPath (6.0f, 3.0f); p.lineTo (12.0f, 9.0f); p.lineTo (18.0f, 3.0f);
            p.startNewSubPath (6.0f, 21.0f); p.lineTo (12.0f, 15.0f); p.lineTo (18.0f, 21.0f);
            p.startNewSubPath (4.0f, 12.0f); p.lineTo (20.0f, 12.0f);
            return p;
        }

        juce::Path reverbPath()
        {
            juce::Path p;
            const juce::Point<float> origin (3.0f, 21.0f);
            for (float radius : { 6.0f, 11.0f, 16.0f })
                p.addCentredArc (origin.x, origin.y, radius, radius,
                                  0.0f, juce::MathConstants<float>::pi * 1.5f, juce::MathConstants<float>::twoPi, true);
            p.addEllipse (origin.x - 1.2f, origin.y - 1.2f, 2.4f, 2.4f);
            return p;
        }

        juce::Path panPath()
        {
            juce::Path p;
            p.startNewSubPath (3.0f, 12.0f); p.lineTo (21.0f, 12.0f);
            p.startNewSubPath (3.0f, 12.0f); p.lineTo (6.0f, 9.0f);
            p.startNewSubPath (3.0f, 12.0f); p.lineTo (6.0f, 15.0f);
            p.startNewSubPath (21.0f, 12.0f); p.lineTo (18.0f, 9.0f);
            p.startNewSubPath (21.0f, 12.0f); p.lineTo (18.0f, 15.0f);
            p.addEllipse (12.5f, 9.5f, 5.0f, 5.0f);
            return p;
        }

        juce::Path delayPath()
        {
            juce::Path p;
            p.addEllipse (0.0f, 8.0f, 8.0f, 8.0f);
            p.addEllipse (10.0f, 9.0f, 6.0f, 6.0f);
            p.addEllipse (18.0f, 10.0f, 4.0f, 4.0f);
            return p;
        }

        juce::Path distortionPath()
        {
            juce::Path p;
            p.startNewSubPath (2.0f, 16.0f);
            p.lineTo (5.0f, 16.0f); p.lineTo (7.0f, 6.0f); p.lineTo (9.0f, 6.0f);
            p.lineTo (11.0f, 18.0f); p.lineTo (13.0f, 18.0f);
            p.lineTo (15.0f, 4.0f); p.lineTo (17.0f, 4.0f);
            p.lineTo (19.0f, 14.0f); p.lineTo (22.0f, 14.0f);
            return p;
        }

        juce::Path stereoWidthPath()
        {
            juce::Path p;
            p.addEllipse (2.0f, 6.0f, 13.0f, 13.0f);
            p.addEllipse (9.0f, 6.0f, 13.0f, 13.0f);
            return p;
        }

        juce::Path gainPath()
        {
            juce::Path p;
            p.startNewSubPath (12.0f, 2.0f); p.lineTo (12.0f, 22.0f);
            p.startNewSubPath (12.0f, 2.0f); p.lineTo (9.0f, 6.0f);
            p.startNewSubPath (12.0f, 2.0f); p.lineTo (15.0f, 6.0f);
            p.startNewSubPath (12.0f, 22.0f); p.lineTo (9.0f, 18.0f);
            p.startNewSubPath (12.0f, 22.0f); p.lineTo (15.0f, 18.0f);
            p.startNewSubPath (8.0f, 12.0f); p.lineTo (16.0f, 12.0f);
            return p;
        }

        juce::Path frequencyRangePath()
        {
            juce::Path p;
            const float xs[] = { 3.0f, 7.0f, 11.0f, 15.0f, 19.0f };
            const float heights[] = { 8.0f, 14.0f, 18.0f, 10.0f, 6.0f };
            for (int i = 0; i < 5; ++i)
                p.addRectangle (xs[i] - 1.5f, 18.0f - heights[i], 3.0f, heights[i]);
            p.startNewSubPath (5.5f, 20.0f); p.lineTo (5.5f, 22.0f);
            p.startNewSubPath (5.5f, 22.0f); p.lineTo (16.5f, 22.0f);
            p.startNewSubPath (16.5f, 22.0f); p.lineTo (16.5f, 20.0f);
            return p;
        }

        juce::Path learnerEQPath()
        {
            juce::Path p;
            p.startNewSubPath (2.0f, 16.0f);
            p.cubicTo (7.0f, 16.0f, 8.0f, 4.0f, 12.0f, 4.0f);
            p.cubicTo (16.0f, 4.0f, 17.0f, 16.0f, 22.0f, 16.0f);
            return p;
        }

        juce::Path learnerCompPath()
        {
            juce::Path p;
            p.addRectangle (3.0f, 4.0f, 18.0f, 4.0f);
            p.addRectangle (3.0f, 16.0f, 12.0f, 4.0f);
            p.startNewSubPath (12.0f, 9.0f); p.lineTo (12.0f, 15.0f);
            p.startNewSubPath (9.0f, 12.0f); p.lineTo (12.0f, 15.0f);
            p.startNewSubPath (15.0f, 12.0f); p.lineTo (12.0f, 15.0f);
            return p;
        }

        juce::Path learnerVerbPath()
        {
            juce::Path p;
            for (float radius : { 3.0f, 7.0f, 11.0f })
                p.addEllipse (12.0f - radius, 12.0f - radius, radius * 2.0f, radius * 2.0f);
            return p;
        }
    }

    juce::Path getPath (Icon icon)
    {
        switch (icon)
        {
            case Icon::eq:             return eqPath();
            case Icon::compression:    return compressionPath();
            case Icon::reverb:         return reverbPath();
            case Icon::pan:            return panPath();
            case Icon::delay:          return delayPath();
            case Icon::distortion:     return distortionPath();
            case Icon::stereoWidth:    return stereoWidthPath();
            case Icon::gain:           return gainPath();
            case Icon::frequencyRange: return frequencyRangePath();
            case Icon::learnerEQ:      return learnerEQPath();
            case Icon::learnerComp:    return learnerCompPath();
            case Icon::learnerVerb:    return learnerVerbPath();
        }

        return eqPath();
    }

    Icon iconForGameName (const juce::String& englishName)
    {
        if (englishName == "Guess the Band")            return Icon::eq;
        if (englishName == "Guess the Compression")     return Icon::compression;
        if (englishName == "Guess the Reverb")          return Icon::reverb;
        if (englishName == "Guess the Pan Position")    return Icon::pan;
        if (englishName == "Guess the Delay Time")      return Icon::delay;
        if (englishName == "Guess the Distortion")      return Icon::distortion;
        if (englishName == "Guess the Stereo Width")    return Icon::stereoWidth;
        if (englishName == "Guess the Gain Change")     return Icon::gain;
        if (englishName == "Name the Range")            return Icon::frequencyRange;

        return Icon::eq; // an as-yet-unmapped game (see translateGameName's identical fallback shape)
    }

    void draw (juce::Graphics& g, Icon icon, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        auto path = getPath (icon);
        path.scaleToFit (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), true);

        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
