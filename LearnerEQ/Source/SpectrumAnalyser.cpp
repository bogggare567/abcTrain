#include "SpectrumAnalyser.h"
#include "FrequencyGuide.h"
#include "FrequencyZones.h"
#include "../../shared/AbcTrainTheme.h"
#include "../../shared/AbcTrainLookAndFeel.h"

namespace
{
    constexpr float zoneLabelHeight = 16.0f;
}

void SpectrumAnalyserComponent::setEQState (double sampleRate, std::vector<Band> activeBands)
{
    eqSampleRate = sampleRate;
    bands = std::move (activeBands);
    setSampleRate (sampleRate);
    repaint();
}

void SpectrumAnalyserComponent::setSelectedBand (int bandIndex) noexcept
{
    selectedBandIndex = bandIndex;
    repaint();
}

void SpectrumAnalyserComponent::setZonesVisible (bool shouldBeVisible)
{
    zonesVisible = shouldBeVisible;
    repaint();
}

juce::Rectangle<float> SpectrumAnalyserComponent::curveArea (juce::Rectangle<float> bounds) const
{
    // Trimmed from the *top*. The zone names went along the bottom first
    // and landed straight on the base class's own frequency labels - two
    // rows of text in one strip, reading "Sub 50 Bass 100 Boom Body 500".
    // The bottom belongs to the Hz axis; the sensations get their own band
    // above it.
    return bounds.withTrimmedTop (zonesVisible ? zoneLabelHeight : 0.0f);
}

float SpectrumAnalyserComponent::xForFrequency (float freqHz, juce::Rectangle<float> area)
{
    return area.getX() + area.getWidth() * FrequencyGuide::frequencyToProportion (freqHz);
}

float SpectrumAnalyserComponent::frequencyForX (float x, juce::Rectangle<float> area)
{
    const auto proportion = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / juce::jmax (1.0f, area.getWidth()));
    return FrequencyGuide::proportionToFrequency (proportion);
}

float SpectrumAnalyserComponent::yForGain (float gainDb, juce::Rectangle<float> area)
{
    return area.getY() + area.getHeight() * (0.5f - juce::jlimit (-maxDb, maxDb, gainDb) / (2.0f * maxDb));
}

float SpectrumAnalyserComponent::gainForY (float y, juce::Rectangle<float> area)
{
    const auto proportion = juce::jlimit (0.0f, 1.0f, (y - area.getY()) / juce::jmax (1.0f, area.getHeight()));
    return (0.5f - proportion) * 2.0f * maxDb;
}

const SpectrumAnalyserComponent::Band* SpectrumAnalyserComponent::findBand (int index) const
{
    for (const auto& band : bands)
        if (band.index == index)
            return &band;

    return nullptr;
}

int SpectrumAnalyserComponent::bandAtPosition (juce::Point<float> position) const
{
    const auto area = curveArea (getLocalBounds().toFloat());

    auto best = -1;
    auto bestDistance = nodeRadius * 2.4f;   // a generous but bounded grab radius

    for (const auto& band : bands)
    {
        const auto centre = juce::Point<float> (
            xForFrequency (band.freqHz, area),
            EQCoefficients::usesGain (band.type) ? yForGain (band.gainDb, area) : area.getCentreY());

        const auto distance = centre.getDistanceFrom (position);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = band.index;
        }
    }

    return best;
}

juce::Path SpectrumAnalyserComponent::buildResponseCurvePath (juce::Rectangle<float> bounds) const
{
    constexpr int numPoints = 512;
    const auto area = curveArea (bounds);
    juce::Path path;

    for (int i = 0; i < numPoints; ++i)
    {
        const auto proportion = (float) i / (float) (numPoints - 1);
        const auto freq = FrequencyGuide::proportionToFrequency (proportion);

        // Summed in dB, which is what putting filters in series does to
        // the magnitude response. Every active band contributes whatever
        // its own type says at this frequency - a pass filter's skirt is
        // just as much a part of the curve as a bell's bump.
        float totalDb = 0.0f;

        for (const auto& band : bands)
        {
            const auto coeffs = EQCoefficients::make (band.type, eqSampleRate,
                                                       band.freqHz, band.gainDb, band.q);
            totalDb += juce::Decibels::gainToDecibels (
                coeffs->getMagnitudeForFrequency ((double) freq, eqSampleRate));
        }

        const auto x = area.getX() + area.getWidth() * proportion;
        const auto y = yForGain (totalDb, area);

        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }

    return path;
}

void SpectrumAnalyserComponent::paintZones (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    const auto& theme = AbcTrainTheme::current();
    const auto area = curveArea (bounds);
    const auto labelStrip = bounds.withBottom (area.getY());

    for (size_t i = 0; i < FrequencyZones::all.size(); ++i)
    {
        const auto& zone = FrequencyZones::all[i];

        const auto left = xForFrequency (zone.lowHz, area);
        const auto right = xForFrequency (zone.highHz, area);
        const auto strip = juce::Rectangle<float> (left, area.getY(), right - left, area.getHeight());

        const auto underPointer = pointerFreq >= zone.lowHz && pointerFreq < zone.highHz;

        // The zone under the pointer lifts; the rest are alternating
        // near-nothing. Eight distinct colours would turn the analyser
        // into a rainbow and fight the curve for attention.
        g.setColour (underPointer ? theme.accent.withAlpha (0.10f)
                                  : theme.textDim.withAlpha (FrequencyZones::shadeFor ((int) i)));
        g.fillRect (strip);

        if (i > 0)
        {
            g.setColour (theme.outline.withAlpha (0.35f));
            g.drawLine (left, area.getY(), left, area.getBottom(), 1.0f);
        }

        // The name only, in the strip below. What the zone *feels* like is
        // a sentence, and a sentence per zone across the width would be a
        // wall of text - the editor puts that one line under the pointer
        // instead.
        const auto labelBox = juce::Rectangle<float> (left, labelStrip.getY(),
                                                       right - left, labelStrip.getHeight());

        if (labelBox.getWidth() > 26.0f)
        {
            g.setColour (underPointer ? theme.accent : theme.textDim.withAlpha (0.55f));
            g.setFont (AbcTrainLookAndFeel::microFont());
            g.drawText (zone.name, labelBox, juce::Justification::centred, false);
        }
    }
}

void SpectrumAnalyserComponent::paintNodes (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    const auto& theme = AbcTrainTheme::current();
    const auto area = curveArea (bounds);

    for (const auto& band : bands)
    {
        const auto usesGain = EQCoefficients::usesGain (band.type);
        const auto x = xForFrequency (band.freqHz, area);
        const auto y = usesGain ? yForGain (band.gainDb, area) : area.getCentreY();

        const auto isSelected = band.index == selectedBandIndex;
        const auto isHovered = band.index == hoveredBandIndex;
        const auto radius = nodeRadius * (isSelected ? 1.22f : isHovered ? 1.1f : 1.0f);

        // A pass filter has no gain, so its node has no meaningful height
        // - it gets a full-height guide line instead, which is also what
        // its shape actually does to the signal.
        if (! usesGain)
        {
            g.setColour (theme.accentWarm.withAlpha (isSelected ? 0.55f : 0.3f));
            g.drawLine (x, area.getY(), x, area.getBottom(), isSelected ? 2.0f : 1.0f);
        }

        g.setColour (theme.accent.withAlpha (isSelected ? 0.32f : 0.16f));
        g.fillEllipse (x - radius * 1.9f, y - radius * 1.9f, radius * 3.8f, radius * 3.8f);

        g.setColour (usesGain ? theme.accent : theme.accentWarm);
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour (theme.displayBackground.withAlpha (0.9f));
        g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }
}

void SpectrumAnalyserComponent::paintOverlay (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto& theme = AbcTrainTheme::current();

    if (zonesVisible)
        paintZones (g, bounds);

    const auto area = curveArea (bounds);

    // The 0 dB line: without it a flat curve and a curve pushed off the
    // top look the same.
    g.setColour (theme.outline.withAlpha (0.5f));
    g.drawLine (area.getX(), area.getCentreY(), area.getRight(), area.getCentreY(), 1.0f);

    const auto curve = buildResponseCurvePath (bounds);

    g.setColour (theme.accent.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));

    g.setColour (theme.accent);
    g.strokePath (curve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

    paintNodes (g, bounds);
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

void SpectrumAnalyserComponent::mouseDown (const juce::MouseEvent& event)
{
    const auto hit = bandAtPosition (event.position);

    if (hit >= 0)
    {
        draggingBandIndex = hit;
        selectedBandIndex = hit;

        if (onBandSelected != nullptr)
            onBandSelected (hit);

        repaint();
    }
}

void SpectrumAnalyserComponent::mouseDrag (const juce::MouseEvent& event)
{
    if (draggingBandIndex < 0 || onBandMoved == nullptr)
        return;

    const auto area = curveArea (getLocalBounds().toFloat());
    const auto freq = frequencyForX (event.position.x, area);
    const auto gain = gainForY (event.position.y, area);

    onBandMoved (draggingBandIndex, freq, gain);
    pointerFreq = freq;
    repaint();
}

void SpectrumAnalyserComponent::mouseUp (const juce::MouseEvent&)
{
    draggingBandIndex = -1;
    repaint();
}

void SpectrumAnalyserComponent::mouseMove (const juce::MouseEvent& event)
{
    const auto area = curveArea (getLocalBounds().toFloat());
    const auto previousHover = hoveredBandIndex;

    hoveredBandIndex = bandAtPosition (event.position);
    pointerFreq = frequencyForX (event.position.x, area);

    setMouseCursor (hoveredBandIndex >= 0 ? juce::MouseCursor::DraggingHandCursor
                                          : juce::MouseCursor::CrosshairCursor);

    if (onPointerMoved != nullptr)
        onPointerMoved();

    if (hoveredBandIndex != previousHover || zonesVisible)
        repaint();
}

void SpectrumAnalyserComponent::mouseExit (const juce::MouseEvent&)
{
    hoveredBandIndex = -1;
    pointerFreq = -1.0f;

    if (onPointerMoved != nullptr)
        onPointerMoved();

    repaint();
}

void SpectrumAnalyserComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    // Double-click a node to remove it, empty space to add one. The same
    // gesture in two places, because "this one" and "here" is the whole
    // vocabulary the surface has.
    const auto hit = bandAtPosition (event.position);

    if (hit >= 0)
    {
        if (onBandRemoved != nullptr)
            onBandRemoved (hit);

        return;
    }

    if (onBandAdded == nullptr)
        return;

    const auto area = curveArea (getLocalBounds().toFloat());
    onBandAdded (frequencyForX (event.position.x, area), gainForY (event.position.y, area));
}

void SpectrumAnalyserComponent::mouseWheelMove (const juce::MouseEvent& event,
                                                 const juce::MouseWheelDetails& wheel)
{
    // Q on the wheel, over the node it belongs to. Nothing happens when
    // the pointer is not over a node, rather than the wheel silently
    // editing whatever was last selected.
    const auto target = bandAtPosition (event.position);

    if (target < 0 || onBandQChanged == nullptr)
        return;

    const auto* band = findBand (target);

    if (band == nullptr)
        return;

    // Multiplied, not added: Q is a ratio, and a fixed step would crawl at
    // 0.2 and leap at 12.
    const auto factor = std::pow (1.25f, wheel.deltaY > 0.0f ? 1.0f : -1.0f);
    onBandQChanged (target, juce::jlimit (0.1f, 18.0f, band->q * factor));
}
