#include "QrCode.h"
#include "shared/ui/third_party/qrcodegen.hpp"

QrCode::QrCode (const juce::String& text)
{
    try
    {
        const auto qr = qrcodegen::QrCode::encodeText (text.toRawUTF8(), qrcodegen::QrCode::Ecc::MEDIUM);
        size = qr.getSize();
        modules.resize ((size_t) (size * size));

        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                modules[(size_t) (y * size + x)] = qr.getModule (x, y);
    }
    catch (...)
    {
        // Longer than a QR code can hold: nothing to draw. Every caller
        // also shows the address as text, so the room stays reachable.
        size = 0;
        modules.clear();
    }
}

bool QrCode::isDark (int x, int y) const noexcept
{
    if (x < 0 || y < 0 || x >= size || y >= size)
        return false;

    return modules[(size_t) (y * size + x)];
}

void QrCode::paint (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, juce::Colour paper) const
{
    if (! isValid())
        return;

    const auto total = size + 2 * quietZone;
    const auto side = juce::jmin (area.getWidth(), area.getHeight());
    const auto module = juce::jmax (1.0f, std::floor (side / (float) total));
    const auto drawn = module * (float) total;
    const auto origin = area.withSizeKeepingCentre (drawn, drawn).getTopLeft();
    const auto x0 = std::round (origin.x), y0 = std::round (origin.y);

    g.setColour (paper);
    g.fillRect (x0, y0, drawn, drawn);

    // One rectangle per horizontal run of dark modules: a 57x57 code is a
    // few hundred fills instead of a couple of thousand.
    juce::RectangleList<float> dark;

    for (int y = 0; y < size; ++y)
    {
        int x = 0;

        while (x < size)
        {
            if (! isDark (x, y))
            {
                ++x;
                continue;
            }

            const auto start = x;
            while (x < size && isDark (x, y))
                ++x;

            dark.addWithoutMerging ({ x0 + (float) (start + quietZone) * module,
                                      y0 + (float) (y + quietZone) * module,
                                      (float) (x - start) * module, module });
        }
    }

    g.setColour (ink);
    g.fillRectList (dark);
}

juce::String QrCode::toSvg (int pixelsPerModule) const
{
    if (! isValid())
        return {};

    const auto total = size + 2 * quietZone;
    juce::String path;

    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (isDark (x, y))
                path << "M" << (x + quietZone) << "," << (y + quietZone) << "h1v1h-1z";

    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + juce::String (total) + " " + juce::String (total)
         + "\" width=\"" + juce::String (total * pixelsPerModule) + "\" height=\"" + juce::String (total * pixelsPerModule)
         + "\" shape-rendering=\"crispEdges\"><rect width=\"100%\" height=\"100%\" fill=\"#fff\"/><path d=\""
         + path + "\" fill=\"#000\"/></svg>";
}
