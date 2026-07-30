#pragma once

#include "../../shared/SpectrumAnalyzer.h"
#include "EQCoefficients.h"
#include <functional>
#include <vector>

// The EQ's working surface, not a picture of one.
//
// The live spectrum comes from shared/SpectrumAnalyzer (see
// decisions/006-unified-visualization.md for why it was extracted). This
// class adds everything that makes the display the *instrument*: the
// combined response curve, a draggable node per active band, and the
// named frequency zones drawn behind it all.
//
// It used to be read-only, with twelve rotary knobs underneath doing the
// work. That shape teaches the wrong reflex. An EQ move begins as
// "something is wrong around *there*", and a row of fixed slots makes you
// translate that into which knob to reach for before you have decided what
// the move even is. Point at the place instead: drag a node to move it,
// double-click empty space to make one, scroll to change Q, double-click a
// node to remove it.
class SpectrumAnalyserComponent : public SpectrumAnalyzerComponent
{
public:
    SpectrumAnalyserComponent() = default;

    struct Band
    {
        int index = -1;                 // the processor's band slot
        EQCoefficients::BandType type = EQCoefficients::BandType::bell;
        float freqHz = 1000.0f;
        float gainDb = 0.0f;
        float q = 0.7f;
    };

    // Called from the message thread once per frame with the current
    // parameter values, so the curve and the nodes track the plugin even
    // with no audio playing and even while a host automates it.
    void setEQState (double sampleRate, std::vector<Band> activeBands);

    void setSelectedBand (int bandIndex) noexcept;
    int getSelectedBand() const noexcept { return selectedBandIndex; }

    // Zones are the point of the redesign, but they are also a strong
    // graphic - anyone who already has the map in their head can turn
    // them off.
    void setZonesVisible (bool shouldBeVisible);
    bool areZonesVisible() const noexcept { return zonesVisible; }

    // What the pointer is currently over, for the editor's readout line.
    // Negative when the pointer is outside.
    float getPointerFrequency() const noexcept { return pointerFreq; }

    // ---- what the editor wires up ----
    // Every one of these is a request, never a direct write: the editor
    // owns the APVTS attachments, so this component asks and the editor
    // decides. That keeps undo, automation and host notification in one
    // place rather than two.
    std::function<void (int band, float freqHz, float gainDb)> onBandMoved;
    std::function<void (int band, float q)> onBandQChanged;
    std::function<void (float freqHz, float gainDb)> onBandAdded;
    std::function<void (int band)> onBandRemoved;
    std::function<void (int band)> onBandSelected;
    std::function<void()> onPointerMoved;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

protected:
    void paintOverlay (juce::Graphics&, juce::Rectangle<float> bounds) override;

private:
    static constexpr float maxDb = 18.0f;
    static constexpr float nodeRadius = 7.0f;

    juce::Path buildResponseCurvePath (juce::Rectangle<float> bounds) const;
    void paintZones (juce::Graphics&, juce::Rectangle<float> bounds) const;
    void paintNodes (juce::Graphics&, juce::Rectangle<float> bounds) const;

    // The area the curve and nodes live in. The zone labels take a strip
    // along the bottom, and a node dragged into that strip would sit on
    // top of its own label.
    juce::Rectangle<float> curveArea (juce::Rectangle<float> bounds) const;

    static float xForFrequency (float freqHz, juce::Rectangle<float> area);
    static float yForGain (float gainDb, juce::Rectangle<float> area);
    static float frequencyForX (float x, juce::Rectangle<float> area);
    static float gainForY (float y, juce::Rectangle<float> area);

    // Which node the pointer is over, or -1. By distance to the centre
    // rather than by a bounding box: the nodes are circles, and square hit
    // areas on a dense curve grab the wrong one.
    int bandAtPosition (juce::Point<float> position) const;
    const Band* findBand (int index) const;

    double eqSampleRate = 44100.0;
    std::vector<Band> bands;

    int selectedBandIndex = -1;
    int hoveredBandIndex = -1;
    int draggingBandIndex = -1;
    bool zonesVisible = true;

    float pointerFreq = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyserComponent)
};
