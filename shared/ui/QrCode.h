#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// A QR code for a URL, drawn with JUCE or written as SVG.
//
// Encoding is Project Nayuki's QR Code generator (MIT, vendored in
// third_party/), which is the reference implementation most libraries are
// checked against. This wrapper only decides the two things that matter for
// a code on a projector or a printed card:
//
// - error correction **M** (15 %): a projected code is photographed at an
//   angle, through moire, from the back of the room; M survives that and
//   still keeps the modules large. H would make the same URL a denser code.
// - a **quiet zone of four modules** on every side, as the standard asks.
//   Without it phones in dark rooms read the frame as data and fail.
//
// Dark modules are drawn in `ink`, the rest in `paper`. Always pass a dark
// ink on a light paper: an inverted QR code is legal, but many camera apps
// still refuse it, and a seminar is the wrong place to find out which.
class QrCode
{
public:
    explicit QrCode (const juce::String& text);

    bool isValid() const noexcept { return size > 0; }
    int getSize() const noexcept { return size; }          // modules per side, without the quiet zone
    bool isDark (int x, int y) const noexcept;

    // Draws into the largest square that fits `area`, snapped to whole
    // pixels per module so the edges stay sharp on a projector.
    void paint (juce::Graphics&, juce::Rectangle<float> area,
                juce::Colour ink = juce::Colours::black,
                juce::Colour paper = juce::Colours::white) const;

    // A standalone <svg>, for the printed code sheet.
    juce::String toSvg (int pixelsPerModule = 4) const;

    static constexpr int quietZone = 4;

private:
    int size = 0;
    std::vector<bool> modules;
};
